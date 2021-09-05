
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "stb/stb_image.h"

#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <talloc.h>

#include "blobby.h"
#include "helpers.h"
#include "trtl.h"
#include "vertex.h"

#include "turtle.h"

#include "stringlist.h"
#include "trtl_object.h"
#include "trtl_seer.h"
#include "trtl_uniform.h"

struct trtl_stringlist *objs_to_load[TRTL_RENDER_LAYER_TOTAL] = {NULL};

struct trtl_uniform *evil_global_uniform;

#define MIN(x, y)                                                                                  \
	({                                                                                         \
		typeof(x) _x = (x);                                                                \
		typeof(y) _y = (y);                                                                \
		(void)(&_x == &_y);                                                                \
		_x < _y ? _x : _y;                                                                 \
	})

#define MAX(x, y)                                                                                  \
	({                                                                                         \
		typeof(x) _x = (x);                                                                \
		typeof(y) _y = (y);                                                                \
		(void)(&_x == &_y);                                                                \
		_x > _y ? _x : _y;                                                                 \
	})

enum trtl_debug {
	TRTL_DEBUG_ERROR = 0,
	TRTL_DEBUG_WARNING = 1,
	TRTL_DEBUG_INFO = 2,
	TRTL_DEBUG_VERBOSE = 3,
	TRTL_DEBUG_DEBUG = 4,
};

static enum trtl_debug debug = TRTL_DEBUG_INFO;

static void
verbose(enum trtl_debug msg_level, const char *fmt, ...)
{
	if (msg_level <= debug) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

#define LOG(x, ...) verbose(TRTL_DEBUG_##x, __VA_ARGS__)

const int MAX_FRAMES_IN_FLIGHT = 2;
static const char *required_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
};
#define N_REQUIRED_EXTENSIONS TRTL_ARRAY_SIZE(required_extensions)

static const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

// Belongs in render frame state
static bool frame_buffer_resized = false;

// Context stored with the main window.
struct window_context {
};
static int swap_chain_data_destructor(struct swap_chain_data *scd);

trtl_alloc static VkDescriptorPool create_descriptor_pool(struct swap_chain_data *scd);
static struct queue_family_indices find_queue_families(VkPhysicalDevice device,
						       VkSurfaceKHR surface);
static void create_image(struct render_context *render, uint32_t width, uint32_t height,
			 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
			 VkMemoryPropertyFlags properties, VkImage *image,
			 VkDeviceMemory *imageMemory);

// This has a big fixme on it
VkDebugUtilsMessengerEXT debugMessenger;

/** End Generic */

/** Window stuff (glfw) */

static void
window_resize_cb(trtl_arg_unused GLFWwindow *window, trtl_arg_unused int width,
		 trtl_arg_unused int height)
{
	frame_buffer_resized = true;
	if (debug) printf("Window resized\n");
}

int posX = 0;
int posY = 0;

void
key_callback(trtl_arg_unused GLFWwindow *window, int key, trtl_arg_unused int scancode, int action,
	     trtl_arg_unused int mods)
{
	if (key == GLFW_KEY_O && action == GLFW_PRESS) printf("Got message");

	if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_RIGHT:
			if (posX < 8) posX++;
			break;
		case GLFW_KEY_LEFT:
			if (posX > 0) posX--;
			break;
		case GLFW_KEY_DOWN:
			if (posY < 8) posY++;
			break;
		case GLFW_KEY_UP:
			if (posY > 0) posY--;
			break;
		}
	}
}

GLFWwindow *
window_init(void)
{
	GLFWwindow *window;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, window_resize_cb);

	glfwSetKeyCallback(window, key_callback);

	return window;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(trtl_arg_unused VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	      trtl_arg_unused VkDebugUtilsMessageTypeFlagsEXT messageType,
	      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	      trtl_arg_unused void *pUserData)
{
	LOG(VERBOSE, "Validation Layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT
populate_debug_messenger_create_info(void)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;

	return createInfo;
}

VkInstance
createInstance(trtl_arg_unused GLFWwindow *window)
{
	VkInstance instance;

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfwExtensionCount;
	const char **glfwExtensions;
	char **allExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	printf("Need %d extensions\n", glfwExtensionCount);
	for (uint32_t i = 0; i < glfwExtensionCount; i++) {
		printf("  - %s\n", glfwExtensions[i]);
	}

	allExtensions = talloc_zero_array(NULL, char *, glfwExtensionCount + 1);
	memcpy(allExtensions, glfwExtensions, glfwExtensionCount * sizeof(char *));
	allExtensions[glfwExtensionCount] = strdup("VK_KHR_get_physical_device_properties2");

	VkDebugUtilsMessengerCreateInfoEXT debug_create_info =
	    populate_debug_messenger_create_info();

	VkInstanceCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = glfwExtensionCount + 1;
	createInfo.ppEnabledExtensionNames = (const char *const *)allExtensions;
	// FIXME: A flag to toggle validation layers
	// createInfo.enabledLayerCount = 0;
	// createInfo.pNext = NULL;
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = &VALIDATION_LAYER;
	createInfo.pNext = &debug_create_info;

	if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
		error("Unable tp create instance");
		return 0;
	}
	return instance;
}

struct queue_family_indices {
	uint32_t graphics_family;
	uint32_t present_family;
	bool has_graphics;
	bool has_present;
};

struct swap_chain_support_details {
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t nformats;
	uint32_t npresentmodes;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *presentModes;
};

static bool
check_device_extension_support(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	VkExtensionProperties *available_extensions;

	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

	available_extensions = talloc_array(NULL, VkExtensionProperties, extensionCount);
	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, available_extensions);

	uint32_t j;
	for (uint32_t i = 0; i < N_REQUIRED_EXTENSIONS; i++) {
		for (j = 0; j < extensionCount; j++) {
			LOG(VERBOSE, "Found Extension: %s (%d)\n",
			    available_extensions[j].extensionName,
			    available_extensions[j].specVersion);
			if (strcmp(required_extensions[i], available_extensions[j].extensionName) ==
			    0) {
				break;
			}
		}
		if (j == extensionCount) {
			return false;
		}
	}
	return true;
}

static struct swap_chain_support_details *
query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	struct swap_chain_support_details *details;
	details = talloc(NULL, struct swap_chain_support_details);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats, NULL);

	if (details->nformats != 0) {
		details->formats = talloc_array(details, VkSurfaceFormatKHR, details->nformats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats,
						     details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes, NULL);

	if (details->npresentmodes != 0) {
		details->presentModes =
		    talloc_array(details, VkPresentModeKHR, details->npresentmodes);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes,
							  details->presentModes);
	}

	return details;
}

static bool
is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	struct queue_family_indices indices = find_queue_families(physical_device, surface);
	struct swap_chain_support_details *swap_chain_support;

	bool extensionsSupported = check_device_extension_support(physical_device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		swap_chain_support = query_swap_chain_support(physical_device, surface);
		if (swap_chain_support->npresentmodes > 0 && swap_chain_support->nformats > 0) {
			swapChainAdequate = true;
		}
		talloc_free(swap_chain_support);
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(physical_device, &supportedFeatures);

	return indices.has_present && indices.has_present && extensionsSupported &&
	       swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

VkSurfaceKHR
create_surface(VkInstance instance, GLFWwindow *window)
{
	VkSurfaceKHR surface;
	VkResult result;
	printf("Instance: %lld, Winf %p\n", (uint64_t)instance, window);
	result = glfwCreateWindowSurface(instance, window, NULL, &surface);
	if (result != VK_SUCCESS) {
		printf("failed to create window surface! %d\n", result);
		error("bailing");
	}
	return surface;
}

VkPhysicalDevice
pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t deviceCount = 0;
	// Urgh; leaky leaky leak
	VkPhysicalDevice *devices;
	VkPhysicalDevice candidate;

	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	if (deviceCount == 0) {
		error("failed to find GPUs with Vulkan support!");
		return NULL;
	}

	devices = talloc_array(NULL, VkPhysicalDevice, deviceCount);

	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
	printf("Found %d devices\n", deviceCount);
	candidate = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < deviceCount; i++) {
		if (is_device_suitable(devices[i], surface)) {
			candidate = devices[i];
			break;
		}
	}

	if (candidate == VK_NULL_HANDLE) {
		error("Unable to find suitable device\n");
	}

	return candidate;
}

static struct queue_family_indices
find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	struct queue_family_indices indices = {0, 0, false, false};
	VkQueueFamilyProperties *properties;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

	properties = talloc_array(NULL, VkQueueFamilyProperties, queueFamilyCount);
	memset(properties, 0, sizeof(VkQueueFamilyProperties) * queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, properties);

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		VkQueueFamilyProperties *queue_family = properties + i;

		if (!indices.has_graphics) {
			if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphics_family = i;
				indices.has_graphics = true;
				continue;
			}
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.present_family = i;
			indices.has_present = true;
		}

		if (indices.has_present && indices.has_graphics) {
			break;
		}
	}

	return indices;
}

static VkFormat
find_supported_format(VkPhysicalDevice physical_device, uint32_t ncandidates, VkFormat *candidates,
		      VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (uint32_t i = 0; i < ncandidates; i++) {
		VkFormat format = candidates[i];
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR &&
		    (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
			   (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	error("Failed to find a supported format");
}

VkFormat
find_depth_format(VkPhysicalDevice physical_device)
{
	VkFormat formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
			      VK_FORMAT_D24_UNORM_S8_UINT};

	return find_supported_format(physical_device, TRTL_ARRAY_SIZE(formats), formats,
				     VK_IMAGE_TILING_OPTIMAL,
				     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static VkDevice
create_logical_device(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue *graphicsQueue,
		      VkQueue *presentQueue)
{
	struct queue_family_indices queue_family_indices;
	float queue_priority = 1.0f;
	VkDevice device;
	VkDeviceQueueCreateInfo queue_info[2] = {0};
	VkPhysicalDeviceFeatures device_features = {
	    .samplerAnisotropy = VK_TRUE,
	};
	VkDeviceCreateInfo device_info = {
	    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .queueCreateInfoCount = TRTL_ARRAY_SIZE(queue_info),
	    .pQueueCreateInfos = queue_info,
	    .enabledLayerCount = 0,
	    .ppEnabledLayerNames = NULL,
	    .enabledExtensionCount = N_REQUIRED_EXTENSIONS,
	    .ppEnabledExtensionNames = required_extensions,
	    .pEnabledFeatures = &device_features,
	};

	// Once for graphics, once for present
	queue_family_indices = find_queue_families(physicalDevice, surface);

	queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info[0].queueFamilyIndex = queue_family_indices.graphics_family;
	queue_info[0].queueCount = 1;
	queue_info[0].pQueuePriorities = &queue_priority;

	queue_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info[1].queueFamilyIndex = queue_family_indices.present_family;
	queue_info[1].queueCount = 1;
	queue_info[1].pQueuePriorities = &queue_priority;
	// FIXME: Should be distinct
	printf("queue_info: %d %d\n", queue_family_indices.graphics_family,
	       queue_family_indices.present_family);

	VkResult result = vkCreateDevice(physicalDevice, &device_info, NULL, &device);
	if (result != VK_SUCCESS) {
		error_msg(result, "vkCreateDevice");
	}

	vkGetDeviceQueue(device, queue_family_indices.graphics_family, 0, graphicsQueue);
	vkGetDeviceQueue(device, queue_family_indices.present_family, 0, presentQueue);

	return device;
}

const VkSurfaceFormatKHR *
chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t nformats)
{
	for (uint32_t i = 0; i < nformats; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
		    availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats + i;
		}
	}

	return availableFormats;
}

VkPresentModeKHR
chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes, uint32_t npresentmodes)
{
	for (uint32_t i = 0; i < npresentmodes; i++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
chooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities)
{
	if (capabilities->currentExtent.width != UINT32_MAX) {
		return capabilities->currentExtent;
	} else {
		int width, height;
		width = 800;
		height = 600;
		fprintf(stderr, "Hardcoded size you fools!\n");
		// glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
		    .width = width,
		    .height = height,
		};

		actualExtent.width =
		    MAX(capabilities->minImageExtent.width,
			MIN(capabilities->maxImageExtent.width, actualExtent.width));
		actualExtent.height =
		    MAX(capabilities->minImageExtent.height,
			MIN(capabilities->maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

struct swap_chain_data *
create_swap_chain(struct turtle *turtle, VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	struct swap_chain_data *scd = talloc_zero(NULL, struct swap_chain_data);
	talloc_set_destructor(scd, swap_chain_data_destructor);

	struct swap_chain_support_details *swapChainSupport =
	    query_swap_chain_support(physical_device, surface);

	const VkSurfaceFormatKHR *surfaceFormat =
	    chooseSwapSurfaceFormat(swapChainSupport->formats, swapChainSupport->nformats);
	VkPresentModeKHR presentMode =
	    chooseSwapPresentMode(swapChainSupport->presentModes, swapChainSupport->npresentmodes);
	VkExtent2D extent = chooseSwapExtent(&swapChainSupport->capabilities);

	// FIXME: Why is this '[1]'??  That seems ... wrong
	uint32_t imageCount = swapChainSupport[0].capabilities.minImageCount + 1;
	if (swapChainSupport->capabilities.maxImageCount > 0 &&
	    imageCount > swapChainSupport->capabilities.maxImageCount) {
		imageCount = swapChainSupport->capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat->format;
	createInfo.imageColorSpace = surfaceFormat->colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	struct queue_family_indices queue_family_indices;
	queue_family_indices = find_queue_families(physical_device, surface);
	uint32_t queueFamilyIndices[2];

	if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		queueFamilyIndices[0] = queue_family_indices.graphics_family;
		queueFamilyIndices[1] = queue_family_indices.present_family;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport->capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(turtle->device, &createInfo, NULL, &scd->swap_chain) !=
	    VK_SUCCESS) {
		error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(turtle->device, scd->swap_chain, &scd->nimages, NULL);
	scd->images = talloc_array(scd, VkImage, scd->nimages);
	vkGetSwapchainImagesKHR(turtle->device, scd->swap_chain, &scd->nimages, scd->images);

	turtle->image_format = surfaceFormat->format;
	scd->extent = extent;

	return scd;
}

static VkImageView
create_image_view(struct render_context *render, VkImage image, VkFormat format,
		  VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo viewInfo = {0};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect_flags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(render->turtle->device, &viewInfo, NULL, &imageView) != VK_SUCCESS) {
		error("failed to create texture image view!");
	}

	return imageView;
}

void
create_image_views(trtl_arg_unused VkDevice device, struct swap_chain_data *scd)
{
	scd->image_views = talloc_array(scd, VkImageView, scd->nimages);

	for (uint32_t i = 0; i < scd->nimages; i++) {
		scd->image_views[i] =
		    create_image_view(scd->render, scd->images[i],
				      scd->render->turtle->image_format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

static VkSampler
create_texture_sampler(struct render_context *render)
{
	VkSampler sampler;
	VkPhysicalDeviceProperties properties = {0};
	vkGetPhysicalDeviceProperties(render->turtle->physical_device, &properties);

	VkSamplerCreateInfo sampler_info = {0};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(render->turtle->device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
		error("failed to create texture sampler!");
	}
	return sampler;
}

VkCommandBuffer
beginSingleTimeCommands(struct render_context *render)
{
	VkCommandBufferAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = render->scd->command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(render->turtle->device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void
endSingleTimeCommands(struct render_context *render, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(render->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(render->graphicsQueue);

	vkFreeCommandBuffers(render->turtle->device, render->scd->command_pool, 1, &commandBuffer);
}

VkDescriptorSetLayout
create_descriptor_set_layout(struct render_context *render)
{
	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.pImmutableSamplers = NULL;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding sampler_layout_binding = {0};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = NULL;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0] = ubo_layout_binding;
	bindings[1] = sampler_layout_binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = TRTL_ARRAY_SIZE(bindings);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(render->turtle->device, &layoutInfo, NULL,
					&descriptor_set_layout) != VK_SUCCESS) {
		error("failed to create descriptor set layout!");
	}
	return descriptor_set_layout;
}

VkCommandPool
create_command_pool(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	VkCommandPool command_pool;
	struct queue_family_indices qfi = find_queue_families(physical_device, surface);

	VkCommandPoolCreateInfo pool_info = {0};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = qfi.graphics_family;
	pool_info.flags = 0;

	if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
		error("Failed to create command pool");
	}

	return command_pool;
}

/**
 *
 * We only need one depth image view as only one render pass is running
 * at a time.
 */
static void
create_depth_resources(struct swap_chain_data *scd)
{
	VkFormat depthFormat = find_depth_format(scd->render->turtle->physical_device);

	create_image(scd->render, scd->extent.width, scd->extent.height, depthFormat,
		     VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &scd->depth_image,
		     &scd->depth_image_memory);
	scd->depth_image_view = create_image_view(scd->render, scd->depth_image, depthFormat,
						  VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkSemaphore
create_semaphores(VkDevice device)
{
	VkSemaphoreCreateInfo sem_info = {0};
	VkSemaphore sem;
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(device, &sem_info, NULL, &sem)) {
		error("failed tocreate sem");
	}
	return sem;
}

VkFence
create_fences(VkDevice device)
{
	VkFenceCreateInfo fenceInfo = {0};
	VkFence fence;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(device, &fenceInfo, NULL, &fence) != VK_SUCCESS) {
		error("Failed to create fence");
	}
	return fence;
}

static int
swap_chain_data_destructor(struct swap_chain_data *scd)
{
	VkDevice device;
	uint32_t i;

	device = scd->render->turtle->device;

	vkFreeCommandBuffers(device, scd->command_pool, scd->nbuffers, scd->command_buffers);

	// FIXME: There are multiple pipelines now
	// vkDestroyPipeline(device, *scd->pipelines, NULL);
	// vkDestroyPipelineLayout(device, scd->pipeline_layout, NULL);
	// vkDestroyRenderPass(device, scd->render_pass, NULL);

	for (i = 0; i < scd->nimages; i++) {
		vkDestroyImageView(device, scd->image_views[i], NULL);
	}

	vkDestroySwapchainKHR(device, scd->swap_chain, NULL);

	vkDestroyDescriptorPool(device, scd->descriptor_pool, NULL);

	return 0;
}

static int
render_context_destructor(trtl_arg_unused struct render_context *render)
{
	// FIXME: Destructor for the objects should do this
	// vkDestroyImage(render->turtle->device, render->texture_image, NULL);
	// vkFreeMemory(render->turtle->device, render->texture_image_memory, NULL);
	return 0;
}

void
recreate_swap_chain(struct render_context *render)
{
	int width = 0, height = 0;
	VkExtent2D size;

	glfwGetFramebufferSize(render->turtle->window, &width, &height);
	size.width = width;
	size.height = height;

	// a width/height of 0 is minimised, just wait for
	// events to recover (for now);
	while (width == 0 || height == 0) {
		printf("Size 0, wait\n");
		glfwWaitEvents();
		glfwGetFramebufferSize(render->turtle->window, &width, &height);
	}

	vkDeviceWaitIdle(render->turtle->device);

	talloc_free(render->scd);

	render->scd = create_swap_chain(render->turtle, render->turtle->physical_device,
					render->turtle->surface);
	struct swap_chain_data *scd = render->scd;
	scd->render = render;
	create_image_views(render->turtle->device, render->scd);
	render->descriptor_set_layout = create_descriptor_set_layout(render);

	scd->descriptor_pool = create_descriptor_pool(scd);

	scd->command_pool = create_command_pool(
	    render->turtle->device, render->turtle->physical_device, render->turtle->surface);

	trtl_seer_resize(size, scd);
	// info =trtl_pipeline_create(render->turtle->device, scd);

	// FIXME: Call object to update it's descriptor sets
	// scd->descriptor_sets = create_descriptor_sets(scd);
	create_depth_resources(scd);
	//	scd->framebuffers = create_frame_buffers(render->turtle->device, scd,
	//			);
	scd->command_buffers = trtl_seer_create_command_buffers(scd, scd->command_pool);

	for (uint32_t i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}
}

trtl_alloc static VkDescriptorPool
create_descriptor_pool(struct swap_chain_data *scd)
{
	VkDescriptorPool descriptor_pool;
	VkDescriptorPoolSize pool_sizes[2];

	// FIXME: Static allocation of '10' here.  Need to amange this correctly
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = scd->nimages * 10;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = scd->nimages * 10;

	VkDescriptorPoolCreateInfo pool_info = {0};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = TRTL_ARRAY_SIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = scd->nimages * 10;

	if (vkCreateDescriptorPool(scd->render->turtle->device, &pool_info, NULL,
				   &descriptor_pool) != VK_SUCCESS) {
		error("failed to create descriptor pool!");
	}

	return descriptor_pool;
}

static void
create_image(struct render_context *render, uint32_t width, uint32_t height, VkFormat format,
	     VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	     VkImage *image, VkDeviceMemory *imageMemory)
{
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(render->turtle->device, &imageInfo, NULL, image) != VK_SUCCESS) {
		error("failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(render->turtle->device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex =
	    findMemoryType(render, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(render->turtle->device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
		error("failed to allocate image memory!");
	}

	vkBindImageMemory(render->turtle->device, *image, *imageMemory, 0);
}

void
transitionImageLayout(struct render_context *render, VkImage image, trtl_arg_unused VkFormat format,
		      VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
	    newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		error("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, NULL, 0, NULL, 1,
			     &barrier);

	endSingleTimeCommands(render, commandBuffer);
}

void
copyBufferToImage(struct render_context *render, VkBuffer buffer, VkImage image, uint32_t width,
		  uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0};
	region.imageExtent = (VkExtent3D){width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &region);

	endSingleTimeCommands(render, commandBuffer);
}

void
copy_buffer_to_image(struct render_context *render, VkBuffer buffer, VkImage image, uint32_t width,
		     uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &region);

	endSingleTimeCommands(render, commandBuffer);
}

VkImage
create_texture_image(struct render_context *render, const char *path)
{
	VkImage image;
	VkDeviceMemory imageMemory;
	int width, height, channels;

	stbi_uc *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == NULL) {
		error("failed to load texture image!");
	}
	VkDeviceSize imageSize = width * height * 4;

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(render, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		      &staging_buffer, &staging_buffer_memory);

	void *data;
	vkMapMemory(render->turtle->device, staging_buffer_memory, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(render->turtle->device, staging_buffer_memory);

	stbi_image_free(pixels);

	create_image(render, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image, &imageMemory);

	transitionImageLayout(render, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copy_buffer_to_image(render, staging_buffer, image, width, height);
	transitionImageLayout(render, image, VK_FORMAT_R8G8B8A8_SRGB,
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(render->turtle->device, staging_buffer, NULL);
	vkFreeMemory(render->turtle->device, staging_buffer_memory, NULL);

	return image;
}

VkImageView
create_texture_image_view(struct render_context *render, VkImage texture_image)
{
	return create_image_view(render, texture_image, VK_FORMAT_R8G8B8A8_SRGB,
				 VK_IMAGE_ASPECT_COLOR_BIT);
}

void
draw_frame(struct render_context *render, struct swap_chain_data *scd, VkSemaphore image_semaphore,
	   VkSemaphore renderFinishedSemaphore, VkFence fence)
{
	VkResult result;
	VkDevice device = render->turtle->device;

	// Make sure this frame was completed
	vkWaitForFences(render->turtle->device, 1, &fence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	result = vkAcquireNextImageKHR(device, scd->swap_chain, UINT64_MAX, image_semaphore,
				       VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain(render);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		error("Failed to get swap chain image");
	}

	trtl_seer_update(imageIndex);

	// FIXME: Device should be some sort of global context
	trtl_uniform_update(evil_global_uniform, imageIndex);

	// Check the system has finished with this image before we start
	// scribbling over the top of it.
	if (render->images_in_flight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &render->images_in_flight[imageIndex], VK_TRUE,
				UINT64_MAX);
	}
	render->images_in_flight[imageIndex] = fence;

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {image_semaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &scd->command_buffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &fence);

	result = vkQueueSubmit(render->graphicsQueue, 1, &submitInfo, fence);
	if (result != VK_SUCCESS) {
		printf("failed to submit draw command buffer %d! %p %p %p\n", result,
		       render->graphicsQueue, &submitInfo, fence);
		exit(1);
	}

	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {scd->swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(render->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
	    frame_buffer_resized) {
		frame_buffer_resized = false;
		recreate_swap_chain(render);
	} else if (result != VK_SUCCESS) {
		error("Failed to present swap chain image");
	}
}

static bool
check_validation_layer_support(void)
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties *available = talloc_array(NULL, VkLayerProperties, layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available);

	for (uint32_t i = 0; i < layer_count; i++) {
		VkLayerProperties layerName = available[i];
		if (strcmp(VALIDATION_LAYER, layerName.layerName) == 0) {
			talloc_free(available);
			return true;
		}
	}

	return false;
}

int
main_loop(GLFWwindow *window)
{
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		// draw_frame();
	}
	return 0;
}

VkResult
CreateDebugUtilsMessengerEXT(VkInstance instance,
			     const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
			     const VkAllocationCallbacks *pAllocator,
			     VkDebugUtilsMessengerEXT *pDebugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT func =
	    (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void
setupDebugMessenger(VkInstance instance)
{
	VkResult err;
	// if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = populate_debug_messenger_create_info();

	err = CreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &debugMessenger);
	if (err != VK_SUCCESS) {
		// error_msg(err, "failed to set up debug messenger!");
	}
}

static void
show_usage(const char *binary)
{
	printf("%s: Simple game\n", binary);
	puts(" --debug | -d   Set debug.  More 'd's more debug.");
	puts(" --help | -h    Show help.");
	puts(" --object | -o [OBJECT] Load a known object");
	puts(" --background | -b [OBJECT] Load a known object as background");
}

static void
parse_arguments(int argc, char **argv)
{
	static struct option options[] = {
	    {"debug", no_argument, NULL, 'd'},
	    {"help", no_argument, NULL, 'h'},
	    {"object", required_argument, NULL, 'o'},
	    {"background", required_argument, NULL, 'b'},
	    {NULL, 0, NULL, 0},
	};
	int option;

	while ((option = getopt_long(argc, argv, "dho:b:", options, NULL)) != -1) {
		switch (option) {
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'd':
			debug = 1;
			continue;
		case 'b':
			objs_to_load[TRTL_RENDER_LAYER_BACKGROUND] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_BACKGROUND], optarg);
			continue;
		case 'o':
			objs_to_load[TRTL_RENDER_LAYER_MAIN] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_MAIN], optarg);
			continue;
		default:
			show_usage(argv[0]);
			exit(0);
		}
	}
}

static int
load_object_default(struct swap_chain_data *scd)
{
	printf("Loading default objects: Background 'background', Main: 'room', 'couch'\n");
	trtl_seer_object_add("background", scd, TRTL_RENDER_LAYER_BACKGROUND);
	trtl_seer_object_add("room", scd, TRTL_RENDER_LAYER_MAIN);
	trtl_seer_object_add("couch", scd, TRTL_RENDER_LAYER_MAIN);
	return 2;
}

int
load_objects(struct swap_chain_data *scd)
{
	int i;

	// Do we have a least one object
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		if (objs_to_load[i] != NULL) {
			// Found something;
			break;
		}
	}
	if (i == TRTL_RENDER_LAYER_TOTAL) {
		return load_object_default(scd);
	}

	// For each layer, load it
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		struct trtl_stringlist *load = objs_to_load[i];
		while (load != NULL) {
			trtl_seer_object_add(load->string, scd, i);
			load = load->next;
		}
		talloc_free(objs_to_load[i]);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	struct render_context *render;
	struct turtle *turtle;
	VkInstance instance;
	VkSemaphore image_ready_sem[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_done_sem[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

	parse_arguments(argc, argv);

	printf("Validation Layer Support: %s\n", check_validation_layer_support() ? "Yes" : "No");

	turtle = talloc(NULL, struct turtle);

	render = talloc(NULL, struct render_context);
	talloc_set_destructor(render, render_context_destructor);
	render->turtle = turtle;

	render->turtle->window = window_init();
	instance = createInstance(render->turtle->window);

	setupDebugMessenger(instance);

	render->turtle->surface = create_surface(instance, render->turtle->window);
	render->turtle->physical_device = pickPhysicalDevice(instance, render->turtle->surface);
	render->turtle->device =
	    create_logical_device(render->turtle->physical_device, render->turtle->surface,
				  &render->graphicsQueue, &render->presentQueue);

	render->scd = create_swap_chain(render->turtle, render->turtle->physical_device,
					render->turtle->surface);
	struct swap_chain_data *scd = render->scd;
	scd->render = render;
	create_image_views(render->turtle->device, render->scd);
	// scd->render_pass = create_render_pass(turtle);
	render->descriptor_set_layout = create_descriptor_set_layout(render);

	scd->command_pool = create_command_pool(
	    render->turtle->device, render->turtle->physical_device, render->turtle->surface);
	create_depth_resources(scd);
	// scd->framebuffers = create_frame_buffers(render->turtle->device, scd);

	// Init the trtl Uniform buffers; We have one currently
	evil_global_uniform = trtl_uniform_init(render, scd->nimages, 1024);
	render->texture_sampler = create_texture_sampler(render);

	scd->descriptor_pool = create_descriptor_pool(scd);

	// FIXME: Object is destroyed when screen chages; wrong
	trtl_seer_init(render->turtle, scd->extent, scd->render->descriptor_set_layout);

	load_objects(scd);

	// FIXME: all object ot update it's descriptor sets
	// scd->descriptor_sets = create_descriptor_sets(scd);

	// FIXME: This is a hack, this shoudl be managed by seer,
	// and shoukd be done dynamically as the state of the worlld changes.
	scd->command_buffers = trtl_seer_create_command_buffers(scd, scd->command_pool);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		image_ready_sem[i] = create_semaphores(render->turtle->device);
		render_done_sem[i] = create_semaphores(render->turtle->device);
		in_flight_fences[i] = create_fences(render->turtle->device);
	}
	render->images_in_flight = talloc_array(render, VkFence, scd->nimages);
	// FIXME: Should do this when creating the Scd structure
	// createInfo.pNext = &debug_create_info;
	for (uint32_t i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}

	int currentFrame = 0;
	while (!glfwWindowShouldClose(render->turtle->window)) {
		glfwPollEvents();
		draw_frame(render, render->scd, image_ready_sem[currentFrame],
			   render_done_sem[currentFrame], in_flight_fences[currentFrame]);
		currentFrame++;
		currentFrame %= MAX_FRAMES_IN_FLIGHT;
	}
	vkDeviceWaitIdle(render->turtle->device);

	//	main_loop(window);

	return 0;
}


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// c++, there is a C library at https://github.com/recp/cglm
//#include <glm/glm.hpp>
// This is the C version of glm; so these don't work
#define CGLM_DEFINE_PRINTS 
#define CGLM_FORCE_RADIANS
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "cglm/cglm.h"   /* for inline */
//#include <cglm/call.h>   /* for library call (this also includes cglm.h) */

#include "stb/stb_image.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <talloc.h>

#include "vertex.h"
#include "blobby.h"
#include "helpers.h"

#define trtl_alloc  __attribute__((warn_unused_result))
#define trtl_noreturn  __attribute__((noreturn))

#define MIN(x,y) ({ \
    typeof(x) _x = (x);     \
    typeof(y) _y = (y);     \
    (void) (&_x == &_y);    \
    _x < _y ? _x : _y; })

#define MAX(x,y) ({ \
    typeof(x) _x = (x);     \
    typeof(y) _y = (y);     \
    (void) (&_x == &_y);    \
    _x > _y ? _x : _y; })

const int MAX_FRAMES_IN_FLIGHT = 2;

static const char *MODEL_PATH = "models/viking_room.obj";
static const char *TEXTURE_PATH = "textures/viking_room.png";

// Temp static vertices
static const struct vertex vertices[] = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f }},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f }},
	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.25f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, -0.25f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, -0.25f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f }},
	{{-0.5f, 0.5f, -0.25f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
};

static const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0,

    4, 5, 6, 6, 7, 4,
    
    8, 9, 10, 10, 11, 8,
};

struct UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
};

#define TRTL_ARRAY_SIZE(array)  ((uint32_t)(sizeof(array)/sizeof(array[0])))

static const char *required_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	//VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	//"VK_KHR_get_physical_device_properties2",
		//"VK_KHR_portability_subset",
};
#define N_REQUIRED_EXTENSIONS TRTL_ARRAY_SIZE(required_extensions)

static const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

// Belongs in render frame state
static bool frame_buffer_resized = false;

// Context stored with the main window.
struct window_context {
	
};

// All data used to render a frame
struct render_context {
	GLFWwindow *window;
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkSurfaceKHR surface;
	VkDescriptorSetLayout descriptor_set_layout;
	
	VkBuffer vertex_buffers;

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	struct swap_chain_data *scd;

	VkFence *images_in_flight;

	// Textures
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	VkImageView texture_image_view;
	VkSampler texture_sampler;
};

struct swap_chain_data {
	// Used in the destructor, not deleted in the destructor
	struct render_context *render;

	VkSwapchainKHR swap_chain;
	VkImage *images;
	uint32_t nimages;
	VkImageView *image_views; 
	VkFormat image_format; // Swap chain image formt
	VkExtent2D extent; // Extent of the images
	VkFramebuffer *framebuffers;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkRenderPass render_pass;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet *descriptor_sets;

	uint32_t nbuffers;
	// Used to recreate (and clean up) swap chain
	VkCommandPool command_pool;
	VkCommandBuffer *command_buffers;

	VkBuffer *uniform_buffers;
	VkDeviceMemory *uniform_buffers_memory;

	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;
};
static int swap_chain_data_destructor(struct swap_chain_data *scd);

uint32_t findMemoryType(struct render_context *render, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void create_uniform_buffers(struct swap_chain_data *scd);
void create_buffer(struct render_context *render, VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer* buffer,
		VkDeviceMemory* bufferMemory);
trtl_alloc static VkDescriptorPool create_descriptor_pool(struct swap_chain_data *scd);
trtl_alloc static VkDescriptorSet *create_descriptor_sets(struct swap_chain_data *scd);
static struct queue_family_indices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
static void create_image(struct render_context *render, uint32_t width, uint32_t
		height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage* image, VkDeviceMemory* imageMemory);

// This has a big fixme on it
VkDebugUtilsMessengerEXT debugMessenger;


trtl_noreturn int
error(const char *msg) {
	fputs(msg, stderr);
	exit(1);
}

trtl_noreturn int
error_msg(VkResult result, const char *msg) {
	fprintf(stderr, "Error: %s Return: %s\n", msg, vk_err_msg(result));
	exit(1);
}

/** End Generic */

/** Window stuff (glfw) */

static void
window_resize_cb(GLFWwindow *window, int width, int height) {
	frame_buffer_resized = true;
	printf("Window resized\n");	
}

GLFWwindow *window_init(void) {
	GLFWwindow *window;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, window_resize_cb);

	return window;
}


static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("Validation Layer: %s\n", pCallbackData->pMessage);

        return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT
populate_debug_messenger_create_info(void) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = { 0 };
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;

	return createInfo;
}


VkInstance createInstance(GLFWwindow *window) {
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
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	printf("Need %d extensions\n", glfwExtensionCount);
	for (int i = 0; i < glfwExtensionCount; i++){
		printf("  - %s\n", glfwExtensions[i]);
	}

	VkDebugUtilsMessengerCreateInfoEXT debug_create_info = populate_debug_messenger_create_info();

	VkInstanceCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
	// FIXME: A flag to toggle validation layers
	// createInfo.enabledLayerCount = 0;
	//createInfo.pNext = NULL;
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
check_device_extension_support(VkPhysicalDevice device) {
        uint32_t extensionCount;
	VkExtensionProperties *available_extensions;

        vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

	available_extensions = talloc_array(NULL, VkExtensionProperties, extensionCount);
        vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, available_extensions);

	int j;
	for (int i = 0 ; i < N_REQUIRED_EXTENSIONS ; i ++) {
		for (j = 0 ; j < extensionCount ; j ++) {
			printf("Found Extension: %s (%d)\n", available_extensions[j].extensionName,
					available_extensions[j].specVersion);
			if (strcmp(required_extensions[i], available_extensions[j].extensionName) == 0) {
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
query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
	struct swap_chain_support_details *details;
	details = talloc(NULL, struct swap_chain_support_details);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats,NULL);

	if (details->nformats != 0) {
		details->formats = talloc_array(details, VkSurfaceFormatKHR, details->nformats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats, details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes, NULL);

	if (details->npresentmodes != 0) {
		details->presentModes = talloc_array(details, VkPresentModeKHR, details->npresentmodes);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes, details->presentModes);
	}

	return details;
}

static bool
is_device_suitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
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

        return indices.has_present && indices.has_present && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

VkSurfaceKHR
create_surface(VkInstance instance, GLFWwindow *window) {
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
pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
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

	for (int i = 0 ; i < deviceCount ; i++) {
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
find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
	struct queue_family_indices indices = { 0,0,false,false };
	VkQueueFamilyProperties *properties;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

	properties = talloc_array(NULL, VkQueueFamilyProperties, queueFamilyCount);
	memset(properties, 0, sizeof(VkQueueFamilyProperties) * queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, properties);

	for (int i = 0; i < queueFamilyCount ; i ++){
		//printf("Loop %d %p %d %p\n",i, device, i, surface);
		VkQueueFamilyProperties *queue_family = properties + i;

		if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
			indices.has_graphics = true;
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
find_supported_format(VkPhysicalDevice physical_device, uint32_t ncandidates, VkFormat *candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (uint32_t i = 0 ; i < ncandidates ; i ++) {
		VkFormat format = candidates[i];
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	error("Failed to find a supported format");
}

static VkFormat
find_depth_format(VkPhysicalDevice physical_device) {
	VkFormat formats[] = {VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

	return find_supported_format(physical_device,
			TRTL_ARRAY_SIZE(formats), formats,
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
			);
}

static VkDevice
create_logical_device(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue *graphicsQueue, VkQueue *presentQueue) {
	struct queue_family_indices queue_family_indices;
	float queue_priority = 1.0f;
	VkDevice device;
	VkDeviceQueueCreateInfo queue_info[2] = { 0 };
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
	queue_info[0].queueFamilyIndex = queue_family_indices.graphics_family;;
	queue_info[0].queueCount = 1;
	queue_info[0].pQueuePriorities = &queue_priority;

	queue_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info[1].queueFamilyIndex = queue_family_indices.present_family;;
	queue_info[1].queueCount = 1;
	queue_info[1].pQueuePriorities = &queue_priority;
	// FIXME: Should be distinct
	printf("queue_info: %d %d\n", queue_family_indices.graphics_family, queue_family_indices.present_family);

	VkResult result = vkCreateDevice(physicalDevice, &device_info, NULL, &device);
	if (result != VK_SUCCESS) {
		error_msg(result, "vkCreateDevice");
	}

	vkGetDeviceQueue(device, queue_family_indices.graphics_family, 0, graphicsQueue);
	vkGetDeviceQueue(device, queue_family_indices.present_family, 0, presentQueue);

	return device;
}




const VkSurfaceFormatKHR *chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t nformats) {
	for (uint32_t i = 0 ; i < nformats ; i ++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats + i;
		}
	}

	return availableFormats;
}


VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR* availablePresentModes, uint32_t npresentmodes) {
	for (uint32_t i = 0; i< npresentmodes ; i ++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}



VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities) {
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

            actualExtent.width = MAX(capabilities->minImageExtent.width, MIN(capabilities->maxImageExtent.width, actualExtent.width));
            actualExtent.height = MAX(capabilities->minImageExtent.height, MIN(capabilities->maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }




struct swap_chain_data *create_swap_chain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	struct swap_chain_data *scd = talloc_zero(NULL, struct swap_chain_data);
	talloc_set_destructor(scd, swap_chain_data_destructor);

	struct swap_chain_support_details *swapChainSupport = query_swap_chain_support(physical_device, surface);

        const VkSurfaceFormatKHR *surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport->formats, swapChainSupport->nformats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport->presentModes, swapChainSupport->npresentmodes);
        VkExtent2D extent = chooseSwapExtent(&swapChainSupport->capabilities);

	// FIXME: Why is this '[1]'??  That seems ... wrong
	uint32_t imageCount = swapChainSupport[0].capabilities.minImageCount + 1;
	if (swapChainSupport->capabilities.maxImageCount > 0 && imageCount > swapChainSupport->capabilities.maxImageCount) {
		imageCount = swapChainSupport->capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = { 0 };
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

	if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		uint32_t queueFamilyIndices[2];
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

	if (vkCreateSwapchainKHR(device, &createInfo, NULL, &scd->swap_chain) != VK_SUCCESS) {
		error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, scd->swap_chain, &scd->nimages, NULL);
	scd->images = talloc_array(scd, VkImage, scd->nimages);
	vkGetSwapchainImagesKHR(device, scd->swap_chain, &scd->nimages, scd->images);

	scd->image_format = surfaceFormat->format;
	scd->extent = extent;

	return scd;
}

static VkImageView
create_image_view(struct render_context *render, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
	VkImageViewCreateInfo viewInfo = { 0 };
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
	if (vkCreateImageView(render->device, &viewInfo, NULL, &imageView) != VK_SUCCESS) {
		error("failed to create texture image view!");
	}

	return imageView;
}

void create_image_views(VkDevice device, struct swap_chain_data *scd) {
	scd->image_views = talloc_array(scd, VkImageView, scd->nimages);

        for (uint32_t i = 0; i < scd->nimages; i++) {
		scd->image_views[i] = create_image_view(scd->render,
				scd->images[i], scd->image_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

static VkSampler
create_texture_sampler(struct render_context *render) {
	VkSampler sampler;
	VkPhysicalDeviceProperties properties = { 0 };
        vkGetPhysicalDeviceProperties(render->physical_device, &properties);

        VkSamplerCreateInfo sampler_info = { 0 };
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

        if (vkCreateSampler(render->device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
		error("failed to create texture sampler!");
        }
	return sampler;
}


VkCommandBuffer beginSingleTimeCommands(struct render_context *render) {
	VkCommandBufferAllocateInfo allocInfo = { 0 };
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = render->scd->command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(render->device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void endSingleTimeCommands(struct render_context *render, VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = { 0 };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(render->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(render->graphicsQueue);

	vkFreeCommandBuffers(render->device, render->scd->command_pool, 1, &commandBuffer);
}


VkShaderModule
create_shader(VkDevice device, struct blobby *blobby) {
	VkShaderModule shader_module;
		
	if (!blobby) return 0;

	VkShaderModuleCreateInfo shader_info = { 0 };
	shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.codeSize = blobby->len;
	shader_info.pCode = (void*)blobby->data;

	if (vkCreateShaderModule(device, &shader_info, NULL, &shader_module) != VK_SUCCESS) {
		error("Create shader Module\n");
		return 0;
	}

	talloc_free(blobby);

	return shader_module;
}

VkRenderPass
create_render_pass(VkDevice device, struct swap_chain_data *scd) {
	VkRenderPass render_pass;

	VkAttachmentDescription colorAttachment = { 0 };
	colorAttachment.format = scd->image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = { 0 };
	depthAttachment.format = find_depth_format(scd->render->physical_device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = { 0 };
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = { 0 };
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency = { 0 };
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassInfo = { 0 };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = TRTL_ARRAY_SIZE(attachments);;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, NULL, &render_pass) != VK_SUCCESS) {
		error("failed to create render pass!");
	}

	return render_pass;
}

VkPipeline
create_graphics_pipeline(VkDevice device, struct swap_chain_data *scd) {
	VkPipeline graphics_pipeline;
	struct blobby *fragcode = blobby_from_file("shaders/frag.spv");
	struct blobby *vertcode = blobby_from_file("shaders/vert.spv");

	VkShaderModule frag = create_shader(device, fragcode);
	VkShaderModule vert = create_shader(device, vertcode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = { 0 };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vert;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = frag;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	VkVertexInputBindingDescription binding_description;
	VkVertexInputAttributeDescription *attribute_description;

	uint32_t nentries;
	binding_description = vertex_binding_description_get(vertices);
	// FIXME: leaky
	attribute_description = get_attribute_description_pair(vertices, &nentries);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 0 };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &binding_description;
	vertexInputInfo.vertexAttributeDescriptionCount = nentries;
	vertexInputInfo.pVertexAttributeDescriptions = attribute_description;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { 0 };
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0 };
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)scd->extent.width;
	viewport.height = (float)scd->extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { 0 };
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = scd->extent;

	VkPipelineViewportStateCreateInfo viewportState = { 0 };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = NULL; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineDepthStencilStateCreateInfo depth_stencil = { 0 };
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending = { 0 };
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = { 0 };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;


	VkPipelineLayoutCreateInfo pipeline_layout_info = { 0 };
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &scd->render->descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_info.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &scd->pipeline_layout) != VK_SUCCESS) {
		error("failed to create pipeline layout!");
	}

        VkGraphicsPipelineCreateInfo pipelineInfo = { 0 };
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depth_stencil; 
        pipelineInfo.pColorBlendState = &colorBlending;

        pipelineInfo.layout = scd->pipeline_layout;
        pipelineInfo.renderPass = scd->render_pass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphics_pipeline) != VK_SUCCESS) {
		error("failed to create graphics pipeline!");
        }

	return graphics_pipeline;
}

VkDescriptorSetLayout
create_descriptor_set_layout(struct render_context *render) {
	VkDescriptorSetLayout descriptor_set_layout; 

        VkDescriptorSetLayoutBinding ubo_layout_binding = { 0 };
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.pImmutableSamplers = NULL;
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding sampler_layout_binding = { 0 };
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = NULL;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0] = ubo_layout_binding;
	bindings[1] = sampler_layout_binding;
        VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = TRTL_ARRAY_SIZE(bindings);
        layoutInfo.pBindings = bindings;

        if (vkCreateDescriptorSetLayout(render->device, &layoutInfo, NULL, &descriptor_set_layout) != VK_SUCCESS) {
            error("failed to create descriptor set layout!");
        }
	return descriptor_set_layout;
}

static VkFramebuffer *
create_frame_buffers(VkDevice device, struct swap_chain_data *scd) {
	VkFramebuffer *framebuffers;

	framebuffers = talloc_array(scd, VkFramebuffer, scd->nimages);

	for (uint32_t i = 0; i < scd->nimages ; i++) {
		VkImageView attachments[] = {
			scd->image_views[i],
			scd->depth_image_view,
		};

		VkFramebufferCreateInfo framebufferInfo = { 0 };
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = scd->render_pass;
		framebufferInfo.attachmentCount = TRTL_ARRAY_SIZE(attachments);
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = scd->extent.width;
		framebufferInfo.height = scd->extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]) != VK_SUCCESS) {
			error("failed to create framebuffer!");

		}
	}

	return framebuffers;
}


VkCommandPool create_command_pool(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	VkCommandPool command_pool;
	struct queue_family_indices qfi = find_queue_families(physical_device, surface);
	
	VkCommandPoolCreateInfo pool_info = { 0 };
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
create_depth_resources(struct swap_chain_data *scd) {
	VkFormat depthFormat = find_depth_format(scd->render->physical_device);

	create_image(scd->render, scd->extent.width, scd->extent.height,
			depthFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &scd->depth_image,
			&scd->depth_image_memory);
	scd->depth_image_view = create_image_view(scd->render, scd->depth_image,
			depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

/**
 * Create a generic buffer with the supplied flags.
 * Return is in VkBuffer/VkDeviceMemory,
 */
void
create_buffer(struct render_context *render, VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer* buffer,
		VkDeviceMemory* bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(render->device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements = { 0 };
    vkGetBufferMemoryRequirements(render->device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = { 0 };
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(render, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(render->device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
        error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(render->device, *buffer, *bufferMemory, 0);
}

VkCommandBuffer *create_command_buffers(struct render_context *render, struct swap_chain_data *scd, VkRenderPass render_pass, VkCommandPool command_pool, VkFramebuffer *framebuffers, VkPipeline pipeline){
	VkCommandBuffer *buffers;

	scd->nbuffers = scd->nimages;
	
	buffers = talloc_array(scd, VkCommandBuffer, scd->nbuffers); 

        VkCommandBufferAllocateInfo allocInfo = { 0 };
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = command_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = scd->nimages;

        if (vkAllocateCommandBuffers(render->device, &allocInfo, buffers) != VK_SUCCESS) {
		error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < scd->nimages; i++) {
            VkCommandBufferBeginInfo beginInfo =  { 0 };
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(buffers[i], &beginInfo) != VK_SUCCESS) {
                error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = { 0 };
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = render_pass;
            renderPassInfo.framebuffer = framebuffers[i];
            renderPassInfo.renderArea.offset.x = 0;
            renderPassInfo.renderArea.offset.y = 0;
            renderPassInfo.renderArea.extent = scd->extent;

            VkClearValue clearValues[] = {
		    { .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f} },
		    { .depthStencil = (VkClearDepthStencilValue){1.0f, 0} },
	    };
            renderPassInfo.clearValueCount = TRTL_ARRAY_SIZE(clearValues);
            renderPassInfo.pClearValues = clearValues;

	    vkCmdBeginRenderPass(buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	    {
		    vkCmdBindPipeline(buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, scd->pipeline);
		    VkBuffer vertexBuffers[] = {render->vertex_buffers};
		    VkDeviceSize offsets[] = {0};
		    vkCmdBindVertexBuffers(buffers[i], 0, 1, vertexBuffers, offsets);
		    vkCmdBindIndexBuffer(buffers[i], render->index_buffer, 0, VK_INDEX_TYPE_UINT16);
	
		    vkCmdBindDescriptorSets(buffers[i],
				    VK_PIPELINE_BIND_POINT_GRAPHICS,
				    scd->pipeline_layout, 0, 1,
				    &scd->descriptor_sets[i], 0, NULL);
		    vkCmdDrawIndexed(buffers[i], TRTL_ARRAY_SIZE(indices), 1, 0, 0, 0);
		}
	    vkCmdEndRenderPass(buffers[i]);

            if (vkEndCommandBuffer(buffers[i]) != VK_SUCCESS) {
		    error("failed to record command buffer!");
            }
        }

	return buffers;
}

VkSemaphore
create_semaphores(VkDevice device) {
	VkSemaphoreCreateInfo sem_info = { 0 };
	VkSemaphore sem;
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(device, &sem_info, NULL, &sem)) {
		error("failed tocreate sem");
	}
	return sem;
}

VkFence
create_fences(VkDevice device) {
	VkFenceCreateInfo fenceInfo = { 0 };
	VkFence fence;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(device, &fenceInfo, NULL, &fence) != VK_SUCCESS) {
		error("Failed to create fence");
	}
	return fence;
}

static int swap_chain_data_destructor(struct swap_chain_data *scd) {
	VkDevice device;
	int i;

	device = scd->render->device;

	for (i = 0; i < scd->nimages; i ++) {
		vkDestroyFramebuffer(device, scd->framebuffers[i], NULL);
	}

	vkFreeCommandBuffers(device, scd->command_pool, scd->nbuffers, scd->command_buffers);

	vkDestroyPipeline(device, scd->pipeline, NULL);
	vkDestroyPipelineLayout(device, scd->pipeline_layout, NULL);
	vkDestroyRenderPass(device, scd->render_pass, NULL);

	for (i = 0 ; i < scd->nimages ; i++) {
		vkDestroyImageView(device, scd->image_views[i], NULL);
	}
	
	vkDestroySwapchainKHR(device, scd->swap_chain, NULL);

	for (i = 0 ; i < scd->nimages ; i++) {
		vkDestroyBuffer(device, scd->uniform_buffers[i], NULL);
		vkFreeMemory(device, scd->uniform_buffers_memory[i], NULL);
	}

	vkDestroyDescriptorPool(device, scd->descriptor_pool, NULL);

	return 0;
}

static int
render_context_destructor(struct render_context *render) {

	vkDestroyImage(render->device, render->texture_image, NULL);
	vkFreeMemory(render->device, render->texture_image_memory, NULL);
	return 0;	
}

void
recreate_swap_chain(struct render_context *render) {
	int width = 0, height = 0;
	glfwGetFramebufferSize(render->window, &width, &height);

	// a width/height of 0 is minimised, just wait for
	// events to recover (for now);
	while (width == 0 || height == 0) {
		printf("Size 0, wait\n");
		glfwWaitEvents();
		glfwGetFramebufferSize(render->window,  &width, &height);
	}

	vkDeviceWaitIdle(render->device);

	talloc_free(render->scd);

	render->scd = create_swap_chain(render->device, render->physical_device, render->surface);
	struct swap_chain_data *scd = render->scd;
	scd->render = render;
        create_image_views(render->device, render->scd);
	scd->render_pass = create_render_pass(render->device, scd);
	render->descriptor_set_layout = create_descriptor_set_layout(render);
	scd->pipeline = create_graphics_pipeline(render->device, scd);

	create_uniform_buffers(scd);
	scd->descriptor_pool = create_descriptor_pool(scd);
    	scd->descriptor_sets = create_descriptor_sets(scd);
	scd->command_pool = create_command_pool(render->device, render->physical_device, render->surface);
	create_depth_resources(scd);
	scd->framebuffers = create_frame_buffers(render->device, scd);
	scd->command_buffers = create_command_buffers(
			render,
			scd,
			scd->render_pass,
			scd->command_pool,
			scd->framebuffers,
			scd->pipeline);

	for (int i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}


}

void copyBuffer(struct render_context *render, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

    VkBufferCopy copyRegion = { 0 };
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(render, commandBuffer);
}

uint32_t findMemoryType(struct render_context *render, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(render->physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	error("failed to find suitable memory type!");
	return -1;
}

VkBuffer
create_vertex_buffers(struct render_context *render) {
	VkDeviceSize bufferSize = sizeof(vertices);
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	create_buffer(render, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
			&stagingBufferMemory);

	void* data;
	vkMapMemory(render->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices, (size_t) bufferSize);
	vkUnmapMemory(render->device, stagingBufferMemory);

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	create_buffer(render, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer,
			&vertex_buffer_memory);

	copyBuffer(render, stagingBuffer, vertex_buffer, bufferSize);

	vkDestroyBuffer(render->device, stagingBuffer, NULL);
	vkFreeMemory(render->device, stagingBufferMemory, NULL);

	return vertex_buffer;
}

VkBuffer
create_index_buffer(struct render_context *render, VkDeviceMemory *memory) {
	VkDeviceSize bufferSize = sizeof(indices);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	create_buffer(render, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
			&stagingBufferMemory);

	void* data;
	vkMapMemory(render->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices, (size_t) bufferSize);
	vkUnmapMemory(render->device, stagingBufferMemory);

	VkBuffer index_buffer;
	create_buffer(render, bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_buffer,
			memory);

	copyBuffer(render, stagingBuffer, index_buffer, bufferSize);

	vkDestroyBuffer(render->device, stagingBuffer, NULL);
	vkFreeMemory(render->device, stagingBufferMemory, NULL);

    	return index_buffer;
}

void create_uniform_buffers(struct swap_chain_data *scd) {
	VkDeviceSize bufferSize = sizeof(struct UniformBufferObject);

	scd->uniform_buffers = talloc_array(scd, VkBuffer, scd->nimages);
	scd->uniform_buffers_memory = talloc_array(scd, VkDeviceMemory,  scd->nimages);

	for (size_t i = 0; i < scd->nimages; i ++) {
		create_buffer(scd->render, bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&scd->uniform_buffers[i],
				&scd->uniform_buffers_memory[i]);
  	}
}


static void
update_uniform_buffer(struct swap_chain_data *scd, uint32_t currentImage) {
	//static int startTime = 0;
	//int currentTime = 1;
	static float time = 1.0f;
	time = time + 1;

	struct UniformBufferObject ubo = { 0 };
	// m4, andgle, vector -> m4
	//ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm_mat4_identity(ubo.model);
	glm_rotate(ubo.model, glm_rad(time), GLM_ZUP);

	//ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm_lookat((vec3){2.0f, 2.0f, 2.0f}  , GLM_VEC3_ZERO, GLM_ZUP, ubo.view);
//t(vec3 eye, vec3 center, vec3 up, mat4 dest)


	glm_perspective(glm_rad(45), 800 / 640.0, 0.1f, 10.0f, ubo.proj);
	//ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(scd->render->device, scd->uniform_buffers_memory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(scd->render->device, scd->uniform_buffers_memory[currentImage]);
}

trtl_alloc static VkDescriptorPool
create_descriptor_pool(struct swap_chain_data *scd) {
	VkDescriptorPool descriptor_pool;
	VkDescriptorPoolSize pool_sizes[2];
	
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = scd->nimages;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = scd->nimages;

        VkDescriptorPoolCreateInfo pool_info = { 0 };
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = TRTL_ARRAY_SIZE(pool_sizes);;
        pool_info.pPoolSizes = pool_sizes;
        pool_info.maxSets = scd->nimages;

	if (vkCreateDescriptorPool(scd->render->device, &pool_info, NULL,
				&descriptor_pool) != VK_SUCCESS) {
		error("failed to create descriptor pool!");
        }
	
	return descriptor_pool;
}

static void
create_image(struct render_context *render, uint32_t width, uint32_t
		height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage* image, VkDeviceMemory* imageMemory) {
	VkImageCreateInfo imageInfo = { 0 };
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

	if (vkCreateImage(render->device, &imageInfo, NULL, image) != VK_SUCCESS) {
		error("failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(render->device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(render, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(render->device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
		error("failed to allocate image memory!");
	}

	vkBindImageMemory(render->device, *image, *imageMemory, 0);
}

void transitionImageLayout(struct render_context *render, VkImage image,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout
		newLayout) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkImageMemoryBarrier barrier = { 0 };
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

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		error("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, NULL,
			0, NULL,
			1, &barrier
			);

	endSingleTimeCommands(render, commandBuffer);
}

void copyBufferToImage(struct render_context *render, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkBufferImageCopy region= { 0 };
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0};
	region.imageExtent = (VkExtent3D){
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(render, commandBuffer);
}

void copy_buffer_to_image(struct render_context *render, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkBufferImageCopy region = { 0 };
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(render, commandBuffer);
}

static VkImage
create_texture_image(struct render_context *render) {
	VkImage image;
	VkDeviceMemory imageMemory;
	int width, height, channels;

	stbi_uc* pixels = stbi_load(TEXTURE_PATH, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == NULL) {
		error("failed to load texture image!");
        }
	VkDeviceSize imageSize = width * height * 4;

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
	create_buffer(render, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
			&staging_buffer_memory);

        void* data;
        vkMapMemory(render->device, staging_buffer_memory, 0, imageSize, 0, &data);
            memcpy(data, pixels, imageSize);
        vkUnmapMemory(render->device, staging_buffer_memory);

        stbi_image_free(pixels);

	create_image(render, width, height, VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image,
			&imageMemory);
	

        transitionImageLayout(render, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            copy_buffer_to_image(render, staging_buffer, image, width, height);
        transitionImageLayout(render, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(render->device, staging_buffer, NULL);
        vkFreeMemory(render->device, staging_buffer_memory, NULL);

	return image;
}

static VkImageView
create_texture_image_view(struct render_context *render, VkImage texture_image) {
	return create_image_view(render, texture_image, VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_ASPECT_COLOR_BIT);
}

trtl_alloc static VkDescriptorSet *
create_descriptor_sets(struct swap_chain_data *scd) {
	VkDescriptorSet *sets = talloc_zero_array(scd, VkDescriptorSet, scd->nimages);
	VkDescriptorSetLayout *layouts = talloc_zero_array(NULL, VkDescriptorSetLayout, scd->nimages);
	for (int i = 0 ; i < scd->nimages; i ++) {
		layouts[i] = scd->render->descriptor_set_layout;
	}

        VkDescriptorSetAllocateInfo alloc_info = { 0 };
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = scd->descriptor_pool;
        alloc_info.descriptorSetCount = scd->nimages;
        alloc_info.pSetLayouts = layouts;

        if (vkAllocateDescriptorSets(scd->render->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
        }

	for (size_t i = 0; i < scd->nimages ; i ++) {
		VkDescriptorBufferInfo buffer_info = { 0 };
		buffer_info.buffer = scd->uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = sizeof(struct UniformBufferObject);

		VkDescriptorImageInfo image_info = { 0 };
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = scd->render->texture_image_view;
		image_info.sampler = scd->render->texture_sampler;

		VkWriteDescriptorSet descriptorWrites[2] = { 0 };

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &image_info;

		vkUpdateDescriptorSets(scd->render->device,
				TRTL_ARRAY_SIZE(descriptorWrites),
				descriptorWrites, 0, NULL);
	}

	return sets;
}

void
draw_frame(struct render_context *render, struct swap_chain_data *scd,
		VkSemaphore image_semaphore, VkSemaphore renderFinishedSemaphore,
		VkFence fence) {
	VkResult result;
	VkDevice device = render->device;

	// Make sure this frame was completed
	vkWaitForFences(render->device, 1, &fence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	result = vkAcquireNextImageKHR(device, scd->swap_chain, UINT64_MAX, image_semaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain(render);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		error("Failed to get swap chain image");
	}

	update_uniform_buffer(scd, imageIndex);

	// Check the system has finished with this image before we start
	// scribbling over the top of it.
	if (render->images_in_flight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &render->images_in_flight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	render->images_in_flight[imageIndex] = fence;

	VkSubmitInfo submitInfo = { 0 };
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
		printf("failed to submit draw command buffer %d! %p %p %p\n",
				result,
				render->graphicsQueue, &submitInfo, fence);
		exit(1);
	}

	VkPresentInfoKHR presentInfo = { 0 };
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {scd->swap_chain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(render->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) {
		frame_buffer_resized = true;
		recreate_swap_chain(render);
	} else if (result != VK_SUCCESS) {
		error("Failed to present swap chain image");
	}
}


static bool
check_validation_layer_support(void) {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties *available = talloc_array(NULL, VkLayerProperties, layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available);

	for (uint32_t i = 0 ; i < layer_count ; i ++) {
		VkLayerProperties layerName = available[i];
                if (strcmp(VALIDATION_LAYER,  layerName.layerName) == 0) {
			talloc_free(available);
			return true;
		}
	}

        return false;
}

int
main_loop(GLFWwindow* window) {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		//draw_frame();
	}
	return 0;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	PFN_vkCreateDebugUtilsMessengerEXT func =  (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void setupDebugMessenger(VkInstance instance) {
	VkResult err;
        //if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = populate_debug_messenger_create_info();

	err = CreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &debugMessenger);
	if (err != VK_SUCCESS) {
		//error_msg(err, "failed to set up debug messenger!");
        }
    }

int
main(int argc, char **argv) {
	struct render_context *render;

	VkInstance instance;
	VkSemaphore image_ready_sem[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_done_sem[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

	printf("Validation Layer SUpport: %s\n", check_validation_layer_support() ? "Yes" : "No");

	render = talloc(NULL, struct render_context);
	talloc_set_destructor(render, render_context_destructor);

	render->window = window_init();
	instance = createInstance(render->window);

	setupDebugMessenger(instance);

	render->surface = create_surface(instance, render->window);
	render->physical_device = pickPhysicalDevice(instance, render->surface);
	render->device = create_logical_device(render->physical_device, render->surface, &render->graphicsQueue, &render->presentQueue);

        render->scd = create_swap_chain(render->device, render->physical_device, render->surface);
	struct swap_chain_data *scd = render->scd;
	scd->render = render;
        create_image_views(render->device, render->scd);
	scd->render_pass = create_render_pass(render->device, scd);
	render->descriptor_set_layout = create_descriptor_set_layout(render);
	scd->pipeline = create_graphics_pipeline(render->device, scd);


	scd->command_pool = create_command_pool(render->device, render->physical_device, render->surface);
	create_depth_resources(scd);
	scd->framebuffers = create_frame_buffers(render->device, scd);

	render->texture_image = create_texture_image(render);
	render->texture_image_view = create_texture_image_view(render, render->texture_image);
	render->texture_sampler = create_texture_sampler(render);
	render->vertex_buffers = create_vertex_buffers(render);
	render->index_buffer = create_index_buffer(render, &render->index_buffer_memory);
	create_uniform_buffers(scd);
	scd->descriptor_pool = create_descriptor_pool(scd);
    	scd->descriptor_sets = create_descriptor_sets(scd);
	scd->command_buffers = create_command_buffers(
			render,
			scd,
			scd->render_pass,
			scd->command_pool,
			scd->framebuffers,
			scd->pipeline);


	for (int i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i ++){
		image_ready_sem[i] = create_semaphores(render->device);
		render_done_sem[i] = create_semaphores(render->device);
		in_flight_fences[i] = create_fences(render->device);
	}
	render->images_in_flight = talloc_array(render, VkFence, scd->nimages);
	// FIXME: Should do this when creating the Scd structure
	// createInfo.pNext = &debug_create_info;u
	for (int i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}

	int currentFrame = 0;
	while (!glfwWindowShouldClose(render->window)) {
		glfwPollEvents();
		draw_frame(render, render->scd,
				image_ready_sem[currentFrame],
				render_done_sem[currentFrame],
				in_flight_fences[currentFrame]);
		currentFrame ++;
		currentFrame %= MAX_FRAMES_IN_FLIGHT;
	}
	vkDeviceWaitIdle(render->device);

//	main_loop(window);

	return 0;
}






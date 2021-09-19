#include <assert.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_barriers.h"
#include "trtl_scribe.h"
#include "trtl_solo.h"
#include "trtl_texture.h"
#include "trtl_vulkan.h"
#include "turtle.h"

static int turtle_destructor(struct turtle *turtle);

// FIXME: move into here or shell
void draw_frame(struct render_context *render, struct trtl_swap_chain *scd,
		VkSemaphore image_semaphore, VkSemaphore renderFinishedSemaphore, VkFence fence);

extern bool frame_buffer_resized;

static void window_resize_cb(trtl_arg_unused GLFWwindow *window, trtl_arg_unused int width,
			     trtl_arg_unused int height);
static void key_callback(trtl_arg_unused GLFWwindow *window, int key, trtl_arg_unused int scancode,
			 int action, trtl_arg_unused int mods);

struct trtl_swap_chain *create_swap_chain(struct turtle *turtle, VkPhysicalDevice physical_device,
					  VkSurfaceKHR surface);

// FIXME: These should be in a nice game state stucture
int posX = 0;
int posY = 0;
int zoom = 128;

static const char *required_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
};
#define N_REQUIRED_EXTENSIONS TRTL_ARRAY_SIZE(required_extensions)

// FIXME: Probably need a trtl_window file
static void
window_init(struct turtle *turtle, const char *title)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	turtle->window = glfwCreateWindow(800, 600, title, NULL, NULL);
	glfwSetFramebufferSizeCallback(turtle->window, window_resize_cb);

	glfwSetKeyCallback(turtle->window, key_callback);
}

void
key_callback(trtl_arg_unused GLFWwindow *window, int key, trtl_arg_unused int scancode, int action,
	     trtl_arg_unused int mods)
{
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
		case GLFW_KEY_EQUAL:
			zoom *= 2;
			// FIXME: This is a terrible way to regen everything
			frame_buffer_resized = true;
			break;
		case GLFW_KEY_MINUS:
			if (zoom > 32) zoom /= 2;
			frame_buffer_resized = true;
			break;
		}
	}
}

static void
window_resize_cb(trtl_arg_unused GLFWwindow *window, trtl_arg_unused int width,
		 trtl_arg_unused int height)
{
	frame_buffer_resized = true;
}

static VkInstance
create_instance(const char *name)
{
	VkInstance instance;

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Turtle";
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

	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appInfo;
	create_info.enabledExtensionCount = glfwExtensionCount + 1;
	create_info.ppEnabledExtensionNames = (const char *const *)allExtensions;
	trtl_scribe_upadate_validation(&create_info);

	if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
		error("Unable to create instance");
	}
	return instance;
}

static VkSurfaceKHR
create_surface(VkInstance instance, GLFWwindow *window)
{
	VkSurfaceKHR surface;
	VkResult result;
	result = glfwCreateWindowSurface(instance, window, NULL, &surface);
	if (result != VK_SUCCESS) {
		printf("failed to create window surface! %d\n", result);
		error("bailing");
	}
	return surface;
}

// FIXME: Should be static
struct queue_family_indices
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
			// LOG(VERBOSE, "Found Extension: %s (%d)\n",
			//   available_extensions[j].extensionName,
			//  available_extensions[j].specVersion);
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

// FIXME: Should be static, but currently in swap chain
struct swap_chain_support_details *
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

VkPhysicalDevice
create_physical_device(VkInstance instance, VkSurfaceKHR surface)
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

VkPhysicalDevice
pick_physical_device(VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t device_count = 0;
	VkPhysicalDevice *devices;
	VkPhysicalDevice candidate;

	vkEnumeratePhysicalDevices(instance, &device_count, NULL);
	printf("Found %d physical devices\n", device_count);
	if (device_count == 0) {
		error("Failed to find GPUs with Vulkan support");
	}

	devices = talloc_array(NULL, VkPhysicalDevice, device_count);
	assert(devices);

	vkEnumeratePhysicalDevices(instance, &device_count, devices);
	candidate = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < device_count; i++) {
		if (is_device_suitable(devices[i], surface)) {
			candidate = devices[i];
			break;
		}
	}

	if (candidate == VK_NULL_HANDLE) {
		error("Unable to find suitable device\n");
	}

	talloc_free(devices);
	return candidate;
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

struct turtle *
turtle_init(void)
{
	struct turtle *turtle = talloc(NULL, struct turtle);
	talloc_set_destructor(turtle, turtle_destructor);
	printf("Validation Layer Support: %s\n",
	       trtl_scribe_check_validation_layer_support() ? "Yes" : "No");

	window_init(turtle, "Turtle");

	turtle->instance = create_instance("Turtle");

	trtl_setup_debug_messenger(turtle->instance);

	turtle->surface = create_surface(turtle->instance, turtle->window);
	turtle->physical_device = pick_physical_device(turtle->instance, turtle->surface);

	turtle->device = create_logical_device(turtle->physical_device, turtle->surface,
					       &turtle->graphicsQueue, &turtle->presentQueue);

	trtl_solo_init(turtle);

	turtle->tsc = create_swap_chain(turtle, turtle->physical_device, turtle->surface);

	turtle->tsc->image_views =
	    create_image_views(turtle, turtle->tsc->images, turtle->tsc->nimages);

	turtle->tsc->command_pool =
	    create_command_pool(turtle->device, turtle->physical_device, turtle->surface);

	// trtl_barriers_init();

	return turtle;
}

int
trtl_main_loop(struct turtle *turtle, struct render_context *render)
{
	int currentFrame = 0;
	while (!glfwWindowShouldClose(render->turtle->window)) {
		glfwPollEvents();
		draw_frame(render, render->scd, turtle->barriers.image_ready_sem[currentFrame],
			   turtle->barriers.render_done_sem[currentFrame],
			   turtle->barriers.in_flight_fences[currentFrame]);
		currentFrame++;
		currentFrame %= TRTL_MAX_FRAMES_IN_FLIGHT;
	}
	vkDeviceWaitIdle(render->turtle->device);

	return 0;
}

/**
 * Create a generic buffer with the supplied flags.
 * Return is in VkBuffer/VkDeviceMemory,
 */
void
create_buffer(struct turtle *turtle, VkDeviceSize size, VkBufferUsageFlags usage,
	      VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(turtle->device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
		error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements = {0};
	vkGetBufferMemoryRequirements(turtle->device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex =
	    findMemoryType(turtle, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(turtle->device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
		error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(turtle->device, *buffer, *bufferMemory, 0);
}

uint32_t
findMemoryType(struct turtle *turtle, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(turtle->physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
		    (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	error("failed to find suitable memory type!");
}

static int
turtle_destructor(struct turtle *turtle)
{
	printf("destructor for %p\n", turtle);

	vkDestroySurfaceKHR(turtle->instance, turtle->surface, NULL);

	return 0;
}

static int
swap_chain_data_destructor(struct trtl_swap_chain *scd)
{
	VkDevice device;
	uint32_t i;

	device = scd->render->turtle->device;

	vkFreeCommandBuffers(device, scd->command_pool, scd->nbuffers, scd->command_buffers);

	// FIXME: There are multiple pipelines now
	// vkDestroyPipeline(device, *scd->pipelines, NULL);
	// vkDestroyPipelineLayout(device, scd->pipeline_layout, NULL);

	for (i = 0; i < scd->nimages; i++) {
		vkDestroyImageView(device, scd->image_views[i], NULL);
	}

	vkDestroySwapchainKHR(device, scd->swap_chain, NULL);

	vkDestroyDescriptorPool(device, scd->descriptor_pool, NULL);

	return 0;
}

struct trtl_swap_chain *
create_swap_chain(struct turtle *turtle, VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	struct trtl_swap_chain *scd = talloc_zero(NULL, struct trtl_swap_chain);
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

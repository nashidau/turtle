#include <assert.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
// Needed for portabllity extensions
#include <vulkan/vulkan_beta.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_barriers.h"
#include "trtl_scribe.h"
#include "trtl_seer.h"
#include "trtl_shader.h"
#include "trtl_solo.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"
#include "trtl_vulkan.h"
#include "turtle.h"

// FIXME: Belongs in render frame state
int posX = 0;
int posY = 0;
int zoom = 128;
bool frame_buffer_resized = false;

static int turtle_destructor(struct turtle *turtle);

// FIXME: move into here or shell
void draw_frame(struct turtle *turtle, struct trtl_swap_chain *scd, VkSemaphore image_semaphore,
		VkSemaphore renderFinishedSemaphore, VkFence fence);

static void window_resize_cb(trtl_arg_unused GLFWwindow *window, trtl_arg_unused int width,
			     trtl_arg_unused int height);

struct trtl_swap_chain *create_swap_chain(struct turtle *turtle, VkPhysicalDevice physical_device,
					  VkSurfaceKHR surface);

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
	allExtensions[glfwExtensionCount] =
	    talloc_strdup(allExtensions, "VK_KHR_get_physical_device_properties2");

	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appInfo;
	create_info.enabledExtensionCount = glfwExtensionCount + 1;
	create_info.ppEnabledExtensionNames = (const char *const *)allExtensions;
	trtl_scribe_upadate_validation(&create_info);

	if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
		error("Unable to create instance");
	}

	talloc_free(allExtensions);
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

	talloc_free(properties);

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

	talloc_free(available_extensions);
	return true;
}

// FIXME: Should be static, but currently in swap chain
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

/**
 *
 * We only need one depth image view as only one render pass is running
 * at a time.
 */
static void
create_depth_resources(struct turtle *turtle)
{
	VkFormat depthFormat = find_depth_format(turtle->physical_device);

	create_image(turtle, turtle->tsc->extent.width, turtle->tsc->extent.height, depthFormat,
		     VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &turtle->tsc->depth_image,
		     &turtle->tsc->depth_image_memory);
	turtle->tsc->depth_image_view = create_image_view(turtle, turtle->tsc->depth_image,
							  depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

struct turtle *
turtle_init(void)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
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

	turtle->shader_cache = trtl_shader_cache_init(turtle);
	trtl_solo_init(turtle);

	turtle->tsc = create_swap_chain(turtle, turtle->physical_device, turtle->surface);
	turtle->tsc->turtle = turtle;

	turtle->tsc->image_views =
	    create_image_views(turtle, turtle->tsc->images, turtle->tsc->nimages);

	turtle->tsc->command_pool =
	    create_command_pool(turtle->device, turtle->physical_device, turtle->surface);

	create_depth_resources(turtle);

	turtle->uniforms = trtl_uniform_init(turtle, turtle->tsc->nimages, 1024);
	// trtl_barriers_init();
	turtle->texture_sampler = create_texture_sampler(turtle);

	turtle->tsc->descriptor_pool = create_descriptor_pool(turtle->tsc);

	turtle->seer = trtl_seer_init(turtle, turtle->tsc->extent);
	assert(turtle->seer);

	trtl_barriers_init(turtle);

	// This should go into main loop
	turtle->images_in_flight = talloc_array(turtle, VkFence, turtle->tsc->nimages);
	// FIXME: Should do this when creating the Scd structure
	for (uint32_t i = 0; i < turtle->tsc->nimages; i++) {
		turtle->images_in_flight[i] = VK_NULL_HANDLE;
	}

	return turtle;
}

int
trtl_main_loop(struct turtle *turtle)
{
	turtle->tsc->command_buffers =
	    trtl_seer_create_command_buffers(turtle, turtle->tsc->command_pool);

	int currentFrame = 0;
	while (!glfwWindowShouldClose(turtle->window)) {
		glfwPollEvents();
		draw_frame(turtle, turtle->tsc, turtle->barriers.image_ready_sem[currentFrame],
			   turtle->barriers.render_done_sem[currentFrame],
			   turtle->barriers.in_flight_fences[currentFrame]);
		currentFrame++;
		currentFrame %= TRTL_MAX_FRAMES_IN_FLIGHT;
	}
	vkDeviceWaitIdle(turtle->device);

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

// FIXME: Definition in the triangle.h
trtl_alloc VkDescriptorPool
create_descriptor_pool(struct trtl_swap_chain *tsc)
{
	VkDescriptorPool descriptor_pool;
	VkDescriptorPoolSize pool_sizes[2];

	// FIXME: Static allocation of '10' here.  Need to amange this correctly
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = tsc->nimages * 10;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = tsc->nimages * 10;

	VkDescriptorPoolCreateInfo pool_info = {0};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = TRTL_ARRAY_SIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = tsc->nimages * 10;

	if (vkCreateDescriptorPool(tsc->turtle->device, &pool_info, NULL, &descriptor_pool) !=
	    VK_SUCCESS) {
		error("failed to create descriptor pool!");
	}

	return descriptor_pool;
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
	// This is obivoulsy no longer useful, free early
	talloc_free(turtle->shader_cache);
	turtle->shader_cache = NULL;

	// we need to free the swap before local clean ups
	talloc_free(turtle->tsc);
	turtle->tsc = NULL;

	vkDestroySurfaceKHR(turtle->instance, turtle->surface, NULL);

	return 0;
}

/**
 * Destructor for the swap chain.
 *
 * It must be called _before_ turtle is destroyed.  Otherwise it can be called at any time.
 *
 * @param tsc The swap chain to delete.
 * @return 0 always.
 */
static int
swap_chain_data_destructor(struct trtl_swap_chain *scd)
{
	VkDevice device;
	uint32_t i;

	device = scd->turtle->device;

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
	struct trtl_swap_chain *scd = talloc_zero(turtle, struct trtl_swap_chain);
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

	talloc_free(swapChainSupport);

	return scd;
}

void
recreate_swap_chain(struct turtle *turtle)
{
	int width = 0, height = 0;
	VkExtent2D size;

	glfwGetFramebufferSize(turtle->window, &width, &height);
	size.width = width;
	size.height = height;

	// a width/height of 0 is minimised, just wait for
	// events to recover (for now);
	while (width == 0 || height == 0) {
		printf("Size 0, wait\n");
		glfwWaitEvents();
		glfwGetFramebufferSize(turtle->window, &width, &height);
	}

	vkDeviceWaitIdle(turtle->device);

	talloc_free(turtle->tsc);

	turtle->tsc = create_swap_chain(turtle, turtle->physical_device, turtle->surface);
	struct trtl_swap_chain *tsc = turtle->tsc;
	turtle->tsc->turtle = turtle;
	tsc->image_views = create_image_views(turtle, tsc->images, tsc->nimages);

	tsc->command_pool =
	    create_command_pool(turtle->device, turtle->physical_device, turtle->surface);

	create_depth_resources(turtle);

	turtle->tsc->descriptor_pool = create_descriptor_pool(turtle->tsc);
	trtl_seer_resize(size, turtle);

	tsc->command_buffers = trtl_seer_create_command_buffers(turtle, tsc->command_pool);

	for (uint32_t i = 0; i < tsc->nimages; i++) {
		turtle->images_in_flight[i] = VK_NULL_HANDLE;
	}
}

void
draw_frame(struct turtle *turtle, struct trtl_swap_chain *tsc, VkSemaphore image_semaphore,
	   VkSemaphore renderFinishedSemaphore, VkFence fence)
{

	VkResult result;
	VkDevice device = turtle->device;

	// Make sure this frame was completed
	vkWaitForFences(turtle->device, 1, &fence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	result = vkAcquireNextImageKHR(device, tsc->swap_chain, UINT64_MAX, image_semaphore,
				       VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain(turtle);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		error("Failed to get swap chain image");
	}

	trtl_seer_update(turtle, imageIndex);

	// FIXME: Device should be some sort of global context
	trtl_uniform_update(turtle->uniforms, imageIndex);

	// Check the system has finished with this image before we start
	// scribbling over the top of it.
	if (turtle->images_in_flight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &turtle->images_in_flight[imageIndex], VK_TRUE,
				UINT64_MAX);
	}
	turtle->images_in_flight[imageIndex] = fence;

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {image_semaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &tsc->command_buffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &fence);

	result = vkQueueSubmit(turtle->graphicsQueue, 1, &submitInfo, fence);
	if (result != VK_SUCCESS) {
		printf("failed to submit draw command buffer %d! %p %p %p\n", result,
		       turtle->graphicsQueue, &submitInfo, fence);
		exit(1);
	}

	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {tsc->swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(turtle->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
	    frame_buffer_resized) {
		frame_buffer_resized = false;
		recreate_swap_chain(turtle);
	} else if (result != VK_SUCCESS) {
		error("Failed to present swap chain image");
	}
}

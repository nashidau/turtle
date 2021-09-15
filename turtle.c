
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <talloc.h>

#include "helpers.h"
#include "turtle.h"
#include "trtl_barriers.h"

static int turtle_destructor(struct turtle *turtle);


// This has a big fixme on it
VkDebugUtilsMessengerEXT debugMessenger;

// FIXME: debug, verbose shoudl be in trtl_debug or similar
enum trtl_debug {
	TRTL_DEBUG_ERROR = 0,
	TRTL_DEBUG_WARNING = 1,
	TRTL_DEBUG_INFO = 2,
	TRTL_DEBUG_VERBOSE = 3,
	TRTL_DEBUG_DEBUG = 4,
};

static enum trtl_debug debug = TRTL_DEBUG_INFO;

__attribute__((format(printf, 2, 3))) static void
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

static const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

// FIXME: move into here or shell
void draw_frame(struct render_context *render, struct swap_chain_data *scd,
		VkSemaphore image_semaphore, VkSemaphore renderFinishedSemaphore, VkFence fence);

extern bool frame_buffer_resized;

static void window_resize_cb(trtl_arg_unused GLFWwindow *window, trtl_arg_unused int width,
			     trtl_arg_unused int height);
static void key_callback(trtl_arg_unused GLFWwindow *window, int key, trtl_arg_unused int scancode,
			 int action, trtl_arg_unused int mods);


// FIXME: These should be in a nice game state stucture
int posX = 0;
int posY = 0;
int zoom = 128;

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

static bool
check_validation_layer_support(void)
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties *available = talloc_array(NULL, VkLayerProperties, layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available);

	for (uint32_t i = 0; i < layer_count ; i++) {
		VkLayerProperties layer_name = available[i];
		if (strcmp(VALIDATION_LAYER, layer_name.layerName) == 0) {
			talloc_free(available);
			return true;
		}
	}
	return false;
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
		error("Unable to create instance");
	}
	return instance;
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
setupDebugMessenger(VkInstance instance) {
	VkResult err;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = populate_debug_messenger_create_info();

	err = CreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &debugMessenger);
	if (err != VK_SUCCESS) {
		printf("Failed to cretate debug messenger\n");
	}
}


struct turtle *
turtle_init(void)
{
	struct turtle *turtle = talloc(NULL, struct turtle);
	talloc_set_destructor(turtle, turtle_destructor);
	printf("Validation Layer Support: %s\n", check_validation_layer_support() ? "Yes" : "No");

	window_init(turtle, "Turtle");

	turtle->instance = create_instance("Turtle");

	setupDebugMessenger(turtle->instance);

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
create_buffer(struct render_context *render, VkDeviceSize size, VkBufferUsageFlags usage,
	      VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(render->turtle->device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
		error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements = {0};
	vkGetBufferMemoryRequirements(render->turtle->device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex =
	    findMemoryType(render, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(render->turtle->device, &allocInfo, NULL, bufferMemory) !=
	    VK_SUCCESS) {
		error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(render->turtle->device, *buffer, *bufferMemory, 0);
}

uint32_t
findMemoryType(struct render_context *render, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(render->turtle->physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
		    (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	error("failed to find suitable memory type!");
}

static int
turtle_destructor(struct turtle *turtle) {
	printf("destructor for %p\n", turtle);

	return 0;
}

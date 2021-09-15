#include <talloc.h>

#include "helpers.h"
#include "turtle.h"
#include "trtl_barriers.h"

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

struct turtle *
turtle_init(void)
{
	struct turtle *turtle = talloc(NULL, struct turtle);

	window_init(turtle, "Turtle");


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

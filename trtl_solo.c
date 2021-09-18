
#include <talloc.h>
#include <vulkan/vulkan.h>

//

#include "trtl_solo.h"
#include "turtle.h"

static struct turtle *solo_turtle;
static VkCommandPool solo_pool;

struct trtl_solo_internal {
	struct trtl_solo solo;
};

// FIXME: Proper decleration in a header file
VkCommandPool create_command_pool(VkDevice device, VkPhysicalDevice physical_device,
				  VkSurfaceKHR surface);

void
trtl_solo_init(struct turtle *turtle)
{
	// FIXME: Handle resize; need the trtl_crier event for that.
	solo_turtle = turtle;

	solo_pool = create_command_pool(turtle->device, turtle->physical_device, turtle->surface);
	// FIXME: destructor to clean this up
}

static int
trtl_solo_destroy(struct trtl_solo_internal *solo) {
	vkEndCommandBuffer(solo->solo.command_buffer);

	VkSubmitInfo submit_info = {0};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &solo->solo.command_buffer;

	vkQueueSubmit(solo_turtle->graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(solo_turtle->graphicsQueue);

	vkFreeCommandBuffers(solo_turtle->device, solo_pool, 1, &solo->solo.command_buffer);

	return 0;
}

struct trtl_solo *
trtl_solo_get(void)
{
	struct trtl_solo_internal *solo;

	solo = talloc_zero(solo_turtle, struct trtl_solo_internal);
	talloc_set_destructor(solo, trtl_solo_destroy);

	VkCommandBufferAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = solo_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(solo_turtle->device, &alloc_info, &command_buffer);

	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer, &beginInfo);

	solo->solo.command_buffer = command_buffer;

	return &solo->solo;
}


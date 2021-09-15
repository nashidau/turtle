/**
 * Helper functions to create barriers.
 *
 */

#include "helpers.h"

#include "trtl_barriers.h"
#include "turtle.h"

static VkSemaphore
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

static VkFence
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

int
trtl_barriers_init(struct turtle *turtle, int nimages)
{
	for (int i = 0; i < nimages; i++) {
		turtle->barriers.image_ready_sem[i] = create_semaphores(turtle->device);
		turtle->barriers.render_done_sem[i] = create_semaphores(turtle->device);
		turtle->barriers.in_flight_fences[i] = create_fences(turtle->device);
	}
	return 0;
}

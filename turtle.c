

#include "turtle.h"
#include "helpers.h"


/**
 * Create a generic buffer with the supplied flags.
 * Return is in VkBuffer/VkDeviceMemory,
 */
void create_buffer(struct render_context *render, VkDeviceSize size, VkBufferUsageFlags usage,
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

	if (vkAllocateMemory(render->turtle->device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
		error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(render->turtle->device, *buffer, *bufferMemory, 0);
}

uint32_t findMemoryType(struct render_context *render, uint32_t typeFilter,
			VkMemoryPropertyFlags properties)
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
	return -1;
}


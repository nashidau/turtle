#include <string.h>

#include <talloc.h>
#include <vulkan/vulkan.h>

#include "trtl_seer.h"
#include "trtl_shell.h"
#include "turtle.h"
#include "vertex.h"

VkCommandBuffer beginSingleTimeCommands(struct render_context *render);

void endSingleTimeCommands(struct render_context *render, VkCommandBuffer commandBuffer);

static void
copyBuffer(struct render_context *render, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(render);

	VkBufferCopy copyRegion = {0};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(render, commandBuffer);
}

// FIXME: Trtl should take a model and create a vertex buffer for this
// FIXME: This code and the create_index buffer are like super similar
// FIXME: This should be in trtl_seer I'm pretty sure.  It owns the objects, so it
// should do the allocation for vertex and index buffers.
VkBuffer
create_vertex_buffers(struct render_context *render, const struct trtl_seer_vertexset *vertices)
{
	uint32_t nvertexes;
	uint32_t nobjects = 1;

	nvertexes = vertices->nvertexes;
	printf("%d vertices; %d objects\n", nvertexes, nobjects);

	if (nvertexes == 0) {
		return VK_NULL_HANDLE;
	}

	VkDeviceSize bufferSize = sizeof(struct vertex) * nvertexes;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	create_buffer(render, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		      &stagingBuffer, &stagingBufferMemory);

	void *data;
	vkMapMemory(render->turtle->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	off_t offset = 0;
	for (uint32_t i = 0; i < nobjects; i++) {
		memcpy(data + offset, vertices[i].vertices,
		       vertices[i].nvertexes * sizeof(struct vertex));
		offset += vertices[i].nvertexes * sizeof(struct vertex);
	}
	// FIXME: Should be a loop here.
	vkUnmapMemory(render->turtle->device, stagingBufferMemory);

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	create_buffer(render, bufferSize,
		      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer, &vertex_buffer_memory);

	copyBuffer(render, stagingBuffer, vertex_buffer, bufferSize);

	vkDestroyBuffer(render->turtle->device, stagingBuffer, NULL);
	vkFreeMemory(render->turtle->device, stagingBufferMemory, NULL);

	return vertex_buffer;
}

/**
 * Copy indexes from base -> dest, adjusting each by adjust.
 *
 * Used to concat a number of index arrays together.
 *
 * Buffers may not overlap.
 *
 * @param dest Destination pointer
 * @param base Where to copy from
 * @param count How many uint32_ts to copy
 */
static void
copy_indexes(uint32_t *restrict dest, const uint32_t *restrict base, uint32_t count)
{
	// FIXME: This is glorifed memcpy
	for (uint32_t i = 0; i < count; i++) {
		*dest = *base;
		dest++;
		base++;
	}
}

VkBuffer
create_index_buffer(struct render_context *render, const struct trtl_seer_indexset *indexes)
{
	// FIXME: Hardcoded indice size
	uint32_t nindexes = indexes->nindexes; // total number of indexes
	VkDeviceSize buffer_size;
	VkDeviceMemory memory; // FIXME: Should be returned so it can be freed

	if (nindexes == 0) {
		return VK_NULL_HANDLE;
	}

	buffer_size = nindexes * sizeof(uint32_t);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	create_buffer(render, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		      &stagingBuffer, &stagingBufferMemory);

	uint32_t *dest;
	{
		void *data;
		vkMapMemory(render->turtle->device, stagingBufferMemory, 0, buffer_size, 0, &data);
		dest = data;
	}
	// This ugly; update the index by the current poistion as we copy it accross
	// Vulkan probalby supports a way to do this
	uint32_t offset = 0;
	for (uint32_t i = 0; i < 1; i++) {
		copy_indexes(dest, indexes[i].indexes, indexes[i].nindexes);
		dest += indexes[i].nindexes;
		offset += indexes[i].indexrange;
	}

	vkUnmapMemory(render->turtle->device, stagingBufferMemory);

	VkBuffer index_buffer;
	create_buffer(render, buffer_size,
		      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_buffer, &memory);

	copyBuffer(render, stagingBuffer, index_buffer, buffer_size);

	vkDestroyBuffer(render->turtle->device, stagingBuffer, NULL);
	vkFreeMemory(render->turtle->device, stagingBufferMemory, NULL);

	return index_buffer;
}

/**
 * Uniform manager:
 * 	Objects allocate a uniform area
 * 	On render it syncs the data with uniform.
 *
 * To allocate call:
 * 	trtl_uniform_alloc()
 * This returns a trtl_uniform_descriptor.
 *
 * TODO:
 * 	Should have multiuple buffers for different types of objects.
 * 	Then use hueristics about which buffers should be updated.
 *
 * TODO: Handle reallocations to compress objects.
 *
 * Internal: Allocate 1mb for now.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_uniform.h"
#include "turtle.h"

/**
 * Internal structore for trtl_uniform info
 */
struct trtl_uniform {
	size_t buffer_size;
	uint8_t nbuffers;

	// Dumb bump allocator: just bump up end address
	struct {
		off_t offset;
	} bump;

	VkDevice device;
	VkBuffer *uniform_buffers;
	VkDeviceMemory *uniform_buffers_memory;

	uint8_t *buffers[0];
};

// Returned from an allocation
struct trtl_uniform_info {
	struct trtl_uniform *uniforms;
	size_t size;
	off_t offset;
};

static size_t TRTL_DEFAULT_SIZE = 2 * 1024;

static int trtl_uniform_destructor(struct trtl_uniform *x);

/**
 * Initialise the trtl_uniform_manager.
 *
 * Pass the size of the uniform object.  A value of zero indicates the default
 * size.
 *
 * @param ctx Context pointer.
 * @param nframes Number of frames in flight at once.
 * @param size Size of the uniform pool.  Zero (0) indicates the default size.
 */
struct trtl_uniform *trtl_uniform_init(struct render_context *render, uint8_t nframes, size_t size)
{
	struct trtl_uniform *uniforms;

	if (nframes < 1) {
		nframes = 1;
	}

	if (size == 0) {
		size = TRTL_DEFAULT_SIZE;
	}

	uniforms = talloc_size(render, offsetof(struct trtl_uniform, buffers[nframes]));
	assert(uniforms);
	talloc_set_name(uniforms, "Uniform Buffer");

	talloc_set_destructor(uniforms, trtl_uniform_destructor);

	uniforms->buffer_size = size;
	uniforms->nbuffers = nframes;

	for (int i = 0; i < nframes; i++) {
		// FIXME: Should alloc in one slab of N times size
		uniforms->buffers[i] = talloc_named(uniforms, size, "Uniform buffer Frame %d", i);
	}

	// We use the device all over
	uniforms->device = render->device;
	uniforms->uniform_buffers = talloc_array(uniforms, VkBuffer, nframes);
	uniforms->uniform_buffers_memory = talloc_array(uniforms, VkDeviceMemory, nframes);

	for (size_t i = 0; i < nframes; i++) {
		create_buffer(render, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			      &uniforms->uniform_buffers[i], &uniforms->uniform_buffers_memory[i]);
	}

	// Initilise the (terrible) bump alloator
	uniforms->bump.offset = 0;

	return uniforms;
}

/**
 * Get the base uniform address for setting up descriptors.
 *
 * @param uniforms The uniform buffer pointer
 * @param frame Frame to get buffer for
 * @return Pointer to base address of the uniform buffer.
 */
void *trtl_uniform_buffer_base_get(struct trtl_uniform *uniforms, uint8_t frame)
{
	return uniforms->uniform_buffers[frame];
}

/**
 * Render a uniform frame.
 *
 * Sends the uniform data to the GPU.
 */
int trtl_uniform_render(trtl_arg_unused int frame)
{
	return 0;
}

struct trtl_uniform_info *trtl_uniform_alloc(struct trtl_uniform *uniforms, size_t size)
{
	struct trtl_uniform_info *info;

	// FIXME: Check size is nicely aligned

	if (uniforms->bump.offset + size > uniforms->buffer_size) {
		warning("Allocation too large for uniform allocator");
		return NULL;
	}

	info = talloc_zero_size(uniforms, size);
	info->uniforms = uniforms;
	info->size = size;
	info->offset = uniforms->bump.offset;

	uniforms->bump.offset += size;

	// Todo: non bump allocator
	// Find a slot
	// Mark is used
	// Allcoate info
	// set destructor

	return info;
}

/**
 * Get the address of the CPU uniform buffer for given info & frame.
 *
 * Gets the address of the uniform buffer.  This is specific to the frame.
 * The uniform is valid until the next update call.
 *
 * @param info Info field to get data from.
 * @param frame Which frame we are rendering.
 * @return Address of the uniform field.
 */
void *trtl_uniform_info_address(struct trtl_uniform_info *info, int frame)
{
	off_t offset = info->offset;

	return info->uniforms->buffers[frame] + offset;
}

/**
 * Get the buffer info for filling out the descriptor set
 * for the given uniform info.
 *
 * @parma info A trtl uniform info pointer.
 * @param frame The frame we are getting info for.
 * @return VkDescriptorBufferInfo with the base, offset and size of the unifrom filled.
 */
VkDescriptorBufferInfo
trtl_uniform_buffer_get_descriptor(struct trtl_uniform_info *info, int frame)
{
	VkDescriptorBufferInfo buffer_info = {0};
	buffer_info.buffer = info->uniforms->uniform_buffers[frame];
	buffer_info.offset = info->offset;
	buffer_info.range = sizeof(struct UniformBufferObject);

	return buffer_info;
}

/**
 * Get the offset of the info field from the base of the uniform buffer.
 *
 * @param info Uniform info field.
 * @return Offset of the uniform field.
 */
off_t trtl_uniform_info_offset(struct trtl_uniform_info *info)
{
	return info->offset;
}

/**
 * Push the uniform array to Vulkan.
 *
 * @param uniforms The uniforms.
 */
void trtl_uniform_update(struct trtl_uniform *uniforms, uint32_t frame)
{
	void *data;
	vkMapMemory(uniforms->device, uniforms->uniform_buffers_memory[frame], 0,
			uniforms->bump.offset, 0, &data);
	memcpy(data, uniforms->buffers[frame], uniforms->bump.offset);
	vkUnmapMemory(uniforms->device, uniforms->uniform_buffers_memory[frame]);
}

static int trtl_uniform_destructor(struct trtl_uniform *uniforms)
{
	// All the children shoudl be called

	// clean
	for (uint32_t i = 0; i < uniforms->nbuffers; i++) {
		vkDestroyBuffer(uniforms->device, uniforms->uniform_buffers[i], NULL);
		vkFreeMemory(uniforms->device, uniforms->uniform_buffers_memory[i], NULL);
	}

	return 0;
}

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

#include <talloc.h>

#include "trtl_uniform.h"
#include "helpers.h"

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
 * Pass the size of the uniform object.  A value of zero indicates the default size.
 *
 * @param ctx Context pointer.
 * @param nframes Number of frames in flight at once.
 * @param size Size of the uniform pool.  Zero (0) indicates the default size.
 */
struct trtl_uniform *
trtl_uniform_init(void *ctx, uint8_t nframes, size_t size) {
	struct trtl_uniform *uniforms;

	if (nframes < 1) {
		nframes = 1;
	}

	if (size == 0) {
		size = TRTL_DEFAULT_SIZE;
	}

	uniforms = talloc_size(ctx, offsetof(struct trtl_uniform, buffers[nframes]));
	assert(uniforms);
	talloc_set_name(uniforms, "Uniform Buffer");

	talloc_set_destructor(uniforms, trtl_uniform_destructor);

	uniforms->buffer_size = size;
	uniforms->nbuffers = nframes;

	for (int i = 0 ; i < nframes ; i++) {
		// FIXME: Should alloc in one slab of N times size
		uniforms->buffers[i] = talloc_named(uniforms, size, "Uniform buffer Frame %d", i); 
	}

	// Initilise the (terrible) bump alloator
	uniforms->bump.offset = 0;

	return uniforms;
}

/**
 * Render a uniform frame.
 *
 * Sends the uniform data to the GPU.
 */
int
trtl_uniform_render(int frame) {
	return 0;
}

struct trtl_uniform_info *
trtl_uniform_alloc(struct trtl_uniform *uniforms, size_t size) {
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

//should ne inline or something
void *
trtl_uniform_info_address(struct trtl_uniform_info *info, int frame) {
	return NULL;
}

static int
trtl_uniform_destructor(struct trtl_uniform *x) {
	// All the children shoudl be called
	
	// clean 
	
	return 0;
}



/**
 * Manager for the system object for rendering.
 *
 * Sets the uniform fields for the system interface.
 *
 * Currently sets the screensize and the current time for shaders.
 *
 * Should be added to the seer in the system layer.
 */
#include <sys/time.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_events.h"
#include "trtl_strata.h"
#include "trtl_strata_grid.h"
#include "trtl_uniform.h"
#include "turtle.h"

#include "shaders/trtl_strata_grid.include"

struct trtl_strata_grid {
	struct trtl_strata strata;
	struct trtl_uniform *uniform;
	struct trtl_uniform_info *uniform_info;

	uint32_t width, height;
	uint32_t tile_size;
	struct {
		struct {
			int32_t x;
			int32_t y;
		} dest;
		struct {
			float x;
			float y;
		} cur;
	} camera;
};

static bool sgrid_update(struct trtl_strata *obj, int frame);
static VkDescriptorSetLayout sgrid_descriptor_set_layout(struct trtl_strata *strata);
trtl_alloc static VkDescriptorSet *sgrid_descriptor_set(struct trtl_strata *strata);

static inline struct trtl_strata_grid *
trtl_strata_grid(struct trtl_strata *strata)
{
	struct trtl_strata_grid *sgrid;
	sgrid = talloc_get_type(strata, struct trtl_strata_grid);
	assert(sgrid != NULL);
	return sgrid;
}

static void
resize_callback(void *sgridv, trtl_arg_unused trtl_crier_cry_t cry, const void *event)
{
	struct trtl_event_resize *resize = talloc_get_type(event, struct trtl_event_resize);
	struct trtl_strata_grid *sgrid = talloc_get_type(sgridv, struct trtl_strata_grid);

	sgrid->width = resize->new_size.width;
	sgrid->height = resize->new_size.height;
}

static void
grid_move_callback(void *sgridv, trtl_arg_unused trtl_crier_cry_t cry, const void *event)
{
	struct trtl_event_grid_move *grid_move =
	    talloc_get_type(event, struct trtl_event_grid_move);
	struct trtl_strata_grid *sgrid = talloc_get_type(sgridv, struct trtl_strata_grid);

	sgrid->camera.x = grid_move->x;
	sgrid->camera.y = grid_move->y;
}

static void
grid_zoom_callback(void *sgridv, trtl_arg_unused trtl_crier_cry_t cry, const void *event)
{
	struct trtl_event_grid_zoom *grid_zoom =
	    talloc_get_type(event, struct trtl_event_grid_zoom);
	struct trtl_strata_grid *sgrid = talloc_get_type(sgridv, struct trtl_strata_grid);

	sgrid->tile_size = grid_zoom->feature_size;
	printf("New tile size: %d\n", sgrid->tile_size);
}

struct trtl_strata *
trtl_strata_grid_init(struct turtle *turtle)
{
	struct trtl_strata_grid *sgrid;

	sgrid = talloc(turtle, struct trtl_strata_grid);
	if (!sgrid) {
		return NULL;
	}

	sgrid->strata.turtle = turtle;
	sgrid->strata.update = sgrid_update;
	sgrid->strata.descriptor_set_layout = sgrid_descriptor_set_layout;
	sgrid->strata.descriptor_set = sgrid_descriptor_set;

	sgrid->uniform = trtl_uniform_init(turtle, "Strata Base Uniforms", turtle->tsc->nimages,
					   sizeof(struct trtl_strata_grid_uniforms));
	sgrid->uniform_info =
	    trtl_uniform_alloc_type(sgrid->uniform, struct trtl_strata_grid_uniforms);

	sgrid->width = 800;
	sgrid->height = 640;

	trtl_crier_listen(turtle->events->crier, "trtl_event_resize", resize_callback, sgrid);
	trtl_crier_listen(turtle->events->crier, "trtl_event_grid_move", grid_move_callback, sgrid);
	trtl_crier_listen(turtle->events->crier, "trtl_event_grid_zoom", grid_zoom_callback, sgrid);

	return &sgrid->strata;
}

static bool
sgrid_update(struct trtl_strata *strata, int frame)
{
	struct trtl_strata_grid *sgrid = trtl_strata_grid(strata);
	struct trtl_strata_grid_uniforms *uniforms =
	    trtl_uniform_info_address(sgrid->uniform_info, frame);

	uniforms->camera_center[0] = sgrid->camera.x;
	uniforms->camera_center[1] = sgrid->camera.y;

	uniforms->tile_size[0] = sgrid->tile_size;

	trtl_uniform_update(sgrid->uniform, frame);

	return true;
}

static VkDescriptorSetLayout
sgrid_descriptor_set_layout(struct trtl_strata *strata)
{
	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.pImmutableSamplers = NULL;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding bindings[1];
	bindings[0] = ubo_layout_binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = TRTL_ARRAY_SIZE(bindings);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(strata->turtle->device, &layoutInfo, NULL,
					&descriptor_set_layout) != VK_SUCCESS) {
		error("failed to create descriptor set layout!");
	}
	return descriptor_set_layout;
}

trtl_alloc static VkDescriptorSet *
sgrid_descriptor_set(struct trtl_strata *strata)
{
	struct trtl_strata_grid *sgrid = talloc_get_type(strata, struct trtl_strata_grid);
	uint32_t nframes = strata->turtle->tsc->nimages;
	VkDescriptorSet *sets = talloc_zero_array(strata, VkDescriptorSet, nframes);
	VkDescriptorSetLayout *layouts = talloc_zero_array(NULL, VkDescriptorSetLayout, nframes);

	for (uint32_t i = 0; i < nframes; i++) {
		// FIXME: Thsi should cache right
		layouts[i] = strata->descriptor_set_layout(strata);
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = strata->turtle->tsc->descriptor_pool;
	alloc_info.descriptorSetCount = nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(strata->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(sgrid->uniform_info, i);

		VkWriteDescriptorSet descriptorWrites[1] = {0};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(strata->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

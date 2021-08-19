/**
 * A canvas object is afairly simple canvas object used to render/manage a shader object.
 *
 * Used for backgrounds. skyboses and similar objects. It provides a number of hooks to insert
 * paramaters into a supplied shader.
 */
#include <assert.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_object_canvas.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h"

struct trtl_object_canvas {
	struct trtl_object parent;

	uint32_t nframes;
	VkDescriptorSet *descriptor_set;
	struct trtl_uniform_info *uniform_info;
};

trtl_alloc static VkDescriptorSet *create_descriptor_sets(struct trtl_object_canvas *canvas,
							  struct swap_chain_data *scd);
// FIXME: Should be a vertex2d here - it's a 2d object - fix this and
// allow 2d objects to be return from indices get.
// static const struct vertex2d vertices[] = {
static const struct vertex canvas_vertices[] = {
    {{-0.5f, -0.5f, 0}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
};

static const uint32_t canvas_indices[] = {0, 1, 2, 2, 3, 0};
static const uint32_t CANVAS_OBJECT_NINDEXES = TRTL_ARRAY_SIZE(canvas_indices);

// Inline function to cast from abstract to concrete type.
// FIXME: Make Debug and non-debug do different things
static inline struct trtl_object_canvas *
trtl_object_canvas(struct trtl_object *obj)
{
	struct trtl_object_canvas *canvas;
	canvas = talloc_get_type(obj, struct trtl_object_canvas);
	assert(canvas != NULL);
	return canvas;
}

static void
canvas_draw(struct trtl_object *obj, VkCommandBuffer cmd_buffer, VkPipelineLayout pipeline_layout,
	    int32_t offset)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
				canvas->descriptor_set, 0, NULL);
	vkCmdDrawIndexed(cmd_buffer, CANVAS_OBJECT_NINDEXES, 1, 0, offset, 0);
}

static uint32_t
canvas_vertices_get(trtl_arg_unused struct trtl_object *obj, const struct vertex **vertices)
{
	if (vertices) *vertices = canvas_vertices;
	return 4; // XXX: Magic
}

static uint32_t
canvas_indices_get(trtl_arg_unused struct trtl_object *obj, const uint32_t **indices, uint32_t *restrict offsetsize)
{
	if (indices) *indices = canvas_indices;
	if (offsetsize) *offsetsize = 4; // XXX: magic
	return CANVAS_OBJECT_NINDEXES;
}

static bool
canvas_update(trtl_arg_unused struct trtl_object *obj, trtl_arg_unused int frame)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);
	struct UniformBufferObject *ubo;

	ubo = trtl_uniform_info_address(canvas->uniform_info, frame);

	glm_mat4_identity(ubo->model);
	glm_scale_uni(ubo->model, 2.0f);

	glm_lookat((vec3){0.0f, 0.5f, 0.5f}, GLM_VEC3_ZERO, GLM_ZUP, ubo->view);

	//glm_perspective(glm_rad(45), 800 / 640.0, 0.1f, 10.0f, ubo->proj);
	//glm_perspective(0.1f, 800 / 640.0, 0.1f, 10.0f, ubo->proj);
	//glm_ortho(-1, 1, -1, 1, -1, 1, ubo->proj);
	//glm_ortho(0, 800.0/640 , 0, 1, 0.1f, 100, ubo->proj);
	glm_ortho_default(800.0/ 640, ubo->proj);
	ubo->proj[1][1] *= -1;

	// We updated
	return true;
}

struct trtl_object *
trtl_canvas_create(void *ctx, struct swap_chain_data *scd, trtl_arg_unused const char *shaderpath)
{
	struct trtl_object_canvas *canvas;

	canvas = talloc_zero(ctx, struct trtl_object_canvas);

	// FIXME: Set a destructor and cleanup

	canvas->parent.draw = canvas_draw;
	canvas->parent.vertices = canvas_vertices_get;
	canvas->parent.indices = canvas_indices_get;
	canvas->parent.update = canvas_update;

	canvas->nframes = scd->nimages;

	canvas->uniform_info =
	    trtl_uniform_alloc_type(evil_global_uniform, struct UniformBufferObject);

	canvas->descriptor_set = create_descriptor_sets(canvas, scd);

	return (struct trtl_object *)canvas;
}

trtl_alloc static VkDescriptorSet *
create_descriptor_sets(struct trtl_object_canvas *canvas, struct swap_chain_data *scd)
{
	VkDescriptorSet *sets = talloc_zero_array(canvas, VkDescriptorSet, canvas->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, canvas->nframes);

	for (uint32_t i = 0; i < canvas->nframes; i++) {
		layouts[i] = scd->render->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = scd->descriptor_pool;
	alloc_info.descriptorSetCount = canvas->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(scd->render->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	// FIXME: So leaky (create_texture_image never freed);
	// FIXME: This isn't needed - it's a short term hack to avoid
	// having to rewrite the descripter set layot config
	VkImageView texture_image_view = create_texture_image_view(
	    scd->render, create_texture_image(scd->render, "images/mac-picchu-512.jpg"));

	for (size_t i = 0; i < canvas->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(canvas->uniform_info, i);

		VkDescriptorImageInfo image_info = {0};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// This should be from the object
		image_info.imageView = texture_image_view;
		image_info.sampler = scd->render->texture_sampler;

		VkWriteDescriptorSet descriptorWrites[2] = {0};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &image_info;

		vkUpdateDescriptorSets(scd->render->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

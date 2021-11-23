/**
 * A canvas object is a fairly simple canvas object used to render/manage a shader object.
 *
 * Used for backgrounds. skyboses and similar objects. It provides a number of hooks to insert
 * paramaters into a supplied shader.
 */
#include <assert.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_object_canvas.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shell.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h"

struct trtl_object_canvas {
	struct trtl_object parent;

	uint32_t nframes;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet *descriptor_set;
	struct trtl_uniform_info *uniform_info;

	struct trtl_pipeline_info *pipeline_info;

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;

	VkExtent2D size;

	struct canvas_type *type;
};

struct canvas_shader_params {
	vec2 screenSize;
	float time;
};

#define PREFIX "shaders/canvas/"
#define PREFIX_GRID "shaders/grid/"

struct canvas_type {
	const char *name;
	struct {
		const char *vertex;
		const char *fragment;
	} shader;
} canvas_types[] = {
	{ "stars", { PREFIX "canvas-vertex.spv", PREFIX "stars-1.spv" } },
	{ "rainbow", { PREFIX "canvas-vertex.spv", PREFIX "test-color-fill.spv" } },
	{ "red", { PREFIX "canvas-vertex.spv", PREFIX_GRID "red.spv" } },
};

trtl_alloc static VkDescriptorSet *create_canvas_descriptor_sets(struct trtl_object_canvas *canvas,
								 struct trtl_swap_chain *scd);
// FIXME: Should be a vertex2d here - it's a 2d object - fix this and
// allow 2d objects to be return from indices get.
// static const struct vertex2d vertices[] = {
static const struct vertex canvas_vertices[] = {
    {{-1.0f, -1.0f, 0}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, -1.0f, 0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{1.0f, 1.0f, 0}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f, 0}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
};

static const uint32_t canvas_indices[] = {0, 1, 2}; //, 3,2,0};
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
canvas_draw(struct trtl_object *obj, VkCommandBuffer cmd_buffer, int32_t offset)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);
	VkDeviceSize offsets = 0;

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  canvas->pipeline_info->pipeline);

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &canvas->vertex_buffer, &offsets);
	vkCmdBindIndexBuffer(cmd_buffer, canvas->index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				canvas->pipeline_info->pipeline_layout, 0, 1, canvas->descriptor_set,
				0, NULL);
	vkCmdDrawIndexed(cmd_buffer, CANVAS_OBJECT_NINDEXES, 1, 0, offset, 0);
}

static bool
canvas_update(struct trtl_object *obj, trtl_arg_unused int frame)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);
	struct canvas_shader_params *params;

	params = trtl_uniform_info_address(canvas->uniform_info, frame);

	params->time = time(NULL);
	params->screenSize[0] = 1.0; // canvas->size.width;
	params->screenSize[1] = 0.5; // canvas->size.height;
	// printf("%f %f %f\n", params->screenSize[0],
	//		params->screenSize[1], params->time);

	// We updated
	return true;
}

static void
canvas_resize(struct trtl_object *obj, struct turtle *turtle, VkRenderPass renderpass,
	      VkExtent2D size)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);
	
	if (canvas->descriptor_set) {
		talloc_free(canvas->descriptor_set);
	}
	if (canvas->pipeline_info) {
		talloc_free(canvas->pipeline_info);
	}
	

	canvas->size = size;
	canvas->descriptor_set = create_canvas_descriptor_sets(canvas, turtle->tsc);
	canvas->pipeline_info = trtl_pipeline_create(
	    turtle, renderpass, size, canvas->descriptor_set_layout,
	    canvas->type->shader.vertex, canvas->type->shader.fragment, NULL, NULL, 0, false);
}

static VkDescriptorSetLayout
canvas_create_descriptor_set_layout(VkDevice device)
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

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &descriptor_set_layout) !=
	    VK_SUCCESS) {
		error("failed to create descriptor set layout!");
	}
	return descriptor_set_layout;
}

trtl_alloc struct trtl_object *
trtl_canvas_create(struct turtle *turtle, const char *typename)
{
	struct trtl_object_canvas *canvas;
	struct canvas_type *type = NULL;

	if (typename == NULL) {
		type = &canvas_types[0];
	} else {
		for (uint32_t i = 0 ; i < TRTL_ARRAY_SIZE(canvas_types) ; i ++) {
			if (streq(typename, canvas_types[i].name)) {
				type = &canvas_types[i];
			}
		}
		if (type == NULL) {
			error("Unknown canvsa type %s\n", typename);
		}
	}

	canvas = talloc_zero(NULL, struct trtl_object_canvas);
	canvas->type = type;

	// FIXME: Set a destructor and cleanup

	canvas->parent.draw = canvas_draw;
	canvas->parent.update = canvas_update;
	canvas->parent.relayer = canvas_resize;

	canvas->nframes = turtle->tsc->nimages;

	canvas->uniform_info =
	    trtl_uniform_alloc_type(turtle->uniforms, struct canvas_shader_params);

	canvas->descriptor_set_layout = canvas_create_descriptor_set_layout(turtle->device);
	canvas->descriptor_set = create_canvas_descriptor_sets(canvas, turtle->tsc);

	{
		struct trtl_seer_vertexset vertices;
		vertices.nvertexes = TRTL_ARRAY_SIZE(canvas_vertices);
		vertices.vertices = canvas_vertices;
		vertices.vertex_size = sizeof(struct vertex);

		canvas->vertex_buffer = create_vertex_buffers(turtle, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = CANVAS_OBJECT_NINDEXES;
		indexes.indexes = canvas_indices;
		canvas->index_buffer = create_index_buffer(turtle, &indexes);
	}

	return (struct trtl_object *)canvas;
}

trtl_alloc static VkDescriptorSet *
create_canvas_descriptor_sets(struct trtl_object_canvas *canvas, struct trtl_swap_chain *scd)
{
	VkDescriptorSet *sets = talloc_zero_array(canvas, VkDescriptorSet, canvas->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, canvas->nframes);

	for (uint32_t i = 0; i < canvas->nframes; i++) {
		layouts[i] = canvas->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = scd->descriptor_pool;
	alloc_info.descriptorSetCount = canvas->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(scd->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < canvas->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(canvas->uniform_info, i);

		VkWriteDescriptorSet descriptorWrites[1] = {0};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(scd->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

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
#include "trtl_shader.h"
#include "trtl_shell.h"
#include "trtl_strata.h"
#include "turtle.h"
#include "vertex.h"

struct trtl_object_canvas {
	struct trtl_object parent;

	uint32_t nframes;
	VkDescriptorSet *descriptor_set;
	VkDescriptorSetLayout descriptor_set_layout;

	struct trtl_pipeline_info *pipeline_info;

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;

	VkExtent2D size;

	struct canvas_type *type;

	// My strata
	struct trtl_strata *base;
};

struct canvas_shader_params {
	vec2 screenSize;
	float time;
};

struct canvas_type {
	const char *name;
	struct {
		const char *vertex;
		const char *fragment;
	} shader;
} canvas_types[] = {
    {"stars", {"canvas_vertex", "stars_1"}},
    {"rainbow", {"canvas_vertex", "test_color_fill"}},
    {"clouds", {"canvas_vertex", "clouds"}},
    //{ "red", { "canvas_vertex", PREFIX_GRID "red.spv" } },
};

EMBED_SHADER(canvas_vertex, "canvas-vertex.spv");
EMBED_SHADER(stars_1, "stars-1.spv");
EMBED_SHADER(clouds, "clouds.spv");
EMBED_SHADER(test_color_fill, "test-color-fill.spv");

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
				canvas->pipeline_info->pipeline_layout, 0, 1,
				canvas->descriptor_set, 0, NULL);
	vkCmdDrawIndexed(cmd_buffer, CANVAS_OBJECT_NINDEXES, 1, 0, offset, 0);
}

static bool
canvas_update(trtl_arg_unused struct trtl_object *obj, trtl_arg_unused int frame)
{
	// We updated
	return true;
}

static void
canvas_relayer(struct trtl_object *obj, struct turtle *turtle, struct trtl_layer *layer)
{
	struct trtl_object_canvas *canvas = trtl_object_canvas(obj);

	if (canvas->descriptor_set) {
		talloc_free(canvas->descriptor_set);
	}
	if (canvas->pipeline_info) {
		talloc_free(canvas->pipeline_info);
	}

	canvas->size = layer->rect.extent;
	canvas->descriptor_set = canvas->base->descriptor_set(canvas->base);
	canvas->descriptor_set_layout = canvas->base->descriptor_set_layout(canvas->base);
	canvas->pipeline_info = trtl_pipeline_create_with_strata(
	    turtle, layer, 1, &canvas->descriptor_set_layout, canvas->type->shader.vertex,
	    canvas->type->shader.fragment, NULL, NULL, 0);
}

trtl_alloc struct trtl_object *
trtl_canvas_create(struct turtle *turtle, const char *typename)
{
	struct trtl_object_canvas *canvas;
	struct canvas_type *type = NULL;

	if (typename == NULL) {
		type = &canvas_types[0];
	} else {
		for (uint32_t i = 0; i < TRTL_ARRAY_SIZE(canvas_types); i++) {
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
	canvas->parent.relayer = canvas_relayer;

	canvas->nframes = turtle->tsc->nimages;

	canvas->base = trtl_seer_strata_get(turtle, "base");

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

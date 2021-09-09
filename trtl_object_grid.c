/**
 * A grid object is afairly simple grid object used to render/manage a shader object.
 *
 * Used for backgrounds. skyboses and similar objects. It provides a number of hooks to insert
 * paramaters into a supplied shader.
 */
#include <assert.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_object_grid.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shell.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h"

extern int posX;
extern int posY;

struct trtl_object_grid {
	struct trtl_object parent;

	uint32_t nframes;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet *descriptor_set;
	struct trtl_uniform_info *uniform_info;

	struct trtl_pipeline_info pipeline_info;

	uint32_t vcount;
	uint32_t icount;

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;
};

trtl_alloc static VkDescriptorSet *grid_create_descriptor_sets(struct trtl_object_grid *grid,
							       struct swap_chain_data *scd);

static VkDescriptorSetLayout grid_create_descriptor_set_layout(VkDevice device);

// Inline function to cast from abstract to concrete type.
// FIXME: Make Debug and non-debug do different things
static inline struct trtl_object_grid *
trtl_object_grid(struct trtl_object *obj)
{
	struct trtl_object_grid *grid;
	grid = talloc_get_type(obj, struct trtl_object_grid);
	assert(grid != NULL);
	return grid;
}

// Width and height are single values.
static void
generate_grid(struct trtl_object_grid *grid, struct swap_chain_data *scd, uint16_t width,
	      uint16_t height)
{
	const int tiles = width * height;
	const int vcount = tiles * 4;
	const int icount = tiles * 6;

	struct vertex *vertex = talloc_zero_array(grid, struct vertex, vcount);
	uint32_t *indices = talloc_zero_array(grid, uint32_t, icount);

	for (int i = 0; i < tiles; i++) {
		int row = i / width;
		int col = i % width;
		vertex[i * 4].pos.x = col;
		vertex[i * 4].pos.y = row;
		vertex[i * 4].tex_coord.x = 0;
		vertex[i * 4].tex_coord.y = 0;

		vertex[i * 4 + 1].pos.x = col + 1;
		vertex[i * 4 + 1].pos.y = row;
		vertex[i * 4 + 1].tex_coord.x = 1;
		vertex[i * 4 + 1].tex_coord.y = 0;

		vertex[i * 4 + 2].pos.x = col;
		vertex[i * 4 + 2].pos.y = row + 1;
		vertex[i * 4 + 2].tex_coord.x = 0;
		vertex[i * 4 + 2].tex_coord.y = 1;

		vertex[i * 4 + 3].pos.x = col + 1;
		vertex[i * 4 + 3].pos.y = row + 1;
		vertex[i * 4 + 3].tex_coord.x = 1;
		vertex[i * 4 + 3].tex_coord.y = 1;

		indices[i * 6] = i * 4;
		indices[i * 6 + 1] = i * 4 + 2;
		indices[i * 6 + 2] = i * 4 + 1;
		indices[i * 6 + 3] = i * 4 + 1;
		indices[i * 6 + 4] = i * 4 + 2;
		indices[i * 6 + 5] = i * 4 + 3;
	}

	{
		struct trtl_seer_vertexset vertices;
		vertices.nvertexes = vcount;
		vertices.vertices = vertex;

		grid->vertex_buffer = create_vertex_buffers(scd->render, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = icount;
		indexes.indexes = indices;
		grid->index_buffer = create_index_buffer(scd->render, &indexes);
	}
	grid->vcount = vcount;
	grid->icount = icount;
}

VkRenderPass renderpasshack;

static void
grid_resize(struct trtl_object *obj, struct swap_chain_data *scd, VkExtent2D size)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	grid->descriptor_set_layout =
	    grid_create_descriptor_set_layout(scd->render->turtle->device);
	grid->descriptor_set = grid_create_descriptor_sets(grid, scd);
	grid->pipeline_info = trtl_pipeline_create(
	    scd->render->turtle->device, renderpasshack, size, grid->descriptor_set_layout,
	    "shaders/grid/grid-vertex.spv", "shaders/grid/browns.spv");
}

static void
grid_draw(struct trtl_object *obj, VkCommandBuffer cmd_buffer, trtl_arg_unused int32_t offset)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	VkDeviceSize offsets = 0;

	printf("Grid draw: Pipeline %p PipeLayout %p Descriptor set %p\n",
	       grid->pipeline_info.pipeline, grid->pipeline_info.pipeline_layout,
	       grid->descriptor_set);

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  grid->pipeline_info.pipeline);

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &grid->vertex_buffer, &offsets);
	vkCmdBindIndexBuffer(cmd_buffer, grid->index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				grid->pipeline_info.pipeline_layout, 0, 1, grid->descriptor_set, 0,
				NULL);
	vkCmdDrawIndexed(cmd_buffer, grid->icount, 1, 0, offset, 0);
}

static bool
grid_update(struct trtl_object *obj, trtl_arg_unused int frame)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	struct pos2d *pos;

	pos = trtl_uniform_info_address(grid->uniform_info, frame);

	pos->x = posX;
	pos->y = posY;

	// We updated
	return true;
}

struct trtl_object *
trtl_grid_create(void *ctx, struct swap_chain_data *scd, VkRenderPass render_pass,
		 VkExtent2D extent, VkDescriptorSetLayout descriptor_set_layout, uint16_t width,
		 uint16_t height)
{
	struct trtl_object_grid *grid;

	grid = talloc_zero(ctx, struct trtl_object_grid);

	// FIXME: Set a destructor and cleanup

	grid->parent.draw = grid_draw;
	grid->parent.update = grid_update;
	grid->parent.resize = grid_resize;

	renderpasshack = render_pass;

	grid->nframes = scd->nimages;

	grid->uniform_info =
	    // FIXME: This causes a crash.
	    // trtl_uniform_alloc_type(evil_global_uniform, struct pos2d);
	    trtl_uniform_alloc_type(evil_global_uniform, struct UniformBufferObject);

	grid->descriptor_set_layout =
	    grid_create_descriptor_set_layout(scd->render->turtle->device);
	grid->descriptor_set = grid_create_descriptor_sets(grid, scd);

	grid->pipeline_info =
	    trtl_pipeline_create(scd->render->turtle->device, render_pass, extent,
				 grid->descriptor_set_layout, "shaders/grid/grid-vertex.spv",
				 // "shaders/grid/lines.spv");
				 "shaders/grid/browns.spv");
	//"shaders/grid/stars-1.spv");
	//"shaders/grid/red.spv");

	generate_grid(grid, scd, width, height);

	return (struct trtl_object *)grid;
}

static VkDescriptorSetLayout
grid_create_descriptor_set_layout(VkDevice device)
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

trtl_alloc static VkDescriptorSet *
grid_create_descriptor_sets(struct trtl_object_grid *grid, struct swap_chain_data *scd)
{
	VkDescriptorSet *sets = talloc_zero_array(grid, VkDescriptorSet, grid->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, grid->nframes);

	for (uint32_t i = 0; i < grid->nframes; i++) {
		layouts[i] = grid->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = scd->descriptor_pool;
	alloc_info.descriptorSetCount = grid->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(scd->render->turtle->device, &alloc_info, sets) !=
	    VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < grid->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(grid->uniform_info, i);

		VkWriteDescriptorSet descriptorWrites[1] = {0};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(scd->render->turtle->device,
				       TRTL_ARRAY_SIZE(descriptorWrites), descriptorWrites, 0,
				       NULL);
	}

	talloc_free(layouts);

	return sets;
}

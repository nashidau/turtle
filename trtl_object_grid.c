/**
 * A grid object is afairly simple grid object used to render a map.
 *
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

#define FRAG_SHADER "shaders/grid/lines.spv"

struct trtl_object_grid {
	struct trtl_object parent;

	struct turtle *turtle;

	// This is the current active 'title'
	struct {
		float x;
		float y;
	} pos;
	struct {
		float x;
		float y;
	} dest;

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
							       struct trtl_swap_chain *scd);

struct grid_vertex {
	struct pos3d pos;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t seed;
	} tile;
	struct pos2d tex_coord;
};

static const VkVertexInputAttributeDescription grid_vertex_description[3] = {
    {
	.binding = 0,
	.location = 0,
	.format = VK_FORMAT_R32G32B32_SFLOAT,
	.offset = offsetof(struct grid_vertex, pos),
    },
    {
	.binding = 0,
	.location = 1,
	.format = VK_FORMAT_R32G32B32_UINT,
	.offset = offsetof(struct grid_vertex, tile),
    },
    {
	.binding = 0,
	.location = 2,
	.format = VK_FORMAT_R32G32_SFLOAT,
	.offset = offsetof(struct grid_vertex, tex_coord),
    },
};

static const VkVertexInputBindingDescription grid_binding_descriptor = {
    .binding = 0,
    .stride = sizeof(struct grid_vertex),
    // Should this be 'instance?'
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

#define N_VERTEX_ATTRIBUTE_DESCRIPTORS TRTL_ARRAY_SIZE(grid_vertex_description)

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

static void
fill_tile(struct grid_vertex *vertex, uint32_t *indices, int i, int row, int col)
{
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

	vertex[i * 4].tile.x = col;
	vertex[i * 4 + 1].tile.x = col;
	vertex[i * 4 + 2].tile.x = col;
	vertex[i * 4 + 3].tile.x = col;

	vertex[i * 4].tile.y = row;
	vertex[i * 4 + 1].tile.y = row;
	vertex[i * 4 + 2].tile.y = row;
	vertex[i * 4 + 3].tile.y = row;

	uint8_t seed = ((row + col * 87) * 48271) >> 4;
	vertex[i * 4].tile.seed = seed;
	vertex[i * 4 + 1].tile.seed = seed;
	vertex[i * 4 + 2].tile.seed = seed;
	vertex[i * 4 + 3].tile.seed = seed;

	indices[i * 6] = i * 4;
	indices[i * 6 + 1] = i * 4 + 2;
	indices[i * 6 + 2] = i * 4 + 1;
	indices[i * 6 + 3] = i * 4 + 1;
	indices[i * 6 + 4] = i * 4 + 2;
	indices[i * 6 + 5] = i * 4 + 3;
}

// Width and height are single values.
static void
generate_grid(struct trtl_object_grid *grid, uint16_t width, uint16_t height)
{
	const int tiles = width * height;
	const int vcount = tiles * 4;
	const int icount = tiles * 6;

	struct grid_vertex *vertex = talloc_zero_array(grid, struct grid_vertex, vcount);
	uint32_t *indices = talloc_zero_array(grid, uint32_t, icount);

	for (int i = 0; i < tiles; i++) {
		int row = i / width;
		int col = i % width;
		fill_tile(vertex, indices, i, row, col);
	}

	{
		struct trtl_seer_vertexset vertices;
		vertices.vertex_size = sizeof(struct grid_vertex);
		vertices.nvertexes = vcount;
		vertices.vertices = vertex;

		grid->vertex_buffer = create_vertex_buffers(grid->turtle, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = icount;
		indexes.indexes = indices;
		grid->index_buffer = create_index_buffer(grid->turtle, &indexes);
	}
	grid->vcount = vcount;
	grid->icount = icount;
}

static void
grid_resize(struct trtl_object *obj, struct turtle *turtle, VkRenderPass renderpass,
	    VkExtent2D size)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	grid->descriptor_set_layout = grid_create_descriptor_set_layout(turtle->device);
	grid->descriptor_set = grid_create_descriptor_sets(grid, turtle->tsc);
	grid->pipeline_info = trtl_pipeline_create(
	    turtle, renderpass, size, grid->descriptor_set_layout, "shaders/grid/grid-vertex.spv",
	    FRAG_SHADER, &grid_binding_descriptor, grid_vertex_description,
	    N_VERTEX_ATTRIBUTE_DESCRIPTORS, false);
}

static void
grid_draw(struct trtl_object *obj, VkCommandBuffer cmd_buffer, trtl_arg_unused int32_t offset)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	VkDeviceSize offsets = 0;

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
grid_update(struct trtl_object *obj, int frame)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	struct pos2d *pos;

	pos = trtl_uniform_info_address(grid->uniform_info, frame);

	if (grid->dest.x != grid->pos.x) {
		float diff = grid->pos.x - grid->dest.x;
		if (fabsf(diff) < 0.01)
			grid->pos.x = grid->dest.x;
		else
			grid->pos.x -= diff / 2;
	}
	if (grid->dest.y != grid->pos.y) {
		float diff = grid->pos.y - grid->dest.y;
		if (fabsf(diff) < 0.01)
			grid->pos.y = grid->dest.y;
		else
			grid->pos.y -= diff / 2;
	}

	pos->x = grid->pos.x;
	pos->y = grid->pos.y;

	// We updated
	return true;
}

struct trtl_object *
trtl_grid_create(struct turtle *turtle)
{
	struct trtl_object_grid *grid;

	grid = talloc_zero(NULL, struct trtl_object_grid);
	grid->turtle = turtle;

	// FIXME: Set a destructor and cleanup

	grid->parent.draw = grid_draw;
	grid->parent.update = grid_update;
	grid->parent.relayer = grid_resize;

	grid->nframes = turtle->tsc->nimages;

	grid->uniform_info =
	    // FIXME: This causes a crash.
	    // trtl_uniform_alloc_type(evil_global_uniform, struct pos2d);
	    trtl_uniform_alloc_type(turtle->uniforms, struct UniformBufferObject);

	return (struct trtl_object *)grid;
}

/**
 * Fill the grid with a pattern.
 *
 * The width * height map is filled if the appropriate position in the grid is the same as
 * presentchar, or skipped otherwsei.
 *
 * @param obj Grid object to fill.
 * @param width The width of the map.
 * @param height The height of the map.
 * @param pattern The (width * height) array of items.
 * @param presentchar The character which indicates a visible tile.
 * @return 0 on success, -1 otherwise.
 */
int
trtl_grid_fill_pattern(struct trtl_object *obj, uint32_t width, uint32_t height,
		       uint8_t pattern[width * height], uint8_t presentchar)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	uint32_t total;
	int filled = 0;

	if (grid->vcount > 0) {
		error("Already confgiured this grid object");
	}

	total = width * height;

	for (uint32_t i = 0; i < total; i++) {
		if (pattern[i] != presentchar) {
			continue;
		}
		filled++;
	}

	// So we need 6 indices, and 4 vertexes for each tile
	uint32_t vcount = filled * 4;
	uint32_t icount = filled * 6;
	struct grid_vertex *vertex = talloc_zero_array(grid, struct grid_vertex, vcount);
	uint32_t *indices = talloc_zero_array(grid, uint32_t, icount);

	// Now fill it
	filled = 0;
	for (uint32_t i = 0; i < total; i++) {
		int row = i / width;
		int col = i % width;
		if (pattern[i] != presentchar) {
			continue;
		}
		fill_tile(vertex, indices, filled++, row, col);
	}

	// FIXME: grid_rectangle has the same code
	{
		struct trtl_seer_vertexset vertices;
		vertices.vertex_size = sizeof(struct grid_vertex);
		vertices.nvertexes = vcount;
		vertices.vertices = vertex;

		grid->vertex_buffer = create_vertex_buffers(grid->turtle, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = icount;
		indexes.indexes = indices;
		grid->index_buffer = create_index_buffer(grid->turtle, &indexes);
	}
	grid->vcount = vcount;
	grid->icount = icount;

	return 0;
}

/**
 * Fill the grid with tiles which are width * height in size.
 *
 * @param obj Grid object.
 * @param width Number of tiles wide.
 * @param height Number of tiles high.
 * @return 0 on success, -1 otherwise.
 */
int
trtl_grid_fill_rectangle(struct trtl_object *obj, uint32_t width, uint32_t height)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);
	generate_grid(grid, width, height);

	return 0;
}

/**
 * Set the active (center) square.
 *
 * Sets the active square for the grid.  Will snap the to the center of the that square in the
 * middle of the screen.  It can be a partial square to show the middle.
 *
 * @param obj Grid object to update.
 * @param x X position of the center square.
 * @param y Y position of the center square.
 * @param motion Function to move to trnsform.
 * @return 0 on success.
 */
int
trtl_grid_set_active_tile(struct trtl_object *obj, float x, float y, int motion)
{
	struct trtl_object_grid *grid = trtl_object_grid(obj);

	if (!motion) {
		grid->pos.x = x;
		grid->pos.y = y;
	}
	grid->dest.x = x;
	grid->dest.y = y;

	return 0;
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
grid_create_descriptor_sets(struct trtl_object_grid *grid, struct trtl_swap_chain *scd)
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

	if (vkAllocateDescriptorSets(scd->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < grid->nframes; i++) {
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

		vkUpdateDescriptorSets(scd->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

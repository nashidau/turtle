/**
 * A simple 2d image object.
 */
#include <assert.h>

#include "trtl_object.h"
#include "trtl_object_sprite.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shell.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h"

#include <talloc.h>

#define VERTEX_SHADER "shaders/sprite/sprite-vertex.spv"
#define FRAGMENT_SHADER "shaders/sprite/sprite-fragment.spv"

struct trtl_object_sprite {
	struct trtl_object parent;

	struct turtle *turtle;
	uint32_t nframes;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet *descriptor_set;
	struct trtl_uniform_info *uniform_info;

	struct trtl_pipeline_info pipeline_info;
	uint32_t vcount;
	uint32_t icount;

	// FIXME: This so needs to be abstraced away
	VkDeviceMemory texture_image_memory;
	VkImage texture_image;
	VkImageView texture_image_view;

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;
};

static const struct vertex sprite_vertices[] = {
    {{-1.0f, -1.0f, 0}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, -1.0f, 0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{1.0f, 1.0f, 0}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f, 0}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
};

struct sprite_vertex {
	struct pos3d pos;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t seed;
	} tile;
	struct pos2d tex_coord;
};

static const VkVertexInputAttributeDescription sprite_vertex_description[3] = {
    {
	.binding = 0,
	.location = 0,
	.format = VK_FORMAT_R32G32B32_SFLOAT,
	.offset = offsetof(struct sprite_vertex, pos),
    },
    {
	.binding = 0,
	.location = 1,
	.format = VK_FORMAT_R32G32B32_SFLOAT,
	.offset = offsetof(struct sprite_vertex, tile),
    },
    {
	.binding = 0,
	.location = 2,
	.format = VK_FORMAT_R32G32_SFLOAT,
	.offset = offsetof(struct sprite_vertex, tex_coord),
    },
};

static const VkVertexInputBindingDescription sprite_binding_descriptor = {
    .binding = 0,
    .stride = sizeof(struct sprite_vertex),
    // Should this be 'instance?'
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

#define N_VERTEX_ATTRIBUTE_DESCRIPTORS TRTL_ARRAY_SIZE(sprite_vertex_description)

static const uint32_t sprite_indices[] = {0, 1, 2, 3, 2, 0};
static const uint32_t CANVAS_OBJECT_NINDEXES = TRTL_ARRAY_SIZE(sprite_indices);

trtl_alloc static VkDescriptorSet *sprite_create_descriptor_sets(struct trtl_object_sprite *sprite,
								 struct trtl_swap_chain *scd);

static VkDescriptorSetLayout sprite_create_descriptor_set_layout(VkDevice device);

// Inline function to cast from abstract to concrete type.
// FIXME: Make Debug and non-debug do different things
static inline struct trtl_object_sprite *
trtl_object_sprite(struct trtl_object *obj)
{
	struct trtl_object_sprite *sprite;
	sprite = talloc_get_type(obj, struct trtl_object_sprite);
	assert(sprite != NULL);
	return sprite;
}

static bool
sprite_update(trtl_arg_unused struct trtl_object *obj, trtl_arg_unused int frame)
{
	return false;
}

static void
sprite_draw(struct trtl_object *obj, VkCommandBuffer cmd_buffer, int32_t offset)
{
	struct trtl_object_sprite *sprite = trtl_object_sprite(obj);
	VkDeviceSize offsets = 0;

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  sprite->pipeline_info.pipeline);

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &sprite->vertex_buffer, &offsets);
	vkCmdBindIndexBuffer(cmd_buffer, sprite->index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				sprite->pipeline_info.pipeline_layout, 0, 1, sprite->descriptor_set,
				0, NULL);
	vkCmdDrawIndexed(cmd_buffer, sprite->icount, 1, 0, offset, 0);
}

// Triggered when an object is first added to a layer or on resize.
// The object should create it's pipeline based on the info in this call
static void
sprite_relayer(struct trtl_object *obj, struct turtle *turtle,
	       trtl_arg_unused VkRenderPass renderpass, trtl_arg_unused VkExtent2D size)
{

	struct trtl_object_sprite *sprite = trtl_object_sprite(obj);
	sprite->descriptor_set_layout = sprite_create_descriptor_set_layout(turtle->device);
	sprite->descriptor_set = sprite_create_descriptor_sets(sprite, turtle->tsc);

	printf("sprite pipeline info %p %p %d/%d %p %s %s %p %p %d\n",
	    turtle, renderpass, size.width, size.height, sprite->descriptor_set_layout,
	    VERTEX_SHADER, FRAGMENT_SHADER, &sprite_binding_descriptor, &sprite_vertex_description,
	    N_VERTEX_ATTRIBUTE_DESCRIPTORS);
	sprite->pipeline_info = trtl_pipeline_create(
	    turtle, renderpass, size, sprite->descriptor_set_layout, VERTEX_SHADER, FRAGMENT_SHADER,
	    &sprite_binding_descriptor, sprite_vertex_description, N_VERTEX_ATTRIBUTE_DESCRIPTORS);
}

struct trtl_object *
trtl_object_sprite_add(struct turtle *turtle, trtl_arg_unused const char *image)
{
	struct trtl_object_sprite *sprite;

	sprite = talloc_zero(NULL, struct trtl_object_sprite);

	sprite->parent.draw = sprite_draw;
	sprite->parent.update = sprite_update;
	sprite->parent.relayer = sprite_relayer;

	sprite->nframes = turtle->tsc->nimages;

	// FIXME: Leaky on create_texture_image
	sprite->texture_image_view =
	    create_texture_image_view(turtle, create_texture_image(turtle, image));

	sprite->uniform_info =
	    trtl_uniform_alloc_type(turtle->uniforms, struct UniformBufferObject);

	sprite->descriptor_set_layout = sprite_create_descriptor_set_layout(turtle->device);
	sprite->descriptor_set = sprite_create_descriptor_sets(sprite, turtle->tsc);

	{
		struct trtl_seer_vertexset vertices;
		vertices.nvertexes = TRTL_ARRAY_SIZE(sprite_vertices);
		vertices.vertices = sprite_vertices;
		vertices.vertex_size = sizeof(struct vertex);

		sprite->vertex_buffer = create_vertex_buffers(turtle, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = CANVAS_OBJECT_NINDEXES;
		indexes.indexes = sprite_indices;
		sprite->index_buffer = create_index_buffer(turtle, &indexes);
	}

	return &sprite->parent;
}

static VkDescriptorSetLayout
sprite_create_descriptor_set_layout(VkDevice device)
{
	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.pImmutableSamplers = NULL;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding sampler_layout_binding = {0};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = NULL;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[2] = {ubo_layout_binding, sampler_layout_binding};
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
sprite_create_descriptor_sets(struct trtl_object_sprite *sprite, struct trtl_swap_chain *scd)
{
	VkDescriptorSet *sets = talloc_zero_array(sprite, VkDescriptorSet, sprite->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, sprite->nframes);

	for (uint32_t i = 0; i < sprite->nframes; i++) {
		layouts[i] = sprite->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = scd->descriptor_pool;
	alloc_info.descriptorSetCount = sprite->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(scd->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < sprite->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(sprite->uniform_info, i);

		VkDescriptorImageInfo image_info = {0};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = sprite->texture_image_view;
		image_info.sampler = scd->turtle->texture_sampler;

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

		vkUpdateDescriptorSets(scd->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

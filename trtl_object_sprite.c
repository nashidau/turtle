/**
 * A sprite object is a used to render a 2d sprite object.  Multiple different objects can be added
 * using the tempplate object.  Each different sprite is identified by a referene object (subsprite)
 * and then an instance of the subsprite.
 *
 */
#include <assert.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_object_sprite.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shell.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h"

struct trtl_object_sprite {
	struct trtl_object parent;

	struct turtle *turtle;

	uint32_t nframes;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet *descriptor_set;
	struct trtl_uniform_info *uniform_info;

	struct trtl_pipeline_info pipeline_info;

	VkImageView texture_image_view;

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;

	VkExtent2D size;
};

struct trtl_sprite_subsprite {
	int count;
};

struct sprite_shader_params {
	vec2 screenSize;
	float time;
};

trtl_alloc static VkDescriptorSet *create_sprite_descriptor_sets(struct trtl_object_sprite *sprite);

// FIXME: Should be a vertex2d here - it's a 2d object - fix this and
// allow 2d objects to be return from indices get.
// static const struct vertex2d vertices[] = {
static const struct vertex sprite_vertices[] = {
    {{-1.0f, -1.0f, 0}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{1.0f, -1.0f, 0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{1.0f, 1.0f, 0}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f, 0}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
};

static const uint32_t sprite_indices[] = {0, 1, 2, 3, 2, 1};
static const uint32_t CANVAS_OBJECT_NINDEXES = TRTL_ARRAY_SIZE(sprite_indices);

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
	vkCmdDrawIndexed(cmd_buffer, CANVAS_OBJECT_NINDEXES, 1, 0, offset, 0);
}

static bool
sprite_update(struct trtl_object *obj, trtl_arg_unused int frame)
{
	struct trtl_object_sprite *sprite = trtl_object_sprite(obj);
	struct sprite_shader_params *params;

	params = trtl_uniform_info_address(sprite->uniform_info, frame);

	params->time = time(NULL);
	params->screenSize[0] = 1.0; // sprite->size.width;
	params->screenSize[1] = 0.5; // sprite->size.height;

	// We updated
	return true;
}

static void
sprite_resize(struct trtl_object *obj, struct turtle *turtle, VkRenderPass renderpass,
	      VkExtent2D size)
{
	struct trtl_object_sprite *sprite = trtl_object_sprite(obj);
	sprite->size = size;
	sprite->descriptor_set = create_sprite_descriptor_sets(sprite);
	// FIXME: Alternative is sprite-red-boder - which has a cool red border instead
	// of alpha.
	sprite->pipeline_info =
	    trtl_pipeline_create(turtle, renderpass, size, sprite->descriptor_set_layout,
				 "shaders/sprite/sprite-vertex.spv",
				 "shaders/sprite/sprite-fragment.spv", NULL, NULL, 0, true);
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

// FIXME: Just imp
trtl_alloc struct trtl_sprite_subsprite *
trtl_sprite_subsprite_add(struct trtl_object *object, const char *image)
{
	struct trtl_object_sprite *sprite = trtl_object_sprite(object);
	assert(image != NULL);

	struct trtl_sprite_subsprite *subsprite;

	// FIXME: Leaky
	sprite->texture_image_view =
	    create_texture_image_view(sprite->turtle, create_texture_image(sprite->turtle, image));

	sprite->uniform_info =
	    trtl_uniform_alloc_type(sprite->turtle->uniforms, struct sprite_shader_params);
	sprite->descriptor_set = create_sprite_descriptor_sets(sprite);

	subsprite = talloc_zero(sprite, struct trtl_sprite_subsprite);
	subsprite->count = 1;
	return subsprite;
}

trtl_subsprite_index trtl_sprite_subsprite_instance_add(struct trtl_sprite_subsprite *subsprite);
int trtl_sprite_subsprite_position_set(struct trtl_sprite_subsprite *subsprite,
				       trtl_subsprite_index index, int x, int y);

trtl_alloc struct trtl_object *
trtl_sprite_create(struct turtle *turtle)
{
	struct trtl_object_sprite *sprite;

	sprite = talloc_zero(NULL, struct trtl_object_sprite);
	sprite->turtle = turtle;

	// FIXME: Set a destructor and cleanup

	sprite->parent.draw = sprite_draw;
	sprite->parent.update = sprite_update;
	sprite->parent.relayer = sprite_resize;

	sprite->nframes = turtle->tsc->nimages;

	sprite->descriptor_set_layout = sprite_create_descriptor_set_layout(turtle->device);

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

	return (struct trtl_object *)sprite;
}

trtl_alloc static VkDescriptorSet *
create_sprite_descriptor_sets(struct trtl_object_sprite *sprite)
{
	VkDescriptorSet *sets = talloc_zero_array(sprite, VkDescriptorSet, sprite->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, sprite->nframes);

	for (uint32_t i = 0; i < sprite->nframes; i++) {
		layouts[i] = sprite->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = sprite->turtle->tsc->descriptor_pool;
	alloc_info.descriptorSetCount = sprite->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(sprite->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < sprite->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(sprite->uniform_info, i);

		VkDescriptorImageInfo image_info = {0};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = sprite->texture_image_view;
		image_info.sampler = sprite->turtle->texture_sampler;

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

		vkUpdateDescriptorSets(sprite->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

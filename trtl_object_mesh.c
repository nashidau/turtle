#include <string.h>
#include <talloc.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "helpers.h"
#include "trtl_object_mesh.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h" // FIXME: has trtl_model in it

struct trtl_object_mesh {
	struct trtl_object parent;

	// What we are drawing (sadly one for now)
	struct trtl_model *model;

	uint32_t nframes;
	VkDescriptorSet *descriptor_set;

	VkDeviceMemory texture_image_memory;
	VkImage texture_image;
	VkImageView texture_image_view;

	struct trtl_uniform_info *uniform_info;

	bool reverse;
};

// Inline function to cast from abstract to concrete type.
// FIXME: Make Debug and non-debug do different things
static inline struct trtl_object_mesh *
trtl_object_mesh(struct trtl_object *obj) {
	struct trtl_object_mesh *mesh;
	mesh = talloc_get_type(obj, struct trtl_object_mesh);
	assert(mesh != NULL);
	return mesh;
}

trtl_alloc static VkDescriptorSet *
create_descriptor_sets(struct trtl_object_mesh *mesh, struct swap_chain_data *scd);

static void trtl_object_draw_(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
			      VkPipelineLayout pipeline_layout, int32_t offset)
{
        struct trtl_object_mesh *mesh = trtl_object_mesh(obj);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
				mesh->descriptor_set, 0, NULL);
	vkCmdDrawIndexed(cmd_buffer, mesh->model->nindices, 1, 0, offset, 0);
}

static uint32_t trtl_object_vertices_get_(struct trtl_object *obj, const struct vertex **vertices)
{
        struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	if (vertices) *vertices = mesh->model->vertices;
	return mesh->model->nvertices;
}

static uint32_t trtl_object_indices_get_(struct trtl_object *obj, const uint32_t **indices)
{
        struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	if (indices) *indices = mesh->model->indices;
	return mesh->model->nindices;
}

static int trtl_object_mesh_destructor(trtl_arg_unused struct trtl_object_mesh *obj)
{
	// FIXME: Free model
	return 0;
}

static bool trtl_object_update_(struct trtl_object *obj, int frame)
{

        struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	// static int startTime = 0;
	// int currentTime = 1;
	static float time = 1.0f;
	time = time + 1;
	struct UniformBufferObject *ubo;

	ubo = trtl_uniform_info_address(mesh->uniform_info, frame);

	glm_mat4_identity(ubo->model);
	if (mesh->reverse) {
		glm_rotate(ubo->model, glm_rad(time), GLM_ZUP);
	} else {
		glm_rotate(ubo->model, -glm_rad(time), GLM_ZUP);
	}

	{
		vec3 y = {0, 0, 0};
		glm_translate(ubo->model, y);
	}

	glm_lookat((vec3){2.0f, 2.0f, 2.0f}, GLM_VEC3_ZERO, GLM_ZUP, ubo->view);

	glm_perspective(glm_rad(45), 800 / 640.0, 0.1f, 10.0f, ubo->proj);
	ubo->proj[1][1] *= -1;

	// We updated
	return true;
}

struct trtl_object *trtl_object_mesh_create(void *ctx, struct swap_chain_data *scd,
					    const char *path, const char *texture)
{
	struct trtl_object_mesh *mesh;

	mesh = talloc_zero(ctx, struct trtl_object_mesh);

	talloc_set_destructor(mesh, trtl_object_mesh_destructor);
	mesh->parent.draw = trtl_object_draw_;
	mesh->parent.vertices = trtl_object_vertices_get_;
	mesh->parent.indices = trtl_object_indices_get_;
	mesh->parent.update = trtl_object_update_;

	mesh->nframes = scd->nimages;
	mesh->model = load_model(path);
	if (!mesh->model) {
		talloc_free(mesh);
		return NULL;
	}

	mesh->uniform_info =
	    trtl_uniform_alloc_type(evil_global_uniform, struct UniformBufferObject);

	// FIXME: So leaky (create_texture_image never freed);
	mesh->texture_image_view =
	    create_texture_image_view(scd->render, create_texture_image(scd->render, texture));

	mesh->descriptor_set = create_descriptor_sets(mesh, scd);

	if (strstr(path, "Couch")) mesh->reverse = 1;

	return (struct trtl_object *)mesh;
}

trtl_alloc static VkDescriptorSet *create_descriptor_sets(struct trtl_object_mesh *mesh,
							  struct swap_chain_data *scd)
{
	VkDescriptorSet *sets = talloc_zero_array(mesh, VkDescriptorSet, mesh->nframes);
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, mesh->nframes);

	for (uint32_t i = 0; i < mesh->nframes; i++) {
		layouts[i] = scd->render->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = scd->descriptor_pool;
	alloc_info.descriptorSetCount = mesh->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(scd->render->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < mesh->nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(mesh->uniform_info, i);

		VkDescriptorImageInfo image_info = {0};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// This hsould be from the object
		image_info.imageView = mesh->texture_image_view;
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

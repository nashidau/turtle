/**
 * Simple 3d mesh object.
 *
 * Descriptor sets are:
 * 	set 0: strata 'base', generic base
 * 	set 1: strata 'grid', isometric rendering support
 * 	set 2: binding 0:  Our uniform buffer
 * 		binding 1: Texture
 *
 *
 * So we track the objects we draw with a big array.  Each  has the index of the model pointer.
 * Then the objects current geometry.
 *
 *
 * FIXME: models should be tracked by the loader - use refernce counting to keep track.
 */
#include <string.h>
#include <talloc.h>

#include <vulkan/vulkan.h>

#include "helpers.h"
#include "trtl_object_mesh.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shader.h"
#include "trtl_shell.h"
#include "trtl_strata.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"
#include "turtle.h"
#include "vertex.h" // FIXME: has trtl_model in it

// Position information about a single object.
struct mesh_geometry {
	struct {
		uint32_t x;
		uint32_t y;
		float facing;
	} dest;
	struct {
		uint32_t x;
		uint32_t y;
		float facing;
	} cur;
};

struct trtl_object_mesh {
	struct trtl_grid_object parent;
	struct turtle *turtle;

	// What we are drawing (sadly one for now)
	struct trtl_model *model;
	float rotation;
	float vrotate; // FIXME: Use a proper set of vectors for this

	uint32_t nframes;

	VkDeviceMemory texture_image_memory;
	VkImage texture_image;
	VkImageView texture_image_view;

	struct trtl_uniform_info *uniform_info;

	struct trtl_pipeline_info *pipeline_info;

	VkDescriptorSetLayout descriptor_set_layout[3];
	VkDescriptorSet *descriptor_set[3];

	VkBuffer index_buffer;
	VkBuffer vertex_buffer;

	struct {
		struct trtl_strata *base;
		struct trtl_strata *grid;
	} strata;

	struct mesh_geometry mgeo;
};

EMBED_SHADER(mesh_vertex, "mesh-vert.spv");
EMBED_SHADER(mesh_fragment, "mesh.spv");
EMBED_SHADER(mesh_fragment_notexture, "mesh-notexture.spv");

trtl_alloc static VkDescriptorSet *create_descriptor_sets(struct trtl_object_mesh *mesh);
static VkDescriptorSetLayout descriptor_set_layout(struct trtl_object_mesh *mesh);

// Inline function to cast from abstract to concrete type.
// FIXME: Make Debug and non-debug do different things
static inline struct trtl_object_mesh *
trtl_object_mesh(struct trtl_object *obj)
{
	struct trtl_object_mesh *mesh;
	mesh = talloc_get_type(obj, struct trtl_object_mesh);
	assert(mesh != NULL);
	return mesh;
}

static void
trtl_object_draw_(struct trtl_object *obj, VkCommandBuffer cmd_buffer, int32_t offset)
{
	VkDeviceSize offsets = 0;
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  mesh->pipeline_info->pipeline);

	// Bind in the strata.
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				mesh->pipeline_info->pipeline_layout, 0, 1, mesh->descriptor_set[0],
				0, NULL);
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				mesh->pipeline_info->pipeline_layout, 1, 1, mesh->descriptor_set[1],
				0, NULL);

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &mesh->vertex_buffer, &offsets);
	vkCmdBindIndexBuffer(cmd_buffer, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				mesh->pipeline_info->pipeline_layout, 2, 1, mesh->descriptor_set[2],
				0, NULL);

	vkCmdDrawIndexed(cmd_buffer, mesh->model->nindices, 1, 0, offset, 0);
}

static int
trtl_object_mesh_destructor(trtl_arg_unused struct trtl_object_mesh *obj)
{
	// FIXME: Free model
	return 0;
}

static bool
trtl_object_update_(struct trtl_object *obj, int frame)
{
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	struct UniformBufferObject *ubo;

	if (mesh->mgeo.cur.facing != mesh->mgeo.dest.facing) {
		float diff = mesh->mgeo.dest.facing - mesh->mgeo.cur.facing;
		diff /= 4;
		if (fabs(diff) < 0.01) {
			mesh->mgeo.cur.facing = mesh->mgeo.dest.facing;
		} else {
			mesh->mgeo.cur.facing += diff;
		}
	}

	ubo = trtl_uniform_info_address(mesh->uniform_info, frame);

	glm_mat4_identity(ubo->model);
	glm_rotate(ubo->model, mesh->mgeo.cur.facing, GLM_ZUP);
	if (mesh->vrotate) glm_rotate(ubo->model, mesh->vrotate, GLM_XUP);
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

static void
mesh_resize(struct trtl_object *obj, struct turtle *turtle, struct trtl_layer *layer)
{
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	mesh->descriptor_set_layout[0] =
	    mesh->strata.base->descriptor_set_layout(mesh->strata.base);
	mesh->descriptor_set_layout[1] =
	    mesh->strata.grid->descriptor_set_layout(mesh->strata.grid);
	mesh->descriptor_set_layout[2] = descriptor_set_layout(mesh);

	mesh->descriptor_set[0] = mesh->strata.base->descriptor_set(mesh->strata.base);
	mesh->descriptor_set[1] = mesh->strata.grid->descriptor_set(mesh->strata.grid);
	mesh->descriptor_set[2] = create_descriptor_sets(mesh);

	uint32_t count;
	VkVertexInputAttributeDescription *vkx = get_attribute_description_pair(&count);
	VkVertexInputBindingDescription vkb = vertex_binding_description_get();

	const char *fragment_shader;
	if (mesh->texture_image_view) {
		fragment_shader = "mesh_fragment";
	} else {
		printf("notextue version\n");
		fragment_shader = "mesh_fragment_notexture";
	}
	mesh->pipeline_info = trtl_pipeline_create_with_strata(
	    turtle, layer, TRTL_ARRAY_SIZE(mesh->descriptor_set_layout),
	    mesh->descriptor_set_layout, "mesh_vertex", fragment_shader, &vkb, vkx, count);
}

static bool
mesh_move(struct trtl_object *obj, uint32_t snap, uint32_t x, uint32_t y)
{
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	assert(mesh);

	mesh->mgeo.dest.x = x;
	mesh->mgeo.dest.y = y;
	if (snap) {
		mesh->mgeo.cur.x = x;
		mesh->mgeo.cur.y = y;
	}

	return true;
}

static bool
mesh_facing(struct trtl_object *obj, uint32_t snap, trtl_grid_direction_t facing)
{
#define constant_dtorad(deg) ((deg)*GLM_PIf / 180.0f)

	static float facings[] = {
	    [TRTL_GRID_NORTH] = constant_dtorad(60.0),
	    [TRTL_GRID_EAST] = constant_dtorad(300.0 + 20),
	    [TRTL_GRID_SOUTH] = constant_dtorad(240.0),
	    [TRTL_GRID_WEST] = constant_dtorad(120.0 + 20),
	};
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	assert(mesh);

	mesh->mgeo.dest.facing = facings[facing] + mesh->rotation;
	if (snap) {
		mesh->mgeo.cur.facing = mesh->mgeo.dest.facing;
	}
	return true;
}

struct trtl_object *
trtl_object_mesh_create(struct turtle *turtle, const char *path, const char *texture)
{
	return trtl_object_mesh_create_scaled(turtle, path, texture, 1.0);
}

struct trtl_object *
trtl_object_mesh_create_scaled(struct turtle *turtle, const char *path, const char *texture,
			       double scale)
{
	struct trtl_object_mesh *mesh;
	struct boundingbox3d bbox = BOUNDINGBOX_INIT;

	mesh = talloc_zero(NULL, struct trtl_object_mesh);

	talloc_set_destructor(mesh, trtl_object_mesh_destructor);
	mesh->turtle = turtle;
	mesh->parent.o.draw = trtl_object_draw_;
	mesh->parent.o.update = trtl_object_update_;
	mesh->parent.o.relayer = mesh_resize;
	mesh->parent.move = mesh_move;
	mesh->parent.facing = mesh_facing;

	mesh->nframes = turtle->tsc->nimages;
	mesh->model = load_model(turtle, path);
	if (!mesh->model) {
		talloc_free(mesh);
		return NULL;
	}

	//	mesh->uniform_info =
	//	    trtl_uniform_alloc_type(evil_global_uniform, struct UniformBufferObject);

	if (texture) {
		// FIXME: So leaky (create_texture_image never freed);
		mesh->texture_image_view =
		    create_texture_image_view(turtle, create_texture_image(turtle, texture));
	}

	mesh->strata.base = trtl_seer_strata_get(turtle, "base");
	mesh->strata.grid = trtl_seer_strata_get(turtle, "grid");
	// mesh->descriptor_set = create_descriptor_sets(mesh, turtle, turtle->tsc);

	mesh->uniform_info = trtl_uniform_alloc_type(turtle->uniforms, struct UniformBufferObject);

	{
		struct trtl_seer_vertexset vertices;
		vertices.nvertexes = mesh->model->nvertices;
		vertices.vertices = mesh->model->vertices;
		vertices.vertex_size = sizeof(struct vertex);

		// so creash is vertices->vertex_size is rubbish
		mesh->vertex_buffer = create_vertex_buffers(turtle, &vertices);
	}
	{
		struct trtl_seer_indexset indexes;

		indexes.nindexes = mesh->model->nindices;
		indexes.indexes = mesh->model->indices;
		mesh->index_buffer = create_index_buffer(turtle, &indexes);
	}

	// FIXME: Hackery for the duck2 model
	mesh->vrotate = M_PI / 2;

	return (struct trtl_object *)mesh;
}

void
trtl_object_mesh_rotation_base_set(struct trtl_object *obj, float xrotation)
{
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	mesh->rotation = xrotation;
}

void
trtl_object_mesh_rotation_upright_set(struct trtl_object *obj, float uprotation)
{
	struct trtl_object_mesh *mesh = trtl_object_mesh(obj);
	mesh->vrotate = uprotation;
}

static VkDescriptorSetLayout
descriptor_set_layout(struct trtl_object_mesh *mesh)
{
	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.pImmutableSamplers = NULL;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding shader_layer_binding = {0};
	shader_layer_binding.binding = 1;
	shader_layer_binding.descriptorCount = 1;
	shader_layer_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shader_layer_binding.pImmutableSamplers = NULL;
	shader_layer_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0] = ubo_layout_binding;
	bindings[1] = shader_layer_binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = TRTL_ARRAY_SIZE(bindings);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(mesh->turtle->device, &layoutInfo, NULL,
					&descriptor_set_layout) != VK_SUCCESS) {
		error("failed to create descriptor set layout!");
	}
	return descriptor_set_layout;
}

trtl_alloc static VkDescriptorSet *
create_descriptor_sets(struct trtl_object_mesh *mesh)
{
	VkDescriptorSet *sets = talloc_zero_array(mesh, VkDescriptorSet, mesh->nframes);
	uint32_t nframes = mesh->turtle->tsc->nimages;
	VkDescriptorSetLayout *layouts =
	    talloc_zero_array(NULL, VkDescriptorSetLayout, mesh->nframes);

	for (uint32_t i = 0; i < nframes; i++) {
		// FIXME: Thsi should cache right
		layouts[i] = descriptor_set_layout(mesh);
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = mesh->turtle->tsc->descriptor_pool;
	alloc_info.descriptorSetCount = mesh->nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(mesh->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < mesh->nframes; i++) {

		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(mesh->uniform_info, i);
		VkWriteDescriptorSet descriptorWrites[2] = {0};
		uint32_t ndescriptors = 1;

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		if (mesh->texture_image_view) {
			ndescriptors++;
			VkDescriptorImageInfo image_info = {0};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			// This hsould be from the object
			image_info.imageView = mesh->texture_image_view;
			image_info.sampler = mesh->turtle->texture_sampler;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = sets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType =
			    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &image_info;
		}

		vkUpdateDescriptorSets(mesh->turtle->device, ndescriptors, descriptorWrites, 0,
				       NULL);
	}

	talloc_free(layouts);

	return sets;
}

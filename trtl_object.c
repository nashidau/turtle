#include <talloc.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "trtl_object.h"
#include "vertex.h"  // FIXME: has trtl_model in it
#include "helpers.h"

static void
trtl_object_draw_(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		VkPipelineLayout  pipeline_layout,
		VkDescriptorSet *descriptor_set,
		int32_t offset) {
	vkCmdBindDescriptorSets(cmd_buffer, 
				    VK_PIPELINE_BIND_POINT_GRAPHICS,
				    pipeline_layout, 0, 1,
				    descriptor_set, 0, NULL);
	vkCmdDrawIndexed(cmd_buffer,  obj->model->nindices, 1, 0, offset, 0);
}

static uint32_t
trtl_object_vertices_get_(struct trtl_object *obj, struct vertex **vertices) {
	if (vertices) *vertices = obj->model->vertices;
	return obj->model->nvertices;
}

static uint32_t
trtl_object_indices_get_(struct trtl_object *obj, uint32_t **indices) {
	if (indices) *indices = obj->model->indices;
	return obj->model->nindices;
}

static int
trtl_object_destructor(trtl_arg_unused struct trtl_object *obj) {
	// FIXME: Free model
	return 0;	
}

struct trtl_object *
trtl_object_create(void *ctx, const char *path) {
	struct trtl_object *obj;

	obj = talloc_zero(ctx, struct trtl_object);
	talloc_set_destructor(obj, trtl_object_destructor);
	obj->draw = trtl_object_draw_;
	obj->vertices = trtl_object_vertices_get_;
	obj->indices = trtl_object_indices_get_;
	
	obj->model = load_model(path);		
	if (!obj->model) {
		talloc_free(obj);
		return NULL;
	}

	return obj;
}

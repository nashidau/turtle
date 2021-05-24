#include <talloc.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "trtl_object.h"
#include "vertex.h"  // FIXME: has trtl_model in it

static void
trtl_object_draw_(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		VkPipelineLayout  pipeline_layout,
		VkDescriptorSet *descriptor_set) {
	vkCmdBindDescriptorSets(cmd_buffer, 
				    VK_PIPELINE_BIND_POINT_GRAPHICS,
				    pipeline_layout, 0, 1,
				    descriptor_set, 0, NULL);
	vkCmdDrawIndexed(cmd_buffer,  obj->model->nindices, 1, 0, 0, 0);
}

// This is a terrible signature.
struct trtl_object *
trtl_object_create(void *ctx, struct trtl_model *model) {
	struct trtl_object *obj;

	obj = talloc_zero(ctx, struct trtl_object);
	obj->draw = trtl_object_draw_;

	obj->model = model;

	return obj;
}

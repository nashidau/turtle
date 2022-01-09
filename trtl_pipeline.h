
#include "vulkan/vulkan.h"

#include "turtle.h"

struct trtl_pipeline_info {
	VkDevice device; // Used in destructor... *sigh*
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
};

struct trtl_pipeline_info *
trtl_pipeline_create(struct turtle *turtle, VkRenderPass render_pass, VkExtent2D extent,
		     VkDescriptorSetLayout descriptor_set_layout, const char *vertex_shader,
		     const char *fragment_shader,
		     const VkVertexInputBindingDescription *binding_description,
		     const VkVertexInputAttributeDescription *attribute_description,
		     uint32_t nattributes, bool layer_blend);

struct trtl_pipeline_info *trtl_pipeline_create_with_strata(
    struct turtle *turtle, struct trtl_layer *layer, VkExtent2D extent,
    VkDescriptorSetLayout descriptor_set_layout, const char *vertex_shader,
    const char *fragment_shader, const VkVertexInputBindingDescription *binding_description,
    const VkVertexInputAttributeDescription *attribute_description, uint32_t nattributes);

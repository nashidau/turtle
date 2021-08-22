
#include "vulkan/vulkan.h"

#include "turtle.h"

struct trtl_pipeline_info {
	VkPipelineLayout pipeline_layout;
	VkPipeline *pipelines;
};


struct trtl_pipeline_info *trtl_pipeline_create(VkDevice device, struct swap_chain_data *scd);

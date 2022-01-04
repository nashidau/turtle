
#pragma once

#include <vulkan/vulkan.h>

struct trtl_layer;
VkRenderPass trtl_layer_info_render_pass(struct trtl_layer*);
int trtl_layer_info_strata_ciynt(struct trtl_layer*);
VkDescriptorSetLayoutBinding trtl_layer_info_strata(struct trtl_layer*, int stata);




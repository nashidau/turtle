/**
 * A mesh based object.
 *
 * Creates an object based on a given mesh & texture.
 *
 * There are a lot of things that can be improved about ths object.
 */
#pragma once

#include "trtl_object.h"

struct trtl_swap_chain;

struct trtl_object *
trtl_object_mesh_create(void *ctx, struct trtl_swap_chain *scd, 
		VkRenderPass render_pass, VkExtent2D extent,
		VkDescriptorSetLayout descriptor_set_layout,
		const char *path, const char *texture);

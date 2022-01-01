#pragma once

#include <stdbool.h>

#include <vulkan/vulkan.h>

struct turtle;

// Generic trtl strata interface
// All strat objects should inherit from this, as the first part of their object.
struct trtl_strata {
	struct turtle *turtle;
	
	// Update the layer
	bool (*update)(struct trtl_strata *state, int frame);

	// Resize the object
	void (*resize)(struct trtl_strata *strata, const VkRenderPass pass, VkExtent2D size);
};

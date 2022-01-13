#pragma once

#include <stdbool.h>

#include <vulkan/vulkan.h>

struct turtle;

// Generic trtl strata interface
// All strat objects should inherit from this, as the first part of their object.
struct trtl_strata {
	struct turtle *turtle;
	
	// Update the strata before drawing.
	// Returns true if it was updated.
	bool (*update)(struct trtl_strata *state, int frame);

	VkDescriptorSetLayout (*descriptor_set_layout)(struct trtl_strata *strata);
	VkDescriptorSet *(*descriptor_set)(struct trtl_strata *strata);
};

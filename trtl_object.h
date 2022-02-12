#pragma once

#include <stdbool.h>

#include <vulkan/vulkan.h>

struct turtle;
struct trtl_layer;

// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer, int32_t offset);
	bool (*update)(struct trtl_object *obj, int frame);

	// Triggered when an object is first added to a layer or on resize.
	// The object should create it's pipeline based on the info in this call
	void (*relayer)(struct trtl_object *obj, struct turtle *turtle, struct trtl_layer *layer);
};

// Grid object; trtl_grid_object
// An object that works within a grid.
// It has three additional properties;
// 	x,y on the grid.
// 	Facing on the grid
struct trtl_grid_object {
	struct trtl_object o;

	bool (*move)(struct trtl_object *obj, uint32_t x, uint32_t y);

	bool (*facing)(struct trtl_object *obj, int facing);
};

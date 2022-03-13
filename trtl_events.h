/**
 * Well known trtl crier event structures.
 *
 */
#pragma once


#include "vulkan/vulkan.h"

#include "trtl_crier.h"

struct trtl_events {
	struct trtl_crier *crier;

	trtl_crier_cry_t resize;
	trtl_crier_cry_t grid_move;
	trtl_crier_cry_t grid_zoom;
};

struct trtl_event_resize {
	struct turtle *turtle;

	VkExtent2D new_size;
};

struct trtl_event_grid_move {
	struct turtle *turtle;

	int32_t x;
	int32_t y;
	bool snap;
};

struct trtl_event_grid_zoom {
	struct turtle *turtle;

	uint32_t feature_size;
};

struct trtl_events *trtl_event_init(void);

int trtl_event_resize(struct turtle *turtle, VkExtent2D new_size);
int trtl_event_grid_move(struct turtle *turtle, int32_t x, int32_t y, bool snap);
int trtl_event_grid_zoom(struct turtle *turtle, uint32_t zoom);

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
};

struct trtl_event_resize {
	struct turtle *turtle;

	VkExtent2D new_size;
};


struct trtl_events *trtl_event_init(void);

int trtl_event_resize(struct turtle *turtle, VkExtent2D new_size);

#pragma once

#include <vulkan/vulkan.h>

#include "helpers.h"

struct turtle;
struct trtl_sprite_subsprite;

typedef struct trtl_subsprite_index {
	int index;
} trtl_subsprite_index;

trtl_alloc struct trtl_object *trtl_sprite_create(struct turtle *turtle, uint32_t nsprites);

trtl_alloc struct trtl_sprite_subsprite *trtl_sprite_subsprite_add(struct trtl_object *sprite,
								   const char *image);
trtl_subsprite_index trtl_sprite_subsprite_instance_add(struct trtl_sprite_subsprite *subsprite);
int trtl_sprite_subsprite_position_set(struct trtl_sprite_subsprite *subsprite,
				       trtl_subsprite_index index, int x, int y);

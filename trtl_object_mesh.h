/**
 * A mesh based object.
 *
 * Creates an object based on a given mesh & texture.
 *
 * There are a lot of things that can be improved about ths object.
 */
#pragma once

#include "trtl_object.h"

struct trtl_object *
trtl_object_mesh_create(void *ctx, struct swap_chain_data *scd, const char *path,
				       const char *texture);

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
struct turtle;

struct trtl_object *trtl_object_mesh_create(struct turtle *turtle, const char *path,
						const char *texture);
struct trtl_object *trtl_object_mesh_create_scaled(struct turtle *turtle, const char *path,
						const char *texture, double scale);


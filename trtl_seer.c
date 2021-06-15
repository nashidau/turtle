/**
 * Turtle Seer; manages objects.
 *
 * Manages a group of objects.   Functions as a factor as well as keeping track of
 * resources used by objects.
 *
 * Created as a singleton.  Has an evil global.
 */
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object.h"

// FIXME: These should be in some loaded metadata
static const char *MODEL_PATH = "models/viking_room.obj";
static const char *TEXTURE_PATH = "textures/viking_room.png";
// Loads; but wrong size
// static const char *MODEL_PATH = "models/bulborb/source/TenKochappy.obj";
// static const char *TEXTURE_PATH = "models/bulborb/textures/kochappy_tex.png";

static const char *MODEL_PATH2 = "models/StreetCouch/Day143.obj";
static const char *TEXTURE_PATH2 = "models/StreetCouch/textures/texture.jpg";


struct {
	void *seer_ctx;

	VkDevice device;

	int nobjects;
	int nalloced;
	struct trtl_object **objects;
} seer; 


int
trtl_seer_init(VkDevice device) {
	if (seer.device) {
		warning("Multiple init of trtl_seer");
		return 0;
	}

	seer.seer_ctx = talloc_init("Turtle Seer Context");

	seer.device = device;	
	seer.nobjects = 0;
	seer.nalloced = 0;

	return 0;
}

int
trtl_seer_object_add(const char *name, struct swap_chain_data *scd) {
	struct trtl_object *object;

	if (streq(name, "counch")) {
		object = trtl_object_create(seer.seer_ctx, scd, MODEL_PATH2, TEXTURE_PATH2);
	} else if (streq(name, "room")) {
		object = trtl_object_create(seer.seer_ctx, scd, MODEL_PATH, TEXTURE_PATH);
	} else {
		error("Unknown object %s\n", name);
		return -1;
	}
	
	if (seer.nobjects >= seer.nalloced) {
		struct trtl_object **objs;
		seer.nalloced *= 2;
		objs = talloc_realloc(NULL, seer.objects, struct trtl_object *, seer.nalloced);
		if (objs == NULL){
			talloc_free(object);
			error("Could not resize object array");
			return -1;
		}
		seer.objects = objs;
	}

	seer.objects[seer.nobjects ++] = object;
	
	return 0;
}

/**
 * Turtle Seer; manages objects.
 *
 * Manages a group of objects.   Functions as a factor as well as keeping track of
 * resources used by objects.
 *
 * Created as a singleton.  Has an evil global.
 *
 * FIXME: Needs to holder the rendering pipeline
 * FIXME: Should hold the trtl_uniform objects.
 */
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_seer.h"

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

	uint32_t nobjects;
	uint32_t nalloced;
	struct trtl_object **objects;
} seer;

int trtl_seer_init(VkDevice device)
{
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

int trtl_seer_object_add(const char *name, struct swap_chain_data *scd)
{
	struct trtl_object *object;

	if (streq(name, "couch")) {
		object = trtl_object_create(seer.seer_ctx, scd, MODEL_PATH2, TEXTURE_PATH2);
	} else if (streq(name, "room")) {
		object = trtl_object_create(seer.seer_ctx, scd, MODEL_PATH, TEXTURE_PATH);
	} else {
		error("Unknown object %s\n", name);
		return -1;
	}

	if (seer.nobjects >= seer.nalloced) {
		struct trtl_object **objs;
		if (seer.nalloced == 0)
			seer.nalloced = 2;
		else
			seer.nalloced *= 2;
		objs = talloc_realloc(NULL, seer.objects, struct trtl_object *, seer.nalloced);
		if (objs == NULL) {
			talloc_free(object);
			error("Could not resize object array");
			return -1;
		}
		seer.objects = objs;
	}

	seer.objects[seer.nobjects++] = object;

	return 0;
}

/**
 * Update all known objects for the current frame / time.
 *
 * Runs any update functions for each object.   Running the update function advances the object one
 * time step, whatever that may be.  An object does not need to rendered between update steps.
 *
 * FIXME: update the description about the side.
 *
 * @param image_index The Index of the current frame.
 * @retuen Number of objects updated.
 */
int trtl_seer_update(uint32_t image_index)
{
	for (uint32_t i = 0; i < seer.nobjects; i++) {
		seer.objects[i]->update(seer.objects[i], image_index);
	}

	return seer.nobjects;
}

int trtl_seer_draw(VkCommandBuffer buffer, VkPipelineLayout pipeline_layout)
{
	uint32_t offset = 0;
	for (uint32_t obj = 0; obj < seer.nobjects; obj++) {
		seer.objects[obj]->draw(seer.objects[obj], buffer, pipeline_layout, offset);
		// Super ugly hack
		offset += seer.objects[obj]->indices(seer.objects[obj], NULL);
	}

	return 0;
}

/**
 * FIXME: Tag this as acquires memory that needs to be referenced or freed.
 * Some sort of sparse acquires tag should do the job.
 */
trtl_alloc struct trtl_seer_vertexset *trtl_seer_vertexes_get(uint32_t *nobjects,
							      uint32_t *nvertexes)
{
	struct trtl_seer_vertexset *vertices;

	vertices = talloc_zero_array(NULL, struct trtl_seer_vertexset, seer.nobjects);
	*nvertexes = 0;
	for (uint32_t i = 0; i < seer.nobjects; i++) {
		vertices[i].nvertexes =
		    seer.objects[i]->vertices(seer.objects[i], &vertices[i].vertices);
		*nvertexes += vertices[i].nvertexes;
	}
	printf("%d vertices\n", *nvertexes);

	*nobjects = seer.nobjects;

	return vertices;
}

struct trtl_seer_indexset *trtl_seer_indexset_get(uint32_t *nobjects, uint32_t *nindexes)
{
	struct trtl_seer_indexset *indexes;

	indexes = talloc_zero_array(NULL, struct trtl_seer_indexset, seer.nobjects);

	for (uint32_t i = 0; i < seer.nobjects; i++) {
		indexes[i].nindexes =
		    seer.objects[i]->indices(seer.objects[i], &indexes[i].indexes);
		*nindexes += indexes[i].nindexes;
	}

	*nobjects = seer.nobjects;

	return indexes;
}

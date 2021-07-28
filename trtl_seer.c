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
#include <assert.h>
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object_canvas.h"
#include "trtl_object_mesh.h"
#include "trtl_seer.h"
#include "turtle.h"

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

	// Number of objects across all layers
	uint32_t nobjects;

	trtl_render_layer_t nlayers;
	struct objlayer *layers;

} seer = {0};

struct objlayer {
	uint32_t nobjects;
	uint32_t nalloced;
	struct trtl_object **objects;
};

int
trtl_seer_init(VkDevice device, uint32_t nlayers)
{
	if (seer.device) {
		warning("Multiple init of trtl_seer");
		return 0;
	}

	if (nlayers < 1 || nlayers > 10) {
		error("Invalid layers param");
	}

	seer.seer_ctx = talloc_init("Turtle Seer Context");

	seer.layers = talloc_zero_array(seer.seer_ctx, struct objlayer, nlayers);
	seer.nlayers = nlayers;

	seer.device = device;

	return 0;
}

int
trtl_seer_object_add(const char *name, struct swap_chain_data *scd, trtl_render_layer_t layerid)
{
	struct trtl_object *object;
	struct objlayer *layer;

	if (layerid >= seer.nlayers) {
		error("Invalid layer for %s", name);
	}

	// FIXME: This is super unscalable.  Shoudl have a DB or a way to search filesystem
	if (streq(name, "couch")) {
		object = trtl_object_mesh_create(seer.seer_ctx, scd, MODEL_PATH2, TEXTURE_PATH2);
	} else if (streq(name, "room")) {
		object = trtl_object_mesh_create(seer.seer_ctx, scd, MODEL_PATH, TEXTURE_PATH);
	} else if (streq(name, "background")) {
		object = trtl_canvas_create(seer.seer_ctx, scd, NULL);
	} else {
		error("Unknown object %s\n", name);
		return -1;
	}

	seer.nobjects++;

	layer = seer.layers + layerid;

	if (layer->nobjects >= layer->nalloced) {
		struct trtl_object **objs;
		if (layer->nalloced == 0)
			layer->nalloced = 2;
		else
			layer->nalloced *= 2;
		objs = talloc_realloc(NULL, layer->objects, struct trtl_object *, layer->nalloced);
		if (objs == NULL) {
			talloc_free(object);
			error("Could not resize object array");
			return -1;
		}
		layer->objects = objs;
	}

	layer->objects[layer->nobjects++] = object;

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
int
trtl_seer_update(uint32_t image_index)
{
	int count = 0;
	for (trtl_render_layer_t layerid = 0; layerid < TRTL_RENDER_LAYER_TOTAL; layerid++) {
		struct objlayer *layer = seer.layers + layerid;
		for (uint32_t i = 0; i < layer->nobjects; i++) {
			layer->objects[i]->update(layer->objects[i], image_index);
			count++;
		}
	}

	return count;
}

int
trtl_seer_draw(VkCommandBuffer buffer, VkPipelineLayout pipeline_layout,
	       trtl_render_layer_t layerid)
{
	uint32_t offset = 0;

	assert(layerid < seer.nlayers);

	struct objlayer *layer = seer.layers + layerid;

	for (uint32_t obj = 0; obj < layer->nobjects; obj++) {
		layer->objects[obj]->draw(layer->objects[obj], buffer, pipeline_layout, offset);
		// Super ugly hack
		offset += layer->objects[obj]->indices(layer->objects[obj], NULL);
	}

	return 0;
}

/**
 * FIXME: Tag this as acquires memory that needs to be referenced or freed.
 * Some sort of sparse acquires tag should do the job.
 */
trtl_alloc struct trtl_seer_vertexset *
trtl_seer_vertexes_get(uint32_t *nobjects, uint32_t *nvertexes)
{
	struct trtl_seer_vertexset *vertices;

	vertices = talloc_zero_array(NULL, struct trtl_seer_vertexset, seer.nobjects);
	*nvertexes = 0;
	for (trtl_render_layer_t l = 0; l < seer.nlayers; l++) {
		struct objlayer *layer = seer.layers + l;
		for (uint32_t i = 0; i < layer->nobjects; i++) {
			vertices[i].nvertexes =
			    layer->objects[i]->vertices(layer->objects[i], &vertices[i].vertices);
			*nvertexes += vertices[i].nvertexes;
		}
		*nobjects += layer->nobjects;
		printf("%d vertices\n", *nvertexes);
	}

	return vertices;
}

struct trtl_seer_indexset *
trtl_seer_indexset_get(uint32_t *nobjects, uint32_t *nindexes)
{
	struct trtl_seer_indexset *indexes;

	indexes = talloc_zero_array(NULL, struct trtl_seer_indexset, seer.nobjects);

	for (trtl_render_layer_t l = 0; l < seer.nlayers; l++) {
		struct objlayer *layer = seer.layers + l;
		for (uint32_t i = 0; i < layer->nobjects; i++) {
			indexes[i].nindexes =
			    layer->objects[i]->indices(layer->objects[i], &indexes[i].indexes);
			*nindexes += indexes[i].nindexes;
		}
		*nobjects += layer->nobjects;
	}

	return indexes;
}

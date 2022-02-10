
#include "vulkan/vulkan.h"

#include "turtle.h"

#include "helpers.h"

struct trtl_swap_chain;

struct trtl_seer_vertexset {
	uint32_t vertex_size;
	uint32_t nvertexes;
	const void *vertices;
};

struct trtl_seer_indexset {
	uint32_t nindexes;
	uint32_t indexrange;
	const uint32_t *indexes;
};

struct trtl_object;

struct trtl_seer *trtl_seer_init(struct turtle turtle[1], VkExtent2D extent,
				 trtl_render_layer_t nlayers,
				 const struct trtl_layer_info info[nlayers]);

struct trtl_object *trtl_seer_predefined_object_add(const char name[static 1], struct turtle turtle[1],
						    trtl_render_layer_t layer);

int trtl_seer_object_add(struct turtle turtle[1], struct trtl_object *object,
			 trtl_render_layer_t layerid);

int trtl_seer_resize(VkExtent2D new_size, struct turtle turtle[1]);

int trtl_seer_draw(struct turtle turtle[1], VkCommandBuffer buffer, trtl_render_layer_t layerid);
int trtl_seer_update(struct turtle turtle[1], uint32_t image_index);
struct trtl_seer_vertexset *trtl_seer_vertexes_get(trtl_render_layer_t, uint32_t *nobjects,
						   uint32_t *nvertexes);
struct trtl_seer_indexset *trtl_seer_indexset_get(trtl_render_layer_t, uint32_t *nobjects,
						  uint32_t *nindexes);

// FIXME: Hackery here
trtl_must_check VkCommandBuffer *trtl_seer_create_command_buffers(struct turtle turtle[1],
								  VkCommandPool command_pool);

// FIXME: This shouldn;t be needed - it should be done automatically by object creation or
// insertion or something
struct trtl_strata *trtl_seer_strata_get(struct turtle turtle[1], const char name[static 1]);

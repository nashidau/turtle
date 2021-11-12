
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

struct trtl_seer *trtl_seer_init(struct turtle *turtle, VkExtent2D extent,
				 trtl_render_layer_t nlayers, const struct trtl_layer_info *info);

int trtl_seer_predefined_object_add(const char *name, struct turtle *turtle,
				    trtl_render_layer_t layer);

int trtl_seer_object_add(struct turtle *turtle, struct trtl_object *object,
			 trtl_render_layer_t layerid);

int trtl_seer_resize(VkExtent2D new_size, struct turtle *turtle);

int trtl_seer_draw(struct turtle *turtle, VkCommandBuffer buffer, trtl_render_layer_t layerid);
int trtl_seer_update(struct turtle *turtle, uint32_t image_index);
struct trtl_seer_vertexset *trtl_seer_vertexes_get(trtl_render_layer_t, uint32_t *nobjects,
						   uint32_t *nvertexes);
struct trtl_seer_indexset *trtl_seer_indexset_get(trtl_render_layer_t, uint32_t *nobjects,
						  uint32_t *nindexes);

// FIXME: Hackery here
trtl_must_check VkCommandBuffer *trtl_seer_create_command_buffers(struct turtle *turtle,
								  VkCommandPool command_pool);

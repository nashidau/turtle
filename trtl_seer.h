
#include "vulkan/vulkan.h"

#include "turtle.h"

#include "helpers.h"

struct swap_chain_data;

struct trtl_seer_vertexset {
	uint32_t nvertexes;
	const struct vertex *vertices;
};

struct trtl_seer_indexset {
	uint32_t nindexes;
	uint32_t indexrange;
	const uint32_t *indexes;
};

int trtl_seer_init(struct turtle *turtle, VkExtent2D extent);

int trtl_seer_object_add(const char *name, struct swap_chain_data *scd, trtl_render_layer_t layer);

int trtl_seer_resize(VkExtent2D new_size, struct swap_chain_data *scd);

int trtl_seer_draw(VkCommandBuffer buffer, struct swap_chain_data *scd,
		   trtl_render_layer_t layerid);
int trtl_seer_update(uint32_t image_index);
struct trtl_seer_vertexset *trtl_seer_vertexes_get(trtl_render_layer_t, uint32_t *nobjects,
						   uint32_t *nvertexes);
struct trtl_seer_indexset *trtl_seer_indexset_get(trtl_render_layer_t, uint32_t *nobjects,
						  uint32_t *nindexes);

// FIXME: Hackery here
trtl_must_check VkCommandBuffer *trtl_seer_create_command_buffers(struct swap_chain_data *scd,
								   VkCommandPool command_pool);

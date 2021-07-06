
#include "vulkan/vulkan.h"

struct swap_chain_data;

struct trtl_seer_vertexset {
	uint32_t nvertexes;
	const struct vertex *vertices;
};

struct trtl_seer_indexset {
	uint32_t nindexes;
	const uint32_t *indexes;
};

int trtl_seer_init(VkDevice device);

int trtl_seer_object_add(const char *name, struct swap_chain_data *scd);

int trtl_seer_draw(VkCommandBuffer buffer,  VkPipelineLayout pipeline_layout);
int trtl_seer_update(uint32_t image_index);
struct trtl_seer_vertexset *trtl_seer_vertexes_get(uint32_t *nobjects, uint32_t *nvertexes);
struct trtl_seer_indexset * trtl_seer_indexset_get(uint32_t *nobjects, uint32_t *nindexes);


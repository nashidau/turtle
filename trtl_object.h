#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

struct vertex;
struct swap_chain_data;
struct trtl_pipeline_info;

// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		    int32_t offset);
	uint32_t (*vertices)(struct trtl_object *obj, const struct vertex **vertices);
	uint32_t (*indices)(struct trtl_object *obj, const uint32_t **indices, uint32_t *range);
	bool (*update)(struct trtl_object *obj, int frame);
	struct trtl_pipeline_info *(*pipeline)(struct trtl_object *obj);
};

// Once again: concreate implementation

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
	bool (*update)(struct trtl_object *obj, int frame);
};

// Once again: concreate implementation

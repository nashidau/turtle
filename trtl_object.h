#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

struct vertex;
struct trtl_swap_chain;
struct trtl_pipeline_info;

// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		    int32_t offset);
	bool (*update)(struct trtl_object *obj, int frame);

	// A window resize just happned; update.
	// This is optional, but will generally be required
	// FIXME: Need to kill the SCD pointer.
	void (*resize)(struct trtl_object *obj, struct trtl_swap_chain *scd, VkExtent2D size);
};


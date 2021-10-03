#include <stdbool.h>

#include <vulkan/vulkan.h>

struct turtle;

// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer, int32_t offset);
	bool (*update)(struct trtl_object *obj, int frame);

	// Triggered when an object is first added to a layer or on resize.
	// The object should create it's pipeline based on the info in this call
	void (*resize)(struct trtl_object *obj, struct turtle *turtle, VkRenderPass pass,
		       VkExtent2D size);
};

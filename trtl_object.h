#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

struct vertex;
struct swap_chain_data;

// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		     VkPipelineLayout pipeline_layout, int32_t offset);
	uint32_t (*vertices)(struct trtl_object *obj, struct vertex **vertices);
	uint32_t (*indices)(struct trtl_object *obj, uint32_t **indices);
	bool (*update)(struct trtl_object *obj, int frame);

	/// This stuff  belongs in a concreate implementation, but we have exactly 1
	// at the moment

	// What we are drawing (sadly one for now)
	struct trtl_model *model;

	uint32_t nframes;
	VkDescriptorSet *descriptor_set;

	VkDeviceMemory texture_image_memory;
	VkImage texture_image;
	VkImageView texture_image_view;

	struct trtl_uniform_info *uniform_info;

	bool reverse;
};

// Once again: concreate implementation
struct trtl_object *trtl_object_create(void *ctx, struct swap_chain_data *scd, const char *path,
				       const char *texture);

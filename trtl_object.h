
// Interface for render objects; implement these methods
struct trtl_object {
	// Interface
	void (*draw)(struct trtl_object *obj, VkCommandBuffer cmd_buffer,
		VkPipelineLayout  pipeline_layout,
		VkDescriptorSet *descriptor_set);


	/// This stuff  belongs in a concreate implementation, but we have exactly 1
	// at the moment

	// What we are drawing (sadly one for now)
	struct trtl_model *model;

};


// Once again: concreate implementation
struct trtl_object *
trtl_object_create(void *ctx, const char *path);

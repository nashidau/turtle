
struct swap_chain_data;


struct trtl_object *
trtl_canvas_create(void *ctx, struct swap_chain_data *scd, VkRenderPass render_pass,
		   VkExtent2D extent, VkDescriptorSetLayout descriptor_set_layout);


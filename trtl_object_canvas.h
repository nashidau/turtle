
struct trtl_swap_chain;


struct trtl_object *
trtl_canvas_create(void *ctx, struct trtl_swap_chain *scd, VkRenderPass render_pass,
		   VkExtent2D extent);


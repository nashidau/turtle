#include <vulkan/vulkan.h>

struct trtl_swap_chain;
struct turtle;

struct trtl_object *trtl_canvas_create(void *ctx, struct turtle *turtle,
				       struct trtl_swap_chain *scd, VkRenderPass render_pass,
				       VkExtent2D extent);

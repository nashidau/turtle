
#include <vulkan/vulkan.h>

struct trtl_swap_chain;
struct turtle;

struct trtl_object *trtl_grid_create(void *ctx, struct turtle *turtle, struct trtl_swap_chain *scd,
				     VkRenderPass render_pass, VkExtent2D extent, uint16_t width,
				     uint16_t height);

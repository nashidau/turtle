
#include <vulkan/vulkan.h>

struct trtl_swap_chain;
struct turtle;

struct trtl_object *trtl_grid_create(struct turtle *turtle);

int trtl_grid_fill_rectangle(struct trtl_object *grid, int width, int height);

// grid_set_square(use a square)

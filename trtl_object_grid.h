
#include <vulkan/vulkan.h>

struct trtl_swap_chain;
struct turtle;

struct trtl_object *trtl_grid_create(struct turtle *turtle);

int trtl_grid_set_type(struct trtl_object *obj, const char *type);

int trtl_grid_fill_rectangle(struct trtl_object *grid, uint32_t width, uint32_t height);
int trtl_grid_fill_pattern(struct trtl_object *grid, uint32_t width, uint32_t height,
		uint8_t *pattern, uint8_t presentchar);

int trtl_grid_set_active_tile(struct trtl_object *grid, float x, float y, int motion);


// grid_set_square(use a square)

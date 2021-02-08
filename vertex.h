#include <assert.h>


struct color {
	float r, g, b;
};

struct pos2d {
	float x, y;
};


struct vertex {
	struct pos2d pos;
	struct color color;
};

static_assert(sizeof(struct pos2d) == sizeof(float) * 2, "Urk");
static_assert(sizeof(struct color) == sizeof(float) * 3, "Glurk");

VkVertexInputBindingDescription 
vertex_binding_description_get(const struct vertex *vertex);
VkVertexInputAttributeDescription *
get_attribute_description_pair(const struct vertex *vertxt);
	

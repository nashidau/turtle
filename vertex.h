#include <assert.h>


struct color {
	float r, g, b;
};

struct pos2d {
	float x, y;
};

struct pos3d {
	float x, y, z;
};

struct vertex {
	struct pos3d pos;
	struct color color;
	struct pos2d tex_coord;
};

struct matrix4 {
	float vals[16];
};

struct trtl_model {
	uint32_t nindices;
	uint32_t *indices;
	struct vertex *vertices;
	uint32_t nvertices;
};

static_assert(sizeof(struct pos2d) == sizeof(float) * 2, "Urk");
static_assert(sizeof(struct color) == sizeof(float) * 3, "Glurk");

VkVertexInputBindingDescription 
vertex_binding_description_get(const struct trtl_model *model);
VkVertexInputAttributeDescription *
get_attribute_description_pair(const struct trtl_model *model,
		uint32_t *nentries);

struct trtl_model *load_model(const char *basename);



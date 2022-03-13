#include "cglm/cglm.h"
#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <stdint.h>

struct color {
	float r, g, b;
};
static_assert(sizeof(struct color) == sizeof(float) * 3, "Glurk");

struct pos2d {
	float x, y;
};

struct pos3d {
	float x, y, z;
};
static_assert(sizeof(struct pos2d) == sizeof(float) * 2, "Urk");

struct vertex {
	struct pos3d pos;
	struct color color;
	struct pos2d tex_coord;
};

struct vertex2d {
	struct pos2d pos;
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

struct UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
};

struct boundingbox3d {
	struct pos3d min;
	struct pos3d max;
};

#define BOUNDINGBOX_INIT                                                                           \
	{                                                                                          \
		{FLT_MAX, FLT_MAX, FLT_MAX},                                                       \
		{                                                                                  \
			FLT_MIN, FLT_MIN, FLT_MIN                                                  \
		}                                                                                  \
	}

VkVertexInputBindingDescription vertex_binding_description_get(void);
VkVertexInputAttributeDescription *get_attribute_description_pair(uint32_t *nentries);

struct trtl_model *load_model(const char *basename);


#include "vulkan/vulkan_core.h"

struct turtle;

struct trtl_shader {
	const char *path;
	VkShaderModule shader;

	void *internal;
};

struct trtl_shader_cache *trtl_shader_cache_init(struct turtle *turtle);

struct trtl_shader *trtl_shader_get(struct turtle *turtle, const char *path);

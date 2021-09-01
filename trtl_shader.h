
#include "vulkan/vulkan_core.h"

struct turtle;

struct trtl_shader {
	const char *path;
	VkShaderModule shader;
};

int trtl_shader_init(struct turtle *turtle);

struct trtl_shader*
trtl_shader_get(const char *path);



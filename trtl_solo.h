
#include <vulkan/vulkan.h>

struct turtle;

struct trtl_solo {
	VkCommandBuffer command_buffer;
};

void trtl_solo_init(struct turtle *turtle);

struct trtl_solo *trtl_solo_get(void);

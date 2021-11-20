#include <talloc.h>
#include <vulkan/vulkan.h>

struct turtle;

struct trtl_solo {
	VkCommandBuffer command_buffer;
};

void trtl_solo_init(struct turtle *turtle);

struct trtl_solo *trtl_solo_start(void);

#define trtl_solo_end(_solo)                                                                       \
	do {                                                                                       \
		struct trtl_solo *_tmp;                                                            \
		(&_tmp == &_solo);                                                                 \
		talloc_free(_solo);                                                                \
	} while (0)

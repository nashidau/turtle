/**
 * A number of vilkan commands need to run in a temporary command buffer.
 *
 * A solo instance allocates ta a command buffer and starts it. Once complete, by freeing the solo
 * context causes it to be submitted.
 *
 * Useage is thus:
 * @code
 * struct trtl_solo *solo = trtl_solo_start();
 *   // Copy image buffer or similar
 * talloc_free(solo); // Queue the command buffer and free it.
 * @endcode
 *
 * Note that the talloc_free blocks until the queue has been completed.
 */

#include <talloc.h>
#include <vulkan/vulkan.h>

struct turtle;

struct trtl_solo {
	VkCommandBuffer command_buffer;
};

void trtl_solo_init(VkDevice device, uint32_t graphics_family);

struct trtl_solo *trtl_solo_start(void);

#define trtl_solo_end(_solo)                                                                       \
	do {                                                                                       \
		struct trtl_solo *_tmp;                                                            \
		(&_tmp == &_solo);                                                                 \
		talloc_free(_solo);                                                                \
	} while (0)

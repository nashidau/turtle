#include "turtle.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

struct trtl_uniform *trtl_uniform_init(struct render_context *render, uint8_t nframes, size_t size);
void *trtl_uniform_buffer_base_get(struct trtl_uniform *uniforms, uint8_t nframes);

struct trtl_uniform_info *trtl_uniform_alloc(struct trtl_uniform *uniforms, size_t size);
#define trtl_uniform_alloc_type(uniforms, obj) trtl_uniform_alloc(uniforms, sizeof(obj))

void trtl_uniform_update(struct trtl_uniform *uniforms, uint32_t frame);

VkDescriptorBufferInfo trtl_uniform_buffer_get_descriptor(struct trtl_uniform_info *info,
							  int frame);
void *trtl_uniform_info_address(struct trtl_uniform_info *info, int frame);

extern struct trtl_uniform *evil_global_uniform;

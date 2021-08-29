#pragma once

#include <vulkan/vulkan.h>

struct render_context;
struct trtl_seer_vertexset;
struct trtl_seer_indexset;

VkBuffer create_vertex_buffers(struct render_context *render, const struct trtl_seer_vertexset *vertices);
VkBuffer create_index_buffer(struct render_context *render, const struct trtl_seer_indexset *indices);

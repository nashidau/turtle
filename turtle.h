
/**
 * General trtl include file
 */

#pragma once


// c++, there is a C library at https://github.com/recp/cglm
//#include <glm/glm.hpp>
// This is the C version of glm; so these don't work
#define CGLM_DEFINE_PRINTS
#define CGLM_FORCE_RADIANS
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "cglm/cglm.h" /* for inline */
//#include <cglm/call.h>   /* for library call (this also includes cglm.h) */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// All data used to render a frame
struct render_context {
	GLFWwindow *window;
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkSurfaceKHR surface;
	VkDescriptorSetLayout descriptor_set_layout;

	// Array; one per layer
	VkBuffer *vertex_buffers;

	// Array; one per layer
	VkBuffer *index_buffers;
	VkDeviceMemory index_buffer_memory;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	struct swap_chain_data *scd;

	VkFence *images_in_flight;

	// Textures
	VkSampler texture_sampler;
};

struct swap_chain_data {
	// Used in the destructor, not deleted in the destructor
	struct render_context *render;

	VkSwapchainKHR swap_chain;
	VkImage *images;
	uint32_t nimages;
	VkImageView *image_views;
	VkFormat image_format; // Swap chain image formt
	VkExtent2D extent;     // Extent of the images
	VkFramebuffer *framebuffers;
	VkPipelineLayout pipeline_layout;
	VkPipeline *pipelines;
	VkRenderPass render_pass;
	VkDescriptorPool descriptor_pool;

	uint32_t nbuffers;
	// Used to recreate (and clean up) swap chain
	VkCommandPool command_pool;
	VkCommandBuffer *command_buffers;

	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;
};

struct UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
};

typedef enum {
	TRTL_RENDER_LAYER_BACKGROUND = 0,
	TRTL_RENDER_LAYER_MAIN = 1,
	// Last counter
	TRTL_RENDER_LAYER_TOTAL
} trtl_render_layer_t;

VkImage create_texture_image(struct render_context *render, const char *path);
VkImageView create_texture_image_view(struct render_context *render, VkImage texture_image);

// FIXME: Should be turtle_create() and render_context should be a struct turtle.
void create_buffer(struct render_context *render, VkDeviceSize size, VkBufferUsageFlags usage,
		   VkMemoryPropertyFlags properties, VkBuffer *buffer,
		   VkDeviceMemory *bufferMemory);

// FIXME: This name style
uint32_t findMemoryType(struct render_context *render, uint32_t typeFilter,
			VkMemoryPropertyFlags properties);

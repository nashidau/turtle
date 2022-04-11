
/**
 * General trtl include file
 */

#pragma once


// c++, there is a C library at https://github.com/recp/cglm
//#include <glm/glm.hpp>
// This is the C version of glm; so these don't work
#include <stdbool.h>
#define CGLM_DEFINE_PRINTS
#define CGLM_FORCE_RADIANS
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
//#include <cglm/call.h>   /* for library call (this also includes cglm.h) */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "helpers.h"

#define TRTL_MAX_FRAMES_IN_FLIGHT 2

typedef uint16_t trtl_render_layer_t;

// Global structure for frequently used global state.
// Everything here should not change frequently
struct turtle {
	GLFWwindow *window;
	VkSurfaceKHR surface;
	VkDevice device;
	VkPhysicalDevice physical_device;

	// XXX: Does this belong here?
	VkFormat image_format; // Swap chain image formt

	struct {
		VkSemaphore image_ready_sem[TRTL_MAX_FRAMES_IN_FLIGHT];
		VkSemaphore render_done_sem[TRTL_MAX_FRAMES_IN_FLIGHT];
		VkFence in_flight_fences[TRTL_MAX_FRAMES_IN_FLIGHT];
	} barriers;

	VkInstance instance;

	// Default synchronous message service.
	struct trtl_events *events;

	struct trtl_swap_chain *tsc;
	struct trtl_shader_cache *shader_cache;
	struct trtl_seer *seer;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	struct trtl_uniform *uniforms;
	VkSampler texture_sampler;

	// FIXME: This should be in the swap chain
	VkFence *images_in_flight;

	struct trtl_timer *timers;

	struct trtl_timer *render_timer;

	PFN_vkDebugMarkerSetObjectNameEXT set_object_name;


	// Our generic loader
	struct trtl_loader *loader;
};

struct trtl_swap_chain {
	// Used in the destructor, not deleted in the destructor
	struct turtle *turtle;

	VkSwapchainKHR swap_chain;
	VkImage *images;
	uint32_t nimages;
	VkImageView *image_views;
	VkExtent2D extent; // Extent of the images
	VkDescriptorPool descriptor_pool;

	uint32_t nbuffers;
	// Used to recreate (and clean up) swap chain
	VkCommandPool command_pool;
	VkCommandBuffer *command_buffers;

	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;

	VkSampler texture_sampler;
};



// FIXME: SHoudl move into the turtle.h (or a make device.h)
// once the swap chain code has moved across
struct swap_chain_support_details {
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t nformats;
	uint32_t npresentmodes;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *presentModes;
};

struct queue_family_indices {
	uint32_t graphics_family;
	uint32_t present_family;
	bool has_graphics;
	bool has_present;
};

int trtl_main_loop(struct turtle *turtle);

typedef enum {
	// Nothing bound; you are on your own.
	TRTL_STRATA_NONE = 0,
	// Standard Turtle strata - Time and Screensize - most layers want this
	TRTL_STRATA_BASE = 0x1 << 0,
	// Orthographic Strata - Time, screensize (base), plus new one with sreen center and sizes
	TRTL_STRATA_ORTHOGRAPHIC = 0x1 << 1 || TRTL_STRATA_BASE,
} trtl_strata_t;

struct trtl_layer_info {
	bool has_depth;
	bool clear_on_load;
	trtl_strata_t strata;
};


struct trtl_layer {
	uint32_t nobjects;
	uint32_t nalloced;
	struct trtl_object **objects;

	VkRenderPass render_pass;

	bool has_depth;
	bool clear_on_load;
	trtl_strata_t strata;

	VkRect2D rect;
};

struct turtle *turtle_init(int nlayers, const struct trtl_layer_info *layers);

// FIXME: Should be turtle_create() and render_context should be a struct turtle.
void create_buffer(struct turtle *turtle, VkDeviceSize size, VkBufferUsageFlags usage,
		   VkMemoryPropertyFlags properties, VkBuffer *buffer,
		   VkDeviceMemory *bufferMemory);

// FIXME: This name style
uint32_t findMemoryType(struct turtle *turtle, uint32_t typeFilter,
			VkMemoryPropertyFlags properties);

// FIXME: This doesn't belong here
void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
			   VkImageLayout newLayout);

// FIXME Temp hack
trtl_alloc VkDescriptorPool
create_descriptor_pool(struct trtl_swap_chain *tsc);


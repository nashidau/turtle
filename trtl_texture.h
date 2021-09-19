#pragma once

#include <vulkan/vulkan.h>

struct turtle;
struct trtl_swap_chain; // FIXME: Delete this when args to create_image_views changes

// FIXME: Should be trtl_texure_<something>
VkImage create_texture_image(struct turtle *turtle, const char *path);

// FIXME: Should be trtl_texure_<something>
VkImageView create_texture_image_view(struct turtle *turtle, VkImage texture_image);

// FIXME: shold be trtl_texture_sometjing probablu
// FIXME: Does this belong here; or somewhere else.
void create_image(struct turtle *turtle, uint32_t width, uint32_t height, VkFormat format,
		  VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		  VkImage *image, VkDeviceMemory *imageMemory);

VkImageView create_image_view(struct turtle *turtle, VkImage image, VkFormat format,
			      VkImageAspectFlags aspect_flags);
VkImageView *create_image_views(struct turtle *turtle, VkImage *images, uint32_t nimages);

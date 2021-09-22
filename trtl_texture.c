#include <assert.h>

#include <vulkan/vulkan.h>

#include "stb/stb_image.h"

#include "helpers.h"
#include "trtl_solo.h"
#include "trtl_texture.h"
#include "turtle.h"

VkImageView
create_image_view(struct turtle *turtle, VkImage image, VkFormat format,
		  VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo viewInfo = {0};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect_flags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(turtle->device, &viewInfo, NULL, &imageView) != VK_SUCCESS) {
		error("failed to create texture image view!");
	}

	return imageView;
}

VkImageView *
create_image_views(struct turtle *turtle, VkImage *images, uint32_t nimages)
{
	VkImageView *image_views;
	image_views = talloc_array(NULL, VkImageView, nimages);

	for (uint32_t i = 0; i < nimages; i++) {
		image_views[i] = create_image_view(turtle, images[i], turtle->image_format,
						   VK_IMAGE_ASPECT_COLOR_BIT);
	}
	return image_views;
}

static void
copy_buffer_to_image(trtl_arg_unused struct turtle *turtle, VkBuffer buffer, VkImage image,
		     uint32_t width, uint32_t height)
{
	struct trtl_solo *solo = trtl_solo_get();

	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){width, height, 1};

	vkCmdCopyBufferToImage(solo->command_buffer, buffer, image,
			       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	trtl_solo_done(solo);
}

VkImage
create_texture_image(struct turtle *turtle, const char *path)
{
	VkImage image;
	VkDeviceMemory imageMemory;
	int width, height, channels;

	stbi_uc *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == NULL) {
		error("failed to load texture image!");
	}
	VkDeviceSize imageSize = width * height * 4;

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(turtle, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		      &staging_buffer, &staging_buffer_memory);

	void *data;
	vkMapMemory(turtle->device, staging_buffer_memory, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(turtle->device, staging_buffer_memory);

	stbi_image_free(pixels);

	create_image(turtle, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image, &imageMemory);

	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
			      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copy_buffer_to_image(NULL, staging_buffer, image, width, height);
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(turtle->device, staging_buffer, NULL);
	vkFreeMemory(turtle->device, staging_buffer_memory, NULL);

	return image;
}

VkSampler
create_texture_sampler(struct turtle *turtle)
{
	VkSampler sampler;
	VkPhysicalDeviceProperties properties = {0};
	vkGetPhysicalDeviceProperties(turtle->physical_device, &properties);

	VkSamplerCreateInfo sampler_info = {0};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(turtle->device, &sampler_info, NULL, &sampler) != VK_SUCCESS) {
		error("failed to create texture sampler!");
	}
	return sampler;
}

VkImageView
create_texture_image_view(struct turtle *render, VkImage texture_image)
{
	return create_image_view(render, texture_image, VK_FORMAT_R8G8B8A8_SRGB,
				 VK_IMAGE_ASPECT_COLOR_BIT);
}

// FIXME: Is this the right file
void
create_image(struct turtle *turtle, uint32_t width, uint32_t height, VkFormat format,
	     VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	     VkImage *image, VkDeviceMemory *imageMemory)
{
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(turtle->device, &imageInfo, NULL, image) != VK_SUCCESS) {
		error("failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(turtle->device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex =
	    findMemoryType(turtle, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(turtle->device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
		error("failed to allocate image memory!");
	}

	vkBindImageMemory(turtle->device, *image, *imageMemory, 0);
}

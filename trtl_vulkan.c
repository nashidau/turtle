
#include <stdio.h>

#include <vulkan/vulkan.h>

#include "helpers.h"
#include "trtl_vulkan.h"

const VkSurfaceFormatKHR *
chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t nformats)
{

	// FIXME: THis was VK_COLOR_SPACE_SRGB_NONLINEAR_KHR.
	// Should have more sophistaiced way of looking and choosing.
	for (uint32_t i = 0; i < nformats; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
		    availableFormats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
			return availableFormats + i;
		}
	}

	return availableFormats;
}

VkPresentModeKHR
chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes, uint32_t npresentmodes)
{
	for (uint32_t i = 0; i < npresentmodes; i++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
chooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities)
{
	if (capabilities->currentExtent.width != UINT32_MAX) {
		return capabilities->currentExtent;
	} else {
		int width, height;
		width = 50;
		height = 50;
		fprintf(stderr, "Hardcoded size you fools!\n");
		// glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
		    .width = width,
		    .height = height,
		};

		actualExtent.width =
		    MAX(capabilities->minImageExtent.width,
			MIN(capabilities->maxImageExtent.width, actualExtent.width));
		actualExtent.height =
		    MAX(capabilities->minImageExtent.height,
			MIN(capabilities->maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

VkFormat
find_depth_format(VkPhysicalDevice physical_device)
{
	VkFormat formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
			      VK_FORMAT_D24_UNORM_S8_UINT};

	return find_supported_format(physical_device, TRTL_ARRAY_SIZE(formats), formats,
				     VK_IMAGE_TILING_OPTIMAL,
				     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat
find_supported_format(VkPhysicalDevice physical_device, uint32_t ncandidates, VkFormat *candidates,
		      VkImageTiling tiling, VkFormatFeatureFlags features)
{

	for (uint32_t i = 0; i < ncandidates; i++) {
		VkFormat format = candidates[i];
		VkFormatProperties props;

		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR &&
		    (props.linearTilingFeatures & features) == features) {
			return format;

		} else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
			   (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	error("Failed to find a supported format");
}

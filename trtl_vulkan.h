#pragma once

#include <vulkan/vulkan.h>

const VkSurfaceFormatKHR *chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats,
						  uint32_t nformats);

VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes,
				       uint32_t npresentmodes);

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities);

VkFormat find_depth_format(VkPhysicalDevice physical_device);

VkFormat find_supported_format(VkPhysicalDevice physical_device, uint32_t ncandidates,
			       VkFormat *candidates, VkImageTiling tiling,
			       VkFormatFeatureFlags features);

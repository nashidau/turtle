#pragma once

#include <vulkan/vulkan.h>

const VkSurfaceFormatKHR *chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats,
						  uint32_t nformats);

VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes,
				       uint32_t npresentmodes);

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities);

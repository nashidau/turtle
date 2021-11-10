/**
 * Class to keep track of events; a logging class.
 *
 */

#include <talloc.h>
#include <vulkan/vulkan.h>

#include "helpers.h"
#include "trtl_scribe.h"
#include "turtle.h"

// FIXME: This has a big fixme on it
static VkDebugUtilsMessengerEXT debugMessenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    trtl_arg_unused VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    trtl_arg_unused VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, trtl_arg_unused void *pUserData);

static const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

static VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debugCallback,
};

/**
 * Update the validatiion layers in the ince create data if
 * validateion is enabled.
 */
void
trtl_scribe_upadate_validation(VkInstanceCreateInfo *create_info)
{
	// FIXME: Allocate these sanely
	//static VkValidationFeatureEnableEXT enabled[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
	static VkValidationFeaturesEXT features;
	features.disabledValidationFeatureCount = 0;
	features.enabledValidationFeatureCount = 0;
	features.pDisabledValidationFeatures = NULL;
	features.pEnabledValidationFeatures = NULL;//enabled;

	// FIXME: Check logging state / verbose state
	create_info->enabledLayerCount = 1;
	create_info->ppEnabledLayerNames = &VALIDATION_LAYER;
	create_info->pNext = &debug_messenger_create_info;

	features.pNext = create_info->pNext;
	create_info->pNext = &features;
}

VkResult
CreateDebugUtilsMessengerEXT(VkInstance instance,
			     const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
			     const VkAllocationCallbacks *pAllocator,
			     VkDebugUtilsMessengerEXT *pDebugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT func =
	    (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != NULL) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(trtl_arg_unused VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	      trtl_arg_unused VkDebugUtilsMessageTypeFlagsEXT messageType,
	      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	      trtl_arg_unused void *pUserData)
{
	LOG(VERBOSE, "Validation Layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

bool
trtl_scribe_check_validation_layer_support(void)
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties *available = talloc_array(NULL, VkLayerProperties, layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available);

	for (uint32_t i = 0; i < layer_count; i++) {
		VkLayerProperties layer_name = available[i];
		if (strcmp(VALIDATION_LAYER, layer_name.layerName) == 0) {
			talloc_free(available);
			return true;
		}
	}
	return false;
}
void
trtl_setup_debug_messenger(VkInstance instance)
{
	VkResult err;

	err = CreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, NULL,
					   &debugMessenger);
	if (err != VK_SUCCESS) {
		printf("Failed to cretate debug messenger\n");
	}
}

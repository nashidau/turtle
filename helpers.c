#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "vulkan/vulkan.h"

#include "helpers.h"

__attribute__((format(printf, 1, 2))) trtl_noreturn int
error(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	exit(1);
}

/*
 * FIXME: Should have (runtime) option to exit on warning
 * FIXME: Should print line number and file of the caller
 */
__attribute__((format(printf, 1, 2))) int
warning(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	return 0;
}

trtl_noreturn int
error_msg(VkResult result, const char *msg)
{
	fprintf(stderr, "Error: %s Return: %s\n", msg, vk_err_msg(result));
	exit(1);
}

const char *
vk_err_msg(VkResult errorCode)
{
	switch (errorCode) {
#define STR(r)                                                                                     \
	case VK_##r:                                                                               \
		return #r
		STR(SUCCESS);
		STR(NOT_READY);
		STR(TIMEOUT);
		STR(EVENT_SET);
		STR(EVENT_RESET);
		STR(INCOMPLETE);
		STR(ERROR_OUT_OF_HOST_MEMORY);
		STR(ERROR_OUT_OF_DEVICE_MEMORY);
		STR(ERROR_INITIALIZATION_FAILED);
		STR(ERROR_DEVICE_LOST);
		STR(ERROR_MEMORY_MAP_FAILED);
		STR(ERROR_LAYER_NOT_PRESENT);
		STR(ERROR_EXTENSION_NOT_PRESENT);
		STR(ERROR_FEATURE_NOT_PRESENT);
		STR(ERROR_INCOMPATIBLE_DRIVER);
		STR(ERROR_TOO_MANY_OBJECTS);
		STR(ERROR_FORMAT_NOT_SUPPORTED);
		STR(ERROR_SURFACE_LOST_KHR);
		STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		STR(SUBOPTIMAL_KHR);
		STR(ERROR_OUT_OF_DATE_KHR);
		STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		STR(ERROR_VALIDATION_FAILED_EXT);
		STR(ERROR_INVALID_SHADER_NV);
		STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
		STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
		STR(ERROR_NOT_PERMITTED_EXT);
		STR(ERROR_FRAGMENTATION);
		STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
		STR(ERROR_INVALID_EXTERNAL_HANDLE);
		STR(ERROR_OUT_OF_POOL_MEMORY);
		STR(ERROR_UNKNOWN);
		STR(ERROR_FRAGMENTED_POOL);
#ifdef VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
		STR(THREAD_IDLE_KHR);
		STR(THREAD_DONE_KHR);
		STR(OPERATION_DEFERRED_KHR);
		STR(OPERATION_NOT_DEFERRED_KHR);
		STR(PIPELINE_COMPILE_REQUIRED_EXT);
#endif
#if VK_HEADER_VERSION < 140
		STR(RESULT_RANGE_SIZE);
#endif
		STR(RESULT_MAX_ENUM);
	default:
		return "Unknown error code";
#undef STR
	}
}

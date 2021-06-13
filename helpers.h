
#include "vulkan/vulkan.h"

#define trtl_alloc  __attribute__((malloc))
#define trtl_noreturn  __attribute__((noreturn))
#define trtl_arg_unused  __attribute__((unused))
#define trtl_pure __attribute__((pure))
#define trtl_const __attribute__((const))

#define TRTL_ARRAY_SIZE(array)  ((uint32_t)(sizeof(array)/sizeof(array[0])))

const char * vk_err_msg(VkResult errorCode);
trtl_noreturn int error(const char *msg, ...);
trtl_noreturn int error_msg(VkResult result, const char *msg);
int warning(const char *msg, ...);
	

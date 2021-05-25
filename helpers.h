
#include "vulkan/vulkan.h"

#define trtl_alloc  __attribute__((warn_unused_result))
#define trtl_noreturn  __attribute__((noreturn))
#define trtl_arg_unused  __attribute__((unused))

const char * vk_err_msg(VkResult errorCode);
trtl_noreturn int error(const char *msg);
trtl_noreturn int error_msg(VkResult result, const char *msg);
int warning(const char *msg);
	

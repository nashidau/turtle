#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include <vulkan/vulkan.h>

// FIXME: debug, verbose shoudl be in trtl_debug or similar
enum trtl_debug {
	TRTL_SCRIBE_ERROR = 0,
	TRTL_SCRIBE_WARNING = 1,
	TRTL_SCRIBE_INFO = 2,
	TRTL_SCRIBE_VERBOSE = 3,
	TRTL_SCRIBE_DEBUG = 4,
};

static enum trtl_debug debug = TRTL_SCRIBE_INFO;

__attribute__((unused)) __attribute__((format(printf, 2, 3))) static void
verbose(enum trtl_debug msg_level, const char *fmt, ...)
{
	if (msg_level <= debug) {
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

#define LOG(x, ...) verbose(TRTL_SCRIBE_##x, __VA_ARGS__)

bool trtl_scribe_check_validation_layer_support(void);
void trtl_scribe_upadate_validation(VkInstanceCreateInfo *create_info);

void trtl_setup_debug_messenger(VkInstance instance);

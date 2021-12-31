
#include "vulkan/vulkan_core.h"

struct turtle;

struct trtl_shader {
	const char *path;
	VkShaderModule shader;

	void *internal;
};

struct trtl_shader_cache *trtl_shader_cache_init(struct turtle *turtle);

struct trtl_shader *trtl_shader_get(struct turtle *turtle, const char *path);

#define PRE COMPILED_SHADER_DIR "/"

// Stolen from https://dox.ipxe.org/embedded_8c_source.html and
// https://www.devever.net/~hl/incbin
#define EMBED_SHADER(NAME, FILENAME)                                                               \
	extern uint8_t shader_##NAME##_start[];                                                    \
	extern uint8_t shader_##NAME##_end;                                                        \
	__asm__(".data\n"                                                                          \
		".align 4\n"                                                                       \
		".global _shader_" #NAME "_start\n"                                                \
		".global _shader_" #NAME "_end\n"                                                  \
		"_shader_" #NAME "_start:\n"                                                       \
		".incbin \"" COMPILED_SHADER_DIR "/" FILENAME "\"\n"                               \
		"_shader_" #NAME "_end:\n"                                                         \
		".previous\n")

// On mac the underscore is stripped in the assembler (or added by the compiler).  On
// Linux it seems to preserved.  So adding a _ for Linux
#ifdef __APPLE__
#define SYMBOL_PREFIX "_shader_"
#else
#define SYMBOL_PREFIX "shader_"
#endif

// Embeds an inline shader - doesn't set the _end token, sets a length token instead.
// This is pretty much only for testing.
#define EMBED_SHADER_STRING(NAME, STRING)                                                          \
	extern uint8_t shader_##NAME##_start[];                                                    \
	extern uint8_t shader_##NAME##_end;                                                        \
	__asm__(".data\n"                                                                          \
		".align 4\n"                                                                       \
		".global " SYMBOL_PREFIX #NAME "_start\n"                                      	   \
		".global " SYMBOL_PREFIX #NAME "_end\n"                                            \
		SYMBOL_PREFIX #NAME "_start:\n"                                                    \
		".asciz \"" STRING "\"\n"                                                          \
		SYMBOL_PREFIX #NAME "_end:\n"                                                      \
		".previous\n")

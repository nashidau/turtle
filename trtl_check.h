#include <check.h>

#include "fff/fff.h"

#include "turtle.h"


Suite *trtl_uniform_suite(void *ctx);
Suite *trtl_timer_suite(trtl_arg_unused void *ctx);
Suite *trtl_shader_suite(trtl_arg_unused void *ctx);

// The ubuntu 20.04 version of check is ancient
#ifndef ck_assert_double_eq
#define ck_assert_double_eq(X, Y)                                                                  \
	do {                                                                                       \
		double _ck_x = (X);                                                                \
		double _ck_y = (Y);                                                                \
		ck_assert_msg(_ck_x == _ck_y, "Assertion '%s' failed: %s == %.*g, %s == %.*g",     \
			      #X " ==  " #Y, #X, 6, _ck_x, #Y, 6, _ck_y);                          \
	} while (0)
#endif



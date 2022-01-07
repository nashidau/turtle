#include <check.h>
#include <talloc.h>

#include "trtl_check.h"
#include "helpers.h"

// FFF needs to inserted somewhere; lets do it here
DEFINE_FFF_GLOBALS;

Suite *blam_suite(void *ctx);

typedef Suite *(*test_module)(void *ctx);

static test_module test_modules[] = {
	trtl_crier_suite,
	trtl_events_suite,
	trtl_uniform_suite,
	trtl_shader_suite,
	trtl_timer_suite,
	trtl_solo_suite,
};
#define N_MODULES ((int)(sizeof(test_modules)/sizeof(test_modules[0])))

int main(trtl_arg_unused int argc, trtl_arg_unused char **argv) {
	SRunner *sr;
	int nfailed;
	int i;

#if !defined(ADDRESS_SANITIZER)
	talloc_enable_leak_report();
#endif

	sr = srunner_create(NULL);
	for (i = 0 ; i < N_MODULES ; i ++) {
		srunner_add_suite(sr, test_modules[i](NULL));
	}

	srunner_run_all(sr, CK_VERBOSE);
	nfailed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return !!nfailed;
}


#include <check.h>
#include <talloc.h>

#include "trtl_uniform.h"
#include "trtl_check.h"

START_TEST(test_uniform_init) {
	struct trtl_uniform *uniforms;

	uniforms = trtl_uniform_init(NULL, 2, 100);
	ck_assert_ptr_ne(uniforms, NULL);
	talloc_free(uniforms);
} END_TEST



Suite *
trtl_uniform_suite(void *ctx) {
	Suite *s = suite_create("Uniform");
	TCase *tc_alloc = tcase_create("Allocation");

        //tcase_add_checked_fixture(tc_seq, sequence_init, sequence_shutdown);
        suite_add_tcase(s, tc_alloc);

        tcase_add_test(tc_alloc, test_uniform_init);
	/*
        tcase_add_test(tc_seq, test_seq_single_value);
        tcase_add_test(tc_seq, test_seq_all_values);
        tcase_add_test(tc_seq, test_seq_loops);
        tcase_add_test(tc_seq, test_seq_repeats_last);
        tcase_add_test(tc_seq, test_seq_change_updates);
        tcase_add_test(tc_seq, test_seq_change_resets_pos);
        tcase_add_test(tc_seq, test_seq_empty_fails);
        tcase_add_test(tc_seq, test_seq_single_item);
        tcase_add_test(tc_seq, test_seq_range_sane);
        tcase_add_test(c_seq, test_seq_range_outside);

        tcase_add_test(tc_seq, test_seq_range_all);
*/
        return s;
}


#include <check.h>
#include <talloc.h>
#include <time.h>

#include "fff/fff.h"

#include "helpers.h"
#include "trtl_crier.h"

FAKE_VOID_FUNC(callback1, void *, trtl_crier_cry_t, const void *);
FAKE_VOID_FUNC(callback2, void *, trtl_crier_cry_t, const void *);

START_TEST(test_crier_create)
{
	struct trtl_crier *crier = trtl_crier_init();
	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_add_cry)
{
	struct trtl_crier *crier = trtl_crier_init();

	// Add a cry, make sure we get a sane index
	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	ck_assert_int_ne(cry, TRTL_CRY_INVALID);

	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_add_cry_again)
{
	struct trtl_crier *crier = trtl_crier_init();

	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	trtl_crier_cry_t cry2 = trtl_crier_new_cry(crier, "Test");
	ck_assert_int_ne(cry, TRTL_CRY_INVALID);
	ck_assert_int_eq(cry2, cry);
	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_add_many_cries)
{
	struct trtl_crier *crier = trtl_crier_init();

	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	trtl_crier_cry_t cry2 = trtl_crier_new_cry(crier, "Test 2");
	ck_assert_int_ne(cry, TRTL_CRY_INVALID);
	ck_assert_int_ne(cry2, TRTL_CRY_INVALID);
	ck_assert_int_ne(cry, cry2);

	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_post)
{
	struct trtl_crier *crier = trtl_crier_init();

	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	bool result = trtl_crier_post(crier, cry, NULL);
	ck_assert(result);

	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_post_unregistered)
{
	struct trtl_crier *crier = trtl_crier_init();

	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	bool result = trtl_crier_post(crier, cry + 7, NULL);
	ck_assert(!result);

	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_cb_register)
{
	struct trtl_crier *crier = trtl_crier_init();
	int local;

	// register a cry and a callback
	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	trtl_crier_listen(crier, "Test", callback1,  &local);
	ck_assert(trtl_crier_post(crier, cry, "Local"));

	ck_assert_int_eq(callback1_fake.call_count, 1);

	talloc_free(crier);
}
END_TEST
START_TEST(test_crier_cb_register_late)
{
	struct trtl_crier *crier = trtl_crier_init();
	int local;

	// register a callback, then the cry
	trtl_crier_listen(crier, "Test", callback1,  &local);
	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	ck_assert(trtl_crier_post(crier, cry, "Local"));

	ck_assert_int_eq(callback1_fake.call_count, 1);

	talloc_free(crier);
}
END_TEST

START_TEST(test_crier_cb_register_multiple)
{
	struct trtl_crier *crier = trtl_crier_init();
	int local;
	int local2;
	
	// register listener, cry and call/post it.  Do with multiple
	trtl_crier_listen(crier, "Test", callback1,  &local);
	trtl_crier_cry_t cry = trtl_crier_new_cry(crier, "Test");
	trtl_crier_cry_t cry2 = trtl_crier_new_cry(crier, "Test2");
	trtl_crier_listen(crier, "Test2", callback2,  &local2);
	ck_assert(trtl_crier_post(crier, cry, "Local"));
	ck_assert(trtl_crier_post(crier, cry2, "Local"));

	ck_assert_int_eq(callback1_fake.call_count, 1);

	talloc_free(crier);
}
END_TEST

Suite *
trtl_crier_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Crier");

	{
		TCase *tc_create = tcase_create("Create");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_crier_create);
	}
	{
		TCase *tc_create = tcase_create("Add Cries");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_crier_add_cry);
		tcase_add_test(tc_create, test_crier_add_cry_again);
		tcase_add_test(tc_create, test_crier_add_many_cries);
	}
	{
		TCase *tc_post = tcase_create("Post Cry");
		suite_add_tcase(s, tc_post);

		tcase_add_test(tc_post, test_crier_post);
		tcase_add_test(tc_post, test_crier_post_unregistered);
	}
	{
		TCase *tc_cbs = tcase_create("Callbacks");
		suite_add_tcase(s, tc_cbs);

		tcase_add_test(tc_cbs, test_crier_cb_register);
		tcase_add_test(tc_cbs, test_crier_cb_register_late);
		tcase_add_test(tc_cbs, test_crier_cb_register_multiple);
	}
	// FIXME: Tests to check the data passed is correct.

	return s;
}

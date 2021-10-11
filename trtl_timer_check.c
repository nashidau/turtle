
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "trtl_check.h"
#include "trtl_timer.h"
#include "turtle.h"

FAKE_VALUE_FUNC(int, clock_gettime, clockid_t, struct timespec *);

static struct timespec saved_timespec;
static int
custom_clock_gettime(trtl_arg_unused clockid_t clock, struct timespec *timespec)
{
	*timespec = saved_timespec;
	return 0;
}

START_TEST(test_timer_zero)
{
	struct timespec tv;
	trtl_timer_double_to_timespec(0.0f, &tv);
	ck_assert_int_eq(tv.tv_sec, 0);
	ck_assert_int_eq(tv.tv_nsec, 0);
}
END_TEST

START_TEST(test_timer_one_second)
{
	struct timespec tv;
	trtl_timer_double_to_timespec(1.0f, &tv);
	ck_assert_int_eq(tv.tv_sec, 1);
	ck_assert_int_eq(tv.tv_nsec, 0);
}
END_TEST

START_TEST(test_timer_one_quarter)
{
	struct timespec tv;
	trtl_timer_double_to_timespec(0.25f, &tv);
	ck_assert_int_eq(tv.tv_sec, 0);
	ck_assert_int_eq(tv.tv_nsec, 250000000);
}
END_TEST

START_TEST(test_timer_timespec_add)
{
	struct timespec a = {.tv_sec = 1, .tv_nsec = 100};
	struct timespec b = {.tv_sec = 10, .tv_nsec = 11};
	trtl_timer_add_timespecs(&a, &b);
	ck_assert_int_eq(a.tv_sec, 11);
	ck_assert_int_eq(a.tv_nsec, 111);
	// b did not change
	ck_assert_int_eq(b.tv_sec, 10);
	ck_assert_int_eq(b.tv_nsec, 11);
}
END_TEST

START_TEST(test_timer_timespec_add_adjust)
{
	// about .6 of a nsec
	struct timespec a = {.tv_sec = 1, .tv_nsec = 600000000};
	struct timespec b = {.tv_sec = 10, .tv_nsec = 600000000};
	trtl_timer_add_timespecs(&a, &b);
	// 1  + 10 + carry
	ck_assert_int_eq(a.tv_sec, 12);
	ck_assert_int_eq(a.tv_nsec, 200000000);
	// b did not change
	ck_assert_int_eq(b.tv_sec, 10);
	ck_assert_int_eq(b.tv_nsec, 600000000);
}
END_TEST

START_TEST(test_timer_compare_secs)
{
	struct timespec a = {.tv_sec = 5, .tv_nsec = 6};
	struct timespec b = {.tv_sec = 7, .tv_nsec = 5};
	ck_assert_int_eq(trtl_timer_timespec_before(&a, &b), 1);
	ck_assert_int_eq(trtl_timer_timespec_before(&b, &a), 0);
}
END_TEST

START_TEST(test_timer_compare_nsecs)
{
	// Seconds the same, so nsec should matter
	struct timespec a = {.tv_sec = 5, .tv_nsec = 2};
	struct timespec b = {.tv_sec = 5, .tv_nsec = 5};
	ck_assert_int_eq(trtl_timer_timespec_before(&a, &b), 1);
	ck_assert_int_eq(trtl_timer_timespec_before(&b, &a), 0);
}
END_TEST

START_TEST(test_timer_compare_equal)
{
	// Seconds the same, so nsec should matter
	struct timespec a = {.tv_sec = 5, .tv_nsec = 2};
	struct timespec b = {.tv_sec = 5, .tv_nsec = 5};
	ck_assert_int_eq(trtl_timer_timespec_before(&a, &b), 1);
	ck_assert_int_eq(trtl_timer_timespec_before(&b, &a), 0);
}
END_TEST

START_TEST(test_timer_create)
{
	struct trtl_timer *timer;

	timer = trtl_timer_add("Timer", 1);
	ck_assert_ptr_ne(timer, NULL);
	talloc_free(timer);
}
END_TEST

START_TEST(test_timer_schedule_one)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer = trtl_timer_add("test", 1);
	trtl_timer_schedule(turtle, timer);
	ck_assert_ptr_eq(turtle->timers, timer);
	talloc_free(turtle);

	ck_assert_int_eq(clock_gettime_fake.call_count, 1);
}
END_TEST

START_TEST(test_timer_schedule_later)
{
	clock_gettime_fake.custom_fake = custom_clock_gettime;

	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timerA = trtl_timer_add("Timer A", 1);
	struct trtl_timer *timerB = trtl_timer_add("Timer B", 2);
	trtl_timer_schedule(turtle, timerA);
	trtl_timer_schedule(turtle, timerB);
	ck_assert_ptr_eq(turtle->timers, timerA);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_schedule_earlier)
{
	clock_gettime_fake.custom_fake = custom_clock_gettime;

	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timerA = trtl_timer_add("Timer A", 1);
	struct trtl_timer *timerB = trtl_timer_add("Timer B", 2);
	trtl_timer_schedule(turtle, timerB);
	trtl_timer_schedule(turtle, timerA);
	ck_assert_ptr_eq(turtle->timers, timerA);
	talloc_free(turtle);
}
END_TEST



START_TEST(test_timer_schedule_middle)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timerA = trtl_timer_add("Timer A", 1);
	struct trtl_timer *timerB = trtl_timer_add("Timer B", 2);
	struct trtl_timer *timerC = trtl_timer_add("Timer C", 3);
	// Reverse the order then the previous test
	trtl_timer_schedule(turtle, timerC);
	trtl_timer_schedule(turtle, timerA);
	trtl_timer_schedule(turtle, timerB);
	ck_assert_ptr_eq(turtle->timers, timerA);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_schedule_many)
{
	printf("\n\n**** Test: timer many\n");
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	const double timeouts[] = {4, 7, 9, 2, 5, 3, 10, 1, 6, 8};
	for (size_t i = 0 ; i < TRTL_ARRAY_SIZE(timeouts) ; i ++){ 
		char buf[100];
		snprintf(buf, sizeof(buf), "Timer %zd (%fs)", i, timeouts[i]); 
		trtl_timer_schedule(turtle, trtl_timer_add(buf, timeouts[i]));
		trtl_timer_print(turtle);
	}
	// Reverse the order then the previous test
	//ck_assert_ptr_eq(turtle->timers, 1);
	printf("All added\n");
	trtl_timer_print(turtle);
	talloc_free(turtle);
}
END_TEST

Suite *
trtl_timer_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Timer");

	{
		TCase *tc_convert = tcase_create("Conversion");
		suite_add_tcase(s, tc_convert);

		tcase_add_test(tc_convert, test_timer_zero);
		tcase_add_test(tc_convert, test_timer_one_second);
		tcase_add_test(tc_convert, test_timer_one_quarter);
	}

	{
		TCase *tc_add = tcase_create("Add");
		suite_add_tcase(s, tc_add);

		tcase_add_test(tc_add, test_timer_timespec_add);
		tcase_add_test(tc_add, test_timer_timespec_add_adjust);
	}
	{
		// Compare two timespecs
		TCase *tc_compare = tcase_create("Compare");
		suite_add_tcase(s, tc_compare);

		tcase_add_test(tc_compare, test_timer_compare_secs);
		tcase_add_test(tc_compare, test_timer_compare_nsecs);
		tcase_add_test(tc_compare, test_timer_compare_equal);
	}
	{
		TCase *tc_create = tcase_create("Create");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_timer_create);
	}
	{
		TCase *tc_schedule = tcase_create("Schedule");
		suite_add_tcase(s, tc_schedule);

		tcase_add_test(tc_schedule, test_timer_schedule_one);
		tcase_add_test(tc_schedule, test_timer_schedule_later);
		tcase_add_test(tc_schedule, test_timer_schedule_earlier);
		tcase_add_test(tc_schedule, test_timer_schedule_middle);
		tcase_add_test(tc_schedule, test_timer_schedule_many);
	}
	return s;
}

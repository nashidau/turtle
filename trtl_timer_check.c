
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "trtl_check.h"
#include "trtl_timer.h"
#include "turtle.h"

FAKE_VALUE_FUNC(int, callback, void *, struct turtle *, struct trtl_timer *);

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

START_TEST(test_timer_ts_zero)
{
	struct timespec tv = {.tv_sec = 0, .tv_nsec = 0};
	double val = trtl_timer_timespec_to_double(&tv);
	ck_assert_double_eq(val, 0);
}
END_TEST

START_TEST(test_timer_ts_one_point_five)
{
	struct timespec tv = {.tv_sec = 1, .tv_nsec = 5e8};
	double val = trtl_timer_timespec_to_double(&tv);
	ck_assert_double_eq(val, 1.5);
}
END_TEST

START_TEST(test_timer_ten_seconds)
{
	struct timespec tv = {.tv_sec = 9, .tv_nsec = 1e9};
	double val = trtl_timer_timespec_to_double(&tv);
	ck_assert_double_eq(val, 10);
}
END_TEST

START_TEST(test_timer_diff_same)
{
	struct timespec out;
	struct timespec a = {.tv_sec = 37, .tv_nsec = 3392};
	struct timespec b = a;
	trtl_timer_timespec_difference(&out, &a, &b);
	ck_assert_int_eq(out.tv_sec, 0);
	ck_assert_int_eq(out.tv_nsec, 0);
}
END_TEST

START_TEST(test_timer_diff_seconds)
{
	struct timespec out;
	struct timespec a = {.tv_sec = 39, .tv_nsec = 0};
	struct timespec b = {.tv_sec = 37, .tv_nsec = 0};
	trtl_timer_timespec_difference(&out, &a, &b);
	ck_assert_int_eq(out.tv_sec, 2);
	ck_assert_int_eq(out.tv_nsec, 0);
}
END_TEST

START_TEST(test_timer_diff_nanoseconds)
{

	struct timespec out;
	struct timespec a = {.tv_sec = 39, .tv_nsec = 2223};
	struct timespec b = {.tv_sec = 37, .tv_nsec = 1123};
	trtl_timer_timespec_difference(&out, &a, &b);
	ck_assert_int_eq(out.tv_sec, 2);
	ck_assert_int_eq(out.tv_nsec, 1100);
}
END_TEST

START_TEST(test_timer_diff_carry)
{
	struct timespec out;
	struct timespec a = {.tv_sec = 39, .tv_nsec = 1000};
	struct timespec b = {.tv_sec = 37, .tv_nsec = 2000};
	trtl_timer_timespec_difference(&out, &a, &b);
	ck_assert_int_eq(out.tv_sec, 1);
	ck_assert_int_eq(out.tv_nsec, 1e9 - 1000);
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
	timer = trtl_timer_add("Timer", 1, callback, NULL);
	ck_assert_ptr_ne(timer, NULL);
	talloc_free(timer);
}
END_TEST

START_TEST(test_timer_invoke)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer = talloc_steal(turtle, trtl_timer_add("test", 1, callback, NULL));
	trtl_timer_schedule(turtle, timer);
	clock_gettime_fake.custom_fake = custom_clock_gettime;
	saved_timespec = (struct timespec){.tv_sec = 2, .tv_nsec = 0};
	trtl_timer_invoke(turtle);
	ck_assert_int_eq(callback_fake.call_count, 1);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_invoke_recurring)
{
	saved_timespec = (struct timespec){.tv_sec = 0, .tv_nsec = 0};
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer = talloc_steal(turtle, trtl_timer_add("test", 1, callback, NULL));
	trtl_timer_schedule(turtle, timer);
	clock_gettime_fake.custom_fake = custom_clock_gettime;
	callback_fake.return_val = 1;
	saved_timespec = (struct timespec){.tv_sec = 1, .tv_nsec = 5};
	trtl_timer_invoke(turtle);
	saved_timespec = (struct timespec){.tv_sec = 2, .tv_nsec = 5};
	trtl_timer_invoke(turtle);

	ck_assert_int_eq(callback_fake.call_count, 2);

	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_schedule_one)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer = talloc_steal(turtle, trtl_timer_add("test", 1, callback, NULL));
	trtl_timer_schedule(turtle, timer);
	ck_assert_ptr_eq(turtle->timers, timer);
	talloc_free(turtle);
	// On ubuntu this gets called twice.  Issue #16
	// ck_assert_int_eq(clock_gettime_fake.call_count, 1);
}
END_TEST

START_TEST(test_timer_schedule_later)
{
	clock_gettime_fake.custom_fake = custom_clock_gettime;

	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timerA =
	    talloc_steal(turtle, trtl_timer_add("Timer A", 1, callback, NULL));
	struct trtl_timer *timerB =
	    talloc_steal(turtle, trtl_timer_add("Timer B", 2, callback, NULL));
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
	struct trtl_timer *timerA =
	    talloc_steal(turtle, trtl_timer_add("Timer A", 1, callback, NULL));
	struct trtl_timer *timerB =
	    talloc_steal(turtle, trtl_timer_add("Timer B", 2, callback, NULL));
	trtl_timer_schedule(turtle, timerB);
	trtl_timer_schedule(turtle, timerA);
	ck_assert_ptr_eq(turtle->timers, timerA);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_schedule_middle)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timerA =
	    talloc_steal(turtle, trtl_timer_add("Timer A", 1, callback, NULL));
	struct trtl_timer *timerB =
	    talloc_steal(turtle, trtl_timer_add("Timer B", 2, callback, NULL));
	struct trtl_timer *timerC =
	    talloc_steal(turtle, trtl_timer_add("Timer C", 3, callback, NULL));
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
	struct trtl_timer *timer;
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	const double timeouts[] = {4, 7, 9, 2, 5, 3, 10, 1, 6, 8};
	for (size_t i = 0; i < TRTL_ARRAY_SIZE(timeouts); i++) {
		char buf[100];
		snprintf(buf, sizeof(buf), "Timer %zd (%fs)", i, timeouts[i]);
		timer = talloc_steal(
		    turtle, trtl_timer_add(buf, timeouts[i], callback, (void *)&timeouts[i]));
		trtl_timer_schedule(turtle, timer);
	}

	// Move time forward to 20 seconds, then invoke them all
	saved_timespec = (struct timespec){.tv_sec = 10, .tv_nsec = 0};
	clock_gettime_fake.custom_fake = custom_clock_gettime;
	trtl_timer_invoke(turtle);
	ck_assert_int_eq(callback_fake.call_count, 10);
	// Check the timers got called in order; the data is a pointer to the timout (as double)
	for (uint32_t i = 0; i < TRTL_ARRAY_SIZE(timeouts); i++) {
		ck_assert_double_eq(*((double *)callback_fake.arg0_history[i]), i + 1);
	}

	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_get_short)
{
	clock_gettime_fake.custom_fake = custom_clock_gettime;
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer = talloc_steal(turtle, trtl_timer_add("Timer", 1, callback, NULL));
	trtl_timer_schedule(turtle, timer);
	double next = trtl_timer_timeout_get(turtle);
	ck_assert_double_eq(next, 1);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_timer_get_half)
{
	clock_gettime_fake.custom_fake = custom_clock_gettime;
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_timer *timer =
	    talloc_steal(turtle, trtl_timer_add("Timer", 0.5, callback, NULL));
	trtl_timer_schedule(turtle, timer);
	double next = trtl_timer_timeout_get(turtle);
	ck_assert_double_eq(next, 0.5);
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
		tcase_add_test(tc_convert, test_timer_ts_zero);
		tcase_add_test(tc_convert, test_timer_ts_one_point_five);
		tcase_add_test(tc_convert, test_timer_ten_seconds);
	}

	{
		TCase *tc_add = tcase_create("Add");
		suite_add_tcase(s, tc_add);

		tcase_add_test(tc_add, test_timer_timespec_add);
		tcase_add_test(tc_add, test_timer_timespec_add_adjust);
	}
	{
		TCase *tc_diff = tcase_create("Difference");
		suite_add_tcase(s, tc_diff);
		tcase_add_test(tc_diff, test_timer_diff_same);
		tcase_add_test(tc_diff, test_timer_diff_seconds);
		tcase_add_test(tc_diff, test_timer_diff_nanoseconds);
		tcase_add_test(tc_diff, test_timer_diff_carry);
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
		TCase *tc_invoke = tcase_create("Invoke");
		suite_add_tcase(s, tc_invoke);

		tcase_add_test(tc_invoke, test_timer_invoke);
		tcase_add_test(tc_invoke, test_timer_invoke_recurring);
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
	{
		TCase *tc_timeout = tcase_create("Timeout");
		suite_add_tcase(s, tc_timeout);

		tcase_add_test(tc_timeout, test_timer_get_short);
		tcase_add_test(tc_timeout, test_timer_get_half);
	}
	return s;
}

#include <math.h>
#include <sys/time.h>

#include <talloc.h>

#include "trtl_timer.h"
#include "trtl_timer_int.h"
#include "turtle.h"

struct trtl_timer {
	struct trtl_timer *next;
	struct trtl_timer *prev;

	const char *name;

	struct timespec due;
	struct timespec period;
};

#define NS_IN_S ((long)1e9)
#define CONVERT_S_TO_NS(x) ((x)*NS_IN_S)

void
trtl_timer_double_to_timespec(double in, struct timespec *tv)
{
	double fval;
	double ival;

	fval = modf(in, &ival);
	tv->tv_sec = ival;
	tv->tv_nsec = CONVERT_S_TO_NS(fval);
}

void
trtl_timer_add_timespecs(struct timespec *a, const struct timespec *b)
{
	a->tv_sec += b->tv_sec;
	a->tv_nsec += b->tv_nsec;
	if (a->tv_nsec > NS_IN_S) {
		a->tv_nsec -= NS_IN_S;
		a->tv_sec += 1;
	}
}

/**
 * Returns true if the first time is before the second time.
 *
 * @param a First timespec
 * @param b Second timespec
 * @return 1 if timespec a occurs before (or equal to) b
 */
int
trtl_timer_timespec_before(struct timespec *a, struct timespec *b)
{
	// If the seconds are lower; easy yes it's first.
	if (a->tv_sec == b->tv_sec) {
		// Need to compare nsec
		return a->tv_nsec < b->tv_nsec;
	}
	if (a->tv_sec < b->tv_sec) return 1;
	return 0;
}

#define trtl_timer_timespec_after(a, b) trtl_timer_timespec_before(b, a)

struct trtl_timer *
trtl_timer_add(const char *name, double peroid)
{
	struct trtl_timer *timer;

	timer = talloc_zero(NULL, struct trtl_timer);
	timer->name = talloc_strdup(timer, name);

	trtl_timer_double_to_timespec(peroid, &timer->period);

	return timer;
}

/**
 * Unconditionally schedules a timer from it's creation time.
 *
 * @param turtle Turtle.
 * @param timer Timer to schedule.
 * @return 0 on successl, or -1 on error.
 */
int
trtl_timer_schedule(struct turtle *turtle, struct trtl_timer *timer)
{

	if (!turtle || !timer) return -1;

	// Set the target time
	clock_gettime(CLOCK_UPTIME_RAW_APPROX, &timer->due);
	trtl_timer_add_timespecs(&timer->due, &timer->period);

	if (turtle->timers == NULL) {
		timer->next = NULL;
		timer->prev = NULL;
		turtle->timers = timer;
		return 0;
	}

	// is it the next timer?
	if (trtl_timer_timespec_before(&timer->due, &turtle->timers->due)) {
		timer->next = turtle->timers;
		timer->prev = NULL;
		turtle->timers->prev = timer;
		turtle->timers = timer;
		return 0;
	}

	// Find the appropriate place to insert it
	struct trtl_timer *prior;
	for (prior = turtle->timers; prior->next != NULL; prior = prior->next) {
		if (trtl_timer_timespec_before(&timer->due, &prior->next->due)) {
			break;
		}
		continue;
	}

	timer->next = prior->next;
	timer->prev = prior;
	prior->next = timer;
	if (timer->next) timer->next->prev = timer;

	return 0;
}

int
trtl_timer_print(struct turtle *turtle)
{
	struct trtl_timer *timer;

	printf("Timers:\n");
	for (timer = turtle->timers; timer; timer = timer->next) {
		printf("\t%s (%p/%p/%p) (Due %ld:%ld)  Period (%ld:%ld)\n", timer->name,
		       timer->prev, timer, timer->next, timer->due.tv_sec, timer->due.tv_nsec,
		       timer->period.tv_sec, timer->period.tv_nsec);
	}
	return 0;
}

#include <math.h>
#include <sys/time.h>
#include <time.h>

#include <talloc.h>

#include "trtl_timer.h"
#include "turtle.h"

// Choose out default clock for clock_gettime.

#ifdef APPLE
// On Apple, the uptime raw clock is the best - dones't trigger when sleeping
// and handles CPU migration
#define TRTL_CLOCK  CLOCK_UPTIME_RAW
#else
// Alas it increments when sleeping.
#define TRTL_CLOCK CLOCK_MONOTONIC
#endif


struct trtl_timer {
	struct trtl_timer *next;
	struct trtl_timer *prev;

	const char *name;

	struct timespec due;
	struct timespec period;

	int (*cb)(void *opaque, struct turtle *turtle, struct trtl_timer *timer);
	void *opaque;
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

double
trtl_timer_timespec_to_double(trtl_arg_unused struct timespec *tv)
{
	return tv->tv_sec + (tv->tv_nsec / (double)NS_IN_S);
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

void
trtl_timer_timespec_difference(struct timespec *out, const struct timespec *a,
			       const struct timespec *b)
{
	if (b->tv_nsec > a->tv_nsec) {
		out->tv_nsec = NS_IN_S + a->tv_nsec - b->tv_nsec;
		out->tv_sec = a->tv_sec - b->tv_sec - 1;
	} else {
		out->tv_nsec = a->tv_nsec - b->tv_nsec;
		out->tv_sec = a->tv_sec - b->tv_sec;
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
trtl_timer_add(const char *name, double peroid,
	       int (*timer_cb)(void *opaque, struct turtle *, struct trtl_timer *timer),
	       void *opaque)
{
	struct trtl_timer *timer;

	timer = talloc_zero(NULL, struct trtl_timer);
	timer->name = talloc_strdup(timer, name);

	trtl_timer_double_to_timespec(peroid, &timer->period);

	timer->cb = timer_cb;
	timer->opaque = opaque;

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
	clock_gettime(TRTL_CLOCK, &timer->due);
	trtl_timer_add_timespecs(&timer->due, &timer->period);

	return trtl_timer_reschedule(turtle, timer);
}

/* Schedules a timer.
 *
 * Like trtl_timer_schedule, except the due time is already set.
 */
int
trtl_timer_reschedule(struct turtle *turtle, struct trtl_timer *timer)
{

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

// FIXME: Handle someone deletign the timer during the callback
int
trtl_timer_invoke_first(struct turtle *turtle, struct timespec *now)
{
	struct trtl_timer *timer;

	if (trtl_timer_timespec_before(now, &turtle->timers->due)) {
		// Not due yet; just return
		return 0;
	}

	// Remove it from the list
	timer = turtle->timers;
	turtle->timers = timer->next;
	if (turtle->timers) {
		turtle->timers->prev = NULL;
	}

	// FIXME: A loop is a terrible way to increment, should use some maths.
	do {
		trtl_timer_add_timespecs(&timer->due, &timer->period);
	} while (trtl_timer_timespec_after(now, &timer->due));

	int rv = timer->cb(timer->opaque, turtle, timer);

	// fixme: unconditional reading this.  It may have been free.
	// Need to do something in the destructor to handle it being deleted.
	if (rv != 0) {
		trtl_timer_reschedule(turtle, timer);
	}

	return 1;
}

/**
 * Call any due timer callbacks.
 *
 * Check to see if any timers have reached their trigger time, if so invoke the
 * callback.
 *
 * If a timer is triggered, and restarted it is reinserted in the list with the appropriate time
 * out.
 *
 * @param turtle Turtle pointer
 * @return Number of timer that were invoked
 */
int
trtl_timer_invoke(struct turtle *turtle)
{
	struct timespec now;
	if (!turtle->timers) return 0;

	// Get 'now' once.  So we always make progress
	clock_gettime(TRTL_CLOCK, &now);

	while (trtl_timer_invoke_first(turtle, &now) && turtle->timers != NULL) {
		// Call again basically ;-)
	}

	return 1;
}

/**
 * Work out how long before the next timer needs to be called.
 *
 * @param turtle Pointer
 * @return How long (in seconds) before the next event.
 */
double
trtl_timer_timeout_get(struct turtle *turtle)
{
	struct timespec now;
	struct timespec res;

	if (turtle->timers == NULL) return 1;

	clock_gettime(TRTL_CLOCK, &now);
	trtl_timer_timespec_difference(&res, &turtle->timers->due, &now);

	return trtl_timer_timespec_to_double(&res);
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

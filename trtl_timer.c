#include <math.h>
#include <sys/time.h>

#include <talloc.h>

struct trtl_timer {
	const char *name;

	struct timeval next;
	struct timeval period;
};

#define US_IN_S ((int)1e6)
#define CONVERT_S_TO_US(x) ((x)*US_IN_S)

static void
double_to_timeval(double in, struct timeval *tv)
{
	double fval;
	double ival;

	fval = modf(in, &ival);
	tv->tv_sec = ival;
	tv->tv_usec = CONVERT_S_TO_US(fval);
}

struct trtl_timer *
trtl_timer_add(const char *name, double peroid)
{
	struct trtl_timer *timer;

	timer = talloc_zero(NULL, struct trtl_timer);
	timer->name = talloc_strdup(timer, name);

	double_to_timeval(peroid, &timer->period);

	return timer;
}


struct turtle;
struct trtl_timer;
struct timespec;

struct trtl_timer *trtl_timer_add(const char *name, double period,
				  int (*timer_cb)(void *opaque, struct turtle *,
						  struct trtl_timer *timer),
				  void *opaque);
int trtl_timer_schedule(struct turtle *clock, struct trtl_timer *);
int trtl_timer_reschedule(struct turtle *turtle, struct trtl_timer *timer);

double trtl_timer_iterate(struct turtle *clock);

int trtl_timer_invoke(struct turtle *turtle);

// Used internally convert tv structure to a double
void trtl_timer_double_to_timespec(double in, struct timespec *tv);
int trtl_timer_timespec_before(struct timespec *a, struct timespec *b);
void trtl_timer_add_timespecs(struct timespec *a, const struct timespec *b);

int trtl_timer_print(struct turtle *turtle);

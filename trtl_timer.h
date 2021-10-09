
struct turtle;
struct trtl_timer;

struct trtl_timer *trtl_timer_add(const char *name, double period);
int trtl_timer_schedule(struct turtle *clock, struct trtl_timer*);

double trtl_timer_iterate(struct turtle *clock);



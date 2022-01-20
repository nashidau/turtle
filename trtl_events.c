#include <assert.h>

#include <talloc.h>

#include "turtle.h"
#include "trtl_events.h"

struct trtl_events *
trtl_event_init(void) {
	struct trtl_events *events = talloc_zero(NULL, struct trtl_events);

	events->crier = talloc_steal(events, trtl_crier_init());
	
	events->resize = trtl_crier_new_cry(events->crier, "trtl_event_resize");
	events->grid_move = trtl_crier_new_cry(events->crier, "trtl_event_grid_move");

	return events;
}

int trtl_event_resize(struct turtle *turtle, VkExtent2D new_size) {
	struct trtl_event_resize *resize = talloc_zero(NULL, struct trtl_event_resize);
	struct trtl_events *events = turtle->events;
	assert(resize);

	resize->turtle = turtle;
	resize->new_size = new_size;

	trtl_crier_post(events->crier, events->resize, resize);
	
	talloc_free(resize);
	return 0;
}

int trtl_event_grid_move(struct turtle *turtle, int32_t x, int32_t y, bool snap) {
	struct trtl_event_grid_move *gmove = talloc_zero(NULL, struct trtl_event_grid_move);
	struct trtl_events *events = turtle->events;
	assert(gmove);

	gmove->turtle = turtle;
	gmove->x = x;
	gmove->y = y;
	gmove->snap = snap;

	trtl_crier_post(events->crier, events->grid_move, gmove);
	
	talloc_free(gmove);
	return 0;
}


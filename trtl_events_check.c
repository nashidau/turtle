
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "fff/fff.h"

#include "helpers.h"
#include "trtl_check.h"
#include "trtl_events.h"
#include "turtle.h"

FAKE_VOID_FUNC(event_resize_callback, void *, trtl_crier_cry_t, const void *);
FAKE_VOID_FUNC(event_grid_move_callback, void *, trtl_crier_cry_t, const void *);

START_TEST(test_event_init)
{
	struct trtl_events *events = trtl_event_init();
	talloc_free(events);
}
END_TEST

static uint32_t fixed_pointer;

// The paramters for the callback are only defiend to exist for the list of the callback.  So we
// can't easily check them afterwards.  Hence we check during the callback itself.
static void
event_resize_callback_handler(void *data, trtl_crier_cry_t cry, const void *event_data)
{
	const struct trtl_event_resize *resize =
	    talloc_get_type(event_data, struct trtl_event_resize);
	ck_assert_ptr_nonnull(resize);
	ck_assert_int_eq(resize->new_size.width, 640);
	ck_assert_int_eq(resize->new_size.height, 480);
	ck_assert_ptr_nonnull(resize->turtle);
	ck_assert_int_eq(resize->turtle->events->resize, cry);
	ck_assert_ptr_eq(data, &fixed_pointer);
}

START_TEST(test_event_resize)
{
	struct turtle turtle = {.events = trtl_event_init()};
	VkExtent2D size = {640, 480};

	// Register our handler
	trtl_crier_listen(turtle.events->crier, "trtl_event_resize", event_resize_callback,
			  &fixed_pointer);
	event_resize_callback_fake.custom_fake = event_resize_callback_handler;

	trtl_event_resize(&turtle, size);

	ck_assert_int_eq(event_resize_callback_fake.call_count, 1);

	talloc_free(turtle.events);
}
END_TEST

static void
event_grid_move_callback_handler(void *data, trtl_crier_cry_t cry, const void *event_data)
{
	printf("gfir move %p %d %p\n", data, cry, event_data);
	const struct trtl_event_grid_move *grid_move =
	    talloc_get_type(event_data, struct trtl_event_grid_move);
	ck_assert_ptr_nonnull(grid_move);
	ck_assert_ptr_nonnull(data);
	ck_assert_int_eq(cry, grid_move->turtle->events->grid_move);
	memcpy(data, grid_move, sizeof(struct trtl_event_grid_move));
}

START_TEST(test_event_grid_move_quadrant1)
{
	struct turtle turtle = {.events = trtl_event_init()};
	struct trtl_event_grid_move move;

	// Register our handler
	trtl_crier_listen(turtle.events->crier, "trtl_event_grid_move", event_grid_move_callback,
			  &move);
	event_grid_move_callback_fake.custom_fake = event_grid_move_callback_handler;

	trtl_event_grid_move(&turtle, 1, 7, false);

	ck_assert_int_eq(event_grid_move_callback_fake.call_count, 1);
	ck_assert_int_eq(move.x, 1);
	ck_assert_int_eq(move.y, 7);
	ck_assert_int_eq(move.snap, false);

	talloc_free(turtle.events);
}
END_TEST

START_TEST(test_event_grid_move_quadrant3)
{
	struct turtle turtle = {.events = trtl_event_init()};
	struct trtl_event_grid_move move;

	// Register our handler
	trtl_crier_listen(turtle.events->crier, "trtl_event_grid_move", event_grid_move_callback,
			  &move);
	event_grid_move_callback_fake.custom_fake = event_grid_move_callback_handler;

	trtl_event_grid_move(&turtle, -9, -33, true);

	ck_assert_int_eq(event_grid_move_callback_fake.call_count, 1);
	ck_assert_int_eq(move.x, -9);
	ck_assert_int_eq(move.y, -33);
	ck_assert_int_eq(move.snap, true);

	talloc_free(turtle.events);
}
END_TEST

Suite *
trtl_events_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Events");

	{
		TCase *tc_create = tcase_create("Create");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_event_init);
	}
	{
		TCase *tc_resize = tcase_create("Resize");
		suite_add_tcase(s, tc_resize);

		tcase_add_test(tc_resize, test_event_resize);
	}
	{
		TCase *tc_resize = tcase_create("Grid Move");
		suite_add_tcase(s, tc_resize);

		tcase_add_test(tc_resize, test_event_grid_move_quadrant1);
		tcase_add_test(tc_resize, test_event_grid_move_quadrant3);
	}

	return s;
}

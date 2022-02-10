
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <talloc.h>

#include "blobby.h"
#include "helpers.h"
#include "trtl.h"
#include "vertex.h"

#include "turtle.h"

#include "stringlist.h"
#include "trtl_barriers.h"
#include "trtl_object.h"
#include "trtl_seer.h"
#include "trtl_solo.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"

struct trtl_stringlist *objs_to_load[3] = {NULL};

extern bool frame_buffer_resized;

static void
show_usage(const char *binary)
{
	printf("%s: Simple game\n", binary);
	puts(" --debug | -d   Set debug.  More 'd's more debug.");
	puts(" --help | -h    Show help.");
	puts(" --object | -o [OBJECT] Load a known object");
	puts(" --background | -b [OBJECT] Load a known object as background");
	puts(" --top | -t [OBJECT] Load a known object on the top layer");
}

static void
parse_arguments(int argc, char **argv)
{
	static struct option options[] = {
	    {"debug", no_argument, NULL, 'd'},
	    {"help", no_argument, NULL, 'h'},
	    {"object", required_argument, NULL, 'o'},
	    {"background", required_argument, NULL, 'b'},
	    {"top", required_argument, NULL, 't'},
	    {NULL, 0, NULL, 0},
	};
	int option;

	while ((option = getopt_long(argc, argv, "dho:b:t:", options, NULL)) != -1) {
		switch (option) {
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'd':
			// debug = 1;
			continue;
		case 'b':
			objs_to_load[0] = trtl_stringlist_add(objs_to_load[0], optarg);
			continue;
		case 'o':
			objs_to_load[1] = trtl_stringlist_add(objs_to_load[1], optarg);
			continue;
		case 't':
			objs_to_load[2] = trtl_stringlist_add(objs_to_load[2], optarg);
			continue;
		default:
			show_usage(argv[0]);
			exit(0);
		}
	}
}

static int
load_object_default(struct turtle *turtle)
{
	printf("Loading default objects: Background 'background', Main: 'grid9'\n");
	trtl_seer_predefined_object_add("background", turtle, 0);
	trtl_seer_predefined_object_add("grid9", turtle, 1);
	return 2;
}

int
load_objects(struct turtle *turtle)
{
	int i;

	// Do we have a least one object
	for (i = 0; i < 2; i++) {
		if (objs_to_load[i] != NULL) {
			// Found something;
			break;
		}
	}
	if (i == 2) {
		return load_object_default(turtle);
	}

	// For each layer, load it
	for (i = 0; i < 3; i++) {
		struct trtl_stringlist *load = objs_to_load[i];
		while (load != NULL) {
			trtl_seer_predefined_object_add(load->string, turtle, i);
			load = load->next;
		}
		talloc_free(objs_to_load[i]);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	static const struct trtl_layer_info layers[] = {
	    /* background */ {.has_depth = true, .clear_on_load = true, .strata = TRTL_STRATA_BASE},
	    /* Grid */
	    {.has_depth = true,
	     .clear_on_load = false,
	     .strata = TRTL_STRATA_ORTHOGRAPHIC | TRTL_STRATA_BASE},
	    /* Foreground */
	    {.has_depth = true,
	     .clear_on_load = false,
	     .strata = TRTL_STRATA_ORTHOGRAPHIC | TRTL_STRATA_BASE},
	};
	struct turtle *turtle;

	parse_arguments(argc, argv);

	turtle = turtle_init(TRTL_ARRAY_SIZE(layers), layers);

	load_objects(turtle);

	trtl_main_loop(turtle);

	return 0;
}

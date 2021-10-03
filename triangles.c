
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

struct trtl_stringlist *objs_to_load[TRTL_RENDER_LAYER_TOTAL] = {NULL};

// FIXME: These should be in a nice game state stucture
extern int posX;
extern int posY;
extern int zoom;
extern bool frame_buffer_resized;

void
key_callback(trtl_arg_unused GLFWwindow *window, int key, trtl_arg_unused int scancode, int action,
	     trtl_arg_unused int mods)
{
	if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_RIGHT:
			if (posX < 8) posX++;
			break;
		case GLFW_KEY_LEFT:
			if (posX > 0) posX--;
			break;
		case GLFW_KEY_DOWN:
			if (posY < 8) posY++;
			break;
		case GLFW_KEY_UP:
			if (posY > 0) posY--;
			break;
		case GLFW_KEY_EQUAL:
			zoom *= 2;
			// FIXME: This is a terrible way to regen everything
			frame_buffer_resized = true;
			break;
		case GLFW_KEY_MINUS:
			if (zoom > 32) zoom /= 2;
			frame_buffer_resized = true;
			break;
		}
	}
}



static void
show_usage(const char *binary)
{
	printf("%s: Simple game\n", binary);
	puts(" --debug | -d   Set debug.  More 'd's more debug.");
	puts(" --help | -h    Show help.");
	puts(" --object | -o [OBJECT] Load a known object");
	puts(" --background | -b [OBJECT] Load a known object as background");
}

static void
parse_arguments(int argc, char **argv)
{
	static struct option options[] = {
	    {"debug", no_argument, NULL, 'd'},
	    {"help", no_argument, NULL, 'h'},
	    {"object", required_argument, NULL, 'o'},
	    {"background", required_argument, NULL, 'b'},
	    {NULL, 0, NULL, 0},
	};
	int option;

	while ((option = getopt_long(argc, argv, "dho:b:", options, NULL)) != -1) {
		switch (option) {
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'd':
			// debug = 1;
			continue;
		case 'b':
			objs_to_load[TRTL_RENDER_LAYER_BACKGROUND] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_BACKGROUND], optarg);
			continue;
		case 'o':
			objs_to_load[TRTL_RENDER_LAYER_MAIN] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_MAIN], optarg);
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
	trtl_seer_predefined_object_add("background", turtle, TRTL_RENDER_LAYER_BACKGROUND);
	trtl_seer_predefined_object_add("grid9", turtle, TRTL_RENDER_LAYER_MAIN);
	return 2;
}

int
load_objects(struct turtle *turtle)
{
	int i;

	// Do we have a least one object
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		if (objs_to_load[i] != NULL) {
			// Found something;
			break;
		}
	}
	if (i == TRTL_RENDER_LAYER_TOTAL) {
		return load_object_default(turtle);
	}

	// For each layer, load it
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
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
	struct turtle *turtle;

	parse_arguments(argc, argv);

	turtle = turtle_init();
	
	glfwSetKeyCallback(turtle->window, key_callback);

	// Above here shold be in turtle_init.

	load_objects(turtle);

	trtl_main_loop(turtle);

	return 0;
}

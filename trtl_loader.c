#include <talloc.h>

#include "turtle.h"
#include "trtl_loader.h"
#include "trtl_loader_file.h"

static struct trtl_loader *
add_path(void *ctx, struct trtl_loader *wrapped, const char *path) {
	struct trtl_loader *loader;

	loader = trtl_loader_file(path);
	loader->next = wrapped;
	talloc_steal(ctx, loader);

	return loader;
}

int trtl_loader_init(struct turtle *turtle){
	if (turtle->loader != NULL) {
		error("Loader already allocated\n");
	}
	
	// FIXME: Should include:
	// 	- preinstalled data path
	// 	- current directory
	// 	- environmanet variables
	// 	- command line args

	turtle->loader = add_path(turtle, turtle->loader, ".");
#ifdef TURTLE_DATA_PATH
	turtle->loader = add_path(turtle, turtle->loader, TURTLE_DATA_PATH);
#endif

	return 0;

}

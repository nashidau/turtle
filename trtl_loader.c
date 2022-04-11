
#include "turtle.h"
#include "trtl_loader.h"
#include "trtl_loader_file.h"

int trtl_loader_init(struct turtle *turtle){
	if (turtle->loader != NULL) {
		error("Loader already allocated\n");
		return -1;
	}
	
	// FIXME: Should include:
	// 	- preinstalled data path
	// 	- current directory
	// 	- environmanet variables
	// 	- command line args

	turtle->loader = trtl_loader_file(".");

	return 0;

}

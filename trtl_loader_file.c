
#include "trtl_loader_file.h"
#include "blobby.h"
#include <talloc.h>

struct trtl_loader_file {
	struct trtl_loader loader;
	const char *path;
};

struct blobby *
floader_load(struct trtl_loader *loader, const char *objname)
{
	struct trtl_loader_file *floader = talloc_get_type(loader, struct trtl_loader_file);
	char *path;
	struct blobby *blobby;

	path = talloc_asprintf(NULL, "%s/%s", floader->path, objname);
printf("file loader loading: %s\n", path);
	if (!path) {
		return NULL;
	}

	blobby = blobby_from_file(path);
	if (blobby) {
		blobby->source = talloc_steal(blobby, path);
		return blobby;
	}
	talloc_free(path);

	if (loader->next) {
		return loader->next->load(loader->next, objname);
	}
	return NULL;
}

struct trtl_loader *
trtl_loader_file(const char *path)
{
	struct trtl_loader_file *floader;

	if (!path) {
		return NULL;
	}

	floader = talloc_zero(NULL, struct trtl_loader_file);
	floader->path = talloc_strdup(floader, path);
	floader->loader.load = floader_load;

	return &floader->loader;
}

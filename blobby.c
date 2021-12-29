#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <talloc.h>

#include "helpers.h"
#include "blobby.h"

/**
 * Allocates and returns a blobby from a file.
 *
 * @param path to file
 * @return blobby or NULL on error
 */
struct blobby *
blobby_from_file_ctx(void *ctx, const char *path)
{
	struct stat fileinfo;
	struct blobby *blobby;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return NULL;
	}

	if (fstat(fd, &fileinfo) != 0) {
		close(fd);
		perror("fstat");
		return NULL;
	}

	blobby = talloc(ctx, struct blobby);
	blobby->len = fileinfo.st_size;
	blobby->data = talloc_size(blobby, blobby->len);

	if (read(fd, (void *)(blobby->data), blobby->len) != (ssize_t)blobby->len) {
		perror("read");
		close(fd);
		talloc_free(blobby);
		return NULL;
	}

	close(fd);
	return blobby;
}

struct blobby *
blobby_from_file(const char *path)
{
	return blobby_from_file_ctx(NULL, path);
}

struct blobby *
blobby_from_string(const char *string) {
	struct blobby *blobby;

	blobby = talloc(NULL, struct blobby);
	blobby->len = strlen(string);
	blobby->data = talloc_strdup(blobby, string);
	return blobby;
}

static char *
make_safe_str(const char *prefix, const char *path)
{
	char *dest;
	dest = talloc_asprintf(NULL, "shader_%s_%s", prefix, path);
	for (size_t i = 0; dest[i]; i++) {
		if (dest[i] == '/' || dest[i] == '-' || dest[i] == '.') dest[i] = '_';
	}
	return dest;
}

/** Generate a blobby from a symbol.  Uses dlopen to find the symbol
 */
struct blobby *
blobby_binary(const char *path)
{
	struct blobby *blobby = talloc(NULL, struct blobby);
	char *blob_start = talloc_steal(blobby, make_safe_str(path, "start"));
	char *blob_end = talloc_steal(blobby, make_safe_str(path, "end"));

	char *start = dlsym(RTLD_DEFAULT, blob_start);
	char *end = dlsym(RTLD_DEFAULT, blob_end);

	if (start == NULL || end == NULL) {
		return NULL;
	}

	blobby->data = start;
	blobby->len = end - start;

	return blobby;
}

/** End Blobby */

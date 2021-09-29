#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <talloc.h>

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

static char *
make_safe_str(const char *prefix, const char *path)
{
	char *dest;
	dest = talloc_asprintf(NULL, "data_%s_%s", prefix, path);
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
	char *blob_start = talloc_steal(blobby, make_safe_str("start", path));
	char *blob_end = talloc_steal(blobby, make_safe_str("end", path));

	char *start = dlsym(RTLD_SELF, blob_start);
	char *end = dlsym(RTLD_SELF, blob_end);

	blobby->len = end - start;
	blobby->data = start;

	return blobby;
}

/** End Blobby */

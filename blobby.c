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
blobby_from_file_ctx(void *ctx, const char *path) {
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

	if (read(fd, (void*)(blobby->data), blobby->len) != (ssize_t)blobby->len) {
		perror("read");
		close(fd);
		talloc_free(blobby);
		return NULL;
	}

	close(fd);
	return blobby;
}

struct blobby *
blobby_from_file(const char *path) {
	return blobby_from_file_ctx(NULL, path);
}

/** End Blobby */

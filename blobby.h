
#include <stddef.h>

struct blobby {
	const char *source;
	size_t len;
	const char *data;
};

struct blobby *blobby_from_file(const char *path);
struct blobby *blobby_from_file_ctx(void *ctx, const char *path);
struct blobby *blobby_binary(const char *path);
struct blobby *blobby_from_string(const char *string);

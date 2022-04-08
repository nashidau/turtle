#pragma once

struct blobby;
struct trtl_loader {
	struct blobby *(*load)(struct trtl_loader *loader, const char *objname);
	struct trtl_loader *next;
};

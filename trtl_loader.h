#pragma once

struct turtle;
struct blobby;
struct trtl_loader {
	struct blobby *(*load)(struct trtl_loader *loader, const char *objname);
	struct trtl_loader *next;
};


int trtl_loader_init(struct turtle *turtle);

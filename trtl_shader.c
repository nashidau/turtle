/**
 * Simple shader cache model.
 *
 * Free the trtl_shader to relaeas info.
 */
#include <string.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "blobby.h"
#include "helpers.h"
#include "trtl_shader.h"
#include "turtle.h"

static VkShaderModule create_shader(VkDevice device, struct blobby *blobby);

// FIXME: Use something better then a linked list.
struct shader_node {
	struct shader_node *next;
	const char *path;
	VkShaderModule shader;

	int count;
};

struct trtl_shader_cache {
	struct shader_node *shaders;
	VkDevice device;
};

struct trtl_shader_cache *
trtl_shader_cache_init(struct turtle *turtle){
	struct trtl_shader_cache *shader_cache;

	shader_cache = talloc_zero(turtle, struct trtl_shader_cache);

	shader_cache->shaders = NULL;
	shader_cache->device = turtle->device;

	return shader_cache;
}

static int
shader_destory(struct trtl_shader *shader) {
	struct shader_node *node = shader->internal;
	
	node->count --;

	// FIXME: Free the shader module
	return 0;
}

// given a shader node create a shader object and set it's destructor
static struct trtl_shader *
shader_create(struct shader_node *node) {
	// Owner should go to the parent object.
	struct trtl_shader *shader = talloc_zero(NULL, struct trtl_shader);
	shader->shader = node->shader;
	shader->path = node->path;
	shader->internal = node;

	node->count ++;
	
	talloc_set_destructor(shader, shader_destory);

	return shader;
}

struct trtl_shader *
trtl_shader_get(struct turtle *turtle, const char *path)
{
	struct trtl_shader_cache *shader_cache = turtle->shader_cache;

	for (struct shader_node *node = shader_cache->shaders ; node ; node = node->next) {
		if (streq(node->path, path)) {
			return shader_create(node);	
		}
	}

	// Allocate and return
	struct shader_node *node = talloc_zero(NULL, struct shader_node);
	node->path = talloc_strdup(node, path);
	node->next = shader_cache->shaders;
	shader_cache->shaders = node;
	node->count = 1;
	node->shader = create_shader(turtle->device, blobby_binary(path));

	return shader_create(node);
}

static VkShaderModule
create_shader(VkDevice device, struct blobby *blobby)
{
	VkShaderModule shader_module;

	if (!blobby) return 0;

	VkShaderModuleCreateInfo shader_info = {0};
	shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.codeSize = blobby->len;
	shader_info.pCode = (void *)blobby->data;

	if (vkCreateShaderModule(device, &shader_info, NULL, &shader_module) != VK_SUCCESS) {
		error("Create shader Module\n");
	}

	talloc_free(blobby);

	return shader_module;
}

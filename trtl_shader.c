/**
 * Simple shader cache model.
 *
 * Free the trtl_shader to relaeas info.
 */
#include <string.h>

#include <vulkan/vulkan_core.h>

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

static struct shader_node *shaders;
VkDevice device;

int
trtl_shader_init(struct turtle *turtle)
{
	shaders = NULL;
	device = turtle->device;
	return 0;
}

static int
shader_destory(struct trtl_shader *shader) {
	for (struct shader_node *node = shaders ; node ; node = node->next) {
		if (streq(node->path, shader->path)) {
			// found it:
			node->count --;
		}
	}
	return 0;
}

// given a shader node create a shader object and set it's destructor
static struct trtl_shader *
shader_create(struct shader_node *node) {
	// Ownwe should go to the parent object.
	struct trtl_shader *shader = talloc_zero(NULL, struct trtl_shader);
	shader->shader = node->shader;
	shader->path = node->path;

	node->count ++;
	
	talloc_set_destructor(shader, shader_destory);

	return shader;
}

struct trtl_shader *
trtl_shader_get(const char *path)
{
	for (struct shader_node *node = shaders ; node ; node = node->next) {
		if (streq(node->path, path)) {
			return shader_create(node);	
		}
	}

	// Allocate and return
	struct shader_node *node = talloc_zero(NULL, struct shader_node);
	node->path = talloc_strdup(node, path);
	node->next = shaders;
	shaders = node;
	node->count = 1;
	node->shader = create_shader(device, blobby_from_file(path));

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

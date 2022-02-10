/**
 * Turtle Seer; manages objects.
 *
 * Manages a group of objects.   Functions as a factor as well as keeping track of
 * resources used by objects.
 *
 * Created as a singleton.
 *
 * FIXME: Needs to holder the rendering pipeline
 * FIXME: Should hold the trtl_uniform objects.
 */
#include <assert.h>
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object.h"
#include "trtl_object_canvas.h"
#include "trtl_object_grid.h"
#include "trtl_object_mesh.h"
#include "trtl_seer.h"
#include "trtl_shader.h"
#include "trtl_strata.h"
#include "trtl_strata_base.h"
#include "trtl_strata_grid.h"
#include "turtle.h"

// FIXME: These should be in some loaded metadata
static const trtl_arg_unused char *MODEL_PATH = "models/viking_room.obj";
static const trtl_arg_unused char *TEXTURE_PATH = "textures/viking_room.png";
// Loads; but wrong size
// static const char *MODEL_PATH = "models/bulborb/source/TenKochappy.obj";
// static const char *TEXTURE_PATH = "models/bulborb/textures/kochappy_tex.png";

static const trtl_arg_unused char *MODEL_PATH2 = "models/StreetCouch/Day143.obj";
static const trtl_arg_unused char *TEXTURE_PATH2 = "models/StreetCouch/textures/texture.jpg";

static const trtl_arg_unused char *DUCK_PATH = "models/Rubber_Duck/10602_Rubber_Duck_v1_L3.obj";
static const trtl_arg_unused char *DUCK_TEXTURE_PATH =
    "models/Rubber_Duck/10602_Rubber_Duck_v1_diffuse.jpg";

struct trtl_seer {
	struct turtle *turtle;

	// Number of objects across all layers
	uint32_t nobjects;

	trtl_render_layer_t nlayers;
	struct trtl_layer *layers;

	VkExtent2D size;

	uint32_t nframebuffers;
	VkFramebuffer *framebuffers;

	struct trtl_strata *base;
	struct trtl_strata *grid;
};

static VkRenderPass create_render_pass(struct turtle *turtle, const struct trtl_layer_info *info);
VkCommandBuffer *create_command_buffers(struct turtle *turtle, struct trtl_swap_chain *scd,
					VkCommandPool command_pool, VkFramebuffer *framebuffers);
static VkFramebuffer *create_frame_buffers(VkDevice device, struct trtl_swap_chain *scd,
					   VkRenderPass render_pass, VkExtent2D extent);

static int
seer_destroy(struct trtl_seer *seer)
{
	printf("Seer destroy %p %p \n", seer, seer->turtle);

	// For each layer; destroy our objects
	for (trtl_render_layer_t i = 0; i < seer->nlayers; i++) {
		struct trtl_layer *layer = seer->layers + i;
		for (uint32_t obj = 0; obj < layer->nobjects; obj++) {
			talloc_free(layer->objects[obj]);
		}
	}

	for (uint32_t i = 0; i < seer->nframebuffers; i++) {
		vkDestroyFramebuffer(seer->turtle->device, seer->framebuffers[i], NULL);
	}

	seer->framebuffers = 0;
	return 0;
}

struct trtl_seer *
trtl_seer_init(struct turtle *turtle, VkExtent2D extent, trtl_render_layer_t nlayers,
	       const struct trtl_layer_info *layer_info)
{
	struct trtl_seer *seer;

	if (turtle->seer) {
		warning("Multiple init of trtl_seer\n");
		return 0;
	}

	if (nlayers < 1 || nlayers > 10) {
		error("Invalid layers param");
	}

	seer = talloc_zero(turtle, struct trtl_seer);
	talloc_set_destructor(seer, seer_destroy);

	seer->layers = talloc_zero_array(seer, struct trtl_layer, nlayers);
	seer->nlayers = nlayers;
	seer->size = extent;

	seer->turtle = turtle;

	for (trtl_render_layer_t i = 0; i < seer->nlayers; i++) {
		seer->layers[i].render_pass = create_render_pass(turtle, layer_info + i);
		seer->layers[i].rect = (VkRect2D){{0, 0}, {extent.width, extent.height}};
	}

	// Create our strata
	seer->base = trtl_strata_base_init(turtle);
	seer->grid = trtl_strata_grid_init(turtle);

	return seer;
}

int
trtl_seer_resize(VkExtent2D new_size, struct turtle *turtle)
{
	struct trtl_seer *seer = turtle->seer;
	seer->size = new_size;

	// recreate pipelines

	for (trtl_render_layer_t i = 0; i < seer->nlayers; i++) {
		struct trtl_layer *layer = seer->layers + i;
		layer->rect.extent = new_size;
		for (uint32_t obj = 0; obj < layer->nobjects; obj++) {
			if (layer->objects[obj]->relayer) {
				layer->objects[obj]->relayer(layer->objects[obj], turtle, layer);
			}
		}
	}

	// grid->pipeline_info = trtl_pipeline_create(
	//  scd->render->turtle->device, render_pass, extent, descriptor_set_layout,

	// create_descriptor_pool()

	// create comand pool
	//   create_command_pool

	// depth resources
	// create_depth_resources()
	return 0;
}

/**
 * Add a trtl_object into the seer system.
 *
 * Adds a single object into seer at the specfied layer.
 * To remove the object, either delete it, or call trtl_seer_object_remove().
 *
 * @param turtle Turtle system.
 * @param object Object to add.
 * @param layerid trtl_render_layer_t to add too.
 * @return 0 on success, -1 on error.
 */
int
trtl_seer_object_add(struct turtle *turtle, struct trtl_object *object, trtl_render_layer_t layerid)
{
	struct trtl_seer *seer;
	struct trtl_layer *layer;

	if (turtle == NULL || object == NULL) {
		return -1;
	}
	seer = turtle->seer;

	if (layerid >= seer->nlayers) {
		error("Invalid layer for %p", object);
	}

	seer->nobjects++;
	layer = seer->layers + layerid;

	if (layer->nobjects >= layer->nalloced) {
		struct trtl_object **objs;
		if (layer->nalloced == 0)
			layer->nalloced = 2;
		else
			layer->nalloced *= 2;
		objs = talloc_realloc(NULL, layer->objects, struct trtl_object *, layer->nalloced);
		if (objs == NULL) {
			talloc_free(object);
			error("Could not resize object array");
		}
		layer->objects = objs;
	}

	layer->objects[layer->nobjects++] = object;

	talloc_steal(seer, object);
	object->relayer(object, turtle, layer);

	return 0;
}

struct trtl_object *
trtl_seer_predefined_object_add(const char *name, struct turtle *turtle,
				trtl_render_layer_t layerid)
{
	struct trtl_object *object = NULL;
	struct trtl_seer *seer;

	seer = turtle->seer;
	assert(turtle->seer);

	if (layerid >= seer->nlayers) {
		error("Invalid layer for %s", name);
	}

	// FIXME: This is super unscalable.  Shoudl have a DB or a way to search filesystem
	if (streq(name, "couch")) {
		printf("Couch diesabled\n");
		// object = trtl_object_mesh_create(
		//   seer.seer_ctx, scd, seer.layers[layerid].render_pass, scd->extent,
		// scd->render->descriptor_set_layout, MODEL_PATH2, TEXTURE_PATH2);
	} else if (streq(name, "room")) {
		object = trtl_object_mesh_create(turtle, MODEL_PATH, TEXTURE_PATH);
	} else if (streq(name, "duck")) {
		object = trtl_object_mesh_create_scaled(turtle, DUCK_PATH, DUCK_TEXTURE_PATH, 0.02);
	} else if (streq(name, "background")) {
		object = trtl_canvas_create(turtle, NULL);
	} else if (strncmp(name, "background:", 11) == 0) {
		object = trtl_canvas_create(turtle, name + 11);
	} else if (streq(name, "grid")) {
		object = trtl_grid_create(turtle);
		trtl_grid_fill_rectangle(object, 3, 5);
	} else if (streq(name, "grid1")) {
		object = trtl_grid_create(turtle);
		trtl_grid_fill_rectangle(object, 1, 1);
	} else if (streq(name, "grid9")) {
		object = trtl_grid_create(turtle);
		trtl_grid_fill_rectangle(object, 9, 9);
	} else {
		error("Unknown object %s\n", name);
	}

	if (!object) return NULL;;

	// FIXME: Check return
	trtl_seer_object_add(turtle, object, layerid);

	return object;
}

/**
 * Update all known objects for the current frame / time.
 *
 * Runs any update functions for each object.   Running the update function advances the object one
 * time step, whatever that may be.  An object does not need to rendered between update steps.
 *
 * FIXME: update the description about the side.
 *
 * @param image_index The Index of the current frame.
 * @retuen Number of objects updated.
 */
int
trtl_seer_update(struct turtle *turtle, uint32_t image_index)
{
	int count = 0;
	struct trtl_seer *seer = turtle->seer;

	// update strata
	seer->base->update(seer->base, image_index);
	seer->grid->update(seer->grid, image_index);

	// Update objects
	for (trtl_render_layer_t layerid = 0; layerid < seer->nlayers; layerid++) {
		struct trtl_layer *layer = seer->layers + layerid;
		for (uint32_t i = 0; i < layer->nobjects; i++) {
			layer->objects[i]->update(layer->objects[i], image_index);
			count++;
		}
	}

	return count;
}

trtl_must_check VkCommandBuffer *
trtl_seer_create_command_buffers(struct turtle *turtle, VkCommandPool command_pool)
{
	struct trtl_seer *seer = turtle->seer;
	// FIXME: What the hell is this 1
	seer->framebuffers = create_frame_buffers(turtle->device, turtle->tsc,
						  seer->layers[1].render_pass, seer->size);
	seer->nframebuffers = turtle->tsc->nimages;
	return create_command_buffers(turtle, turtle->tsc, command_pool, seer->framebuffers);
}

int
trtl_seer_draw(struct turtle *turtle, VkCommandBuffer buffer, trtl_render_layer_t layerid)
{
	uint32_t offset = 0;
	struct trtl_seer *seer;

	seer = turtle->seer;

	assert(layerid < seer->nlayers);

	struct trtl_layer *layer = seer->layers + layerid;

	for (uint32_t obj = 0; obj < layer->nobjects; obj++) {
		layer->objects[obj]->draw(layer->objects[obj], buffer, offset);
	}

	return 0;
}

// FIXME: Urgh; put this somewhere sane
VkFormat find_depth_format(VkPhysicalDevice physical_device);

static VkRenderPass
create_render_pass(struct turtle *turtle, const struct trtl_layer_info *layerinfo)
{
	VkRenderPass render_pass;

	// Background color:
	VkAttachmentDescription colorAttachment = {0};
	colorAttachment.format = turtle->image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	if (layerinfo->clear_on_load) {
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	} else {
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	}
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {0};
	if (layerinfo->has_depth) {
		depthAttachment.format = find_depth_format(turtle->physical_device);
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//	if (layerinfo->clear_on_load) {
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//	} else {
		///		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		///	}
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	VkAttachmentReference colorAttachmentRef = {0};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {0};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	/*
	VkSubpassDescription subpass_background = {0};
	subpass_background.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_background.colorAttachmentCount = 1;
	subpass_background.pColorAttachments = &colorAttachmentRef;
	subpass_background.pDepthStencilAttachment = NULL;
*/
	VkSubpassDescription subpass_main = {0};
	subpass_main.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_main.colorAttachmentCount = 1;
	subpass_main.pColorAttachments = &colorAttachmentRef;
	if (layerinfo->has_depth) {
		subpass_main.pDepthStencilAttachment = &depthAttachmentRef;
	}

	/*
	VkSubpassDependency dependencies[2] = {0};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
				       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
				       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask =
	    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
				       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
				       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[1].dstAccessMask =
	    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
*/

	VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};

	VkSubpassDescription subpasses[] = {subpass_main};

	VkRenderPassCreateInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	// FIXME: Uhh.. so ugly
	renderPassInfo.attachmentCount = layerinfo->has_depth ? 2 : 1;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = subpasses;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = NULL; // dependencies;

	if (vkCreateRenderPass(turtle->device, &renderPassInfo, NULL, &render_pass) != VK_SUCCESS) {
		error("failed to create render pass!");
	}

	return render_pass;
}

VkCommandBuffer *
create_command_buffers(struct turtle *turtle, struct trtl_swap_chain *scd,
		       VkCommandPool command_pool, VkFramebuffer *framebuffers)
{
	VkCommandBuffer *buffers;

	scd->nbuffers = scd->nimages;

	buffers = talloc_array(scd, VkCommandBuffer, scd->nbuffers);

	VkCommandBufferAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = command_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = scd->nimages;

	if (vkAllocateCommandBuffers(turtle->device, &allocInfo, buffers) != VK_SUCCESS) {
		error("failed to allocate command buffers!");
	}

	for (size_t i = 0; i < scd->nimages; i++) {
		VkCommandBufferBeginInfo beginInfo = {0};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(buffers[i], &beginInfo) != VK_SUCCESS) {
			error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo = {0};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = framebuffers[i];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = scd->extent;

		VkClearValue clearValues[] = {
		    {.color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}}},
		    {.depthStencil = (VkClearDepthStencilValue){1.0f, 0}},
		};
		renderPassInfo.clearValueCount = TRTL_ARRAY_SIZE(clearValues);
		renderPassInfo.pClearValues = clearValues;

		// FIXME: Should get render pass for each layer here
		for (trtl_render_layer_t li = 0; li < turtle->seer->nlayers; li++) {
			if (turtle->seer->layers[li].nobjects == 0) continue;

			renderPassInfo.renderPass = turtle->seer->layers[li].render_pass;

			vkCmdBeginRenderPass(buffers[i], &renderPassInfo,
					     VK_SUBPASS_CONTENTS_INLINE);
			{
				trtl_seer_draw(turtle, buffers[i], li);
			}
			vkCmdEndRenderPass(buffers[i]);
		}

		if (vkEndCommandBuffer(buffers[i]) != VK_SUCCESS) {
			error("failed to record command buffer!");
		}
	}

	return buffers;
}

static VkFramebuffer *
create_frame_buffers(VkDevice device, struct trtl_swap_chain *scd, VkRenderPass render_pass,
		     VkExtent2D extent)
{
	VkFramebuffer *framebuffers;

	framebuffers = talloc_array(scd, VkFramebuffer, scd->nimages);

	for (uint32_t i = 0; i < scd->nimages; i++) {
		VkImageView attachments[] = {
		    scd->image_views[i],
		    scd->depth_image_view,
		};

		VkFramebufferCreateInfo framebufferInfo = {0};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = render_pass;
		framebufferInfo.attachmentCount = TRTL_ARRAY_SIZE(attachments);
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = extent.width;
		framebufferInfo.height = extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]) !=
		    VK_SUCCESS) {
			error("failed to create framebuffer!");
		}
	}

	return framebuffers;
}

struct trtl_strata *
trtl_seer_strata_get(struct turtle *turtle, const char *name)
{
	if (streq(name, "base")) {
		return turtle->seer->base;
	} else if (streq(name, "grid")) {
		return turtle->seer->grid;
	}

	assert(!name);
	return NULL;
}

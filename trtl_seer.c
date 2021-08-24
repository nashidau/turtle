/**
 * Turtle Seer; manages objects.
 *
 * Manages a group of objects.   Functions as a factor as well as keeping track of
 * resources used by objects.
 *
 * Created as a singleton.  Has an evil global.
 *
 * FIXME: Needs to holder the rendering pipeline
 * FIXME: Should hold the trtl_uniform objects.
 */
#include <assert.h>
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object_canvas.h"
#include "trtl_object_mesh.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "turtle.h"

// FIXME: These should be in some loaded metadata
static const char *MODEL_PATH = "models/viking_room.obj";
static const char *TEXTURE_PATH = "textures/viking_room.png";
// Loads; but wrong size
// static const char *MODEL_PATH = "models/bulborb/source/TenKochappy.obj";
// static const char *TEXTURE_PATH = "models/bulborb/textures/kochappy_tex.png";

static const char *MODEL_PATH2 = "models/StreetCouch/Day143.obj";
static const char *TEXTURE_PATH2 = "models/StreetCouch/textures/texture.jpg";

struct {
	void *seer_ctx;

	struct turtle *turtle;

	// Number of objects across all layers
	uint32_t nobjects;

	trtl_render_layer_t nlayers;
	struct objlayer *layers;

	VkFramebuffer *framebuffers;

} seer = {0};

struct objlayer {
	uint32_t nobjects;
	uint32_t nalloced;
	struct trtl_object **objects;

	struct trtl_pipeline_info pipeline_info;

	VkRenderPass render_pass;
};

static VkRenderPass create_render_pass(struct turtle *turtle);
VkCommandBuffer *create_command_buffers(struct turtle *turtle, struct swap_chain_data *scd,
					VkCommandPool command_pool, VkFramebuffer *framebuffers);
static VkFramebuffer *create_frame_buffers(VkDevice device, struct swap_chain_data *scd,
					   VkRenderPass render_pass, VkExtent2D extent);

int
trtl_seer_init(struct turtle *turtle, VkExtent2D extent,
	       VkDescriptorSetLayout descriptor_set_layout)
{
	int nlayers = TRTL_RENDER_LAYER_TOTAL;

	if (seer.turtle) {
		warning("Multiple init of trtl_seer");
		return 0;
	}

	if (nlayers < 1 || nlayers > 10) {
		error("Invalid layers param");
	}

	seer.seer_ctx = talloc_init("Turtle Seer Context");

	seer.layers = talloc_zero_array(seer.seer_ctx, struct objlayer, nlayers);
	seer.nlayers = nlayers;

	seer.turtle = turtle;

	for (trtl_render_layer_t i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		seer.layers[i].render_pass = create_render_pass(turtle);

		seer.layers[i].pipeline_info = trtl_pipeline_create(
		    turtle->device, seer.layers[i].render_pass, extent, descriptor_set_layout,
		    "shaders/vert.spv", "shaders/canvas/test-color-fill.spv");
	}

	return 0;
}

int
trtl_seer_object_add(const char *name, struct swap_chain_data *scd, trtl_render_layer_t layerid)
{
	struct trtl_object *object;
	struct objlayer *layer;

	if (layerid >= seer.nlayers) {
		error("Invalid layer for %s", name);
	}

	// FIXME: This is super unscalable.  Shoudl have a DB or a way to search filesystem
	if (streq(name, "couch")) {
		object = trtl_object_mesh_create(seer.seer_ctx, scd, MODEL_PATH2, TEXTURE_PATH2);
	} else if (streq(name, "room")) {
		object = trtl_object_mesh_create(seer.seer_ctx, scd, MODEL_PATH, TEXTURE_PATH);
	} else if (streq(name, "background")) {
		object = trtl_canvas_create(seer.seer_ctx, scd, NULL);
	} else {
		error("Unknown object %s\n", name);
		return -1;
	}

	seer.nobjects++;

	layer = seer.layers + layerid;

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
			return -1;
		}
		layer->objects = objs;
	}

	layer->objects[layer->nobjects++] = object;

	return 0;
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
trtl_seer_update(uint32_t image_index)
{
	int count = 0;
	for (trtl_render_layer_t layerid = 0; layerid < TRTL_RENDER_LAYER_TOTAL; layerid++) {
		struct objlayer *layer = seer.layers + layerid;
		for (uint32_t i = 0; i < layer->nobjects; i++) {
			layer->objects[i]->update(layer->objects[i], image_index);
			count++;
		}
	}

	return count;
}

VkCommandBuffer *
trtl_seer_create_command_buffers(struct swap_chain_data *scd, VkCommandPool command_pool)
{
	seer.framebuffers =
	    create_frame_buffers(seer.turtle->device, scd, seer.layers[1].render_pass, scd->extent);
	return create_command_buffers(seer.turtle, scd, command_pool, seer.framebuffers);
}

int
trtl_seer_draw(VkCommandBuffer buffer, struct swap_chain_data *scd, trtl_render_layer_t layerid)
{
	uint32_t offset = 0;
	uint32_t last;

	assert(layerid < seer.nlayers);

	/*
	seer.command_buffers = create_command_buffers(render, scd, scd->render_pass,
						      scd->command_pool, scd->framebuffers);
*/

	struct objlayer *layer = seer.layers + layerid;

	vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layer->pipeline_info.pipeline);
	VkDeviceSize offsets[2] = {0, 1};

	vkCmdBindVertexBuffers(buffer, 0, 1, scd->render->vertex_buffers, offsets);
	vkCmdBindIndexBuffer(buffer, scd->render->index_buffers[0], 0, VK_INDEX_TYPE_UINT32);

	for (uint32_t obj = 0; obj < layer->nobjects; obj++) {
		layer->objects[obj]->draw(layer->objects[obj], buffer,
					  layer->pipeline_info.pipeline_layout, offset);
		// How much is the thing offset by
		// FIXME: this should be part of the layer info
		layer->objects[obj]->indices(layer->objects[obj], NULL, &last);
		offset += last;
	}

	return 0;
}

/**
 * FIXME: Tag this as acquires memory that needs to be referenced or freed.
 * Some sort of sparse acquires tag should do the job.
 */
trtl_alloc struct trtl_seer_vertexset *
trtl_seer_vertexes_get(trtl_render_layer_t layer, uint32_t *nobjects, uint32_t *nvertexes)
{
	struct trtl_seer_vertexset *vertices;
	uint32_t verti;

	assert(layer < TRTL_RENDER_LAYER_TOTAL);
	if (layer >= TRTL_RENDER_LAYER_TOTAL) {
		warning("Invalid layer %d", layer);
		return NULL;
	}

	vertices = talloc_zero_array(NULL, struct trtl_seer_vertexset, seer.nobjects);
	*nvertexes = 0;
	*nobjects = 0;
	verti = 0;
	struct objlayer *lp = seer.layers + layer;
	for (uint32_t i = 0; i < lp->nobjects; i++, verti++) {
		vertices[verti].nvertexes =
		    lp->objects[i]->vertices(lp->objects[i], &vertices[verti].vertices);
		*nvertexes += vertices[verti].nvertexes;
		printf("%s: layer %d: %d +%d = %d\n", __FUNCTION__, i, verti,
		       vertices[verti].nvertexes, *nvertexes);
	}
	*nobjects += lp->nobjects;
	printf("%s:%d vertices %d objects\n", __FUNCTION__, *nvertexes, lp->nobjects);

	return vertices;
}

struct trtl_seer_indexset *
trtl_seer_indexset_get(trtl_render_layer_t layer, uint32_t *nobjects, uint32_t *nindexes)
{
	struct trtl_seer_indexset *indexes;

	indexes = talloc_zero_array(NULL, struct trtl_seer_indexset, seer.nobjects);
	*nobjects = 0;
	*nindexes = 0;

	struct objlayer *lp = seer.layers + layer;
	for (uint32_t i = 0; i < lp->nobjects; i++) {
		indexes[i].nindexes = lp->objects[i]->indices(lp->objects[i], &indexes[i].indexes,
							      &indexes[i].indexrange);
		*nindexes += indexes[i].nindexes;
	}
	*nobjects += lp->nobjects;

	return indexes;
}

// FIXME: Urgh; put this somewhere sane
VkFormat find_depth_format(VkPhysicalDevice physical_device);

static VkRenderPass
create_render_pass(struct turtle *turtle)
{
	VkRenderPass render_pass;

	// Background color:
	VkAttachmentDescription colorAttachment = {0};
	colorAttachment.format = turtle->image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {0};
	depthAttachment.format = find_depth_format(turtle->physical_device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {0};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {0};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_background = {0};
	subpass_background.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_background.colorAttachmentCount = 1;
	subpass_background.pColorAttachments = &colorAttachmentRef;
	subpass_background.pDepthStencilAttachment = NULL;

	VkSubpassDescription subpass_main = {0};
	subpass_main.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_main.colorAttachmentCount = 1;
	subpass_main.pColorAttachments = &colorAttachmentRef;
	subpass_main.pDepthStencilAttachment = &depthAttachmentRef;

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

	VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};

	VkSubpassDescription subpasses[] = {subpass_background, subpass_main};

	VkRenderPassCreateInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = TRTL_ARRAY_SIZE(attachments);
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 2;
	renderPassInfo.pSubpasses = subpasses;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	if (vkCreateRenderPass(turtle->device, &renderPassInfo, NULL, &render_pass) != VK_SUCCESS) {
		error("failed to create render pass!");
	}

	return render_pass;
}

VkCommandBuffer *
create_command_buffers(struct turtle *turtle, struct swap_chain_data *scd,
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

		// FIXME: Should get render pass for each layer here

		VkRenderPassBeginInfo renderPassInfo = {0};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = seer.layers[1].render_pass;
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

		vkCmdBeginRenderPass(buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// trtl_seer_draw(buffers[i], scd, 0);

			// vkCmdNextSubpass(buffers[i], VK_SUBPASS_CONTENTS_INLINE);

			trtl_seer_draw(buffers[i], scd, 1);
		}
		vkCmdEndRenderPass(buffers[i]);

		if (vkEndCommandBuffer(buffers[i]) != VK_SUCCESS) {
			error("failed to record command buffer!");
		}
	}

	return buffers;
}

static VkFramebuffer *
create_frame_buffers(VkDevice device, struct swap_chain_data *scd, VkRenderPass render_pass,
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

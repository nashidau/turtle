
#include <assert.h>

#include <talloc.h>

#include "turtle.h"
#include "trtl_layer.h"
#include "trtl_vulkan.h"

struct trtl_layer {
	uint32_t nobjects;
	uint32_t nalloced;
	struct trtl_object **objects;

	VkRenderPass render_pass;
};


static VkRenderPass
create_render_pass(struct turtle *turtle, const struct trtl_layer_info *layerinfo);

VkRenderPass
trtl_layer_info_render_pass(struct trtl_layer *layer)
{
	return 0;
}
int
trtl_layer_info_strata_count(struct trtl_layer *layer)
{
	return 1;
}
VkDescriptorSetLayoutBinding
trtl_layer_info_strata(struct trtl_layer *layer, int stata)
{
	VkDescriptorSetLayoutBinding binding = {0};
	return binding;
}



struct trtl_layer *
trtl_layer_init(struct turtle *turtle, struct trtl_layer_info *layer_info, int nlayers) {
	struct trtl_layer *layers;

	layers = talloc_zero_array(NULL, struct trtl_layer, nlayers);
	assert(layers);

	for (trtl_render_layer_t i = 0; i < nlayers; i++) {
		layers[i].render_pass = create_render_pass(turtle, layer_info + i);
	}

	return layers;
}


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

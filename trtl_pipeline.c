/**
 * Turtle Pipeline.
 *
 * Creates and manages pipeline objects.
 *
 *
 * Sets specialisation data:
 * 	0 is width
 * 	1 is height (both floats)
 *
 */
#include <assert.h>
#include <string.h>

#include <talloc.h>

#include "vulkan/vulkan_core.h"

#include "helpers.h"
#include "trtl_object_canvas.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shader.h"
#include "trtl_strata.h"
#include "turtle.h"
#include "vertex.h"

#include "blobby.h"

// FIXME: Is this freed correctly?
trtl_alloc static VkSpecializationInfo *
set_specialise_info(VkExtent2D *extent)
{
	VkSpecializationInfo *info = talloc_zero(NULL, VkSpecializationInfo);
	info->mapEntryCount = 2;
	int *data = talloc_zero_array(info, int, info->mapEntryCount);
	VkSpecializationMapEntry *entries =
	    talloc_zero_array(info, VkSpecializationMapEntry, info->mapEntryCount);
	info->pMapEntries = entries;

	// Width
	entries[0].constantID = 0;
	entries[0].size = sizeof(int);
	entries[0].offset = 0;
	data[0] = extent->width;

	// Height
	entries[1].constantID = 1;
	entries[1].size = sizeof(int);
	entries[1].offset = sizeof(int);
	data[1] = extent->height;

	info->dataSize = entries[1].offset + entries[1].size;
	info->pData = data;
	return info;
}

static int
pipeline_destroy(struct trtl_pipeline_info *info)
{
	// free the pipeline
	vkDestroyPipeline(info->device, info->pipeline, NULL);
	vkDestroyPipelineLayout(info->device, info->pipeline_layout, NULL);
	return 0;
}

/*
 * FIXME: We creeate a pipeline layout and a pipeline here.  The layout is pretty much the same so
 * we should create it once then reuse it.  A pipeline layout creator with the shaders and other
 * attachments added.
 */
struct trtl_pipeline_info *
trtl_pipeline_create(struct turtle *turtle, VkRenderPass render_pass, VkExtent2D extent,
		     VkDescriptorSetLayout descriptor_set_layout, const char *vertex_shader,
		     const char *fragment_shader,
		     const VkVertexInputBindingDescription *binding_description,
		     const VkVertexInputAttributeDescription *attribute_description,
		     uint32_t nattributes, bool layer_blend)
{
	struct trtl_pipeline_info *info = talloc_zero(turtle, struct trtl_pipeline_info);
	info->device = turtle->device;
	talloc_set_destructor(info, pipeline_destroy);

	struct trtl_shader *vert = talloc_steal(info, trtl_shader_get(turtle, vertex_shader));
	struct trtl_shader *frag = talloc_steal(info, trtl_shader_get(turtle, fragment_shader));

	// FIXME: Handle this a little more gracefully.
	assert(vert != NULL);
	assert(frag != NULL);

	VkSpecializationInfo *specialisationInfo = set_specialise_info(&extent);
	VkVertexInputAttributeDescription *tofree = NULL;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vert->shader;
	vertShaderStageInfo.pName = "main";
	vertShaderStageInfo.pSpecializationInfo = specialisationInfo;

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = frag->shader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	// So this should be cleverer, built ffrom the list objects
	// And the vertexes for multiple objects, coallesced
	VkVertexInputBindingDescription btmp;
	if (binding_description == NULL) {
		// FIXME: binding & attribute should both be set
		// FIXME: Should pass these as a isngle struct to
		// make the paramster sane
		btmp = vertex_binding_description_get();
		binding_description = &btmp;
	}
	if (attribute_description == NULL) {
		tofree = get_attribute_description_pair(&nattributes);
		attribute_description = tofree;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = binding_description;
	vertexInputInfo.vertexAttributeDescriptionCount = nattributes;
	vertexInputInfo.pVertexAttributeDescriptions = attribute_description;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {0};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)extent.width;
	viewport.height = (float)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {0};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = extent;

	VkPipelineViewportStateCreateInfo viewportState = {0};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {0};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	// FIXME: Cull mode sould be customisable.
	// A flag to set the appropriate way.
	// NONE is probalby the worst choice here.
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f;	   // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f;	   // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = {0};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;		// Optional
	multisampling.pSampleMask = NULL;		// Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE;	// Optional

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = layer_blend;
	/* FIXME: Sort out if I can just use hte blnd flag.
	 * this is the original; I think ignored if layer_blend is false
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;		 // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;		 // Optional
	*/
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {0};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_info.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(turtle->device, &pipeline_layout_info, NULL,
				   &info->pipeline_layout) != VK_SUCCESS) {
		error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {0};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	// FIXME: First behidn a flag
	pipelineInfo.pDepthStencilState = &depth_stencil;
	pipelineInfo.pColorBlendState = &colorBlending;

	pipelineInfo.layout = info->pipeline_layout;
	pipelineInfo.renderPass = render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkResult rv = vkCreateGraphicsPipelines(turtle->device, VK_NULL_HANDLE, 1, &pipelineInfo,
						NULL, &info->pipeline);
	if (rv != VK_SUCCESS) {
		error_msg(rv, "failed to create graphics pipeline!");
	}

	if (tofree) talloc_free(tofree);
	talloc_free(specialisationInfo);

	return info;
}

struct trtl_pipeline_info *
trtl_pipeline_create_with_strata(
    struct turtle *turtle, struct trtl_layer *layer, uint32_t ndescriptorsets,
    VkDescriptorSetLayout descriptor_set_layout[static ndescriptorsets], const char *vertex_shader,
    const char *fragment_shader, const VkVertexInputBindingDescription *binding_description,
    const VkVertexInputAttributeDescription *attribute_description, uint32_t nattributes)
{
	struct trtl_pipeline_info *info = talloc_zero(turtle, struct trtl_pipeline_info);
	info->device = turtle->device;
	talloc_set_destructor(info, pipeline_destroy);

	struct trtl_shader *vert = talloc_steal(info, trtl_shader_get(turtle, vertex_shader));
	struct trtl_shader *frag = talloc_steal(info, trtl_shader_get(turtle, fragment_shader));

	// FIXME: Handle this a little more gracefully.
	assert(vert != NULL);
	assert(vert->shader != NULL);
	assert(frag != NULL);
	assert(frag->shader != NULL);

	VkSpecializationInfo *specialisationInfo = set_specialise_info(&layer->rect.extent);
	VkVertexInputAttributeDescription *tofree = NULL;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vert->shader;
	vertShaderStageInfo.pName = "main";
	vertShaderStageInfo.pSpecializationInfo = specialisationInfo;

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = frag->shader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	// So this should be cleverer, built ffrom the list objects
	// And the vertexes for multiple objects, coallesced
	VkVertexInputBindingDescription btmp;
	if (binding_description == NULL) {
		// FIXME: binding & attribute should both be set
		// FIXME: Should pass these as a isngle struct to
		// make the paramster sane
		btmp = vertex_binding_description_get();
		binding_description = &btmp;
	}
	if (attribute_description == NULL) {
		tofree = get_attribute_description_pair(&nattributes);
		attribute_description = tofree;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = binding_description;
	vertexInputInfo.vertexAttributeDescriptionCount = nattributes;
	vertexInputInfo.pVertexAttributeDescriptions = attribute_description;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {0};
	viewport.x = (float)layer->rect.offset.x;
	viewport.y = (float)layer->rect.offset.y;
	viewport.width = (float)layer->rect.extent.width;
	viewport.height = (float)layer->rect.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkPipelineViewportStateCreateInfo viewportState = {0};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &layer->rect;

	VkPipelineRasterizationStateCreateInfo rasterizer = {0};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f;	   // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f;	   // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = {0};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;		// Optional
	multisampling.pSampleMask = NULL;		// Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE;	// Optional

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// FIXME: Should be from ovjlayer
	colorBlendAttachment.blendEnable = false; // layer_blend;
	/* FIXME: Sort out if I can just use hte blnd flag.
	 * this is the original; I think ignored if layer_blend is false
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;		 // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;		 // Optional
	*/
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {0};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// This is just a sanity check
	assert(ndescriptorsets < 20);

	VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = ndescriptorsets;
	pipeline_layout_info.pSetLayouts = descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_info.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(turtle->device, &pipeline_layout_info, NULL,
				   &info->pipeline_layout) != VK_SUCCESS) {
		error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {0};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	// FIXME: First behidn a flag
	pipelineInfo.pDepthStencilState = &depth_stencil;
	pipelineInfo.pColorBlendState = &colorBlending;

	pipelineInfo.layout = info->pipeline_layout;
	pipelineInfo.renderPass = layer->render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkResult rv = vkCreateGraphicsPipelines(turtle->device, VK_NULL_HANDLE, 1, &pipelineInfo,
						NULL, &info->pipeline);
	if (rv != VK_SUCCESS) {
		error_msg(rv, "failed to create graphics pipeline!");
	}

	if (tofree) talloc_free(tofree);
	talloc_free(specialisationInfo);

	return info;
}

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
#include "trtl_object_mesh.h"
#include "trtl_pipeline.h"
#include "trtl_seer.h"
#include "trtl_shader.h"
#include "turtle.h"
#include "vertex.h"

#include "blobby.h"

extern int zoom;

// FIXME: Is this freed correctly?
static VkSpecializationInfo *
set_specialise_info(VkExtent2D *extent)
{
	VkSpecializationInfo *info = talloc_zero(NULL, VkSpecializationInfo);
	info->mapEntryCount = 3;
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
	data[1] = extent->width;

	// Tile size
	entries[2].constantID = 2;
	entries[2].size = sizeof(int);
	// FIXME: This whole thing should be a stricut, then the casing is much better
	entries[2].offset = 2 * sizeof(int);
	data[2] = zoom;

	info->dataSize = entries[2].offset + entries[2].size;
	info->pData = data;
	return info;
}

struct trtl_pipeline_info
trtl_pipeline_create(VkDevice device, VkRenderPass render_pass, VkExtent2D extent,
		     VkDescriptorSetLayout descriptor_set_layout, const char *vertex_shader,
		     const char *fragment_shader,
		     const VkVertexInputBindingDescription *binding_description,
		     const VkVertexInputAttributeDescription *attribute_description,
		     uint32_t nattributes)
{
	struct trtl_pipeline_info info;

	struct trtl_shader *vert = trtl_shader_get(vertex_shader);
	struct trtl_shader *frag = trtl_shader_get(fragment_shader);

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
	bool free_attribute = false;
	if (attribute_description == NULL) {
		tofree = get_attribute_description_pair(&nattributes);
		attribute_description = tofree;
		free_attribute = true;
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
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;		 // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	 // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;		 // Optional

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

	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};

	VkPipelineDynamicStateCreateInfo dynamicState = {0};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_info.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &info.pipeline_layout) !=
	    VK_SUCCESS) {
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

	pipelineInfo.layout = info.pipeline_layout;
	pipelineInfo.renderPass = render_pass;
	pipelineInfo.subpass = TRTL_RENDER_LAYER_BACKGROUND;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL,
				      &info.pipeline) != VK_SUCCESS) {
		error("failed to create graphics pipeline!");
	}

	if (tofree) talloc_free(tofree);

	return info;
}

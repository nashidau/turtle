
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <talloc.h>

#include "blobby.h"
#include "helpers.h"
#include "trtl.h"
#include "vertex.h"

#include "turtle.h"

#include "stringlist.h"
#include "trtl_barriers.h"
#include "trtl_object.h"
#include "trtl_seer.h"
#include "trtl_solo.h"
#include "trtl_texture.h"
#include "trtl_uniform.h"

struct trtl_stringlist *objs_to_load[TRTL_RENDER_LAYER_TOTAL] = {NULL};

const int MAX_FRAMES_IN_FLIGHT = 2;
// Belongs in render frame state
bool frame_buffer_resized = false;

trtl_alloc static VkDescriptorPool create_descriptor_pool(struct trtl_swap_chain *scd);

/** End Generic */

// In turtle .c
struct queue_family_indices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);

struct swap_chain_support_details *query_swap_chain_support(VkPhysicalDevice device,
							    VkSurfaceKHR surface);

void
recreate_swap_chain(struct render_context *render)
{
	int width = 0, height = 0;
	VkExtent2D size;
	struct turtle *turtle = render->turtle;

	glfwGetFramebufferSize(render->turtle->window, &width, &height);
	size.width = width;
	size.height = height;

	// a width/height of 0 is minimised, just wait for
	// events to recover (for now);
	while (width == 0 || height == 0) {
		printf("Size 0, wait\n");
		glfwWaitEvents();
		glfwGetFramebufferSize(render->turtle->window, &width, &height);
	}

	vkDeviceWaitIdle(render->turtle->device);

	talloc_free(render->scd);

	// FIXME: when we resize, I need to fix this
	// render->turtle->tsc = create_swap_chain(render->turtle, render->turtle->physical_device,
	//					render->turtle->surface);
	struct trtl_swap_chain *scd = turtle->tsc;
	scd->render = render;
	scd->image_views = create_image_views(render->turtle, scd->images, scd->nimages);

	scd->descriptor_pool = create_descriptor_pool(scd);
	//
	//	scd->command_pool = create_command_pool(
	//	    render->turtle->device, render->turtle->physical_device,
	//render->turtle->surface);

	trtl_seer_resize(size, scd);
	// info =trtl_pipeline_create(render->turtle->device, scd);

	// FIXME: Call object to update it's descriptor sets
	// scd->descriptor_sets = create_descriptor_sets(scd);
	// create_depth_resources(scd);
	//	scd->framebuffers = create_frame_buffers(render->turtle->device, scd,
	//			);
	scd->command_buffers = trtl_seer_create_command_buffers(scd, scd->command_pool);

	for (uint32_t i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}
}

trtl_alloc static VkDescriptorPool
create_descriptor_pool(struct trtl_swap_chain *scd)
{
	VkDescriptorPool descriptor_pool;
	VkDescriptorPoolSize pool_sizes[2];

	// FIXME: Static allocation of '10' here.  Need to amange this correctly
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = scd->nimages * 10;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = scd->nimages * 10;

	VkDescriptorPoolCreateInfo pool_info = {0};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = TRTL_ARRAY_SIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = scd->nimages * 10;

	if (vkCreateDescriptorPool(scd->render->turtle->device, &pool_info, NULL,
				   &descriptor_pool) != VK_SUCCESS) {
		error("failed to create descriptor pool!");
	}

	return descriptor_pool;
}

// FIXME: Fix the args on this.
void
transitionImageLayout(trtl_arg_unused struct render_context *render, VkImage image,
		      trtl_arg_unused VkFormat format, VkImageLayout oldLayout,
		      VkImageLayout newLayout)
{
	struct trtl_solo *solo = trtl_solo_get();

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
	    newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		error("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(solo->command_buffer, sourceStage, destinationStage, 0, 0, NULL, 0,
			     NULL, 1, &barrier);

	talloc_free(solo);
}

void
copyBufferToImage(trtl_arg_unused struct turtle *turtle,
		  trtl_arg_unused struct render_context *render, VkBuffer buffer, VkImage image,
		  uint32_t width, uint32_t height)
{
	struct trtl_solo *solo = trtl_solo_get();

	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = (VkOffset3D){0};
	region.imageExtent = (VkExtent3D){width, height, 1};

	vkCmdCopyBufferToImage(solo->command_buffer, buffer, image,
			       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	talloc_free(solo);
}

void
draw_frame(struct render_context *render, struct trtl_swap_chain *scd, VkSemaphore image_semaphore,
	   VkSemaphore renderFinishedSemaphore, VkFence fence)
{
	VkResult result;
	VkDevice device = render->turtle->device;

	// Make sure this frame was completed
	vkWaitForFences(render->turtle->device, 1, &fence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	result = vkAcquireNextImageKHR(device, scd->swap_chain, UINT64_MAX, image_semaphore,
				       VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain(render);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		error("Failed to get swap chain image");
	}

	trtl_seer_update(imageIndex);

	// FIXME: Device should be some sort of global context
	trtl_uniform_update(render->turtle->uniforms, imageIndex);

	// Check the system has finished with this image before we start
	// scribbling over the top of it.
	if (render->images_in_flight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &render->images_in_flight[imageIndex], VK_TRUE,
				UINT64_MAX);
	}
	render->images_in_flight[imageIndex] = fence;

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {image_semaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &scd->command_buffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &fence);

	result = vkQueueSubmit(render->turtle->graphicsQueue, 1, &submitInfo, fence);
	if (result != VK_SUCCESS) {
		printf("failed to submit draw command buffer %d! %p %p %p\n", result,
		       render->turtle->graphicsQueue, &submitInfo, fence);
		exit(1);
	}

	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {scd->swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(render->turtle->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
	    frame_buffer_resized) {
		frame_buffer_resized = false;
		recreate_swap_chain(render);
	} else if (result != VK_SUCCESS) {
		error("Failed to present swap chain image");
	}
}

static void
show_usage(const char *binary)
{
	printf("%s: Simple game\n", binary);
	puts(" --debug | -d   Set debug.  More 'd's more debug.");
	puts(" --help | -h    Show help.");
	puts(" --object | -o [OBJECT] Load a known object");
	puts(" --background | -b [OBJECT] Load a known object as background");
}

static void
parse_arguments(int argc, char **argv)
{
	static struct option options[] = {
	    {"debug", no_argument, NULL, 'd'},
	    {"help", no_argument, NULL, 'h'},
	    {"object", required_argument, NULL, 'o'},
	    {"background", required_argument, NULL, 'b'},
	    {NULL, 0, NULL, 0},
	};
	int option;

	while ((option = getopt_long(argc, argv, "dho:b:", options, NULL)) != -1) {
		switch (option) {
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'd':
			// debug = 1;
			continue;
		case 'b':
			objs_to_load[TRTL_RENDER_LAYER_BACKGROUND] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_BACKGROUND], optarg);
			continue;
		case 'o':
			objs_to_load[TRTL_RENDER_LAYER_MAIN] =
			    trtl_stringlist_add(objs_to_load[TRTL_RENDER_LAYER_MAIN], optarg);
			continue;
		default:
			show_usage(argv[0]);
			exit(0);
		}
	}
}

static int
load_object_default(struct trtl_swap_chain *scd, struct turtle *turtle)
{
	printf("Loading default objects: Background 'background', Main: 'grid9'\n");
	trtl_seer_object_add("background", turtle, scd, TRTL_RENDER_LAYER_BACKGROUND);
	trtl_seer_object_add("grid9", turtle, scd, TRTL_RENDER_LAYER_MAIN);
	return 2;
}

int
load_objects(struct trtl_swap_chain *scd, struct turtle *turtle)
{
	int i;

	// Do we have a least one object
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		if (objs_to_load[i] != NULL) {
			// Found something;
			break;
		}
	}
	if (i == TRTL_RENDER_LAYER_TOTAL) {
		return load_object_default(scd, turtle);
	}

	// For each layer, load it
	for (i = 0; i < TRTL_RENDER_LAYER_TOTAL; i++) {
		struct trtl_stringlist *load = objs_to_load[i];
		while (load != NULL) {
			trtl_seer_object_add(load->string, turtle, scd, i);
			load = load->next;
		}
		talloc_free(objs_to_load[i]);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	struct render_context *render = talloc_zero(NULL, struct render_context);
	struct turtle *turtle;

	parse_arguments(argc, argv);

	turtle = turtle_init();
	render->turtle = turtle;

	struct trtl_swap_chain *scd = turtle->tsc;
	render->scd = scd;
	scd->render = render;

	scd->descriptor_pool = create_descriptor_pool(scd);

	trtl_seer_init(turtle, scd->extent);

	// Above here shold be in turtle_init.

	load_objects(scd, turtle);

	// FIXME: This is a hack, this shoudl be managed by seer,
	// and shoukd be done dynamically as the state of the worlld changes.
	scd->command_buffers = trtl_seer_create_command_buffers(scd, scd->command_pool);

	trtl_barriers_init(turtle);

	// This should go into main loop
	render->images_in_flight = talloc_array(render, VkFence, scd->nimages);
	// FIXME: Should do this when creating the Scd structure
	for (uint32_t i = 0; i < scd->nimages; i++) {
		render->images_in_flight[i] = VK_NULL_HANDLE;
	}

	trtl_main_loop(turtle, render);

	//	main_loop(window);

	return 0;
}

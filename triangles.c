
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

trtl_alloc VkDescriptorPool create_descriptor_pool(struct trtl_swap_chain *scd);

/** End Generic */

// In turtle .c
struct queue_family_indices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);

struct swap_chain_support_details *query_swap_chain_support(VkPhysicalDevice device,
							    VkSurfaceKHR surface);





// FIXME: Fix the args on this.
void
transitionImageLayout(VkImage image,
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
copyBufferToImage(trtl_arg_unused struct turtle *turtle, VkBuffer buffer, VkImage image,
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
	struct turtle *turtle;

	parse_arguments(argc, argv);

	turtle = turtle_init();

	struct trtl_swap_chain *tsc = turtle->tsc;

	// Above here shold be in turtle_init.

	load_objects(tsc, turtle);

	trtl_main_loop(turtle);

	return 0;
}

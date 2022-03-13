/**
 * Manager for the system object for rendering.
 *
 * Sets the uniform fields for the system interface.
 *
 * Currently sets the screensize and the current time for shaders.
 *
 * Should be added to the seer in the system layer.
 */
#include <sys/time.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_events.h"
#include "trtl_strata.h"
#include "trtl_strata_base.h"
#include "trtl_uniform.h"
#include "turtle.h"

#include "shaders/trtl_strata_base.include"

struct trtl_strata_base {
	struct trtl_strata strata;
	struct trtl_uniform *uniform;
	struct trtl_uniform_info *uniform_info;

	uint32_t width, height;
};

static bool sbase_update(struct trtl_strata *obj, int frame);
static VkDescriptorSetLayout sbase_descriptor_set_layout(struct trtl_strata *strata);
trtl_alloc static VkDescriptorSet *sbase_descriptor_set(struct trtl_strata *strata);

static inline struct trtl_strata_base *
trtl_strata_base(struct trtl_strata *strata)
{
	struct trtl_strata_base *sbase;
	sbase = talloc_get_type(strata, struct trtl_strata_base);
	assert(sbase != NULL);
	return sbase;
}

static void
resize_callback(void *sbasev, trtl_arg_unused trtl_crier_cry_t cry, const void *event)
{
	struct trtl_event_resize *resize = talloc_get_type(event, struct trtl_event_resize);
	struct trtl_strata_base *sbase = talloc_get_type(sbasev, struct trtl_strata_base);

	sbase->width = resize->new_size.width;
	sbase->height = resize->new_size.height;
	printf("Screen size is %d, %d\n", sbase->width, sbase->height);
}

struct trtl_strata *
trtl_strata_base_init(struct turtle *turtle)
{
	struct trtl_strata_base *sbase;

	sbase = talloc(turtle, struct trtl_strata_base);
	if (!sbase) {
		return NULL;
	}

	sbase->strata.turtle = turtle;
	sbase->strata.update = sbase_update;
	sbase->strata.descriptor_set_layout = sbase_descriptor_set_layout;
	sbase->strata.descriptor_set = sbase_descriptor_set;

	sbase->uniform = trtl_uniform_init(turtle, "Strata Base Uniforms", turtle->tsc->nimages,
					   sizeof(struct trtl_strata_base_uniforms));
	sbase->uniform_info =
	    trtl_uniform_alloc_type(sbase->uniform, struct trtl_strata_base_uniforms);

	sbase->width = 800;
	sbase->height = 640;

	trtl_crier_listen(turtle->events->crier, "trtl_event_resize", resize_callback, sbase);

	return &sbase->strata;
}

static bool
sbase_update(struct trtl_strata *strata, int frame)
{
	static float time = 0;
	struct trtl_strata_base *sbase = trtl_strata_base(strata);
	struct trtl_strata_base_uniforms *uniforms =
	    trtl_uniform_info_address(sbase->uniform_info, frame);

	uniforms->screen_size[0] = sbase->width;
	uniforms->screen_size[1] = sbase->height;
	// FIXME: Shoving an int into a float here.  Should get gettime of something to get a float
	time += 0.1;
	// uniforms->time = time;
	uniforms->screen_size[2] = time;

	trtl_uniform_update(sbase->uniform, frame);

	return true;
}

static VkDescriptorSetLayout
sbase_descriptor_set_layout(struct trtl_strata *strata)
{
	VkDescriptorSetLayout descriptor_set_layout;

	VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.pImmutableSamplers = NULL;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding bindings[1];
	bindings[0] = ubo_layout_binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = TRTL_ARRAY_SIZE(bindings);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(strata->turtle->device, &layoutInfo, NULL,
					&descriptor_set_layout) != VK_SUCCESS) {
		error("failed to create descriptor set layout!");
	}
	return descriptor_set_layout;
}

trtl_alloc static VkDescriptorSet *
sbase_descriptor_set(struct trtl_strata *strata)
{
	struct trtl_strata_base *sbase = talloc_get_type(strata, struct trtl_strata_base);
	uint32_t nframes = strata->turtle->tsc->nimages;
	VkDescriptorSet *sets = talloc_zero_array(strata, VkDescriptorSet, nframes);
	VkDescriptorSetLayout *layouts = talloc_zero_array(NULL, VkDescriptorSetLayout, nframes);

	for (uint32_t i = 0; i < nframes; i++) {
		// FIXME: Thsi should cache right
		layouts[i] = strata->descriptor_set_layout(strata);
	}

	VkDescriptorSetAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = strata->turtle->tsc->descriptor_pool;
	alloc_info.descriptorSetCount = nframes;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(strata->turtle->device, &alloc_info, sets) != VK_SUCCESS) {
		error("failed to allocate descriptor sets!");
	}

	for (uint32_t i = 0; i < nframes; i++) {
		VkDescriptorBufferInfo buffer_info =
		    trtl_uniform_buffer_get_descriptor(sbase->uniform_info, i);

		VkWriteDescriptorSet descriptorWrites[1] = {0};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(strata->turtle->device, TRTL_ARRAY_SIZE(descriptorWrites),
				       descriptorWrites, 0, NULL);
	}

	talloc_free(layouts);

	return sets;
}

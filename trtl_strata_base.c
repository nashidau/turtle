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
#include "trtl_strata.h"
#include "trtl_strata_base.h"
#include "trtl_events.h"
#include "trtl_uniform.h"
#include "turtle.h"

#include "shaders/trtl_strata_base.include"

struct trtl_strata_base {
	struct trtl_strata strata;
	struct trtl_uniform *uniform;
	struct trtl_uniform_info *uniform_info;

	uint32_t width, height;
};

static bool sysm_update(struct trtl_strata *obj, int frame);
static void sysm_resize(struct trtl_strata *, const VkRenderPass renderpass,
			VkExtent2D extent);

static inline struct trtl_strata_base *
trtl_strata_base(struct trtl_strata *strata)
{
	struct trtl_strata_base *sbase;
	sbase = talloc_get_type(strata, struct trtl_strata_base);
	assert(sbase != NULL);
	return sbase;
}

static void
resize_callback(void *sbasev, trtl_arg_unused trtl_crier_cry_t cry, const void *event) {
	struct trtl_event_resize *resize = talloc_get_type(event, struct trtl_event_resize);
	struct trtl_strata_base *sbase = talloc_get_type(sbasev, struct trtl_strata_base);

	sbase->width = resize->new_size.width;
	sbase->height = resize->new_size.height;
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
	sbase->strata.update = sysm_update;
	sbase->strata.resize = sysm_resize;

	sbase->uniform =
	    trtl_uniform_init(turtle, turtle->tsc->nimages, sizeof(struct trtl_system_uniforms));
	sbase->uniform_info = trtl_uniform_alloc_type(sbase->uniform, struct trtl_system_uniforms);

	sbase->width = 800;
	sbase->height = 640;

	trtl_crier_listen(turtle->events->crier, "trtl_event_resize", resize_callback,
			sbase);

	return &sbase->strata;
}

static bool
sysm_update(struct trtl_strata *strata, int frame)
{
	struct trtl_strata_base *sbase = trtl_strata_base(strata);
	struct trtl_system_uniforms *uniforms =
	    trtl_uniform_info_address(sbase->uniform_info, frame);

	uniforms->screen_size[0] = sbase->width;
	uniforms->screen_size[1] = sbase->height;
	// FIXME: Shoving an int into a float here.  Should get gettime of something to get a float
	uniforms->time = time(NULL);

	return true;
}

static void
sysm_resize(struct trtl_strata *strata, trtl_arg_unused const VkRenderPass renderpass, VkExtent2D extent)
{
	struct trtl_strata_base *sbase = trtl_strata_base(strata);

	sbase->width = extent.width;
	sbase->height = extent.height;
}

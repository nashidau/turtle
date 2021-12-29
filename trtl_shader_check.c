
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "trtl_check.h"
#include "trtl_shader.h"
#include "turtle.h"

// FAKE_VALUE_FUNC(int, clock_gettime, clockid_t, struct timespec *);
FAKE_VALUE_FUNC(VkResult, vkCreateShaderModule, VkDevice, const VkShaderModuleCreateInfo *,
		const VkAllocationCallbacks*, VkShaderModule *);
FAKE_VOID_FUNC(vkDestroyShaderModule, VkDevice, VkShaderModule, const VkAllocationCallbacks *);

static const VkDevice fake_device = (VkDevice)0xfeedcafe;

static struct turtle *
init_shader(void)
{
	struct trtl_shader_cache *cache;
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	turtle->device = fake_device;
	cache = trtl_shader_cache_init(turtle);
	turtle->shader_cache = cache;
	ck_assert_ptr_nonnull(cache);
	return turtle;
}

START_TEST(test_shader_create)
{
	struct trtl_shader_cache *cache;
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	turtle->device = fake_device;
	cache = trtl_shader_cache_init(turtle);
	ck_assert_ptr_nonnull(cache);
	// Freed with parent
	talloc_free(turtle);
}
END_TEST

START_TEST(test_shader_get)
{
	struct turtle *turtle = init_shader();
	struct trtl_shader *shader;

	shader = trtl_shader_get(turtle, "test_shader");
	ck_assert_ptr_nonnull(shader);

	talloc_free(shader);

	ck_assert_int_eq(vkCreateShaderModule_fake.call_count, 1);
	ck_assert_int_eq(vkDestroyShaderModule_fake.call_count, 1);

	talloc_free(turtle);
}
END_TEST

START_TEST(test_shader_get_multiple)
{
	struct turtle *turtle = init_shader();
	struct trtl_shader *shader;
	struct trtl_shader *shader2;

	shader = trtl_shader_get(turtle, "test_shader");
	ck_assert_ptr_nonnull(shader);
	shader2 = trtl_shader_get(turtle, "test_shader");
	ck_assert_ptr_nonnull(shader2);
	// They should be distinct
	ck_assert_ptr_ne(shader2, shader);

	talloc_free(shader);

	ck_assert_int_eq(vkCreateShaderModule_fake.call_count, 1);
	ck_assert_int_eq(vkDestroyShaderModule_fake.call_count, 0);

	talloc_free(shader2);

	ck_assert_int_eq(vkDestroyShaderModule_fake.call_count, 1);

	talloc_free(turtle);
}
END_TEST

Suite *
trtl_shader_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Shader");

	{
		TCase *tc_create = tcase_create("Create");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_shader_create);
	}

	{
		TCase *tc_get = tcase_create("Get");
		suite_add_tcase(s, tc_get);

		tcase_add_test(tc_get, test_shader_get);
		tcase_add_test(tc_get, test_shader_get_multiple);
	}


	return s;
}


#include <vulkan/vulkan.h>

#include <check.h>
#include <talloc.h>

#include "trtl_check.h"
#include "trtl_uniform.h"
#include "turtle.h"

struct turtle;

FAKE_VOID_FUNC(create_buffer, struct turtle *, VkDeviceSize, VkBufferUsageFlags,
	       VkMemoryPropertyFlags, VkBuffer *, VkDeviceMemory *);
FAKE_VOID_FUNC(vkDestroyBuffer, VkDevice, VkBuffer, const VkAllocationCallbacks *);
FAKE_VOID_FUNC(vkFreeMemory, VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);

static const VkDevice fake_device = (VkDevice)0xfeedcafe;

static struct turtle *
create_fake_turtle(void) {
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	turtle->device = fake_device;
	return turtle;
}

START_TEST(test_uniform_init)
{
	struct turtle *turtle;
	struct trtl_uniform *uniforms;

	turtle = talloc_zero(NULL, struct turtle);
	uniforms = trtl_uniform_init(turtle, "Test", 2, 100);
	ck_assert_ptr_ne(uniforms, NULL);
	talloc_free(uniforms);
	talloc_free(turtle);
}
END_TEST

START_TEST(test_uniform_alloc)
{
	struct turtle *turtle = talloc_zero(NULL, struct turtle);
	struct trtl_uniform *uniforms = trtl_uniform_init(turtle, "Test", 2, 100);

	struct trtl_uniform_info *info = trtl_uniform_alloc(uniforms, 32);
	ck_assert_ptr_ne(info, NULL);

	// Free should release the info too
	talloc_free(uniforms);
}
END_TEST


START_TEST(test_uniform_alloc_all)
{
	struct turtle *turtle = create_fake_turtle();
	// Init with an odd size, but reasonable default
	struct trtl_uniform *uniforms = trtl_uniform_init(turtle, "Test", 2, 12);

	// Allocate the same size; should work
	struct trtl_uniform_info *info = trtl_uniform_alloc(uniforms, 12);
	ck_assert_ptr_ne(info, NULL);

	talloc_free(uniforms);
} END_TEST

START_TEST(test_uniform_alloc_too_much)
{
	struct turtle *turtle = create_fake_turtle();
	struct trtl_uniform *uniforms = trtl_uniform_init(turtle,"test", 2, 100);

	struct trtl_uniform_info *info = trtl_uniform_alloc(uniforms, 96);
	ck_assert_ptr_ne(info, NULL);

	// 192 bytes is larger than 100 -> should barf
	info = trtl_uniform_alloc(uniforms, 96);
	ck_assert_ptr_eq(info, NULL);

	// Free should release the info too
	talloc_free(uniforms);
}
END_TEST

Suite *
trtl_uniform_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Uniform");

	{
		TCase *tc_alloc = tcase_create("Allocation");

		tcase_add_checked_fixture(tc_alloc, talloc_enable_leak_report_full, NULL);
		suite_add_tcase(s, tc_alloc);

		tcase_add_test(tc_alloc, test_uniform_init);
		tcase_add_test(tc_alloc, test_uniform_alloc);
		tcase_add_test(tc_alloc, test_uniform_alloc_all);
		tcase_add_test(tc_alloc, test_uniform_alloc_too_much);
	}

	return s;
}

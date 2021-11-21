
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "trtl_check.h"
#include "trtl_solo.h"
#include "turtle.h"

FAKE_VALUE_FUNC(VkResult, vkAllocateCommandBuffers, VkDevice, const VkCommandBufferAllocateInfo *,
		VkCommandBuffer*);
FAKE_VOID_FUNC(vkFreeCommandBuffers, VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer *);
FAKE_VALUE_FUNC(VkResult, vkBeginCommandBuffer, VkCommandBuffer, const VkCommandBufferBeginInfo *);
FAKE_VALUE_FUNC(VkResult, vkEndCommandBuffer, VkCommandBuffer);
FAKE_VALUE_FUNC(VkResult, vkQueueSubmit, VkQueue, uint32_t, const VkSubmitInfo *, VkFence);
FAKE_VALUE_FUNC(VkResult, vkQueueWaitIdle, VkQueue);
FAKE_VALUE_FUNC(VkResult, vkCreateCommandPool, VkDevice, const VkCommandPoolCreateInfo *,
		const VkAllocationCallbacks *, VkCommandPool *);

static const VkDevice fake_device = (VkDevice)0xfeedcafe;
static const VkQueue fake_queue = (VkQueue)0xdeadbeef;
static VkCommandPool fake_command_pool = (VkCommandPool)0xba5eba11;

static VkResult
vkCreateCommandPool_custom(trtl_arg_unused VkDevice device,
			   trtl_arg_unused const VkCommandPoolCreateInfo *info,
			   trtl_arg_unused const VkAllocationCallbacks *callbacks,
			   VkCommandPool *command_pool)
{
	ck_assert_ptr_nonnull(command_pool);
	*command_pool = fake_command_pool;
	return 0;
}

// Simply add and then delete a solor instance.
START_TEST(test_solo_create)
{
	vkCreateCommandPool_fake.custom_fake = vkCreateCommandPool_custom;
	trtl_solo_init(fake_device, fake_queue, 27);

	struct trtl_solo *solo = trtl_solo_start();
	talloc_free(solo);
}
END_TEST

// Simply add and then delete a solor instance.
START_TEST(test_solo_executes)
{
	trtl_solo_init(fake_device, fake_queue, 27);

	struct trtl_solo *solo = trtl_solo_start();

	ck_assert_int_eq(vkBeginCommandBuffer_fake.call_count, 1);
	ck_assert_int_eq(vkEndCommandBuffer_fake.call_count, 0);

	talloc_free(solo);
	
	ck_assert_int_eq(vkBeginCommandBuffer_fake.call_count, 1);
	ck_assert_int_eq(vkEndCommandBuffer_fake.call_count, 1);
	ck_assert_int_eq(vkQueueSubmit_fake.call_count, 1);
	ck_assert_int_eq(vkQueueWaitIdle_fake.call_count, 1);

	ck_assert_ptr_eq(vkQueueSubmit_fake.arg0_val, fake_queue);
	ck_assert_ptr_eq(vkQueueWaitIdle_fake.arg0_val, fake_queue);

}
END_TEST

Suite *
trtl_solo_suite(trtl_arg_unused void *ctx)
{
	Suite *s = suite_create("Solo");

	{
		TCase *tc_create = tcase_create("Create");
		suite_add_tcase(s, tc_create);

		tcase_add_test(tc_create, test_solo_create);
		tcase_add_test(tc_create, test_solo_executes);
	}
	return s;
}

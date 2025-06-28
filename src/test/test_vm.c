#include "test_framework.h"

// testing direct VM construction
enum TEST_RESULT test_can_read_from_vm_memory_direct0(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|0, 0x444000, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction
enum TEST_RESULT test_can_read_from_vm_memory_direct1(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0x800100400800ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction
enum TEST_RESULT test_can_read_from_vm_memory_direct2(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|0, 0x446000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction
enum TEST_RESULT test_can_read_from_vm_memory_direct3(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0x15600000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction (PDE-as-PTE)
enum TEST_RESULT test_can_read_from_vm_memory_direct5(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0x30380F000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction (4-level with 256KB PTE)
enum TEST_RESULT test_can_read_from_vm_memory_direct6(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0x304A0F000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction (PTE with T (further) bit set)
enum TEST_RESULT test_can_read_from_vm_memory_direct7(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0xff0064e000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// testing direct VM construction (PTE with T (further) bit set)
enum TEST_RESULT test_can_read_from_vm_memory_direct8(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0xff00650000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// three-level with PTE.further
enum TEST_RESULT test_can_read_from_vm_memory_direct9(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|8, 0x7ffff6768000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// four-level with PTE.further
enum TEST_RESULT test_can_read_from_vm_memory_direct10(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|8, 0x7f3bcca00000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// four-level with 2M page
enum TEST_RESULT test_can_read_from_vm_memory_direct11(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|8, 0x7f26faa00000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// four-level with 4K page
enum TEST_RESULT test_can_read_from_vm_memory_direct12(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|8, 0x7f334f600000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// VMID=0 read on gfx8
enum TEST_RESULT test_can_read_from_vm_memory_direct13(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|0, 0xff00402000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// VMID>0 read on gfx8
enum TEST_RESULT test_can_read_from_vm_memory_direct14(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|6, 0x00233000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// read from SAM on navi10
enum TEST_RESULT test_can_read_from_vm_memory_direct15(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB, 0x8000001000ULL, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// read from outside the page table span on navi10
enum TEST_RESULT test_can_read_from_vm_memory_direct16(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    asic->std_msg("[NOTE]: We expect an error message from the 16'th test\n");
    ASSERT_FAILURE(umr_read_vram(asic, -1, UMR_GFX_HUB, 0xffffe11000, sizeof(read_data), &read_data));
    return TEST_SUCCESS;
}

// read four level + 1 extra level where PDE0 has tfs
enum TEST_RESULT test_can_read_from_vm_memory_direct17(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|5, 0x2e0a2000, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

// read three level page table, but with PDE0 as PTE
enum TEST_RESULT test_can_read_from_vm_memory_direct18(struct umr_asic* asic)
{
    uint64_t read_data = 0;
    ASSERT_SUCCESS(umr_read_vram(asic, -1, UMR_GFX_HUB|3, 0x7f8047e00000, sizeof(read_data), &read_data));
    ASSERT_EQ(read_data, 0x0706050403020100);
    return TEST_SUCCESS;
}

DEFINE_TESTS(vm_tests)
#if 0
TEST(test_can_read_from_vm_memory_direct1, "direct_vm_test1.envdef", "raven1"),
#endif
#if 1
TEST(test_can_read_from_vm_memory_direct0, "direct_vm_test0.envdef", "raven1"),
TEST(test_can_read_from_vm_memory_direct1, "direct_vm_test1.envdef", "raven1"),
TEST(test_can_read_from_vm_memory_direct2, "direct_vm_test2.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct3, "direct_vm_test3.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct5, "direct_vm_test5.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct6, "direct_vm_test6.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct7, "direct_vm_test7.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct8, "direct_vm_test8.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct9, "direct_vm_test9.envdef", "vega10"),
TEST(test_can_read_from_vm_memory_direct10, "direct_vm_test10.envdef", "vega10"),
TEST(test_can_read_from_vm_memory_direct11, "direct_vm_test11.envdef", "vega10"),
TEST(test_can_read_from_vm_memory_direct12, "direct_vm_test12.envdef", "vega10"),
TEST(test_can_read_from_vm_memory_direct13, "direct_vm_test13.envdef", "polaris11"),
TEST(test_can_read_from_vm_memory_direct14, "direct_vm_test14.envdef", "polaris11"),
TEST(test_can_read_from_vm_memory_direct15, "direct_vm_test15.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct16, "direct_vm_test16.envdef", "navi10"),
TEST(test_can_read_from_vm_memory_direct17, "direct_vm_test17.envdef", "gfx11_vm_test"),
TEST(test_can_read_from_vm_memory_direct18, "direct_vm_test18.envdef", "aldebaran"),
#endif
END_TESTS(vm_tests);

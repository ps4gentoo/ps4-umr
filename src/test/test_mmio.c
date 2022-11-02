#include "test_framework.h"

enum TEST_RESULT test_reg_name_to_offset(struct umr_asic* asic, char* name, uint32_t byteoffset, uint32_t value)
{
    struct umr_reg* reg_desc;
    struct umr_ip_block* ip;
    uint32_t reg_value;
    reg_desc = umr_find_reg_by_addr(asic, byteoffset/4, &ip);
    ASSERT_NOT_NULL(reg_desc);
    ASSERT_STR_EQ(reg_desc->regname, name);
    reg_value = umr_read_reg_by_name_by_ip(asic, ip->ipname, reg_desc->regname);
    ASSERT_EQ(reg_value, value);
    return TEST_SUCCESS;
}

enum TEST_RESULT test_reg_name_to_offset_navi(struct umr_asic* asic)
{
    return test_reg_name_to_offset(asic, "mmGCMC_VM_FB_LOCATION_BASE", 0xA600, 0x4E563131);
}

enum TEST_RESULT test_reg_name_to_offset_raven(struct umr_asic* asic)
{
    return test_reg_name_to_offset(asic, "mmPWR_MISC_CNTL_STATUS", 0x5AE0C,0x52564E31);
}

enum TEST_RESULT test_reg_name_to_offset_renoir(struct umr_asic* asic)
{
    return test_reg_name_to_offset(asic, "mmSMUIO_GFX_MISC_CNTL", 0x5A320, 0x524E5231);
}

DEFINE_TESTS(mmio_tests)
TEST(test_reg_name_to_offset_navi, "navi_reg_only.envdef", "navi10"),
TEST(test_reg_name_to_offset_raven, "raven_reg_only.envdef", "raven1"),
TEST(test_reg_name_to_offset_renoir, "renoir_reg_only.envdef", "renoir"),
END_TESTS(mmio_tests);

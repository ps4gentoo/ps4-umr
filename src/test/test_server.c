#include "test_framework.h"
#include "parson.h"

extern void parse_sysfs_clock_file(char *content, int *min, int *max);
extern JSON_Value *compare_fence_infos(const char *before, const char *after);
extern JSON_Array *parse_vm_info(const char *content);
extern JSON_Array *parse_kms_framebuffer_sysfs_file(const char *content);
extern JSON_Object *parse_kms_state_sysfs_file(const char *content);
extern JSON_Object *parse_pp_features_sysfs_file(const char *content);

enum TEST_RESULT test_parse_sysfs_clock_file()
{
    char *content =
        "0: 500Mhz \n"
        "1: 0Mhz *\n"
        "2: 2575Mhz \n";

    int min, max;
    parse_sysfs_clock_file(content, &min, &max);
    ASSERT_EQ(0, min);
    ASSERT_EQ(2575, max);
    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_fence_info()
{
    const char *before =
        "--- ring 0 (gfx_0.0.0) ---\n"
        "Last signaled fence          0x000e67ee\n"
        "Last emitted                 0x000e67ee\n"
        "Last signaled trailing fence 0x00000000\n"
        "Last emitted                 0x00000000\n"
        "Last preempted               0x00000000\n"
        "Last reset                   0x00000000\n"
        "Last both                    0x00000000\n"
        "--- ring 1 (comp_1.0.0) ---\n"
        "Last signaled fence          0x00000002\n"
        "Last emitted                 0x00000002\n"
        "--- ring 2 (comp_1.1.0) ---\n"
        "Last signaled fence          0x00000012\n"
        "Last emitted                 0x00000012\n";

    const char *after =
        "--- ring 0 (gfx_0.0.0) ---\n"
        "Last signaled fence          0x000f67ee\n"
        "Last emitted                 0x000f67ee\n"
        "Last signaled trailing fence 0x00000000\n"
        "Last emitted                 0x00000000\n"
        "Last preempted               0x00000000\n"
        "Last reset                   0x00000000\n"
        "Last both                    0x00000000\n"
        "--- ring 1 (comp_1.0.0) ---\n"
        "Last signaled fence          0x00000010\n"
        "Last emitted                 0x00000010\n"
        "--- ring 2 (comp_1.1.0) ---\n"
        "Last signaled fence          0x00000098\n"
        "Last emitted                 0x00000098\n";

    const char *names[] = { "gfx_0.0.0", "comp_1.0.0", "comp_1.1.0" };
    const int deltas[]  = { 0x000f67ee - 0x000e67ee, 0x00000010 - 0x00000002, 0x00000098 -  0x00000012 };

    JSON_Value *fences = compare_fence_infos(before, after);
    ASSERT_EQ(json_array_get_count(json_array(fences)), 3);
    for (int i = 0; i < 3; i++) {
        JSON_Object *v = json_object(json_array_get_value(json_array(fences), i));
        ASSERT_STR_EQ(json_object_get_string(v, "name"), names[i]);
        ASSERT_EQ(json_object_get_number(v, "delta"), deltas[i]);
    }
    json_value_free(fences);
    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_vm_info()
{
    const char *content =
        "pid:0\tProcess: ----------\n"
        "\tIdle BOs:\n"
        "\tEvicted BOs:\n"
        "\t\t0x00000000:         4096 byte  GTT CPU_GTT_USWC VRAM_CONTIGUOUS\n"
        "\tRelocated BOs:\n"
        "\tMoved BOs:\n"
        "\tInvalidated BOs:\n"
        "\tDone BOs:\n"
        "\tTotal idle size:                   0\tobjs:\t0\n"
        "\tTotal evicted size:             4096\tobjs:\t1\n"
        "\tTotal relocated size:              0\tobjs:\t0\n"
        "\tTotal moved size:                  0\tobjs:\t0\n"
        "\tTotal invalidated size:            0\tobjs:\t0\n"
        "\tTotal done size:                   0\tobjs:\t0\n"
        "pid:3140\tProcess:GeckoMain ----------\n"
        "\tIdle BOs:\n"
        "\t\t0x00000000:         4096 byte VRAM CPU_GTT_USWC VRAM_CONTIGUOUS\n"
        "\t\t0x00000001:         4096 byte   GTT exported as 000000000a49a273 NO_CPU_ACCESS CPU_GTT_USWC\n"
        "\tEvicted BOs:\n"
        "\tRelocated BOs:\n"
        "\tMoved BOs:\n"
        "\tInvalidated BOs:\n"
        "\tDone BOs:\n"
        "\t\t0x00000001:      2097152 byte  GTT CPU_ACCESS_REQUIRED CPU_GTT_USWC\n"
        "\tTotal idle size:                8192\tobjs:\t2\n"
        "\tTotal evicted size:                0\tobjs:\t0\n"
        "\tTotal relocated size:              0\tobjs:\t0\n"
        "\tTotal moved size:                  0\tobjs:\t0\n"
        "\tTotal invalidated size:            0\tobjs:\t0\n"
        "\tTotal done size:             2097152\tobjs:\t1\n";

    const char *names[] = { "", "GeckoMain" };
    int pids[] = { 0, 3140 };
    JSON_Array *out = parse_vm_info(content);
    ASSERT_EQ(json_array_get_count(out), 2);
    for (int i = 0; i < 2; i++) {
        JSON_Object *v = json_object(json_array_get_value(out, i));
        ASSERT_STR_EQ(json_object_get_string(v, "name"), names[i]);
        ASSERT_EQ(json_object_get_number(v, "pid"), pids[i]);
        ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Relocated")), 0);
        ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Moved")), 0);
        ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Invalidated")), 0);
        if (i == 0) {
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Done")), 0);
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Idle")), 0);
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Evicted")), 1);
            ASSERT_EQ(json_object_get_number(v, "total"), 4096);
        } else {
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Evicted")), 0);
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Done")), 1);
            ASSERT_EQ(json_array_get_count(json_object_get_array(v, "Idle")), 2);
            ASSERT_EQ(json_object_get_number(v, "total"), 2105344);
            JSON_Value *bo = json_array_get_value(json_object_get_array(v, "Idle"), 1);
            JSON_Array *attr = json_object_get_array(json_object(bo), "attributes");
            ASSERT_EQ(json_array_get_count(attr), 4);
            const char *exp[] = {
                "GTT",
                "exported as 000000000a49a273",
                "NO_CPU_ACCESS",
                "CPU_GTT_USWC"
            };
            for (int i = 0; i < 4; i++)
                ASSERT_STR_EQ(json_array_get_string(attr, i), exp[i]);
        }
    }
    json_value_free(json_array_get_wrapping_value(out));
    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_sysfs_framebuffer()
{
    const char *content =
        "framebuffer[135]:\n"
        "\tallocated by = gnome-shell\n"
        "\trefcount=1\n"
        "\tformat=AR24 little-endian (0x34325241)\n"
        "\tmodifier=0x0\n"
        "\tsize=256x256\n"
        "\tlayers:\n"
        "\t\tsize[0]=256x256\n"
        "\t\tpitch[0]=1024\n"
        "\t\toffset[0]=0\n"
        "\t\tobj[0]:\n"
        "\t\t\tname=0\n"
        "\t\t\trefcount=2\n"
        "\t\t\tstart=00103c38\n"
        "\t\t\tsize=262144\n"
        "\t\t\timported=no\n"
        "framebuffer[119]:\n"
        "\tallocated by = [fbcon]\n"
        "\trefcount=1\n"
        "\tformat=XR24 little-endian (0x34325258)\n"
        "\tmodifier=0x1234\n"
        "\tsize=3440x1440\n"
        "\tlayers:\n"
        "\t\tsize[0]=3440x1440\n"
        "\t\tpitch[0]=13824\n"
        "\t\toffset[0]=0\n"
        "\t\tobj[0]:\n"
        "\t\t\tname=0\n"
        "\t\t\trefcount=2\n"
        "\t\t\tstart=00100000\n"
        "\t\t\tsize=19906560\n"
        "\t\t\timported=no\n";

    JSON_Array *out = parse_kms_framebuffer_sysfs_file(content);

    const char *expected_json =
        "[ \
            { \
                \"id\": 135, \"allocated by\": \"gnome-shell\", \"format\": \"AR24 little-endian (0x34325241)\", \
                \"modifier\": 0, \"size\": { \"w\": 256, \"h\": 256 }, \
                \"layers\": [ \
                    { \
                        \"size\": { \"w\": 256, \"h\": 256 }, \
                        \"pitch\": 1024 \
                    } \
                ] \
            }, \
            { \
                \"id\": 119, \"allocated by\": \"[fbcon]\", \"format\": \"XR24 little-endian (0x34325258)\", \
                \"modifier\": 4660, \"size\": { \"w\": 3440, \"h\": 1440 }, \
                \"layers\": [ \
                    { \
                        \"size\": { \"w\": 3440, \"h\": 1440 }, \
                        \"pitch\": 13824 \
                    } \
                ] \
            } \
        ]";

    JSON_Value *expected = json_parse_string(expected_json);

    ASSERT_EQ(json_value_equals(json_array_get_wrapping_value(out), expected), 1);
    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_sysfs_state()
{
    const char *content =
        "plane[65]: plane-5\n"
        "\tcrtc=crtc-0\n"
        "\tfb=120\n"
        "\t\tallocated by = gnome-shell\n"
        "\t\trefcount=2\n"
        "\t\tformat=XR24 little-endian (0x34325258)\n"
        "\t\tmodifier=0x200000020801b03\n"
        "\t\tsize=3440x1440\n"
        "\t\tlayers:\n"
        "\t\t\tsize[0]=3440x1440\n"
        "\t\t\tpitch[0]=13824\n"
        "\t\t\toffset[0]=0\n"
        "\t\t\tobj[0]:\n"
        "\t\t\t\tname=0\n"
        "\t\t\t\trefcount=2\n"
        "\t\t\t\tstart=001096aa\n"
        "\t\t\t\tsize=21422080\n"
        "\t\t\t\timported=no\n"
        "\tcrtc-pos=3440x1440+0+0\n"
        "\tsrc-pos=3440.000000x1440.000000+0.000000+0.000000\n"
        "\trotation=1\n"
        "\tnormalized-zpos=0\n"
        "\tcolor-encoding=ITU-R BT.601 YCbCr\n"
        "\tcolor-range=YCbCr limited range\n"
        "crtc[77]: crtc-0\n"
        "\tenable=1\n"
        "\tactive=1\n"
        "\tself_refresh_active=0\n"
        "\tplanes_changed=1\n"
        "\tmode_changed=0\n"
        "\tactive_changed=0\n"
        "\tconnectors_changed=0\n"
        "\tcolor_mgmt_changed=0\n"
        "\tplane_mask=a0\n"
        "\tconnector_mask=1\n"
        "\tencoder_mask=1\n"
        "\tmode: \"3440x1440\": 60 319750 3440 3520 3552 3600 1440 1468 1478 1481 0x48 0x9\n"
        "connector[94]: DP-1\n"
        "\tcrtc=crtc-0\n"
        "\tself_refresh_aware=0\n";

    JSON_Object *out = parse_kms_state_sysfs_file(content);

    const char * frames = "[{\"name\": \"plane-0\", \"fb\": 120, \"crtc\": { \"id\": 0, \"pos\": { \"x\": 0, \"y\": 0, \"w\": 3440, \"h\": 1440 } }]";
    const char * crtcs = "[{\"id\": 0, \"enable\": 1, \"active\": 1}]";
    const char * connectors = "[{\"name\": \"DP-1\", \"crtc\": 0}]";

    JSON_Value *frames_ref = json_parse_string(frames);
    ASSERT_EQ(json_value_equals(json_object_get_value(out, "frames"), frames_ref), 1);

    JSON_Value *crtcs_ref = json_parse_string(crtcs);
    ASSERT_EQ(json_value_equals(json_object_get_value(out, "crtcs"), crtcs_ref), 1);

    JSON_Value *connectors_ref = json_parse_string(connectors);
    ASSERT_EQ(json_value_equals(json_object_get_value(out, "connectors"), connectors_ref), 1);

    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_sysfs_pp_features()
{
    const char *content =
        "features high: 0x00003763 low: 0xa37f7dff\n"
        "No. Feature               Bit : State\n"
        "00. DPM_PREFETCHER       ( 0) : enabled\n"
        "01. DPM_GFXCLK           ( 1) : enabled\n"
        "02. DPM_GFX_GPO          ( 2) : enabled\n"
        "03. DPM_UCLK             ( 3) : enabled\n"
        "04. DPM_FCLK             ( 4) : enabled\n"
        "05. DPM_SOCCLK           ( 5) : enabled\n"
        "06. DPM_MP0CLK           ( 6) : enabled\n"
        "07. DPM_LINK             ( 7) : enabled\n"
        "08. DPM_DCEFCLK          ( 8) : enabled\n"
        "09. DPM_XGMI             ( 9) : disabled\n"
        "10. MEM_VDDCI_SCALING    (10) : enabled\n"
        "11. MEM_MVDD_SCALING     (11) : enabled\n"
        "12. DS_GFXCLK            (12) : enabled\n"
        "13. DS_SOCCLK            (13) : enabled\n"
        "14. DS_FCLK              (14) : enabled\n"
        "15. DS_LCLK              (15) : disabled\n"
        "16. DS_DCEFCLK           (16) : enabled\n"
        "17. DS_UCLK              (17) : enabled\n"
        "18. GFX_ULV              (18) : enabled\n"
        "19. FW_DSTATE            (19) : enabled\n"
        "20. GFXOFF               (20) : enabled\n"
        "21. BACO                 (21) : enabled\n"
        "22. MM_DPM_PG            (22) : enabled\n"
        "23. PPT                  (24) : enabled\n"
        "24. TDC                  (25) : enabled\n"
        "25. APCC_PLUS            (26) : disabled\n"
        "26. GTHR                 (27) : disabled\n"
        "27. ACDC                 (28) : disabled\n"
        "28. VR0HOT               (29) : enabled\n"
        "29. VR1HOT               (30) : disabled\n"
        "30. FW_CTF               (31) : enabled\n"
        "31. FAN_CONTROL          (32) : enabled\n"
        "32. THERMAL              (33) : enabled\n"
        "33. GFX_DCS              (34) : disabled\n"
        "34. RM                   (35) : disabled\n"
        "35. LED_DISPLAY          (36) : disabled\n"
        "36. GFX_SS               (37) : enabled\n"
        "37. OUT_OF_BAND_MONITOR  (38) : enabled\n"
        "38. TEMP_DEPENDENT_VMIN  (39) : disabled\n"
        "39. MMHUB_PG             (40) : enabled\n"
        "40. ATHUB_PG             (41) : enabled\n"
        "41. APCC_DFLL            (42) : enabled\n"
        "42. RSMU_SMN_CG          (44) : enabled;\n";

    JSON_Object *out = parse_pp_features_sysfs_file(content);

    const char *exp = "{\"raw_value\":25716712242687, \"features\":["
        "{\"name\":\"DPM_PREFETCHER\", \"on\":true},"
        "{\"name\":\"DPM_GFXCLK\", \"on\":true},"
        "{\"name\":\"DPM_GFX_GPO\", \"on\":true},"
        "{\"name\":\"DPM_UCLK\", \"on\":true},"
        "{\"name\":\"DPM_FCLK\", \"on\":true},"
        "{\"name\":\"DPM_SOCCLK\", \"on\":true},"
        "{\"name\":\"DPM_MP0CLK\", \"on\":true},"
        "{\"name\":\"DPM_LINK\", \"on\":true},"
        "{\"name\":\"DPM_DCEFCLK\", \"on\":true},"
        "{\"name\":\"DPM_XGMI\", \"on\":false},"
        "{\"name\":\"MEM_VDDCI_SCALING\", \"on\":true},"
        "{\"name\":\"MEM_MVDD_SCALING\", \"on\":true},"
        "{\"name\":\"DS_GFXCLK\", \"on\":true},"
        "{\"name\":\"DS_SOCCLK\", \"on\":true},"
        "{\"name\":\"DS_FCLK\", \"on\":true},"
        "{\"name\":\"DS_LCLK\", \"on\":false},"
        "{\"name\":\"DS_DCEFCLK\", \"on\":true},"
        "{\"name\":\"DS_UCLK\", \"on\":true},"
        "{\"name\":\"GFX_ULV\", \"on\":true},"
        "{\"name\":\"FW_DSTATE\", \"on\":true},"
        "{\"name\":\"GFXOFF\", \"on\":true},"
        "{\"name\":\"BACO\", \"on\":true},"
        "{\"name\":\"MM_DPM_PG\", \"on\":true},"
        "{},"
        "{\"name\":\"PPT\", \"on\":true},"
        "{\"name\":\"TDC\", \"on\":true},"
        "{\"name\":\"APCC_PLUS\", \"on\":false},"
        "{\"name\":\"GTHR\", \"on\":false},"
        "{\"name\":\"ACDC\", \"on\":false},"
        "{\"name\":\"VR0HOT\", \"on\":true},"
        "{\"name\":\"VR1HOT\", \"on\":false},"
        "{\"name\":\"FW_CTF\", \"on\":true},"
        "{\"name\":\"FAN_CONTROL\", \"on\":true},"
        "{\"name\":\"THERMAL\", \"on\":true},"
        "{\"name\":\"GFX_DCS\", \"on\":false},"
        "{\"name\":\"RM\", \"on\":false},"
        "{\"name\":\"LED_DISPLAY\", \"on\":false},"
        "{\"name\":\"GFX_SS\", \"on\":true},"
        "{\"name\":\"OUT_OF_BAND_MONITOR\", \"on\":true},"
        "{\"name\":\"TEMP_DEPENDENT_VMIN\", \"on\":false},"
        "{\"name\":\"MMHUB_PG\", \"on\":true},"
        "{\"name\":\"ATHUB_PG\", \"on\":true},"
        "{\"name\":\"APCC_DFLL\", \"on\":true},"
        "{},"
        "{\"name\":\"RSMU_SMN_CG\", \"on\":true}"
    "]}";

    JSON_Value *expected = json_parse_string(exp);

    ASSERT_EQ(json_value_equals(json_object_get_wrapping_value(out), expected), 1);

    return TEST_SUCCESS;
}

enum TEST_RESULT test_parse_sysfs_pp_features2()
{
    const char *content =
        "Current ppfeatures: 0x0000000019f0e3cf\n"
        "FEATURES            BITMASK                ENABLEMENT\n"
        "DPM_PREFETCHER      0x0000000000000001      Y\n"
        "GFXCLK_DPM          0x0000000000000002      Y\n"
        "UCLK_DPM            0x0000000000000004      Y\n"
        "SOCCLK_DPM          0x0000000000000008      Y\n"
        "UVD_DPM             0x0000000000000010      N\n"
        "VCE_DPM             0x0000000000000020      N\n"
        "ULV                 0x0000000000000040      Y\n"
        "MP0CLK_DPM          0x0000000000000080      Y\n"
        "LINK_DPM            0x0000000000000100      Y\n"
        "DCEFCLK_DPM         0x0000000000000200      Y\n"
        "GFXCLK_DS           0x0000000000000400      N\n"
        "SOCCLK_DS           0x0000000000000800      N\n"
        "LCLK_DS             0x0000000000001000      N\n"
        "PPT                 0x0000000000002000      Y\n"
        "TDC                 0x0000000000004000      Y\n"
        "THERMAL             0x0000000000008000      Y\n"
        "GFX_PER_CU_CG       0x0000000000010000      N\n"
        "RM                  0x0000000000020000      N\n"
        "DCEFCLK_DS          0x0000000000040000      N\n"
        "ACDC                0x0000000000080000      N\n"
        "VR0HOT              0x0000000000100000      Y\n"
        "VR1HOT              0x0000000000200000      Y\n"
        "FW_CTF              0x0000000000400000      Y\n"
        "LED_DISPLAY         0x0000000000800000      Y\n"
        "FAN_CONTROL         0x0000000001000000      Y\n"
        "GFX_EDC             0x0000000002000000      N\n"
        "GFXOFF              0x0000000004000000      N\n"
        "CG                  0x0000000008000000      Y\n"
        "FCLK_DPM            0x0000000010000000      Y\n"
        "FCLK_DS             0x0000000020000000      N\n"
        "MP1CLK_DS           0x0000000040000000      N\n"
        "MP0CLK_DS           0x0000000080000000      N\n"
        "XGMI                0x0000000100000000      N\n"
        "ECC                 0x0000000200000000      N\n";

    JSON_Object *out = parse_pp_features_sysfs_file(content);

    const char *exp = "{\"raw_value\":435217359, \"features\":["
        "{\"name\":\"DPM_PREFETCHER\", \"on\":true},"
        "{\"name\":\"GFXCLK_DPM\", \"on\":true},"
        "{\"name\":\"UCLK_DPM\", \"on\":true},"
        "{\"name\":\"SOCCLK_DPM\", \"on\":true},"
        "{\"name\":\"UVD_DPM\", \"on\":false},"
        "{\"name\":\"VCE_DPM\", \"on\":false},"
        "{\"name\":\"ULV\", \"on\":true},"
        "{\"name\":\"MP0CLK_DPM\", \"on\":true},"
        "{\"name\":\"LINK_DPM\", \"on\":true},"
        "{\"name\":\"DCEFCLK_DPM\", \"on\":true},"
        "{\"name\":\"GFXCLK_DS\", \"on\":false},"
        "{\"name\":\"SOCCLK_DS\", \"on\":false},"
        "{\"name\":\"LCLK_DS\", \"on\":false},"
        "{\"name\":\"PPT\", \"on\":true},"
        "{\"name\":\"TDC\", \"on\":true},"
        "{\"name\":\"THERMAL\", \"on\":true},"
        "{\"name\":\"GFX_PER_CU_CG\", \"on\":false},"
        "{\"name\":\"RM\", \"on\":false},"
        "{\"name\":\"DCEFCLK_DS\", \"on\":false},"
        "{\"name\":\"ACDC\", \"on\":false},"
        "{\"name\":\"VR0HOT\", \"on\":true},"
        "{\"name\":\"VR1HOT\", \"on\":true},"
        "{\"name\":\"FW_CTF\", \"on\":true},"
        "{\"name\":\"LED_DISPLAY\", \"on\":true},"
        "{\"name\":\"FAN_CONTROL\", \"on\":true},"
        "{\"name\":\"GFX_EDC\", \"on\":false},"
        "{\"name\":\"GFXOFF\", \"on\":false},"
        "{\"name\":\"CG\", \"on\":true},"
        "{\"name\":\"FCLK_DPM\", \"on\":true},"
        "{\"name\":\"FCLK_DS\", \"on\":false},"
        "{\"name\":\"MP1CLK_DS\", \"on\":false},"
        "{\"name\":\"MP0CLK_DS\", \"on\":false},"
        "{\"name\":\"XGMI\", \"on\":false},"
        "{\"name\":\"ECC\", \"on\":false}"
    "]}";

    JSON_Value *expected = json_parse_string(exp);

    ASSERT_EQ(json_value_equals(json_object_get_wrapping_value(out), expected), 1);

    return TEST_SUCCESS;
}

DEFINE_TESTS(server_tests)
TEST(test_parse_sysfs_clock_file, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_fence_info, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_vm_info, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_sysfs_framebuffer, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_sysfs_state, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_sysfs_pp_features, "navi_reg_only.envdef", "navi10"),
TEST(test_parse_sysfs_pp_features2, "navi_reg_only.envdef", "navi10"),
END_TESTS(server_tests);

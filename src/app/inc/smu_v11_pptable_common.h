/*
 * Copyright 2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef SMU_11_PPTABLE_COMMON_H
#define SMU_11_PPTABLE_COMMON_H

#define SMU_11_0_MAX_PPCLOCK      16          //Maximum Number of PP Clocks
#define SMU_11_0_MAX_ODSETTING    32          //Maximum Number of ODSettings
#define SMU_11_0_MAX_ODFEATURE    32          //Maximum Number of OD Features

#define NUM_GFXCLK_DPM_LEVELS  16
#define NUM_VCLK_DPM_LEVELS    8
#define NUM_DCLK_DPM_LEVELS    8
#define NUM_ECLK_DPM_LEVELS    8
#define NUM_MP0CLK_DPM_LEVELS  2
#define NUM_SOCCLK_DPM_LEVELS  8
#define NUM_UCLK_DPM_LEVELS    4
#define NUM_FCLK_DPM_LEVELS    8
#define NUM_DCEFCLK_DPM_LEVELS 8
#define NUM_DISPCLK_DPM_LEVELS 8
#define NUM_PIXCLK_DPM_LEVELS  8
#define NUM_PHYCLK_DPM_LEVELS  8
#define NUM_LINK_LEVELS        2
#define NUM_XGMI_LEVELS        2

#define NUM_I2C_CONTROLLERS                8
typedef enum {
  POWER_SOURCE_AC,
  POWER_SOURCE_DC,
  POWER_SOURCE_COUNT,
} POWER_SOURCE_e;

typedef enum {
  VOLTAGE_MODE_AVFS = 0,
  VOLTAGE_MODE_AVFS_SS,
  VOLTAGE_MODE_SS,
  VOLTAGE_MODE_COUNT,
} VOLTAGE_MODE_e;

typedef enum {
  AVFS_VOLTAGE_GFX = 0,
  AVFS_VOLTAGE_SOC,
  AVFS_VOLTAGE_COUNT,
} AVFS_VOLTAGE_TYPE_e;

//Only Clks that have DPM descriptors are listed here
typedef enum {
  PPCLK_GFXCLK = 0,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_DCLK,
  PPCLK_VCLK,
  PPCLK_DCEFCLK,
  PPCLK_DISPCLK,
  PPCLK_PIXCLK,
  PPCLK_PHYCLK,
  PPCLK_COUNT,
} PPCLK_e;

typedef enum {
  I2C_CONTROLLER_NAME_VR_GFX = 0,
  I2C_CONTROLLER_NAME_VR_SOC,
  I2C_CONTROLLER_NAME_VR_VDDCI,
  I2C_CONTROLLER_NAME_VR_MVDD,
  I2C_CONTROLLER_NAME_LIQUID0,
  I2C_CONTROLLER_NAME_LIQUID1,
  I2C_CONTROLLER_NAME_PLX,
  I2C_CONTROLLER_NAME_SPARE,
  I2C_CONTROLLER_NAME_COUNT,
} I2cControllerName_e;

typedef enum  {
  PPT_THROTTLER_PPT0,
  PPT_THROTTLER_PPT1,
  PPT_THROTTLER_PPT2,
  PPT_THROTTLER_PPT3,
  PPT_THROTTLER_COUNT,
} PPT_THROTTLER_e;

typedef struct {
	uint32_t m;
	uint32_t b;
} LinearInt_t;

typedef struct {
	uint32_t a;
	uint32_t b;
	uint32_t c;
} QuadraticInt_t;

typedef struct {
  uint32_t a;
  uint32_t b;
  uint32_t c;
} DroopInt_t;

typedef struct {
  uint8_t        VoltageMode;
  uint8_t        SnapToDiscrete;
  uint8_t        NumDiscreteLevels;
  uint8_t        padding;
  LinearInt_t    ConversionToAvfsClk;
  QuadraticInt_t SsCurve;
} DpmDescriptor_t;

struct atom_common_table_header
{
	uint16_t structuresize;
	uint8_t  format_revision;
	uint8_t  content_revision;
};

typedef struct {
  uint8_t   Enabled;
  uint8_t   Speed;
  uint8_t   Padding[2];
  uint32_t  SlaveAddress;
  uint8_t   ControllerPort;
  uint8_t   ControllerName;
  uint8_t   ThermalThrotter;
  uint8_t   I2cProtocol;
} I2cControllerConfig_t;

struct smu_11_0_power_saving_clock_table
{
	uint8_t  revision;                                        //Revision = SMU_11_0_PP_POWERSAVINGCLOCK_VERSION
	uint8_t  reserve[3];                                      //Zero filled field reserved for future use
	uint32_t count;                                           //power_saving_clock_count = SMU_11_0_PPCLOCK_COUNT
	uint32_t max[SMU_11_0_MAX_PPCLOCK];                       //PowerSavingClock Mode Clock Maximum array In MHz
	uint32_t min[SMU_11_0_MAX_PPCLOCK];                       //PowerSavingClock Mode Clock Minimum array In MHz
} __attribute__((packed));

struct smu_11_0_overdrive_table
{
    uint8_t  revision;                                        //Revision = SMU_11_0_PP_OVERDRIVE_VERSION
    uint8_t  reserve[3];                                      //Zero filled field reserved for future use
    uint32_t feature_count;                                   //Total number of supported features
    uint32_t setting_count;                                   //Total number of supported settings
    uint8_t  cap[SMU_11_0_MAX_ODFEATURE];                     //OD feature support flags
    uint32_t max[SMU_11_0_MAX_ODSETTING];                     //default maximum settings
    uint32_t min[SMU_11_0_MAX_ODSETTING];                     //default minimum settings
} __attribute__((packed));

struct smu_11_0_powerplay_table
{
	struct atom_common_table_header header;
	uint8_t  table_revision;
	uint16_t table_size;                          //Driver portion table size. The offset to smc_pptable including header size
	uint32_t golden_pp_id;
	uint32_t golden_revision;
	uint16_t format_id;
	uint32_t platform_caps;                       //POWERPLAYABLE::ulPlatformCaps

	uint8_t  thermal_controller_type;             //one of SMU_11_0_PP_THERMALCONTROLLER

	uint16_t small_power_limit1;
	uint16_t small_power_limit2;
	uint16_t boost_power_limit;
	uint16_t od_turbo_power_limit;                //Power limit setting for Turbo mode in Performance UI Tuning.
	uint16_t od_power_save_power_limit;           //Power limit setting for PowerSave/Optimal mode in Performance UI Tuning.
	uint16_t software_shutdown_temp;

	uint16_t reserve[6];                          //Zero filled field reserved for future use

	struct smu_11_0_power_saving_clock_table      power_saving_clock;
	struct smu_11_0_overdrive_table               overdrive_table;
} __attribute__((packed));

int umr_navi10_pptable_print(const char* param, FILE* fp);
#endif

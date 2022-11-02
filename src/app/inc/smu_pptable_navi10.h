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

#ifndef SMU_PPTABLE_NAVI10_H
#define SMU_PPTABLE_NAVI10_H
#include "smu_v11_pptable_common.h"

struct Navi10_PPTable_t{
	uint32_t Version;

	// SECTION: Feature Enablement
	uint32_t FeaturesToRun[2];

	// SECTION: Infrastructure Limits
	uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT];
	uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT];
	uint16_t SocketPowerLimitDc[PPT_THROTTLER_COUNT];
	uint16_t SocketPowerLimitDcTau[PPT_THROTTLER_COUNT];

	uint16_t TdcLimitSoc;             // Amps
	uint16_t TdcLimitSocTau;          // Time constant of LPF in ms
	uint16_t TdcLimitGfx;             // Amps
	uint16_t TdcLimitGfxTau;          // Time constant of LPF in ms

	uint16_t TedgeLimit;              // Celcius
	uint16_t ThotspotLimit;           // Celcius
	uint16_t TmemLimit;               // Celcius
	uint16_t Tvr_gfxLimit;            // Celcius
	uint16_t Tvr_mem0Limit;           // Celcius
	uint16_t Tvr_mem1Limit;           // Celcius
	uint16_t Tvr_socLimit;            // Celcius
	uint16_t Tliquid0Limit;           // Celcius
	uint16_t Tliquid1Limit;           // Celcius
	uint16_t TplxLimit;               // Celcius
	uint32_t FitLimit;                // Failures in time (failures per million parts over the defined lifetime)

	uint16_t PpmPowerLimit;           // Switch this this power limit when temperature is above PpmTempThreshold
	uint16_t PpmTemperatureThreshold;

	// SECTION: Throttler settings
	uint32_t ThrottlerControlMask;   // See Throtter masks defines

	// SECTION: FW DSTATE Settings
	uint32_t FwDStateMask;           // See FW DState masks defines

	// SECTION: ULV Settings
	uint16_t  UlvVoltageOffsetSoc; // In mV(Q2)
	uint16_t  UlvVoltageOffsetGfx; // In mV(Q2)

	uint8_t   GceaLinkMgrIdleThreshold;        //Set by SMU FW during enablment of SOC_ULV. Controls delay for GFX SDP port disconnection during idle events
	uint8_t   paddingRlcUlvParams[3];

	uint8_t  UlvSmnclkDid;     //DID for ULV mode. 0 means CLK will not be modified in ULV.
	uint8_t  UlvMp1clkDid;     //DID for ULV mode. 0 means CLK will not be modified in ULV.
	uint8_t  UlvGfxclkBypass;  // 1 to turn off/bypass Gfxclk during ULV, 0 to leave Gfxclk on during ULV
	uint8_t  Padding234;

	uint16_t     MinVoltageUlvGfx; // In mV(Q2)  Minimum Voltage ("Vmin") of VDD_GFX in ULV mode
	uint16_t     MinVoltageUlvSoc; // In mV(Q2)  Minimum Voltage ("Vmin") of VDD_SOC in ULV mode

	// SECTION: Voltage Control Parameters
	uint16_t     MinVoltageGfx;     // In mV(Q2) Minimum Voltage ("Vmin") of VDD_GFX
	uint16_t     MinVoltageSoc;     // In mV(Q2) Minimum Voltage ("Vmin") of VDD_SOC
	uint16_t     MaxVoltageGfx;     // In mV(Q2) Maximum Voltage allowable of VDD_GFX
	uint16_t     MaxVoltageSoc;     // In mV(Q2) Maximum Voltage allowable of VDD_SOC

	uint16_t     LoadLineResistanceGfx;   // In mOhms with 8 fractional bits
	uint16_t     LoadLineResistanceSoc;   // In mOhms with 8 fractional bits

	//SECTION: DPM Config 1
	DpmDescriptor_t DpmDescriptor[PPCLK_COUNT];

	uint16_t       FreqTableGfx      [NUM_GFXCLK_DPM_LEVELS  ];     // In MHz
	uint16_t       FreqTableVclk     [NUM_VCLK_DPM_LEVELS    ];     // In MHz
	uint16_t       FreqTableDclk     [NUM_DCLK_DPM_LEVELS    ];     // In MHz
	uint16_t       FreqTableSocclk   [NUM_SOCCLK_DPM_LEVELS  ];     // In MHz
	uint16_t       FreqTableUclk     [NUM_UCLK_DPM_LEVELS    ];     // In MHz
	uint16_t       FreqTableDcefclk  [NUM_DCEFCLK_DPM_LEVELS ];     // In MHz
	uint16_t       FreqTableDispclk  [NUM_DISPCLK_DPM_LEVELS ];     // In MHz
	uint16_t       FreqTablePixclk   [NUM_PIXCLK_DPM_LEVELS  ];     // In MHz
	uint16_t       FreqTablePhyclk   [NUM_PHYCLK_DPM_LEVELS  ];     // In MHz
	uint32_t       Paddingclks[16];

	uint16_t       DcModeMaxFreq     [PPCLK_COUNT            ];     // In MHz
	uint16_t       Padding8_Clks;

	uint8_t        FreqTableUclkDiv  [NUM_UCLK_DPM_LEVELS    ];     // 0:Div-1, 1:Div-1/2, 2:Div-1/4, 3:Div-1/8

	// SECTION: DPM Config 2
	uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];       // in MHz
	uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];       // mV(Q2)
	uint16_t       MemVddciVoltage   [NUM_UCLK_DPM_LEVELS];         // mV(Q2)
	uint16_t       MemMvddVoltage    [NUM_UCLK_DPM_LEVELS];         // mV(Q2)
	// GFXCLK DPM
	uint16_t        GfxclkFgfxoffEntry;   // in Mhz
	uint16_t        GfxclkFinit;          // in Mhz
	uint16_t        GfxclkFidle;          // in MHz
	uint16_t        GfxclkSlewRate;       // for PLL babystepping???
	uint16_t        GfxclkFopt;           // in Mhz
	uint8_t         Padding567[2];
	uint16_t        GfxclkDsMaxFreq;      // in MHz
	uint8_t         GfxclkSource;         // 0 = PLL, 1 = DFLL
	uint8_t         Padding456;

	// UCLK section
	uint8_t      LowestUclkReservedForUlv; // Set this to 1 if UCLK DPM0 is reserved for ULV-mode only
	uint8_t      paddingUclk[3];

	uint8_t      MemoryType;          // 0-GDDR6, 1-HBM
	uint8_t      MemoryChannels;
	uint8_t      PaddingMem[2];

	// Link DPM Settings
	uint8_t      PcieGenSpeed[NUM_LINK_LEVELS];           ///< 0:PciE-gen1 1:PciE-gen2 2:PciE-gen3 3:PciE-gen4
	uint8_t      PcieLaneCount[NUM_LINK_LEVELS];          ///< 1=x1, 2=x2, 3=x4, 4=x8, 5=x12, 6=x16
	uint16_t     LclkFreq[NUM_LINK_LEVELS];

	// GFXCLK Thermal DPM (formerly 'Boost' Settings)
	uint16_t     EnableTdpm;
	uint16_t     TdpmHighHystTemperature;
	uint16_t     TdpmLowHystTemperature;
	uint16_t     GfxclkFreqHighTempLimit; // High limit on GFXCLK when temperature is high, for reliability.

	// SECTION: Fan Control
	uint16_t     FanStopTemp;          //Celcius
	uint16_t     FanStartTemp;         //Celcius

	uint16_t     FanGainEdge;
	uint16_t     FanGainHotspot;
	uint16_t     FanGainLiquid0;
	uint16_t     FanGainLiquid1;
	uint16_t     FanGainVrGfx;
	uint16_t     FanGainVrSoc;
	uint16_t     FanGainVrMem0;
	uint16_t     FanGainVrMem1;
	uint16_t     FanGainPlx;
	uint16_t     FanGainMem;
	uint16_t     FanPwmMin;
	uint16_t     FanAcousticLimitRpm;
	uint16_t     FanThrottlingRpm;
	uint16_t     FanMaximumRpm;
	uint16_t     FanTargetTemperature;
	uint16_t     FanTargetGfxclk;
	uint8_t      FanTempInputSelect;
	uint8_t      FanPadding;
	uint8_t      FanZeroRpmEnable;
	uint8_t      FanTachEdgePerRev;
	//uint8_t      padding8_Fan[2];

	// The following are AFC override parameters. Leave at 0 to use FW defaults.
	int16_t      FuzzyFan_ErrorSetDelta;
	int16_t      FuzzyFan_ErrorRateSetDelta;
	int16_t      FuzzyFan_PwmSetDelta;
	uint16_t     FuzzyFan_Reserved;

	// SECTION: AVFS
	// Overrides
	uint8_t           OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
	uint8_t           Padding8_Avfs[2];

	QuadraticInt_t    qAvfsGb[AVFS_VOLTAGE_COUNT];              // GHz->V Override of fused curve
	DroopInt_t        dBtcGbGfxPll;         // GHz->V BtcGb
	DroopInt_t        dBtcGbGfxDfll;        // GHz->V BtcGb
	DroopInt_t        dBtcGbSoc;            // GHz->V BtcGb
	LinearInt_t       qAgingGb[AVFS_VOLTAGE_COUNT];          // GHz->V

	QuadraticInt_t    qStaticVoltageOffset[AVFS_VOLTAGE_COUNT]; // GHz->V

	uint16_t          DcTol[AVFS_VOLTAGE_COUNT];            // mV Q2

	uint8_t           DcBtcEnabled[AVFS_VOLTAGE_COUNT];
	uint8_t           Padding8_GfxBtc[2];

	uint16_t          DcBtcMin[AVFS_VOLTAGE_COUNT];       // mV Q2
	uint16_t          DcBtcMax[AVFS_VOLTAGE_COUNT];       // mV Q2

	// SECTION: Advanced Options
	uint32_t          DebugOverrides;
	QuadraticInt_t    ReservedEquation0;
	QuadraticInt_t    ReservedEquation1;
	QuadraticInt_t    ReservedEquation2;
	QuadraticInt_t    ReservedEquation3;

	// Total Power configuration, use defines from PwrConfig_e
	uint8_t      TotalPowerConfig;    //0-TDP, 1-TGP, 2-TCP Estimated, 3-TCP Measured
	uint8_t      TotalPowerSpare1;
	uint16_t     TotalPowerSpare2;

	// APCC Settings
	uint16_t     PccThresholdLow;
	uint16_t     PccThresholdHigh;
	uint32_t     MGpuFanBoostLimitRpm;
	uint32_t     PaddingAPCC[5];

	// Temperature Dependent Vmin
	uint16_t     VDDGFX_TVmin;       //Celcius
	uint16_t     VDDSOC_TVmin;       //Celcius
	uint16_t     VDDGFX_Vmin_HiTemp; // mV Q2
	uint16_t     VDDGFX_Vmin_LoTemp; // mV Q2
	uint16_t     VDDSOC_Vmin_HiTemp; // mV Q2
	uint16_t     VDDSOC_Vmin_LoTemp; // mV Q2

	uint16_t     VDDGFX_TVminHystersis; // Celcius
	uint16_t     VDDSOC_TVminHystersis; // Celcius

	// BTC Setting
	uint32_t     BtcConfig;

	 uint16_t     SsFmin[10]; // PPtable value to function similar to VFTFmin for SS Curve; Size is PPCLK_COUNT rounded to nearest multiple of 2
	uint16_t     DcBtcGb[AVFS_VOLTAGE_COUNT];

	// SECTION: Board Reserved
	uint32_t     Reserved[8];

	// SECTION: BOARD PARAMETERS
	// I2C Control
	I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];

	// SVI2 Board Parameters
	uint16_t     MaxVoltageStepGfx; // In mV(Q2) Max voltage step that SMU will request. Multiple steps are taken if voltage change exceeds this value.
	uint16_t     MaxVoltageStepSoc; // In mV(Q2) Max voltage step that SMU will request. Multiple steps are taken if voltage change exceeds this value.

	uint8_t      VddGfxVrMapping;   // Use VR_MAPPING* bitfields
	uint8_t      VddSocVrMapping;   // Use VR_MAPPING* bitfields
	uint8_t      VddMem0VrMapping;  // Use VR_MAPPING* bitfields
	uint8_t      VddMem1VrMapping;  // Use VR_MAPPING* bitfields

	uint8_t      GfxUlvPhaseSheddingMask; // set this to 1 to set PSI0/1 to 1 in ULV mode
	uint8_t      SocUlvPhaseSheddingMask; // set this to 1 to set PSI0/1 to 1 in ULV mode
	uint8_t      ExternalSensorPresent; // External RDI connected to TMON (aka TEMP IN)
	uint8_t      Padding8_V;

	// Telemetry Settings
	uint16_t     GfxMaxCurrent;   // in Amps
	int8_t       GfxOffset;       // in Amps
	uint8_t      Padding_TelemetryGfx;

	uint16_t     SocMaxCurrent;   // in Amps
	int8_t       SocOffset;       // in Amps
	uint8_t      Padding_TelemetrySoc;

	uint16_t     Mem0MaxCurrent;   // in Amps
	int8_t       Mem0Offset;       // in Amps
	uint8_t      Padding_TelemetryMem0;

	uint16_t     Mem1MaxCurrent;   // in Amps
	int8_t       Mem1Offset;       // in Amps
	uint8_t      Padding_TelemetryMem1;

	// GPIO Settings
	uint8_t      AcDcGpio;        // GPIO pin configured for AC/DC switching
	uint8_t      AcDcPolarity;    // GPIO polarity for AC/DC switching
	uint8_t      VR0HotGpio;      // GPIO pin configured for VR0 HOT event
	uint8_t      VR0HotPolarity;  // GPIO polarity for VR0 HOT event

	uint8_t      VR1HotGpio;      // GPIO pin configured for VR1 HOT event
	uint8_t      VR1HotPolarity;  // GPIO polarity for VR1 HOT event
	uint8_t      GthrGpio;        // GPIO pin configured for GTHR Event
	uint8_t      GthrPolarity;    // replace GPIO polarity for GTHR

	// LED Display Settings
	uint8_t      LedPin0;         // GPIO number for LedPin[0]
	uint8_t      LedPin1;         // GPIO number for LedPin[1]
	uint8_t      LedPin2;         // GPIO number for LedPin[2]
	uint8_t      padding8_4;

	// GFXCLK PLL Spread Spectrum
	uint8_t      PllGfxclkSpreadEnabled;   // on or off
	uint8_t      PllGfxclkSpreadPercent;   // Q4.4
	uint16_t     PllGfxclkSpreadFreq;      // kHz

	// GFXCLK DFLL Spread Spectrum
	uint8_t      DfllGfxclkSpreadEnabled;   // on or off
	uint8_t      DfllGfxclkSpreadPercent;   // Q4.4
	uint16_t     DfllGfxclkSpreadFreq;      // kHz

	// UCLK Spread Spectrum
	uint8_t      UclkSpreadEnabled;   // on or off
	uint8_t      UclkSpreadPercent;   // Q4.4
	uint16_t     UclkSpreadFreq;      // kHz

	// SOCCLK Spread Spectrum
	uint8_t      SoclkSpreadEnabled;   // on or off
	uint8_t      SocclkSpreadPercent;   // Q4.4
	uint16_t     SocclkSpreadFreq;      // kHz

	// Total board power
	uint16_t     TotalBoardPower;     //Only needed for TCP Estimated case, where TCP = TGP+Total Board Power
	uint16_t     BoardPadding;

	// Mvdd Svi2 Div Ratio Setting
	uint32_t     MvddRatio; // This is used for MVDD Vid workaround. It has 16 fractional bits (Q16.16)

	uint8_t      RenesesLoadLineEnabled;
	uint8_t      GfxLoadlineResistance;
	uint8_t      SocLoadlineResistance;
	uint8_t      Padding8_Loadline;

	uint32_t     BoardReserved[8];

	// Padding for MMHUB - do not modify this
	uint32_t     MmHubPadding[8]; // SMU internal use

} __attribute__((packed));
#endif

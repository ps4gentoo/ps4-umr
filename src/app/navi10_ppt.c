/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
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
#include "umrapp.h"
#include "smu_v11_pptable_common.h"
#include "smu_pptable_navi10.h"

struct smu_11_0_powerplay_table pp_table_header;
struct Navi10_PPTable_t pp_table;

struct {
	char* name;
	void* address;
	uint32_t len;
} navi10_pp_table[] = {
	{ "header.structuresize", &pp_table_header.header.structuresize, 16 },
	{ "header.format_revision", &pp_table_header.header.format_revision, 8},
	{ "header.content_revision", &pp_table_header.header.content_revision, 8},
	{ "power_saving_clock.revision", &pp_table_header.power_saving_clock.revision, 8},
	{ "power_saving_clock.count", &pp_table_header.power_saving_clock.count, 32},
	{ "overdrive_table.revision", &pp_table_header.overdrive_table.revision, 8},
	{ "overdrive_table.feature_count", &pp_table_header.overdrive_table.feature_count, 32},
	{ "overdrive_table.setting_count", &pp_table_header.overdrive_table.setting_count, 32},
	{ "table_revision", &pp_table_header.table_revision, 8},
	{ "table_size", &pp_table_header.table_size, 16},
	{ "golden_pp_id", &pp_table_header.golden_pp_id, 32},
	{ "format_id", &pp_table_header.format_id, 16},
	{ "platform_caps", &pp_table_header.platform_caps, 32},
	{ "thermal_controller_type", &pp_table_header.thermal_controller_type, 8},
	{ "small_power_limit1", &pp_table_header.small_power_limit1, 16},
	{ "small_power_limit2", &pp_table_header.small_power_limit2, 16},
	{ "boost_power_limit", &pp_table_header.boost_power_limit, 16},
	{ "od_turbo_power_limit", &pp_table_header.od_turbo_power_limit, 16},
	{ "od_power_save_power_limit", &pp_table_header.od_power_save_power_limit, 16},
	{ "software_shutdown_temp", &pp_table_header.software_shutdown_temp, 16},
	{ "Version", &pp_table.Version, 32},
	{ "FeaturesToRun[0]", &pp_table.FeaturesToRun[0], 32},
	{ "FeaturesToRun[1]", &pp_table.FeaturesToRun[1], 32},
	{ "SocketPowerLimitAc[0]", &pp_table.SocketPowerLimitAc[0], 16},
	{ "SocketPowerLimitAc[1]", &pp_table.SocketPowerLimitAc[1], 16},
	{ "SocketPowerLimitAc[2]", &pp_table.SocketPowerLimitAc[2], 16},
	{ "SocketPowerLimitAc[3]", &pp_table.SocketPowerLimitAc[3], 16},
	{ "SocketPowerLimitAcTau[0]", &pp_table.SocketPowerLimitAcTau[0], 16},
	{ "SocketPowerLimitAcTau[1]", &pp_table.SocketPowerLimitAcTau[1], 16},
	{ "SocketPowerLimitAcTau[2]", &pp_table.SocketPowerLimitAcTau[2], 16},
	{ "SocketPowerLimitAcTau[3]", &pp_table.SocketPowerLimitAcTau[3], 16},
	{ "SocketPowerLimitDc[0]", &pp_table.SocketPowerLimitDc[0], 16},
	{ "SocketPowerLimitDc[1]", &pp_table.SocketPowerLimitDc[1], 16},
	{ "SocketPowerLimitDc[2]", &pp_table.SocketPowerLimitDc[2], 16},
	{ "SocketPowerLimitDc[3]", &pp_table.SocketPowerLimitDc[3], 16},
	{ "SocketPowerLimitDcTau[0]", &pp_table.SocketPowerLimitDcTau[0], 16},
	{ "SocketPowerLimitDcTau[1]", &pp_table.SocketPowerLimitDcTau[1], 16},
	{ "SocketPowerLimitDcTau[2]", &pp_table.SocketPowerLimitDcTau[2], 16},
	{ "SocketPowerLimitDcTau[3]", &pp_table.SocketPowerLimitDcTau[3], 16},
	{ "TdcLimitSoc", &pp_table.TdcLimitSoc, 16},
	{ "TdcLimitSocTau", &pp_table.TdcLimitSocTau, 16},
	{ "TdcLimitGfx", &pp_table.TdcLimitGfx, 16},
	{ "TedgeLimit", &pp_table.TedgeLimit, 16},
	{ "ThotspotLimit", &pp_table.ThotspotLimit, 16},
	{ "TmemLimit", &pp_table.TmemLimit, 16},
	{ "Tvr_gfxLimit", &pp_table.Tvr_gfxLimit, 16},
	{ "Tvr_mem0Limit", &pp_table.Tvr_mem0Limit, 16},
	{ "Tvr_mem1Limit", &pp_table.Tvr_mem1Limit, 16},
	{ "Tvr_socLimit", &pp_table.Tvr_socLimit, 16},
	{ "Tliquid0Limit", &pp_table.Tliquid0Limit, 16},
	{ "Tliquid1Limit", &pp_table.Tliquid1Limit, 16},
	{ "TplxLimit", &pp_table.TplxLimit, 16},
	{ "FitLimit", &pp_table.FitLimit, 16},
	{ "PpmPowerLimit", &pp_table.PpmPowerLimit, 16},
	{ "PpmTemperatureThreshold", &pp_table.PpmTemperatureThreshold, 16},
	{ "ThrottlerControlMask", &pp_table.ThrottlerControlMask, 32},
	{ "FwDStateMask", &pp_table.FwDStateMask, 32},
	{ "UlvVoltageOffsetSoc", &pp_table.UlvVoltageOffsetSoc, 16},
	{ "UlvVoltageOffsetGfx", &pp_table.UlvVoltageOffsetGfx, 16},
	{ "GceaLinkMgrIdleThreshold", &pp_table.GceaLinkMgrIdleThreshold, 8},
	{ "UlvSmnclkDid", &pp_table.UlvSmnclkDid, 8},
	{ "UlvMp1clkDid", &pp_table.UlvMp1clkDid, 8},
	{ "UlvGfxclkBypass", &pp_table.UlvGfxclkBypass, 8},
	{ "MinVoltageUlvGfx", &pp_table.MinVoltageUlvGfx, 16},
	{ "MinVoltageUlvSoc", &pp_table.MinVoltageUlvSoc, 16},
	{ "MinVoltageGfx", &pp_table.MinVoltageSoc, 16},
	{ "MaxVoltageGfx", &pp_table.MaxVoltageGfx, 16},
	{ "MaxVoltageSoc", &pp_table.MaxVoltageSoc, 16},
	{ "LoadLineResistanceGfx", &pp_table.LoadLineResistanceGfx, 16},
	{ "LoadLineResistanceSoc", &pp_table.LoadLineResistanceSoc, 16},
	{ "FreqTableGfx[0]", &pp_table.FreqTableGfx[0], 16},
	{ "FreqTableGfx[1]", &pp_table.FreqTableGfx[1], 16},
	{ "FreqTableGfx[2]", &pp_table.FreqTableGfx[2], 16},
	{ "FreqTableGfx[3]", &pp_table.FreqTableGfx[3], 16},
	{ "FreqTableGfx[4]", &pp_table.FreqTableGfx[4], 16},
	{ "FreqTableGfx[5]", &pp_table.FreqTableGfx[5], 16},
	{ "FreqTableGfx[6]", &pp_table.FreqTableGfx[6], 16},
	{ "FreqTableGfx[7]", &pp_table.FreqTableGfx[7], 16},
	{ "FreqTableGfx[8]", &pp_table.FreqTableGfx[8], 16},
	{ "FreqTableGfx[9]", &pp_table.FreqTableGfx[9], 16},
	{ "FreqTableGfx[10]", &pp_table.FreqTableGfx[10], 16},
	{ "FreqTableGfx[11]", &pp_table.FreqTableGfx[11], 16},
	{ "FreqTableGfx[12]", &pp_table.FreqTableGfx[12], 16},
	{ "FreqTableGfx[13]", &pp_table.FreqTableGfx[13], 16},
	{ "FreqTableGfx[14]", &pp_table.FreqTableGfx[14], 16},
	{ "FreqTableGfx[15]", &pp_table.FreqTableGfx[15], 16},
	{ "FreqTableVclk[0]", &pp_table.FreqTableVclk[0], 16},
	{ "FreqTableVclk[1]", &pp_table.FreqTableVclk[1], 16},
	{ "FreqTableVclk[2]", &pp_table.FreqTableVclk[2], 16},
	{ "FreqTableVclk[3]", &pp_table.FreqTableVclk[3], 16},
	{ "FreqTableVclk[4]", &pp_table.FreqTableVclk[4], 16},
	{ "FreqTableVclk[5]", &pp_table.FreqTableVclk[5], 16},
	{ "FreqTableVclk[6]", &pp_table.FreqTableVclk[6], 16},
	{ "FreqTableVclk[7]", &pp_table.FreqTableVclk[7], 16},
	{ "FreqTableDclk[0]", &pp_table.FreqTableDclk[0], 16},
	{ "FreqTableDclk[1]", &pp_table.FreqTableDclk[1], 16},
	{ "FreqTableDclk[2]", &pp_table.FreqTableDclk[2], 16},
	{ "FreqTableDclk[3]", &pp_table.FreqTableDclk[3], 16},
	{ "FreqTableDclk[4]", &pp_table.FreqTableDclk[4], 16},
	{ "FreqTableDclk[5]", &pp_table.FreqTableDclk[5], 16},
	{ "FreqTableDclk[6]", &pp_table.FreqTableDclk[6], 16},
	{ "FreqTableDclk[7]", &pp_table.FreqTableDclk[7], 16},
	{ "FreqTableSocclk[0]", &pp_table.FreqTableSocclk[0], 16},
	{ "FreqTableSocclk[1]", &pp_table.FreqTableSocclk[1], 16},
	{ "FreqTableSocclk[2]", &pp_table.FreqTableSocclk[2], 16},
	{ "FreqTableSocclk[3]", &pp_table.FreqTableSocclk[3], 16},
	{ "FreqTableSocclk[4]", &pp_table.FreqTableSocclk[4], 16},
	{ "FreqTableSocclk[5]", &pp_table.FreqTableSocclk[5], 16},
	{ "FreqTableSocclk[6]", &pp_table.FreqTableSocclk[6], 16},
	{ "FreqTableSocclk[7]", &pp_table.FreqTableSocclk[7], 16},
	{ "FreqTableUclk[0]", &pp_table.FreqTableUclk[0], 16},
	{ "FreqTableUclk[1]", &pp_table.FreqTableUclk[1], 16},
	{ "FreqTableUclk[2]", &pp_table.FreqTableUclk[2], 16},
	{ "FreqTableUclk[3]", &pp_table.FreqTableUclk[3], 16},
	{ "FreqTableDcefclk[0]", &pp_table.FreqTableDcefclk[0], 16},
	{ "FreqTableDcefclk[1]", &pp_table.FreqTableDcefclk[1], 16},
	{ "FreqTableDcefclk[2]", &pp_table.FreqTableDcefclk[2], 16},
	{ "FreqTableDcefclk[3]", &pp_table.FreqTableDcefclk[3], 16},
	{ "FreqTableDcefclk[4]", &pp_table.FreqTableDcefclk[4], 16},
	{ "FreqTableDcefclk[5]", &pp_table.FreqTableDcefclk[5], 16},
	{ "FreqTableDcefclk[6]", &pp_table.FreqTableDcefclk[6], 16},
	{ "FreqTableDcefclk[7]", &pp_table.FreqTableDcefclk[7], 16},
	{ "FreqTableDispclk[0]", &pp_table.FreqTableDispclk[0], 16},
	{ "FreqTableDispclk[1]", &pp_table.FreqTableDispclk[1], 16},
	{ "FreqTableDispclk[2]", &pp_table.FreqTableDispclk[2], 16},
	{ "FreqTableDispclk[3]", &pp_table.FreqTableDispclk[3], 16},
	{ "FreqTableDispclk[4]", &pp_table.FreqTableDispclk[4], 16},
	{ "FreqTableDispclk[5]", &pp_table.FreqTableDispclk[5], 16},
	{ "FreqTableDispclk[6]", &pp_table.FreqTableDispclk[6], 16},
	{ "FreqTableDispclk[7]", &pp_table.FreqTableDispclk[7], 16},
	{ "FreqTablePixclk[0]", &pp_table.FreqTablePixclk[0], 16},
	{ "FreqTablePixclk[1]", &pp_table.FreqTablePixclk[1], 16},
	{ "FreqTablePixclk[2]", &pp_table.FreqTablePixclk[2], 16},
	{ "FreqTablePixclk[3]", &pp_table.FreqTablePixclk[3], 16},
	{ "FreqTablePixclk[4]", &pp_table.FreqTablePixclk[4], 16},
	{ "FreqTablePixclk[5]", &pp_table.FreqTablePixclk[5], 16},
	{ "FreqTablePixclk[6]", &pp_table.FreqTablePixclk[6], 16},
	{ "FreqTablePixclk[7]", &pp_table.FreqTablePixclk[7], 16},
	{ "FreqTablePhyclk[0]", &pp_table.FreqTablePhyclk[0], 16},
	{ "FreqTablePhyclk[1]", &pp_table.FreqTablePhyclk[1], 16},
	{ "FreqTablePhyclk[2]", &pp_table.FreqTablePhyclk[2], 16},
	{ "FreqTablePhyclk[3]", &pp_table.FreqTablePhyclk[3], 16},
	{ "FreqTablePhyclk[4]", &pp_table.FreqTablePhyclk[4], 16},
	{ "FreqTablePhyclk[5]", &pp_table.FreqTablePhyclk[5], 16},
	{ "FreqTablePhyclk[6]", &pp_table.FreqTablePhyclk[6], 16},
	{ "FreqTablePhyclk[7]", &pp_table.FreqTablePhyclk[7], 16},
	{ "FreqTableUclkDiv[0]", &pp_table.FreqTableUclkDiv[0], 8},
	{ "FreqTableUclkDiv[1]", &pp_table.FreqTableUclkDiv[1], 8},
	{ "FreqTableUclkDiv[2]", &pp_table.FreqTableUclkDiv[2], 8},
	{ "FreqTableUclkDiv[3]", &pp_table.FreqTableUclkDiv[3], 8},
	{ "Mp0clkFreq[0]", &pp_table.Mp0clkFreq[0], 16},
	{ "Mp0clkFreq[1]", &pp_table.Mp0clkFreq[1], 16},
	{ "Mp0DpmVoltage[0]", &pp_table.Mp0DpmVoltage[0], 16},
	{ "Mp0DpmVoltage[1]", &pp_table.Mp0DpmVoltage[1], 16},
	{ "MemVddciVoltage[0]", &pp_table.MemVddciVoltage[0], 16},
	{ "MemVddciVoltage[1]", &pp_table.MemVddciVoltage[1], 16},
	{ "MemVddciVoltage[2]", &pp_table.MemVddciVoltage[2], 16},
	{ "MemVddciVoltage[3]", &pp_table.MemVddciVoltage[3], 16},
	{ "MemMvddVoltage[0]", &pp_table.MemMvddVoltage[0], 16},
	{ "MemMvddVoltage[1]", &pp_table.MemMvddVoltage[1], 16},
	{ "MemMvddVoltage[2]", &pp_table.MemMvddVoltage[2], 16},
	{ "MemMvddVoltage[3]", &pp_table.MemMvddVoltage[3], 16},
	{ "GfxclkFgfxoffEntry", &pp_table.GfxclkFgfxoffEntry, 16},
	{ "GfxclkFinit", &pp_table.GfxclkFinit, 16},
	{ "GfxclkFidle", &pp_table.GfxclkFidle, 16},
	{ "GfxclkSlewRate", &pp_table.GfxclkSlewRate, 16},
	{ "GfxclkFopt", &pp_table.GfxclkFopt, 16},
	{ "GfxclkDsMaxFreq", &pp_table.GfxclkDsMaxFreq, 16},
	{ "GfxclkSource", &pp_table.GfxclkSource, 8},
	{ "LowestUclkReservedForUlv", &pp_table.LowestUclkReservedForUlv, 8},
	{ "MemoryType", &pp_table.MemoryType, 8},
	{ "MemoryChannels", &pp_table.MemoryChannels, 8},
	{ "PcieGenSpeed[0]", &pp_table.PcieGenSpeed[0], 8},
	{ "PcieGenSpeed[1]", &pp_table.PcieGenSpeed[1], 8},
	{ "PcieLaneCount[0]", &pp_table.PcieLaneCount[0], 8},
	{ "PcieLaneCount[1]", &pp_table.PcieLaneCount[1], 8},
	{ "LclkFreq[0]", &pp_table.LclkFreq[0], 16},
	{ "LclkFreq[1]", &pp_table.LclkFreq[1], 16},
	{ "EnableTdpm", &pp_table.EnableTdpm, 16},
	{ "TdpmHighHystTemperature", &pp_table.TdpmHighHystTemperature, 16},
	{ "TdpmLowHystTemperature", &pp_table.TdpmLowHystTemperature, 16},
	{ "GfxclkFreqHighTempLimit", &pp_table.GfxclkFreqHighTempLimit, 16},
	{ "FanStopTemp", &pp_table.FanStopTemp, 16},
	{ "FanStartTemp", &pp_table.FanStartTemp, 16},
	{ "FanGainEdge", &pp_table.FanGainEdge, 16},
	{ "FanGainHotspot", &pp_table.FanGainHotspot, 16},
	{ "FanGainLiquid0", &pp_table.FanGainLiquid0, 16},
	{ "FanGainLiquid1", &pp_table.FanGainLiquid1, 16},
	{ "FanGainVrGfx", &pp_table.FanGainVrGfx, 16},
	{ "FanGainVrSoc", &pp_table.FanGainVrSoc, 16},
	{ "FanGainVrMem0", &pp_table.FanGainVrMem0, 16},
	{ "FanGainVrMem1", &pp_table.FanGainVrMem1, 16},
	{ "FanGainPlx", &pp_table.FanGainPlx, 16},
	{ "FanGainMem", &pp_table.FanGainMem, 16},
	{ "FanPwmMin", &pp_table.FanPwmMin, 16},
	{ "FanAcousticLimitRpm", &pp_table.FanAcousticLimitRpm, 16},
	{ "FanThrottlingRpm", &pp_table.FanThrottlingRpm, 16},
	{ "FanMaximumRpm", &pp_table.FanMaximumRpm, 16},
	{ "FanTargetTemperature", &pp_table.FanTargetTemperature, 16},
	{ "FanTargetGfxclk", &pp_table.FanTargetGfxclk, 16},
	{ "FanTempInputSelect", &pp_table.FanTempInputSelect, 8},
	{ "FanZeroRpmEnable", &pp_table.FanZeroRpmEnable, 8},
	{ "FanTachEdgePerRev", &pp_table.FanTachEdgePerRev, 8},
	{ "FuzzyFan_ErrorSetDelta", &pp_table.FuzzyFan_ErrorSetDelta, 16},
	{ "FuzzyFan_ErrorRateSetDelta", &pp_table.FuzzyFan_ErrorRateSetDelta, 16},
	{ "FuzzyFan_PwmSetDelta", &pp_table.FuzzyFan_PwmSetDelta, 16},
	{ "OverrideAvfsGb[0]", &pp_table.OverrideAvfsGb[0], 8},
	{ "OverrideAvfsGb[1]", &pp_table.OverrideAvfsGb[1], 8},
	{ "DcTol[0]", &pp_table.DcTol[0], 16},
	{ "DcTol[1]", &pp_table.DcTol[1], 16},
	{ "DcBtcEnabled[0]", &pp_table.DcBtcEnabled[0], 8},
	{ "DcBtcEnabled[1]", &pp_table.DcBtcEnabled[1], 8},
	{ "DcBtcMin[0]", &pp_table.DcBtcMin[0], 16},
	{ "DcBtcMin[1]", &pp_table.DcBtcMin[1], 16},
	{ "DcBtcMax[0]", &pp_table.DcBtcMax[0], 16},
	{ "DcBtcMax[1]", &pp_table.DcBtcMax[1], 16},
	{ "DebugOverrides", &pp_table.DebugOverrides, 32},
	{ "TotalPowerConfig", &pp_table.TotalPowerConfig, 8},
	{ "TotalPowerSpare1", &pp_table.TotalPowerSpare1, 8},
	{ "TotalPowerSpare2", &pp_table.TotalPowerSpare2, 16},
	{ "PccThresholdLow", &pp_table.PccThresholdLow, 16},
	{ "PccThresholdHigh", &pp_table.PccThresholdHigh, 16},
	{ "MGpuFanBoostLimitRpm", &pp_table.MGpuFanBoostLimitRpm, 32},
	{ "VDDGFX_TVmin", &pp_table.VDDGFX_TVmin, 16},
	{ "VDDSOC_TVmin", &pp_table.VDDSOC_TVmin, 16},
	{ "VDDGFX_Vmin_HiTemp", &pp_table.VDDGFX_Vmin_HiTemp, 16},
	{ "VDDGFX_Vmin_LoTemp", &pp_table.VDDGFX_Vmin_LoTemp, 16},
	{ "VDDSOC_Vmin_HiTemp", &pp_table.VDDSOC_Vmin_HiTemp, 16},
	{ "VDDSOC_Vmin_LoTemp", &pp_table.VDDSOC_Vmin_LoTemp, 16},
	{ "VDDSOC_TVminHystersis", &pp_table.VDDSOC_TVminHystersis, 16},
	{ "BtcConfig", &pp_table.BtcConfig, 32},
	{ "DcBtcGb[0]", &pp_table.DcBtcGb[0], 16},
	{ "DcBtcGb[1]", &pp_table.DcBtcGb[1], 16},
	{ "MaxVoltageStepGfx", &pp_table.MaxVoltageStepGfx, 16},
	{ "MaxVoltageStepSoc", &pp_table.MaxVoltageStepSoc, 16},
	{ "VddGfxVrMapping", &pp_table.VddGfxVrMapping, 8},
	{ "VddSocVrMapping", &pp_table.VddSocVrMapping, 8},
	{ "VddMem0VrMapping", &pp_table.VddMem0VrMapping, 8},
	{ "VddMem1VrMapping", &pp_table.VddMem1VrMapping, 8},
	{ "GfxUlvPhaseSheddingMask", &pp_table.GfxUlvPhaseSheddingMask, 8},
	{ "SocUlvPhaseSheddingMask", &pp_table.SocUlvPhaseSheddingMask, 8},
	{ "ExternalSensorPresent", &pp_table.ExternalSensorPresent, 8},
	{ "GfxMaxCurrent", &pp_table.GfxMaxCurrent, 16},
	{ "GfxOffset", &pp_table.GfxOffset, 8},
	{ "SocMaxCurrent", &pp_table.SocMaxCurrent, 16},
	{ "SocOffset", &pp_table.SocOffset, 8},
	{ "Mem0MaxCurrent", &pp_table.Mem0MaxCurrent, 16},
	{ "Mem0Offset", &pp_table.Mem0Offset, 8},
	{ "Mem1MaxCurrent", &pp_table.Mem1MaxCurrent, 16},
	{ "Mem1Offset", &pp_table.Mem1Offset, 8},
	{ "AcDcGpio", &pp_table.AcDcGpio, 8},
	{ "AcDcPolarity", &pp_table.AcDcPolarity, 8},
	{ "VR0HotGpio", &pp_table.VR0HotGpio, 8},
	{ "VR0HotPolarity", &pp_table.VR0HotPolarity, 8},
	{ "VR1HotGpio", &pp_table.VR1HotGpio, 8},
	{ "VR1HotPolarity", &pp_table.VR1HotPolarity, 8},
	{ "GthrGpio", &pp_table.GthrGpio, 8},
	{ "GthrPolarity", &pp_table.GthrPolarity, 8},
	{ "LedPin0", &pp_table.LedPin0, 8},
	{ "LedPin1", &pp_table.LedPin1, 8},
	{ "LedPin2", &pp_table.LedPin2, 8},
	{ "PllGfxclkSpreadEnabled", &pp_table.PllGfxclkSpreadEnabled, 8},
	{ "PllGfxclkSpreadPercent", &pp_table.PllGfxclkSpreadPercent, 8},
	{ "PllGfxclkSpreadFreq", &pp_table.PllGfxclkSpreadFreq, 16},
	{ "DfllGfxclkSpreadEnabled", &pp_table.DfllGfxclkSpreadEnabled, 8},
	{ "DfllGfxclkSpreadPercent", &pp_table.DfllGfxclkSpreadPercent, 8},
	{ "DfllGfxclkSpreadFreq", &pp_table.DfllGfxclkSpreadFreq, 16},
	{ "UclkSpreadEnabled", &pp_table.UclkSpreadEnabled, 8},
	{ "UclkSpreadPercent", &pp_table.UclkSpreadPercent, 8},
	{ "UclkSpreadFreq", &pp_table.UclkSpreadFreq, 16},
	{ "SoclkSpreadEnabled", &pp_table.SoclkSpreadEnabled, 8},
	{ "SocclkSpreadPercent", &pp_table.SocclkSpreadPercent, 8},
	{ "SocclkSpreadFreq", &pp_table.SocclkSpreadFreq, 16},
	{ "TotalBoardPower", &pp_table.TotalBoardPower, 16},
	{ "MvddRatio", &pp_table.MvddRatio, 32},
	{ "RenesesLoadLineEnabled", &pp_table.RenesesLoadLineEnabled, 8},
	{ "GfxLoadlineResistance", &pp_table.GfxLoadlineResistance, 8},
	{ "SocLoadlineResistance", &pp_table.SocLoadlineResistance, 8},
	{ NULL, NULL, 0},
};

int umr_navi10_pptable_print(const char* param, FILE* fp)
{
	uint32_t  table_header_len = 0;
	uint32_t  table_len = 0;
	void* table_container = NULL;
	uint32_t  table_container_len = 0;
	uint32_t r = 0;
	int i = 0;
	int flag = 0;

	table_header_len = sizeof(pp_table_header);
	table_len = sizeof(pp_table);
	table_container_len = table_header_len + table_len;

	table_container = calloc(1, table_container_len);
	if (table_container == NULL) {
		fprintf(stderr, "[ERROR]: Out of memory\n");
		return -1;
	}

	r = fread(table_container, 1, table_container_len, fp);
	if (r != table_container_len) {
		free(table_container);
		return -1;
	} else {
		memcpy(&pp_table_header, table_container, table_header_len);
		memcpy(&pp_table, (char*)table_container + table_header_len, table_len);
	}

	if (param == NULL) {
		printf("Table header length is %d, table length is %d, total structure size is %d\n",
			table_header_len, table_len, table_container_len);
		for (i = 0; navi10_pp_table[i].name != NULL; i++) {
			if (navi10_pp_table[i].len == 8)
				printf("%s : %u\n", navi10_pp_table[i].name, *(uint8_t *)navi10_pp_table[i].address);
			else if (navi10_pp_table[i].len == 16)
				printf("%s : %u\n", navi10_pp_table[i].name, *(uint16_t *)navi10_pp_table[i].address);
			else if (navi10_pp_table[i].len == 32)
				printf("%s : %u\n", navi10_pp_table[i].name, *(uint32_t *)navi10_pp_table[i].address);
		}
	} else {
		for (i = 0; navi10_pp_table[i].name != NULL; i++) {
			if (strstr(navi10_pp_table[i].name, param)) {
				flag++;
				if (navi10_pp_table[i].len == 8)
					printf("%s : %u\n", navi10_pp_table[i].name, *(uint8_t *)navi10_pp_table[i].address);
				else if (navi10_pp_table[i].len == 16)
					printf("%s : %u\n", navi10_pp_table[i].name, *(uint16_t *)navi10_pp_table[i].address);
				else if (navi10_pp_table[i].len == 32)
					printf("%s : %u\n", navi10_pp_table[i].name, *(uint32_t *)navi10_pp_table[i].address);
			}
		}
		if (flag == 0) {
			printf("Can not find %s in pptable\n", param);
			free(table_container);
			return -1;
		}
	}

	free(table_container);
	return 0;
}

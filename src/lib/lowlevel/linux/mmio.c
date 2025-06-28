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
 * Authors: Tom St Denis <tom.stdenis@amd.com>
 *
 */
#include "umr.h"
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/ioctl.h>
#include <sys/ioctl.h>

struct amdgpu_debugfs_regs2_iocdata {
	__u32 use_srbm, use_grbm, pg_lock;
	struct {
		__u32 se, sh, instance;
	} grbm;
	struct {
		__u32 me, pipe, queue, vmid;
	} srbm;
};

struct amdgpu_debugfs_regs2_iocdata_v2 {
	__u32 use_srbm, use_grbm, pg_lock;
	struct {
		__u32 se, sh, instance;
	} grbm;
	struct {
		__u32 me, pipe, queue, vmid;
	} srbm;
	__u32 xcc_id;
};

enum AMDGPU_DEBUGFS_REGS2_CMDS {
	AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE=0,
	AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE_V2,
};

#define AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE _IOWR(0x20, AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE, struct amdgpu_debugfs_regs2_iocdata)
#define AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE_V2 _IOWR(0x20, AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE_V2, struct amdgpu_debugfs_regs2_iocdata_v2)

/**
 * umr_pcie_read - Read a PCIE register
 *
 * Reads a PCIE register via debugfs or direct MMIO.
 */
static uint32_t umr_pcie_read(struct umr_asic *asic, uint64_t addr)
{
	uint32_t value;
	if (asic->options.use_pci) {
		switch (asic->config.gfx.family) {
#if 0
			case 110: // SI
			case 120: // CIK
			case 125: // KV
			case 130: // VI
				umr_write_reg_by_name(asic, "mmSMC_IND_INDEX_1", addr);
				return umr_read_reg_by_name(asic, "mmSMC_IND_DATA_1");
			case 135: // CZ
				umr_write_reg_by_name(asic, "mmMP0PUB_IND_INDEX_1", addr);
				return umr_read_reg_by_name(asic, "mmMP0PUB_IND_DATA_1");
#endif
			default:
				asic->err_msg("[BUG]: Unsupported family type in umr_pcie_read()\n");
				return 0;
		}
	} else {
		if (lseek(asic->fd.pcie, addr, SEEK_SET) < 0)
			asic->err_msg("[ERROR]: Cannot seek to PCIE address\n");
		if (read(asic->fd.pcie, &value, 4) != 4)
			asic->err_msg("[ERROR]: Cannot read from PCIE reg\n");
		return value;
	}
}

/**
 * umr_pcie_write - Write a PCIE register
 *
 * Write a PCIE register via debugfs or direct MMIO access.
 */
static uint32_t umr_pcie_write(struct umr_asic *asic, uint64_t addr, uint32_t value)
{
	if (asic->options.use_pci) {
		switch (asic->config.gfx.family) {
#if 0
			case 110: // SI
			case 120: // CIK
			case 125: // KV
			case 130: // VI
				umr_write_reg_by_name(asic, "mmSMC_IND_INDEX_1", addr);
				return umr_write_reg_by_name(asic, "mmSMC_IND_DATA_1", value);
			case 135: // CZ
				umr_write_reg_by_name(asic, "mmMP0PUB_IND_INDEX_1", addr);
				return umr_write_reg_by_name(asic, "mmMP0PUB_IND_DATA_1", value);
#endif
			default:
				asic->err_msg("[BUG]: Unsupported family type in umr_pcie_write()\n");
				return -1;
		}
	} else {
		if (lseek(asic->fd.pcie, addr, SEEK_SET) < 0) {
			asic->err_msg("[ERROR]: Cannot seek to PCIE address\n");
			return -1;
		}
		if (write(asic->fd.pcie, &value, 4) != 4) {
			asic->err_msg("[ERROR]: Cannot write to PCIE reg\n");
			return -1;
		}
	}
	return 0;
}

/**
 * umr_smc_read - Read an SMC register
 *
 * Reads an SMC register via debugfs or direct MMIO.
 */
static uint32_t umr_smc_read(struct umr_asic *asic, uint64_t addr)
{
	uint32_t value;
	if (asic->options.use_pci) {
		switch (asic->config.gfx.family) {
			case 110: // SI
			case 120: // CIK
			case 125: // KV
			case 130: // VI
				umr_write_reg_by_name(asic, "mmSMC_IND_INDEX_1", addr);
				return umr_read_reg_by_name(asic, "mmSMC_IND_DATA_1");
			case 135: // CZ
				umr_write_reg_by_name(asic, "mmMP0PUB_IND_INDEX_1", addr);
				return umr_read_reg_by_name(asic, "mmMP0PUB_IND_DATA_1");
			default:
				asic->err_msg("[BUG]: Unsupported family type in umr_smc_read()\n");
				return 0;
		}
	} else {
		if (lseek(asic->fd.smc, addr, SEEK_SET) < 0)
			asic->err_msg("[ERROR]: Cannot seek to SMC address\n");
		if (read(asic->fd.smc, &value, 4) != 4)
			asic->err_msg("[ERROR]: Cannot read from SMC reg\n");
		return value;
	}

}

/**
 * umr_smc_write - Write an SMC register
 *
 * Write an SMC register via debugfs or direct MMIO access.
 */
static uint32_t umr_smc_write(struct umr_asic *asic, uint64_t addr, uint32_t value)
{
	if (asic->options.use_pci) {
		switch (asic->config.gfx.family) {
			case 110: // SI
			case 120: // CIK
			case 125: // KV
			case 130: // VI
				umr_write_reg_by_name(asic, "mmSMC_IND_INDEX_1", addr);
				return umr_write_reg_by_name(asic, "mmSMC_IND_DATA_1", value);
			case 135: // CZ
				umr_write_reg_by_name(asic, "mmMP0PUB_IND_INDEX_1", addr);
				return umr_write_reg_by_name(asic, "mmMP0PUB_IND_DATA_1", value);
			default:
				asic->err_msg("[BUG]: Unsupported family type in umr_smc_write()\n");
				return -1;
		}
	} else {
		if (lseek(asic->fd.smc, addr, SEEK_SET) < 0) {
			asic->err_msg("[ERROR]: Cannot seek to SMC address\n");
			return -1;
		}
		if (write(asic->fd.smc, &value, 4) != 4) {
			asic->err_msg("[ERROR]: Cannot write to SMC reg\n");
			return -1;
		}
	}
	return 0;
}

// this sends the grbm/srbm data up based on flags...
static int mmio2_apply_bank(struct umr_asic *asic)
{
	struct amdgpu_debugfs_regs2_iocdata id;
	struct amdgpu_debugfs_regs2_iocdata_v2 id_v2;
	int r;

	memset(&id, 0, sizeof id);
	memset(&id_v2, 0, sizeof id_v2);

	if (!asic->options.use_v1_regs_debugfs) {
		if (asic->options.pg_lock) {
			id_v2.pg_lock = 1;
		}

		if (asic->options.use_bank == 1) {
			id_v2.grbm.se = asic->options.bank.grbm.se;
			id_v2.grbm.sh = asic->options.bank.grbm.sh;
			id_v2.grbm.instance = asic->options.bank.grbm.instance;
			id_v2.use_grbm = 1;
		}

		if (asic->options.use_bank == 2) {
			id_v2.srbm.me    = asic->options.bank.srbm.me;
			id_v2.srbm.pipe  = asic->options.bank.srbm.pipe;
			id_v2.srbm.queue = asic->options.bank.srbm.queue;
			id_v2.srbm.vmid  = asic->options.bank.srbm.vmid;
			id_v2.use_srbm = 1;
		}
		id_v2.xcc_id = asic->options.vm_partition == -1 ? 0 : asic->options.vm_partition;
		r = ioctl(asic->fd.mmio2, AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE_V2, &id_v2);
		if (!r)
			return r;

		// stop trying this and fall back by default now
		asic->options.use_v1_regs_debugfs = 1;
	}

	// fall back to old IOCTL that isn't XCC aware
	if (asic->options.pg_lock) {
		id.pg_lock = 1;
	}

	if (asic->options.use_bank == 1) {
		id.grbm.se = asic->options.bank.grbm.se;
		id.grbm.sh = asic->options.bank.grbm.sh;
		id.grbm.instance = asic->options.bank.grbm.instance;
		id.use_grbm = 1;
	}

	if (asic->options.use_bank == 2) {
		id.srbm.me    = asic->options.bank.srbm.me;
		id.srbm.pipe  = asic->options.bank.srbm.pipe;
		id.srbm.queue = asic->options.bank.srbm.queue;
		id.srbm.vmid  = asic->options.bank.srbm.vmid;
		id.use_srbm = 1;
	}

	return ioctl(asic->fd.mmio2, AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE, &id);
}

/** @brief Reads a register by address, applying bank selection if necessary.
 *
 * @param asic Pointer to the umr_asic structure containing ASIC information.
 * @param addr The address of the register to read.
 * @param type The type of register (REG_SMN, REG_PCIE, REG_MMIO, REG_SMC).
 * @return The value of the register.
 */
uint32_t umr_read_reg(struct umr_asic *asic, uint64_t addr, enum regclass type)
{
	uint32_t value=0;
	uint64_t mmio_addr = addr & 0xFFFFFFFF;
	int use_bank = 0;

	if (addr == 0xFFFFFFFF)
		asic->err_msg("[BUG]: reading from addr==0xFFFFFFFF is likely a bug\n");

	// apply bank bits
	if (type == REG_MMIO && asic->options.no_kernel) {
		use_bank = asic->options.use_bank;
		if (use_bank == 1) {
			umr_grbm_select_index(asic, asic->options.bank.grbm.se, asic->options.bank.grbm.sh, asic->options.bank.grbm.instance);
		} else if (use_bank == 2) {
			umr_srbm_select_index(asic, asic->options.bank.srbm.me, asic->options.bank.srbm.pipe, asic->options.bank.srbm.queue, asic->options.bank.srbm.vmid);
		}
	}

	// apply context banking
	if ((mmio_addr >= (0xA000*4)) && (mmio_addr < (0xB000*4)))
		addr += asic->options.context_reg_bank * 0x1000;

	switch (type) {
		case REG_SMN:
		case REG_PCIE:
			value = umr_pcie_read(asic, addr);
			break;
		case REG_MMIO:
			if (asic->pci.mem && (addr < asic->pci.pdevice->regions[asic->pci.region].size)) {
				value = asic->pci.mem[addr/4];
				break;
			} else {
				if (asic->fd.mmio2 >= 0) {
					// use new interface
					if (mmio2_apply_bank(asic)) {
						asic->err_msg("[ERROR]: Could not set register IOCTL state\n");
						return 0;
					}
					if (lseek(asic->fd.mmio2, addr, SEEK_SET) < 0) {
						asic->err_msg("[ERROR]: Cannot seek to MMIO address for read\n");
						return 0;
					}
					if (read(asic->fd.mmio2, &value, 4) != 4) {
						asic->err_msg("[ERROR]: Cannot read from MMIO reg\n");
						return 0;
					}
				} else {
					// this is the older debugfs route and will be deprecated eventually
					addr &= 0xFFFFFFUL;
					if (lseek(asic->fd.mmio, addr | umr_apply_bank_selection_address(asic), SEEK_SET) < 0)
						asic->err_msg("[ERROR]: Cannot seek to MMIO address\n");
					if (read(asic->fd.mmio, &value, 4) != 4)
						asic->err_msg("[ERROR]: Cannot read from MMIO reg\n");
				}
				break;
			}
			break;
		case REG_SMC:
			value = umr_smc_read(asic, addr);
			break;
		default:
			asic->err_msg("[BUG]: Unsupported register type in umr_read_reg().\n");
			return 0;
	}

	switch (use_bank) {
		case 1:
			umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
			break;
		case 2:
			umr_srbm_select_index(asic, 0, 0, 0, 0);
			break;
	}

	if (asic->options.test_log && asic->options.test_log_fd) {
		if (strstr(umr_reg_name(asic, addr>>2), "SQ_IND_DATA")) {
			fprintf(asic->options.test_log_fd, "SQ@0x%"PRIx64" = { 0x%"PRIx32" } ; %s\n", asic->test_harness.sq_ind_index, value, umr_reg_name(asic, addr>>2));
		} else {
			fprintf(asic->options.test_log_fd, "MMIO@0x%"PRIx64" = { 0x%"PRIx32" } ; %s\n", mmio_addr, value, umr_reg_name(asic, addr>>2));
		}
	}

	return value;
}

/** @brief Writes a register by address, applying bank selection if necessary.
 *
 * @param asic Pointer to the umr_asic structure containing ASIC information.
 * @param addr The address of the register to write to.
 * @param value The value to write to the register.
 * @param type The type of register (REG_SMN, REG_PCIE, REG_MMIO, REG_SMC).
 * @return 0 on success, -1 on failure.
 */
int umr_write_reg(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type)
{
	uint64_t mmio_addr = addr & 0xFFFFFFFF;
	int use_bank = 0, r = 0;

	if (addr == 0xFFFFFFFF)
		asic->err_msg("[BUG]: reading from addr==0xFFFFFFFF is likely a bug\n");

	// apply bank bits
	if (type == REG_MMIO && asic->options.no_kernel) {
		use_bank = asic->options.use_bank;
		if (use_bank == 1) {
			umr_grbm_select_index(asic, asic->options.bank.grbm.se, asic->options.bank.grbm.sh, asic->options.bank.grbm.instance);
		} else if (use_bank == 2) {
			umr_srbm_select_index(asic, asic->options.bank.srbm.me, asic->options.bank.srbm.pipe, asic->options.bank.srbm.queue, asic->options.bank.srbm.vmid);
		}
	}

	// apply context banking
	if ((mmio_addr >= (0xA000*4)) && (mmio_addr < (0xB000*4)))
		addr += asic->options.context_reg_bank * 0x1000;

	switch (type) {
		case REG_SMN:
		case REG_PCIE:
			r = umr_pcie_write(asic, addr, value);
			break;
		case REG_MMIO:
			if (asic->pci.mem && (addr < asic->pci.pdevice->regions[asic->pci.region].size)) {
				asic->pci.mem[addr/4] = value;
			} else {
				if (asic->fd.mmio2 >= 0) {
					// use new interface
					if (mmio2_apply_bank(asic)) {
						asic->err_msg("[ERROR]: Could not set register IOCTL state\n");
						return 0;
					}
					if (lseek(asic->fd.mmio2, addr, SEEK_SET) < 0) {
						asic->err_msg("[ERROR]: Cannot seek to MMIO address\n");
						r = -1;
					} else if (write(asic->fd.mmio2, &value, 4) != 4) {
						asic->err_msg("[ERROR]: Cannot write to MMIO reg\n");
						r = -1;
					}
				} else {
					// this is the older debugfs route and will be deprecated eventually
					addr &= 0xFFFFFFUL;
					if (lseek(asic->fd.mmio, addr | umr_apply_bank_selection_address(asic), SEEK_SET) < 0) {
						asic->err_msg("[ERROR]: Cannot seek to MMIO address for write\n");
						r = -1;
					} else if (write(asic->fd.mmio, &value, 4) != 4) {
						asic->err_msg("[ERROR]: Cannot write to MMIO reg\n");
						r = -1;
					}
				}
			}
			if (asic->options.test_log && asic->options.test_log_fd) {
				if (strstr(umr_reg_name(asic, addr>>2), "SQ_IND_INDEX")) {
					asic->test_harness.sq_ind_index = value;
				}
			}
			break;
		case REG_SMC:
			r = umr_smc_write(asic, addr, value);
			break;
		default:
			asic->err_msg("[BUG]: Unsupported register type in umr_write_reg().\n");
			return -1;
	}

	switch (use_bank) {
		case 1:
			umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
			break;
		case 2:
			umr_srbm_select_index(asic, 0, 0, 0, 0);
			break;
	}

	return r;
}

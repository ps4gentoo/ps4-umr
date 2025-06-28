#include "umrapp.h"

#include <inttypes.h>

static uint32_t read_banked_reg(struct umr_asic *asic, char *name)
{
	struct umr_reg *reg;
	uint64_t bank_addr;
	reg = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, name);
	if (!reg) {
		asic->err_msg("[ERROR]: Cannot find CPG registers on ASIC.\n");
		return 0xDEADBEEF;
	}
	bank_addr = umr_apply_bank_selection_address(asic);
	return asic->reg_funcs.read_reg(asic, bank_addr | (reg->addr * 4), REG_MMIO);
}

void umr_print_cpg(struct umr_asic *asic)
{
	uint32_t rs64_en, mes_en;
	uint32_t max_me_num, queues_per_pipe, pipes_per_me;
	int maj, min;

	rs64_en = mes_en = asic->family >= FAMILY_GFX11;
	asic->options.use_bank = 2;

	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 10:
			max_me_num = 2;
			if (min == 1) {
				pipes_per_me = 1;
				queues_per_pipe = 8;
			} else {
				pipes_per_me = 2;
				queues_per_pipe = 2;
			}
			break;
		case 11:
			max_me_num = 2;
			pipes_per_me = 1;
			queues_per_pipe = 2;
			break;
		case 12:
			max_me_num = 2;
			pipes_per_me = 1;
			queues_per_pipe = 8;
			break;
		default:
			max_me_num = 2;
			pipes_per_me = 1;
			queues_per_pipe = 2;
			break;
	}

	for (uint32_t me = 1; me < max_me_num; ++ me) {
		asic->options.bank.srbm.me = me;

		char iptr_name_me_f32[] = "mmCP_ME_INSTR_PNTR";
		char istat_name[] = "mmCP_INT_STAT_DEBUG";

		char iptr_name_me_rs64[] = "mmCP_GFX_RS64_INSTR_PNTR0";
		char iptr_name_mes[] = "mmCP_MES_INSTR_PNTR";

		for (uint32_t pipe = 0; pipe < pipes_per_me; ++pipe) {
			asic->options.bank.srbm.pipe = pipe;

			for (uint32_t queue = 0; queue < queues_per_pipe; ++queue) {
				asic->options.bank.srbm.me = me;
				asic->options.bank.srbm.pipe = pipe;
				asic->options.bank.srbm.queue = queue;

				if (read_banked_reg(asic, "mmCP_GFX_HQD_ACTIVE") & 0x1) {
					uint32_t vmid = read_banked_reg(asic, "mmCP_GFX_HQD_VMID") & 0xF;

					uint32_t hqd_base_lo = read_banked_reg(asic, "mmCP_GFX_HQD_BASE");
					uint32_t hqd_base_hi = read_banked_reg(asic, "mmCP_GFX_HQD_BASE_HI");
					uint64_t hqd_base = ((((uint64_t)hqd_base_hi) << 0x20) | hqd_base_lo) << 0x8;
					uint32_t hqd_rptr = read_banked_reg(asic, "mmCP_GFX_HQD_RPTR");
					uint32_t csmd_rptr = read_banked_reg(asic, "mmCP_GFX_HQD_CSMD_RPTR");
					uint32_t hqd_wptr;
					if (asic->family < FAMILY_AI) {
						hqd_wptr = read_banked_reg(asic, "mmCP_GFX_HQD_WPTR");
					} else {
						uint32_t hqd_wptr_lo = read_banked_reg(asic, "mmCP_GFX_HQD_WPTR");
						uint32_t hqd_wptr_hi = read_banked_reg(asic, "mmCP_GFX_HQD_WPTR_HI");
						hqd_wptr = (((uint64_t)hqd_wptr_hi) << 0x20) | hqd_wptr_lo;
					}
					uint32_t hqd_rptr_addr_lo = read_banked_reg(asic, "mmCP_GFX_HQD_RPTR_ADDR");
					uint32_t hqd_rptr_addr_hi = read_banked_reg(asic, "mmCP_GFX_HQD_RPTR_ADDR");
					uint64_t hqd_rptr_addr = (((uint64_t)hqd_rptr_addr_hi) << 0x20) | hqd_rptr_addr_lo;
					uint32_t hqd_cntl = read_banked_reg(asic, "mmCP_GFX_HQD_CNTL");
					uint32_t hqd_offset = read_banked_reg(asic, "mmCP_GFX_HQD_OFFSET");

					uint32_t mqd_base_lo = read_banked_reg(asic, "mmCP_GFX_MQD_BASE_ADDR");
					uint32_t mqd_base_hi = read_banked_reg(asic, "mmCP_GFX_MQD_BASE_ADDR_HI");
					uint64_t mqd_base = (((uint64_t)mqd_base_hi) << 0x20) | mqd_base_lo;
					uint32_t deq_req = read_banked_reg(asic, "mmCP_GFX_HQD_DEQUEUE_REQUEST");
					uint32_t iq_timer = read_banked_reg(asic, "mmCP_GFX_HQD_IQ_TIMER");

					printf("Pipe %u  Queue %u  VMID %u\n", pipe, queue, vmid);
					printf("  HQD BASE 0x%" PRIx64 "  RPTR 0x%x CSMD_RPTR 0x%x WPTR 0x%x  RPTR_ADDR 0x%lx\n",
					hqd_base, hqd_rptr, csmd_rptr, hqd_wptr, hqd_rptr_addr);
					printf("  HQD CNTL 0x%x" PRIx64 " OFFSET 0x%x\n", hqd_cntl, hqd_offset);
					printf("  MQD 0x%" PRIx64 "  DEQ_REQ 0x%x  IQ_TIMER 0x%x\n\n",
					mqd_base, deq_req, iq_timer);
				}
			}

			uint32_t iptr = read_banked_reg(asic, (me == 3) ? iptr_name_mes :
									(rs64_en ? iptr_name_me_rs64 : iptr_name_me_f32));
			if (asic->family < FAMILY_AI) {
				uint32_t istat = read_banked_reg(asic, istat_name);
				printf("ME %u Pipe %u: INSTR_PTR 0x%x  INT_STAT_DEBUG 0x%x\n", me, pipe, iptr, istat);
			} else if (rs64_en) {
				printf("ME %u Pipe %u: INSTR_PTR 0x%x (ASM 0x%x)\n", me, pipe, iptr, iptr << 2);
			} else {
				printf("ME %u Pipe %u: INSTR_PTR 0x%x\n", me, pipe, iptr);
			}
		}
	}
	asic->options.use_bank = 0;
}

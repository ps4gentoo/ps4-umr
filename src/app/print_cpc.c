#include "umrapp.h"

#include <inttypes.h>

static uint32_t read_banked_reg(struct umr_asic *asic, char *name)
{
	return umr_read_reg_by_name_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, name);
}

void umr_print_cpc(struct umr_asic *asic)
{
	/**
	 * GFX11+:
	 * 	- ME2 does not exist
	 * 	- ME3 is MES SCHED/KIQ which only uses one queue per pipe
	 * 	- Assumes always using RS64
	 * 	- 4 queues/pipe (pre-GFX10, 8 queues/pipe)
	 */
	uint32_t rs64_en, mes_en;
	uint32_t max_me_num, queues_per_pipe, pipes_per_mec;
	int maj, min;
	struct umr_options opts;

	rs64_en = mes_en = asic->family >= FAMILY_GFX11;
	opts = asic->options;

	asic->options.use_bank = 2;

	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 10:
			max_me_num = 3;
			pipes_per_mec = 4;
			queues_per_pipe = 4;
			break;
		case 11:
			max_me_num = 4;
			pipes_per_mec = 4;
			queues_per_pipe = 4;
			break;
		case 12:
			max_me_num = 4;
			pipes_per_mec = 2;
			queues_per_pipe = 4;
			break;
		default:
			max_me_num = 3;
			pipes_per_mec = 4;
			queues_per_pipe = 8;
			break;
	}

	for (uint32_t me = 1; me < max_me_num; ++ me) {
		if (mes_en && me == 2)
			continue;

		asic->options.bank.srbm.me = me;

		char iptr_name_mec_f32[] = "mmCP_MECx_INSTR_PNTR";
		iptr_name_mec_f32[8] = '0' + me;

		char istat_name[] = "mmCP_MEx_INT_STAT_DEBUG";
		istat_name[7] = '0' + me;

		char iptr_name_mec_rs64[] = "mmCP_MEC_RS64_INSTR_PNTR";
		char iptr_name_mes[] = "mmCP_MES_INSTR_PNTR";

		for (uint32_t pipe = 0; pipe < (me == 1 ? pipes_per_mec : 2); ++pipe) {
			asic->options.bank.srbm.pipe = pipe;

			for (uint32_t queue = 0; queue < (me == 3 ? 1 : queues_per_pipe); ++queue) {
				asic->options.bank.srbm.me = me;
				asic->options.bank.srbm.pipe = pipe;
				asic->options.bank.srbm.queue = queue;

				if (read_banked_reg(asic, "mmCP_HQD_ACTIVE") & 0x1) {
					uint32_t vmid = read_banked_reg(asic, "mmCP_HQD_VMID") & 0xF;

					uint32_t pq_base_lo = read_banked_reg(asic, "mmCP_HQD_PQ_BASE");
					uint32_t pq_base_hi = read_banked_reg(asic, "mmCP_HQD_PQ_BASE_HI");
					uint64_t pq_base = ((((uint64_t)pq_base_hi) << 0x20) | pq_base_lo) << 0x8;
					uint32_t pq_rptr = read_banked_reg(asic, "mmCP_HQD_PQ_RPTR");
					uint32_t pq_wptr;
					if (asic->family < FAMILY_AI) {
						pq_wptr = read_banked_reg(asic, "mmCP_HQD_PQ_WPTR");
					} else {
						uint32_t pq_wptr_lo = read_banked_reg(asic, "mmCP_HQD_PQ_WPTR_LO");
						uint32_t pq_wptr_hi = read_banked_reg(asic, "mmCP_HQD_PQ_WPTR_HI");
						pq_wptr = (((uint64_t)pq_wptr_hi) << 0x20) | pq_wptr_lo;
					}
					uint32_t pq_rptr_addr_lo = read_banked_reg(asic, "mmCP_HQD_PQ_RPTR_REPORT_ADDR");
					uint32_t pq_rptr_addr_hi = read_banked_reg(asic, "mmCP_HQD_PQ_RPTR_REPORT_ADDR_HI");
					uint64_t pq_rptr_addr = (((uint64_t)pq_rptr_addr_hi) << 0x20) | pq_rptr_addr_lo;
					uint32_t pq_cntl = read_banked_reg(asic, "mmCP_HQD_PQ_CONTROL");
					uint32_t eop_base_lo = read_banked_reg(asic, "mmCP_HQD_EOP_BASE_ADDR");
					uint32_t eop_base_hi = read_banked_reg(asic, "mmCP_HQD_EOP_BASE_ADDR_HI");
					uint64_t eop_base = ((((uint64_t)eop_base_hi) << 0x20) | eop_base_lo) << 0x8;
					uint32_t eop_rptr = read_banked_reg(asic, "mmCP_HQD_EOP_RPTR");
					uint32_t eop_wptr = read_banked_reg(asic, "mmCP_HQD_EOP_WPTR");
					uint32_t eop_wptr_mem = read_banked_reg(asic, "mmCP_HQD_EOP_WPTR_MEM");
					uint32_t mqd_base_lo = read_banked_reg(asic, "mmCP_MQD_BASE_ADDR");
					uint32_t mqd_base_hi = read_banked_reg(asic, "mmCP_MQD_BASE_ADDR_HI");
					uint64_t mqd_base = (((uint64_t)mqd_base_hi) << 0x20) | mqd_base_lo;
					uint32_t deq_req = read_banked_reg(asic, "mmCP_HQD_DEQUEUE_REQUEST");
					uint32_t iq_timer = read_banked_reg(asic, "mmCP_HQD_IQ_TIMER");
					uint32_t aql_cntl = read_banked_reg(asic, "mmCP_HQD_AQL_CONTROL");
					uint32_t save_base_lo = read_banked_reg(asic, "mmCP_HQD_CTX_SAVE_BASE_ADDR_LO");
					uint32_t save_base_hi = read_banked_reg(asic, "mmCP_HQD_CTX_SAVE_BASE_ADDR_HI");
					uint64_t save_base = (((uint64_t)save_base_hi) << 0x20) | save_base_lo;
					uint32_t save_size = read_banked_reg(asic, "mmCP_HQD_CTX_SAVE_SIZE");
					uint32_t stack_off = read_banked_reg(asic, "mmCP_HQD_CNTL_STACK_OFFSET");
					uint32_t stack_size = read_banked_reg(asic, "mmCP_HQD_CNTL_STACK_SIZE");

					printf("Pipe %u  Queue %u  VMID %u\n", pipe, queue, vmid);
					printf("  PQ BASE 0x%" PRIx64 "  RPTR 0x%x  WPTR 0x%x  RPTR_ADDR 0x%" PRIx64 "  CNTL 0x%x\n",
					pq_base, pq_rptr, pq_wptr, pq_rptr_addr, pq_cntl);
					printf("  EOP BASE 0x%" PRIx64 "  RPTR 0x%x  WPTR 0x%x  WPTR_MEM 0x%x\n",
					eop_base, eop_rptr, eop_wptr, eop_wptr_mem);
					printf("  MQD 0x%" PRIx64 "  DEQ_REQ 0x%x  IQ_TIMER 0x%x  AQL_CONTROL 0x%x\n",
					mqd_base, deq_req, iq_timer, aql_cntl);
					printf("  SAVE BASE 0x%" PRIx64 "  SIZE 0x%x  STACK OFFSET 0x%x  SIZE 0x%x\n\n",
					save_base, save_size, stack_off, stack_size);
				}
			}

			uint32_t iptr = read_banked_reg(asic, (me == 3) ? iptr_name_mes :
									(rs64_en ? iptr_name_mec_rs64 : iptr_name_mec_f32));
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
	asic->options = opts;
}

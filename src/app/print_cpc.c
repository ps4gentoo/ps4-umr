#include "umrapp.h"

#include <inttypes.h>

static uint32_t read_banked_reg(struct umr_asic *asic, char *name)
{
	struct umr_reg *reg;
	uint64_t bank_addr;
	reg = umr_find_reg_data_by_ip_by_instance(asic, NULL, asic->options.vm_partition, name);
	if (!reg) {
		asic->err_msg("[ERROR]: Cannot find CPC registers on ASIC.\n");
		return 0xDEADBEEF;
	}
	bank_addr = umr_apply_bank_selection_address(asic);
	return umr_read_reg(asic, bank_addr | (reg->addr * 4), REG_MMIO);
}

void umr_print_cpc(struct umr_asic *asic)
{
	asic->options.use_bank = 2;
	for (uint32_t me = 1; me < 3; ++ me) {
		asic->options.bank.srbm.me = me;

		char iptr_name[] = "mmCP_MECx_INSTR_PNTR";
		char istat_name[] = "mmCP_MEx_INT_STAT_DEBUG";

		iptr_name[8] = '0' + me;
		istat_name[7] = '0' + me;

		for (uint32_t pipe = 0; pipe < (me == 1 ? 4 : 2); ++ pipe) {
			asic->options.bank.srbm.pipe = pipe;

			for (uint32_t queue = 0; queue < 8; ++ queue) {
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

			uint32_t iptr = read_banked_reg(asic, iptr_name);
			if (asic->family < FAMILY_AI) {
				uint32_t istat = read_banked_reg(asic, istat_name);
				printf("ME %u Pipe %u: INSTR_PTR 0x%x  INT_STAT_DEBUG 0x%x\n", me, pipe, iptr, istat);
			} else {
				printf("ME %u Pipe %u: INSTR_PTR 0x%x\n", me, pipe, iptr);
			}
		}
	}
	asic->options.use_bank = 0;
}

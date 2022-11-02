#include "umrapp.h"

#include <inttypes.h>
#include <stdio.h>

uint32_t read_sdma_reg(struct umr_asic *asic,
                       char *prefix,
                       char *suffix)
{
	char name[256];
	struct umr_reg *reg;
	uint64_t bank_addr;

	sprintf(name, "%s%s", prefix, suffix);
	reg = umr_find_reg_data_by_ip(asic, NULL, name);
	if (!reg) {
		asic->err_msg("[ERROR]: Cannot find register %s in ASIC\n");
		return 0xDEADBEEF;
	} else {
		bank_addr = umr_apply_bank_selection_address(asic);
		return umr_read_reg(asic, bank_addr | (reg->addr * 4), REG_MMIO);
	}
}

void umr_print_sdma(struct umr_asic *asic)
{
	uint32_t rings_per_eng;
	int use_queue = 0;

	printf("\n");

	if (umr_find_reg_data_by_ip(asic, NULL, "@mmSDMA0_RLC0_RB_BASE")) {
		rings_per_eng = umr_find_reg_data_by_ip(asic, NULL, "@mmSDMA0_RLC7_RB_BASE") ? 8 : 2;
	} else {
		rings_per_eng = umr_find_reg_data_by_ip(asic, NULL, "@mmSDMA0_QUEUE7_RB_BASE") ? 8 : 2;
		use_queue = 1;
	}

	for (uint32_t eng = 0; eng < 2; ++ eng) {
		for (uint32_t ring = 0; ring < rings_per_eng; ++ ring) {
			char prefix[64];
			if (!use_queue) {
				strcpy(prefix, "mmSDMAx_RLCx_");
				prefix[6] = '0' + eng;
				prefix[11] = '0' + ring;
			} else {
				strcpy(prefix, "mmSDMAx_QUEUEx_");
				prefix[6] = '0' + eng;
				prefix[13] = '0' + ring;
			}

			uint32_t rb_cntl = read_sdma_reg(asic, prefix, "RB_CNTL");

			if (rb_cntl & (1 << 12)) {
				uint32_t rb_base_lo = read_sdma_reg(asic, prefix, "RB_BASE");
				uint32_t rb_base_hi = read_sdma_reg(asic, prefix, "RB_BASE_HI");
				uint64_t rb_base = (((uint64_t)rb_base_hi << 0x20) | rb_base_lo) << 0x8;
				uint32_t rb_rptr_lo = read_sdma_reg(asic, prefix, "RB_RPTR");
				uint32_t rb_rptr_hi = read_sdma_reg(asic, prefix, "RB_RPTR_HI");
				uint64_t rb_rptr = ((uint64_t)rb_rptr_hi << 0x20) | rb_rptr_lo;
				uint32_t rb_wptr_lo = read_sdma_reg(asic, prefix, "RB_WPTR");
				uint32_t rb_wptr_hi = read_sdma_reg(asic, prefix, "RB_WPTR_HI");
				uint64_t rb_wptr = ((uint64_t)rb_wptr_hi << 0x20) | rb_wptr_lo;
				uint32_t rb_rptr_addr_lo = read_sdma_reg(asic, prefix, "RB_RPTR_ADDR_LO");
				uint32_t rb_rptr_addr_hi = read_sdma_reg(asic, prefix, "RB_RPTR_ADDR_HI");
				uint64_t rb_rptr_addr = ((uint64_t)rb_rptr_addr_hi << 0x20) | rb_rptr_addr_lo;

				printf("SDMA %u  RLC %u\n", eng, ring);
				printf("  RB BASE 0x%" PRIx64 "  RPTR 0x%" PRIx64 "  WPTR 0x%" PRIx64 "  RPTR_ADDR 0x%" PRIx64 "  CNTL 0x%x\n\n",
				rb_base, rb_rptr, rb_wptr, rb_rptr_addr, rb_cntl);
			}
		}
	}
}

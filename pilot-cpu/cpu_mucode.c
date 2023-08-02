#include "cpu_regs.h"
#include "cpu_decode.h"

static inline mucode_entry
base_entry_ (mucode_entry_spec spec)
{
	mucode_entry prg;
	prg.next = (mucode_entry_spec)
	{
		MU_NONE,
		spec.reg_select,
		spec.size,
		spec.is_write
	};
	prg.operation.srcs[0].size = SIZE_24_BIT;
	prg.operation.srcs[0].sign_extend = FALSE;

	prg.operation.srcs[1].size = spec.size;
	prg.operation.srcs[1].sign_extend = FALSE;

	prg.operation.operation = ALU_OFF;
	prg.operation.shifter_mode = SHIFTER_NONE;
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	prg.operation.mem_size = spec.size;
	prg.operation.mem_access_suppress = FALSE;
	
	return prg;
}

static mucode_entry
ind_1cyc_imm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_1;
	prg.operation.srcs[0].size = spec.size;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_imm_rm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_RM_1;
	prg.operation.srcs[0].size = spec.size;

	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_imm0_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	prg.operation.srcs[0].size = SIZE_8_BIT;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_reg_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].size = spec.size;
	switch (spec.size)
	{
		case SIZE_8_BIT:
			prg.operation.srcs[0].location = DATA_REG_L0 + (spec.reg_select & 0x7);
			break;
		case SIZE_16_BIT:
			prg.operation.srcs[0].location = DATA_REG_W0 + (spec.reg_select & 0x7);
			break;
		case SIZE_24_BIT:
			prg.operation.srcs[0].location = DATA_REG_P0 + (spec.reg_select & 0x7);
	}
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_reg_post_auto_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_reg_(spec);
	prg.next.entry_idx = MU_POST_AUTOIDX;
	
	return prg;
}

static mucode_entry
ind_1cyc_reg_auto_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_reg_(spec);
	
	prg.operation.srcs[1].location = DATA_SIZE;
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = TRUE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = prg.operation.srcs[0].location;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_2cyc_withimm_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_reg_(spec);
	prg.operation.srcs[1].location = (!(spec.reg_select & 0x10)) ? DATA_LATCH_IMM_1 : DATA_LATCH_RM_1;
	prg.operation.srcs[1].size = SIZE_16_BIT;
	prg.operation.srcs[1].sign_extend = TRUE;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_imm_withbits_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = (!(spec.reg_select & 0x10)) ? DATA_LATCH_IMM_HML_RM : DATA_LATCH_RM_HML;
	
	prg.operation.srcs[1].location = (!(spec.reg_select & 0x10)) ? DATA_REG_IMM_2_8 : DATA_REG_RM_2_8;
	prg.operation.srcs[1].sign_extend = spec.reg_select & 0x8;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_reg_withbits_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = (!(spec.reg_select & 0x10)) ? DATA_REG_IMM_1_2 : DATA_REG_RM_1_2;
	
	prg.operation.srcs[1].location = (!(spec.reg_select & 0x10)) ? DATA_REG_IMM_1_8 : DATA_REG_RM_1_8;
	prg.operation.srcs[1].sign_extend = spec.reg_select & 0x8;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_pgc_withimm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_REG_PGC;
	
	prg.operation.srcs[1].location = DATA_LATCH_IMM_0;
	prg.operation.srcs[1].size = SIZE_8_BIT;
	prg.operation.srcs[1].sign_extend = TRUE;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_pgc_withimm_rm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_REG_PGC;
	
	prg.operation.srcs[1].location = (!(spec.reg_select & 0x10)) ? DATA_LATCH_IMM_1 : DATA_LATCH_RM_1;
	prg.operation.srcs[1].size = spec.size;
	prg.operation.srcs[1].sign_extend = (spec.size == SIZE_16_BIT);
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_pgc_withhml_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_REG_PGC;
	
	prg.operation.srcs[1].location = DATA_LATCH_IMM_HML;
	prg.operation.srcs[1].size = SIZE_24_BIT;
	prg.operation.srcs[1].sign_extend = FALSE;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
ind_2cyc_pgc_withhml_rm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_REG_PGC;
	
	prg.operation.srcs[1].location = (!(spec.reg_select & 0x10)) ? DATA_LATCH_IMM_HML_RM : DATA_LATCH_RM_HML;
	prg.operation.srcs[1].size = SIZE_24_BIT;
	prg.operation.srcs[1].sign_extend = FALSE;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2;
	return prg;
}

static mucode_entry
after_autoidx_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_reg_(spec);
	prg.operation.srcs[1].location = DATA_SIZE;
	prg.operation.srcs[1].size = SIZE_24_BIT;
	
	prg.operation.src2_add1 = FALSE;
	prg.operation.src2_negate = FALSE;
	
	prg.operation.operation = ALU_ADD;
	
	prg.operation.dest = prg.operation.srcs[0].location;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	return prg;
}

mucode_entry
decode_mucode_entry (mucode_entry_spec spec)
{
	mucode_entry result;
	switch (spec.entry_idx)
	{
		case MU_IND_IMM:
			return result = ind_1cyc_imm_(spec);
		case MU_IND_IMM_RM:
			return result = ind_1cyc_imm_rm_(spec);
		case MU_IND_REG:
			return result = ind_1cyc_reg_(spec);
		case MU_IND_REG_POST_AUTO:
			return result = ind_1cyc_reg_post_auto_(spec);
		case MU_IND_REG_AUTO:
			return result = ind_1cyc_reg_auto_(spec);
		case MU_IND_REG_WITH_IMM:
			return result = ind_2cyc_withimm_(spec);
		case MU_IND_IMM0:
			return result = ind_1cyc_imm0_(spec);
		case MU_IND_IMM_WITH_BITS:
			return result = ind_2cyc_imm_withbits_(spec);
		case MU_IND_REG_WITH_BITS:
			return result = ind_2cyc_reg_withbits_(spec);
		case MU_IND_PGC_WITH_IMM:
			return result = ind_2cyc_pgc_withimm_(spec);
		case MU_IND_PGC_WITH_IMM_RM:
			return result = ind_2cyc_pgc_withimm_rm_(spec);
		case MU_IND_PGC_WITH_HML:
			return result = ind_2cyc_pgc_withhml_(spec);
		case MU_IND_PGC_WITH_HML_RM:
			return result = ind_2cyc_pgc_withhml_rm_(spec);
		case MU_POST_AUTOIDX:
			return result = after_autoidx_(spec);
		default:
			decode_unreachable_();
			return base_entry_(spec);
	}
}

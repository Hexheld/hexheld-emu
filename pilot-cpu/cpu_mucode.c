#include "cpu_decode.h"

/*
 Microcode entries I'll need:
 - Indirect read8, read16, write8, write16
	- 1+ cycle
		- bank0 imm
		- bank1 imm
		- AB
		- HL
		- HLi/HLd
		- IX
		- IXi/IXd
	- 2+ cycle
		- IX+imm
		- SP+imm
	- 2+ cycle with DS
		- DS:imm
		- DS:reg
	- 3+ cycle with DS
		- DS:IX+imm
		- DS:HLi/HLd/IXi/IXd
 - DS indexing (1 extra cycle)
 */

static inline mucode_entry
base_entry_ ()
{
	mucode_entry prg;
	prg.next = (mucode_entry_spec){MU_NONE, 0, FALSE, FALSE};
	prg.operation.srcs[0].is_16bit = TRUE;
	prg.operation.srcs[0].sign_extend = FALSE;
	return prg;
}

static mucode_entry
ind_1cyc_imm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = !(spec.reg_select & 1) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	prg.operation.operation = ALU_OFF;
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_B1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_reg_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	switch (spec.reg_select & 3)
	{
		case 0:
			prg.operation.srcs[0].location = DATA_REG_AB;
		case 1:
			prg.operation.srcs[0].location = DATA_REG_HL;
		case 2:
			prg.operation.srcs[0].location = DATA_REG_IX;
		case 3:
			prg.operation.srcs[0].location = DATA_REG_DS;
	}
	prg.operation.operation = ALU_OFF;
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_BD;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_reg_auto_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = (spec.reg_select & 1) ? DATA_REG_HL : DATA_REG_IX;
	
	prg.operation.srcs[1].location = DATA_ZERO;
	prg.operation.src2_add1 = TRUE;
	prg.operation.src2_negate = (spec.reg_select & 8);
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = spec.is_16bit ? SHIFTER_LEFT : SHIFTER_NONE;
	
	prg.operation.dest = prg.operation.srcs[0].location;
	
	prg.operation.flag_write_mask = F_DS_ADJ;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_BD;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_2cyc_withimm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = !(spec.reg_select & 1) ? DATA_REG_IX : DATA_REG_SP;
	
	prg.operation.srcs[1].is_16bit = TRUE;
	prg.operation.srcs[1].sign_extend = FALSE;
	prg.operation.srcs[1].location = !(spec.reg_select & 2) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF2_B0 : MEM_LATCH_HALF2_BD;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_2cyc_ds_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	switch (spec.reg_select & 7)
	{
		case 0:
			prg.operation.srcs[0].location = DATA_REG_AB;
		case 1:
			prg.operation.srcs[0].location = DATA_REG_HL;
		case 2:
			prg.operation.srcs[0].location = DATA_REG_IX;
		case 4:
			prg.operation.srcs[0].location = DATA_LATCH_IMM_1;
		case 5:
			prg.operation.srcs[0].location = DATA_LATCH_IMM_2;
		case 6:
			prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
		case 7:
			prg.operation.srcs[0].location = DATA_LATCH_MEM_DATA;
		default:
			decode_unreachable_();
	}
	
	prg.operation.srcs[1].is_16bit = TRUE;
	prg.operation.srcs[1].sign_extend = FALSE;
	prg.operation.srcs[1].location = DATA_REG_S_;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2_BD_ALUC;
	prg.operation.mem_write_ctl = (spec.is_write == 0) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
after_ds_autoidx_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = (spec.reg_select & 1) ? DATA_REG_HL : DATA_REG_IX;
	
	prg.operation.srcs[1].location = DATA_ZERO;
	prg.operation.src2_add1 = TRUE;
	prg.operation.src2_negate = (spec.reg_select & 8);
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = spec.is_16bit ? SHIFTER_LEFT : SHIFTER_NONE;
	
	prg.operation.dest = prg.operation.srcs[0].location;
	
	prg.operation.flag_write_mask = F_DS_ADJ;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	return prg;
}

static mucode_entry
ix_imm_index_before_ds_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = DATA_REG_IX;
	
	prg.operation.srcs[1].is_16bit = TRUE;
	prg.operation.srcs[1].sign_extend = FALSE;
	prg.operation.srcs[1].location = (spec.reg_select & 2) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = prg.operation.srcs[1].location;
	
	prg.operation.flag_write_mask = F_DS_ADJ;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	return prg;
}

static mucode_entry
zn_direct_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_imm_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	return prg;
}

static mucode_entry
zn_ld_reg_memdata_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = DATA_ZERO;
	
	prg.operation.srcs[1].location = DATA_LATCH_MEM_DATA;
	prg.operation.srcs[1].is_16bit = spec.is_16bit;
	prg.operation.operation = ALU_OR;
	if (spec.is_16bit)
	{
		prg.operation.dest = !(spec.reg_select & 1) ? DATA_REG_AB : DATA_REG_HL;
	}
	else
	{
		prg.operation.dest = !(spec.reg_select & 1) ? DATA_REG__A : DATA_REG__B;
	}
	return prg;
}

static mucode_entry
zn_ld_memdata_reg_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = DATA_LATCH_MEM_DATA;
	
	if (spec.is_16bit)
	{
		prg.operation.srcs[1].location = !(spec.reg_select & 1) ? DATA_REG_AB : DATA_REG_HL;
	}
	else
	{
		prg.operation.srcs[1].location = !(spec.reg_select & 1) ? DATA_REG__A : DATA_REG__B;
	}
	prg.operation.srcs[1].is_16bit = spec.is_16bit;
	prg.operation.srcs[1].sign_extend = FALSE;
	prg.operation.operation = ALU_OFF;
	prg.operation.dest = DATA_ZERO;

	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	prg.operation.mem_write_ctl = MEM_WRITE_FROM_SRC1;
	return prg;
}

static mucode_entry
zn_buffer_autoidx_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = DATA_LATCH_MEM_DATA;
	
	prg.operation.srcs[1].location = DATA_ZERO;
	prg.operation.src2_add1 = TRUE;
	prg.operation.src2_negate = (spec.reg_select & 8);
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = spec.is_16bit ? SHIFTER_LEFT : SHIFTER_NONE;
	
	prg.operation.dest = DATA_LATCH_IMM_1;
	
	prg.operation.flag_write_mask = F_DS_ADJ;
	return prg;
}


static mucode_entry
zn_autoidx_writeback (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_();
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	
	prg.operation.srcs[1].location = DATA_LATCH_IMM_1;
	prg.operation.srcs[1].is_16bit = TRUE;
	prg.operation.src2_add1 = TRUE;
	prg.operation.src2_negate = (spec.reg_select & 8);
	prg.operation.operation = ALU_OFF;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	prg.operation.mem_write_ctl = MEM_WRITE_FROM_SRC1;
	return prg;
}

static mucode_entry
mem_data_dereference_ (mucode_entry_spec spec)
{
	mucode_entry prg;
	prg.next = (mucode_entry_spec){MU_NONE, 0, FALSE, FALSE};
	prg.operation.srcs[0].is_16bit = spec.is_16bit;
	prg.operation.srcs[0].sign_extend = FALSE;
	prg.operation.srcs[0].location = DATA_LATCH_MEM_DATA;
	prg.operation.operation = ALU_OFF;
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	prg.operation.mem_write_ctl = MEM_READ;
	return prg;
}

mucode_entry
decode_mucode_entry (mucode_entry_spec spec)
{
	
}

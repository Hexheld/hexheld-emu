#include "cpu_regs.h"
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
base_entry_ (mucode_entry_spec spec)
{
	mucode_entry prg;
	prg.next = (mucode_entry_spec)
	{
		MU_NONE,
		spec.reg_select,
		spec.bank_select,
		spec.is_16bit,
		spec.is_write
	};
	prg.operation.srcs[0].is_16bit = TRUE;
	prg.operation.srcs[0].sign_extend = FALSE;

	prg.operation.srcs[1].is_16bit = TRUE;
	prg.operation.srcs[1].sign_extend = FALSE;

	prg.operation.operation = ALU_OFF;
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	prg.operation.mem_16bit = spec.is_16bit;
	prg.operation.mem_access_suppress = FALSE;
	return prg;
}

static mucode_entry
ind_1cyc_imm_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = !(spec.reg_select & 2) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_B1;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_imm_d_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_1cyc_imm_(spec);
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_BD;
	return prg;
}

static mucode_entry
ind_1cyc_imm0_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}


static mucode_entry
ind_1cyc_reg_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
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
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_BD;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_1cyc_reg_auto_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
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
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = !(spec.reg_select & 1) ? DATA_REG_IX : DATA_REG_SP;
	prg.operation.srcs[1].location = !(spec.reg_select & 2) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = DATA_ZERO;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF2_B0 : MEM_LATCH_HALF2_BD;
	prg.operation.mem_write_ctl = !(spec.is_write) ? MEM_READ : MEM_WRITE_FROM_MDR;
	return prg;
}

static mucode_entry
ind_2cyc_withimm0_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_2cyc_withimm_(spec);
	prg.operation.srcs[1].location = DATA_LATCH_IMM_0;
	return prg;
}

static mucode_entry
ind_2cyc_ds_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
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
		case 6:
			prg.operation.srcs[0].location = DATA_LATCH_IMM_2;
		default:
			decode_unreachable_();
	}
	
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
	mucode_entry prg = base_entry_(spec);
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
imm_ix_index_before_ds_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_REG_IX;
	
	prg.operation.srcs[1].location = (spec.reg_select & 2) ? DATA_LATCH_IMM_1 : DATA_LATCH_IMM_2;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = prg.operation.srcs[1].location;
	
	prg.operation.flag_write_mask = F_DS_ADJ;
	
	prg.operation.mem_latch_ctl = MEM_NO_LATCH;
	
	prg.next.entry_idx = MU_IND_WITH_DS;
	prg.next.reg_select = 4 | (spec.reg_select & 2);
	return prg;
}

static mucode_entry
imm0_ix_index_before_ds_ (mucode_entry_spec spec)
{
	mucode_entry prg = imm_ix_index_before_ds_(spec);
	prg.operation.srcs[1].location = DATA_LATCH_IMM_0;
	return prg;
}

static mucode_entry
zn_buffer_autoidx_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
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
zn_autoidx_writeback_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	
	prg.operation.srcs[1].location = DATA_LATCH_IMM_1;
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
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_MEM_DATA;
	
	prg.operation.mem_latch_ctl = !(spec.bank_select) ? MEM_LATCH_HALF1_B0 : MEM_LATCH_HALF1_BD;
	prg.operation.mem_write_ctl = MEM_READ;
	if (spec.is_write)
	{
		prg.operation.mem_access_suppress = TRUE;
	}
	return prg;
}

static mucode_entry
ld_mov_ (mucode_entry_spec spec)
{
	mucode_entry prg;
	if (!spec.is_write)
	{
		prg.operation.srcs[0].location = DATA_ZERO;
		if (spec.is_16bit)
		{
			prg.operation.srcs[1].location = !(spec.reg_select & 1) ? DATA_REG_AB : DATA_REG_HL;
		}
		else
		{
			prg.operation.srcs[1].location = !(spec.reg_select & 1) ? DATA_REG__A : DATA_REG__B;
		}
		prg.operation.srcs[1].is_16bit = spec.is_16bit;
		prg.operation.operation = ALU_OR;
		prg.operation.dest = DATA_LATCH_MEM_DATA;
		
		prg.operation.mem_latch_ctl = MEM_LATCH_HALF2_MAR;
		prg.operation.mem_write_ctl = MEM_WRITE_FROM_DEST;
	}
	else
	{
		prg = base_entry_(spec);
		prg.operation.srcs[1].location = DATA_LATCH_MEM_DATA;
		prg.operation.srcs[1].is_16bit = spec.is_16bit;

		prg.operation.srcs[0].location = DATA_ZERO;
		prg.operation.operation = ALU_OR;
		if (spec.is_16bit)
		{
			prg.operation.dest = !(spec.reg_select & 1) ? DATA_REG_AB : DATA_REG_HL;
		}
		else
		{
			prg.operation.dest = !(spec.reg_select & 1) ? DATA_REG__A : DATA_REG__B;
		}
	}
	
	return prg;
}

static mucode_entry
zn_ds_calc_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	prg.operation.srcs[1].location = DATA_REG_S_;
	
	prg.operation.operation = ALU_ADD;
	prg.operation.shifter_mode = SHIFTER_NONE;
	
	prg.operation.dest = DATA_LATCH_MEM_ADDR;
	prg.operation.mem_latch_ctl = MEM_LATCH_HALF2_BD_ALUC;
	if (spec.is_write)
	{
		prg.operation.mem_access_suppress = TRUE;
	}
	return prg;
}

static mucode_entry
zn_fetch_ (mucode_entry_spec spec)
{
	mucode_entry prg = base_entry_(spec);
	prg.operation.srcs[0].location = DATA_LATCH_IMM_0;
	prg.operation.mem_16bit = TRUE;

	prg.operation.mem_latch_ctl = MEM_LATCH_HALF1_B0;
	prg.operation.mem_write_ctl = MEM_READ;
	return prg;
}

static mucode_entry
zn_ix_fetch_ (mucode_entry_spec spec)
{
	mucode_entry prg = ind_2cyc_withimm_(spec);
	prg.operation.srcs[0].location = DATA_REG_IX;
	prg.operation.srcs[1].location = DATA_LATCH_IMM_0;

	prg.operation.mem_write_ctl = MEM_READ;
	return prg;
}

mucode_entry
decode_mucode_entry (mucode_entry_spec spec, bool d_flag_set)
{
	mucode_entry result;
	switch (spec.entry_idx)
	{
		case MU_IND_IMM:
			return result = ind_1cyc_imm_(spec);
		case MU_IND_REG:
			return result = ind_1cyc_reg_(spec);
		case MU_IND_REG_AUTO:
			return result = ind_1cyc_reg_auto_(spec);
		case MU_IND_REG_WITH_IMM:
			return result = ind_2cyc_withimm_(spec);
		case MU_IND_WITH_DS:
			if (d_flag_set)
			{
				if ((spec.reg_select & 4) == 0)
				{
					spec.bank_select = TRUE;
					return result = ind_1cyc_reg_(spec);
				}
				else
				{
					return result = ind_1cyc_imm_d_(spec);
				}
			}
			return result = ind_2cyc_ds_(spec);
		case MU_IND_WITH_DS_AUTO:
			if (d_flag_set)
			{
				spec.bank_select = TRUE;
				result = ind_1cyc_reg_(spec);
			}
			else
			{
				result = ind_2cyc_ds_(spec);
			}
			result.next.entry_idx = MU_POST_AUTOIDX;
			return result;
		case MU_IND_DS_IX_IMM:
			return result = imm_ix_index_before_ds_(spec);
		case MU_IND_IMM0:
			return result = ind_1cyc_imm0_(spec);
		case MU_IND_REG_WITH_IMM0:
			return result = ind_2cyc_withimm0_(spec);
		case MU_IND_DS_IX_IMM0:
			result = imm0_ix_index_before_ds_(spec);


		case MU_LD_ZN_IND:
			result = zn_fetch_(spec);
			result.next.entry_idx = MU_LD_ZN_IND__DEREFER_;
			return result;
		case MU_LD_ZN_IND__DEREFER_:
			result = mem_data_dereference_(spec);
			result.next.entry_idx = MU_LD_ZN_IND__MOV_;
			return result;
		case MU_LD_ZN_IND__MOV_:
			return result = ld_mov_(spec);


		case MU_LD_ZN_AUTO_IND:
			result = zn_fetch_(spec);
			result.operation.mem_16bit = TRUE;
			result.next.entry_idx = MU_LD_ZN_AUTO_IND__AUTO_BUFFER_;
			return result;
		case MU_LD_ZN_AUTO_IND__AUTO_BUFFER_:
			result = zn_buffer_autoidx_(spec);
			result.next.entry_idx = MU_LD_ZN_AUTO_IND__DEREFER_;
			return result;
		case MU_LD_ZN_AUTO_IND__DEREFER_:
			result = mem_data_dereference_(spec);
			result.next.entry_idx = MU_LD_ZN_AUTO_IND__MOV_;
			return result;
		case MU_LD_ZN_AUTO_IND__MOV_:
			result = ld_mov_(spec);
			result.next.entry_idx = MU_LD_ZN_AUTO_IND__AUTO_WRITEBACK_;
			return result;
		case MU_LD_ZN_AUTO_IND__AUTO_WRITEBACK_:
			return result = zn_autoidx_writeback_(spec);


		case MU_LD_ZN_WITH_DS_IND:
			result = zn_fetch_(spec);
			result.operation.mem_16bit = TRUE;
			result.next.entry_idx = MU_LD_ZN_WITH_DS_IND__DEREFER_;
			return result;
		case MU_LD_ZN_WITH_DS_IND__DEREFER_:
			if (d_flag_set)
			{
				spec.bank_select = TRUE;
			}
			result = mem_data_dereference_(spec);
			if (d_flag_set)
			{
				result.next.entry_idx = MU_LD_ZN_IND__MOV_;
			}
			else
			{
				result.operation.mem_access_suppress = TRUE;
				result.next.entry_idx = MU_LD_ZN_WITH_DS_IND__DS_CALC_;
			}
			return result;
		case MU_LD_ZN_WITH_DS_IND__DS_CALC_:
			result = zn_ds_calc_(spec);
			result.next.entry_idx = MU_LD_ZN_IND__MOV_;
			return result;


		case MU_LD_ZN_DS_AUTO_IND:
			result = zn_fetch_(spec);
			result.operation.mem_16bit = TRUE;
			result.next.entry_idx = MU_LD_ZN_DS_AUTO_IND__AUTO_BUFFER_;
			return result;
		case MU_LD_ZN_DS_AUTO_IND__AUTO_BUFFER_:
			result = zn_buffer_autoidx_(spec);
			result.next.entry_idx = MU_LD_ZN_DS_AUTO_IND__DEREFER_;
			return result;
		case MU_LD_ZN_DS_AUTO_IND__DEREFER_:
			if (d_flag_set)
			{
				spec.bank_select = TRUE;
			}
			result = mem_data_dereference_(spec);
			if (d_flag_set)
			{
				result.next.entry_idx = MU_LD_ZN_AUTO_IND__MOV_;
			}
			else
			{
				result.operation.mem_access_suppress = TRUE;
				result.next.entry_idx = MU_LD_ZN_DS_AUTO_IND__DS_CALC_;
			}
			result.next.entry_idx = MU_LD_ZN_DS_AUTO_IND__DS_CALC_;
			return result;
		case MU_LD_ZN_DS_AUTO_IND__DS_CALC_:
			result = zn_ds_calc_(spec);
			result.next.entry_idx = MU_LD_ZN_AUTO_IND__MOV_;
			return result;


		case MU_LD_ZN_WITH_IX_IND:
			result = zn_ix_fetch_(spec);
			result.next.entry_idx = MU_LD_ZN_IND__DEREFER_;
			return result;


		case MU_LD_ZN_DS_IX_IND:
			result = zn_ix_fetch_(spec);
			result.operation.mem_16bit = TRUE;
			result.next.entry_idx = MU_LD_ZN_WITH_DS_IND__DEREFER_;
			return result;


		case MU_POST_AUTOIDX:
			return result = after_ds_autoidx_(spec);

		default:
			decode_unreachable_();
			return base_entry_(spec);
	}
}

#include "cpu_decode.h"

typedef bool (*rm_decode_func_t) (pilot_decode_state *, bool);

static data_bus_specifier imm_word_index[] =
{
	DATA_ZERO,
	DATA_LATCH_IMM_0,
	DATA_LATCH_IMM_1,
	DATA_LATCH_IMM_2
};

static bool
rm_imm(pilot_decode_state *state, bool is_dest)
{
	decode_read_word_(state);
	execute_control_word *core_op = &state->work_regs.core_op;
	
	if (is_dest)
	{
		core_op->dest = imm_word_index[state->inst_length];
	}
	else
	{
		core_op->srcs[1].location = imm_word_index[state->inst_length];
	}
	
	return TRUE;
}

static bool
rm_ind_hl_i(pilot_decode_state *state, bool is_dest)
{
	decode_read_word_(state);
	execute_control_word *core_op = &state->work_regs.core_op;
	
	if (is_dest)
	{
		core_op->dest = imm_word_index[state->inst_length];
	}
	else
	{
		core_op->srcs[1].location = imm_word_index[state->inst_length];
	}
	
	return TRUE;
}



static bool
rm_invalid(pilot_decode_state *state, bool is_dest)
{
	(void) state;
	(void) is_dest;
	return FALSE;
}

static rm_decode_func_t decode_jump_table_8bit[] =
{
	rm_invalid
};

static data_bus_specifier rm_register_mapping_8bit[] =
{
	DATA_REG__A,
	DATA_REG__B,
	DATA_REG__H,
	DATA_REG__L,
	DATA_REG__I,
	DATA_REG__X,
	DATA_REG__D,
	DATA_REG__S
};

static data_bus_specifier rm_register_mapping_16bit[] =
{
	DATA_REG_AB,
	DATA_REG__A,
	DATA_REG_HL,
	DATA_REG__B,
	DATA_REG_IX,
	DATA_REG__A,
	DATA_REG_DS,
	DATA_REG_SP
};

static inline bool
rm_requires_mem_fetch_ (rm_spec rm)
{
	return rm != 0 & (rm & 0x03) != 3;
}

// Decodes an RM specifier:
// - Reads additional immediate value if needed
// - Sets alu_src_control fields for core_op
bool
decode_rm_specifier (pilot_decode_state *state, rm_spec rm, bool is_dest, bool is_16bit)
{
	execute_control_word *core_op = &state->work_regs.core_op;
	mucode_entry_spec *run_mucode;
	struct alu_src_control *src_affected;
	if (!is_dest)
	{
		run_mucode = &state->work_regs.run_before;
		src_affected = &state->work_regs.core_op.srcs[0];
	}
	else
	{
		run_mucode = &state->work_regs.run_after;
		src_affected = &state->work_regs.core_op.srcs[1];
	}
	src_affected->is_16bit = is_16bit;
	switch (rm & 0x03)
	{
		case 0:
			// uses imm word
			decode_read_word_(state);
			src_affected->sign_extend = FALSE;
			switch (rm & 0x1c)
			{
				case 0:
					// imm
					if (is_dest)
					{
						return FALSE;
					}
					src_affected->location = DATA_LATCH_IMM_1 + (state->inst_length > 2);
					return TRUE;
				case 28:
					// invalid
					return FALSE;
				case 4: case 8:
					// 00/01:[imm]
					run_mucode->entry_idx = MU_IND_IMM;
					run_mucode->reg_select = (state->inst_length > 2);
					run_mucode->bank_select = (rm == 8);
					break;
				case 12:
					// DS:[imm]
					run_mucode->entry_idx = MU_IND_WITH_DS;
					run_mucode->reg_select = 4 + (state->inst_length > 2);
					break;
				case 16: case 24:
					// [IX/SP+imm]
					run_mucode->entry_idx = MU_IND_REG_WITH_IMM;
					run_mucode->reg_select = (rm == 24) | (state->inst_length > 2) << 1;
					run_mucode->bank_select = 0;
					break;
				case 20:
					// DS:[IX+imm]
					run_mucode->entry_idx = MU_IND_DS_IX_IMM;
					run_mucode->reg_select = (state->inst_length > 2) << 1;
					break;
				default:
					decode_unreachable_();
			}
			// Falls through if rm & 0x1c is not 0 or 28
			run_mucode->is_16bit = is_16bit;
			run_mucode->is_write = is_dest;
			src_affected->location = DATA_LATCH_MEM_DATA;
			if (is_dest)
			{
				core_op->dest = src_affected->location;
			}
			return TRUE;
		case 1:
			// HL/IX auto-index indirect
		case 2:
			// AB/HL/IX/DS indirect
		case 3:
			// register direct
			if (is_16bit)
			{
				src_affected->location = rm_register_mapping_16bit[rm >> 2];
				src_affected->sign_extend = (rm & 0x1c) == 4 || (rm & 0x1c) == 12;
			}
			else
			{
				src_affected->location = rm_register_mapping_8bit[rm >> 2];
				src_affected->sign_extend = FALSE;
			}

			if (is_dest)
			{
				core_op->dest = src_affected->location;
			}
			return TRUE;
		default:
			decode_unreachable_();
	}
	decode_unreachable_();
	return FALSE;
} 

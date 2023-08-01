#include "cpu_decode.h"

typedef bool (*rm_decode_func_t) (pilot_decode_state *, bool);

static inline bool
rm_requires_mem_fetch_ (rm_spec rm)
{
	// oh boy, K-maps
	return ((rm & 0x21) == 0x20) || ((rm & 0x23) == 0x01) || ((rm & 0x23) == 0x02) || (((rm & 0x23) == 0x21) && ((rm & 0x1c) > 0x14));
}

// Decodes an RM specifier:
// - Reads additional immediate value if needed
// - Sets alu_src_control fields for core_op
void
decode_rm_specifier (pilot_decode_state *state, rm_spec rm, bool is_dest, bool src_is_left, data_size_spec size)
{
	execute_control_word *core_op = &state->work_regs.core_op;
	mucode_entry_spec *run_mucode;
	alu_src_control *src_affected;
	
	state->rm_ops++;
	
	if (src_is_left)
	{
		src_affected = &state->work_regs.core_op.srcs[0];
	}
	else if (!is_dest)
	{
		src_affected = &state->work_regs.core_op.srcs[1];
	}
	else
	{
		src_affected = 0;
	}
	
	if (src_affected)
	{
		src_affected->size = size;
	}
	if (rm_requires_mem_fetch_(rm))
	{
		if (src_affected)
		{
			src_affected->location = DATA_LATCH_MEM_DATA;
		}
		if (is_dest && src_is_left)
		{
			// left source and destination are the same
			// fetch and set up writeback
			run_mucode = &state->work_regs.run_before;
			core_op->dest = DATA_LATCH_MEM_DATA;
			core_op->mem_latch_ctl = MEM_LATCH_HALF2_MAR;
			core_op->mem_write_ctl = MEM_WRITE_FROM_DEST;
			core_op->mem_size = size;
		}
		else if (is_dest)
		{
			// destination only, not part of core op; no fetch
			run_mucode = &state->work_regs.run_after;
		}
		else
		{
			// source only, fetch
			run_mucode = &state->work_regs.run_before;
		}
		
		run_mucode->is_write = is_dest;
	}
	
	if (src_affected && ((rm & 0x03) == 0x03))
	{
		// Short form immediate
		src_affected->sign_extend = FALSE;
		src_affected->location = !(state->rm_ops == 1) ? DATA_LATCH_SFI_1 : DATA_LATCH_SFI_2;
	}
	else if ((rm & 0x23) == 0x22)
	{
		// Register indirect with pre-decrement
		run_mucode->entry_idx = MU_IND_REG_AUTO;
		run_mucode->reg_select = (rm >> 2) & 0x7;
	}
	else if ((rm & 0x3b) == 0x39)
	{
		// Extra word needed
		decode_queue_read_word_(state);
		
		if (!(rm & 0x4))
		{
			// Register indexed
			run_mucode->entry_idx = MU_IND_REG_WITH_BITS;
			run_mucode->reg_select = 0;
		}
		else
		{
			// Absolute indexed
			decode_queue_read_word_(state);
			run_mucode->entry_idx = MU_IND_IMM_WITH_BITS;
			run_mucode->reg_select = 0;
		}
	}
	else if ((rm & 0x3b) == 0x31)
	{
		// PGC relative
		decode_queue_read_word_(state);
		run_mucode->entry_idx = MU_IND_PGC_WITH_IMM_RM;
		run_mucode->reg_select = 0;
		if (!(rm & 0x04))
		{
			// signed 16-bit
			run_mucode->reg_select |= 0x8;
		}
		else
		{
			// unsigned 24-bit
			decode_queue_read_word_(state);
		}
	}
	else if ((rm & 0x3b) == 0x29)
	{
		// Absolute
		run_mucode->size = !(rm & 0x04) ? SIZE_16_BIT : SIZE_24_BIT;
		run_mucode->reg_select = !(rm & 0x04) ? 8 : 0;
	}
	else if (src_affected && ((rm & 0x3b) == 0x21))
	{
		// Immediate
		src_affected->size = !(rm & 0x04) ? SIZE_16_BIT : SIZE_24_BIT;
		src_affected->location = !(state->rm_ops == 1) ? DATA_LATCH_IMM_1 : DATA_LATCH_RM_1;
		src_affected->sign_extend = !(rm & 0x04);
	}
	else if ((rm & 0x23) == 0x20)
	{
		// Register indirect with post-increment
		run_mucode->entry_idx = MU_IND_REG_POST_AUTO;
		run_mucode->reg_select = (rm >> 2) & 0x7;
	}
	else if ((rm & 0x23) == 0x02)
	{
		// Register indirect
		run_mucode->entry_idx = MU_IND_REG;
		run_mucode->reg_select = (rm >> 2) & 0x7;
	}
	else if ((rm & 0x23) == 0x01)
	{
		// Register relative
		run_mucode->entry_idx = MU_IND_REG_WITH_IMM;
		run_mucode->reg_select = (rm >> 2) & 0x7;
	}
	else if (src_affected && ((rm & 0x23) == 0x00))
	{
		// Register direct
		uint_fast8_t reg = (rm >> 2) & 0x7;
		src_affected->location = (size == SIZE_8_BIT) ? DATA_REG_L0 + reg : DATA_REG_P0 + reg;
	}
	
	if (state->rm_ops == 2)
	{
		run_mucode->reg_select |= 0x10;
	}
	
	return;
} 

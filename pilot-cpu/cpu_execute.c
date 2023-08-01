#include <string.h>
#include "cpu_regs.h"
#include "cpu_decode.h"
#include "cpu_execute.h"
#include "memory.h"
#include "types.h"

typedef struct {
	Pilot_system *sys;
	
	inst_decoded_flags decoded_inst;
	mucode_entry_spec mucode_control;
	execute_control_word mucode_decoded_buffer;
	execute_control_word *control;
	
	uint_fast24_t alu_input_latches[2];
	uint_fast24_t alu_output_latch;
	bool alu_shifter_carry_bit;
	
	// Memory address and data registers for requesting memory accesses
	uint_fast24_t mem_addr;
	uint16_t mem_data;
	
	// When reading memory, this flag will be high until the memory access has been completed.
	// During this time, any reads from mem_data will block until this flag goes low.
	bool mem_access_waiting;
	bool mem_access_was_read;
	
	enum
	{
		EXEC_HALF1_READY,
		EXEC_HALF1_MEM_WAIT,
		EXEC_HALF1_OPERAND_LATCH,
		EXEC_HALF1_MEM_PREPARE,
		EXEC_HALF1_MEM_ASSERT,
		
		EXEC_HALF2_READY,
		EXEC_HALF2_RESULT_LATCH,
		EXEC_HALF2_MEM_PREPARE,
		EXEC_HALF2_MEM_ASSERT,
		
		EXEC_ADVANCE_SEQUENCER,
		EXEC_EXCEPTION
	} execution_phase;
	
	enum
	{
		EXEC_SEQ_WAIT_NEXT_INS,
		EXEC_SEQ_EVAL_CONTROL,
		EXEC_SEQ_OVERRIDE_OP,
		EXEC_SEQ_RUN_BEFORE,
		EXEC_SEQ_CORE_OP,
		EXEC_SEQ_CORE_OP_EXECUTED,
		EXEC_SEQ_RUN_AFTER,
		EXEC_SEQ_FINAL_STEPS,
		EXEC_SEQ_SIGNAL_BRANCH,
	} sequencer_phase;
} pilot_execute_state;

void execute_unreachable_ ();

#define ACCESS_REG_BITS_(state, r, size) fetch_data_(state, (size == SIZE_8_BIT ? DATA_REG_L0 : DATA_REG_P0) + r)
#define READ_IMM_LATCH_(state, imm, size) (size == SIZE_24_BIT ? (((state->decoded_inst.imm_words[imm] & 0xff) << 16) | state->decoded_inst.imm_words[imm + 1]) : state->decoded_inst.imm_words[imm])

static uint_fast24_t
fetch_data_ (pilot_execute_state *state, data_bus_specifier src)
{
	switch (src)
	{
		case DATA_ZERO:
			return 0;
		case DATA_SIZE:
			{
				data_size_spec size = state->mucode_decoded_buffer.srcs[0].size;
				if (size == SIZE_8_BIT) {
					if (state->mucode_decoded_buffer.srcs[0].location == DATA_REG_SP || state->mucode_decoded_buffer.srcs[1].location == DATA_REG_SP)
						return 2;
					else
						return 1;
				}
				else if (size == SIZE_16_BIT)
					return 2;
				else
					return 4;
			}
		case DATA_REG_L0:
			return state->sys->core.regs[0] & 0xff;
		case DATA_REG_L1:
			return state->sys->core.regs[1] & 0xff;
		case DATA_REG_L2:
			return state->sys->core.regs[2] & 0xff;
		case DATA_REG_L3:
			return state->sys->core.regs[3] & 0xff;
		case DATA_REG_M0:
			return (state->sys->core.regs[0] >> 8) & 0xff;
		case DATA_REG_M1:
			return (state->sys->core.regs[1] >> 8) & 0xff;
		case DATA_REG_M2:
			return (state->sys->core.regs[2] >> 8) & 0xff;
		case DATA_REG_M3:
			return (state->sys->core.regs[3] >> 8) & 0xff;
		case DATA_REG_W0:
			return state->sys->core.regs[0] & 0xffff;
		case DATA_REG_W1:
			return state->sys->core.regs[1] & 0xffff;
		case DATA_REG_W2:
			return state->sys->core.regs[2] & 0xffff;
		case DATA_REG_W3:
			return state->sys->core.regs[3] & 0xffff;
		case DATA_REG_W4:
			return state->sys->core.regs[4] & 0xffff;
		case DATA_REG_W5:
			return state->sys->core.regs[5] & 0xffff;
		case DATA_REG_W6:
			return state->sys->core.regs[6] & 0xffff;
		case DATA_REG_W7:
			return state->sys->core.regs[7] & 0xffff;
		case DATA_REG_P0:
			return state->sys->core.regs[0];
		case DATA_REG_P1:
			return state->sys->core.regs[1];
		case DATA_REG_P2:
			return state->sys->core.regs[2];
		case DATA_REG_P3:
			return state->sys->core.regs[3];
		case DATA_REG_P4:
			return state->sys->core.regs[4];
		case DATA_REG_P5:
			return state->sys->core.regs[5];
		case DATA_REG_P6:
			return state->sys->core.regs[6];
		case DATA_REG_SP:
			return state->sys->core.regs[7];
		case DATA_REG_PGC:
			return state->sys->core.pgc;
		case DATA_REG__F:
			return state->sys->core.wf & 0xff;
		case DATA_REG__W:
			return state->sys->core.wf >> 8;
		case DATA_REG_WF:
			return state->sys->core.wf;
		case DATA_LATCH_REPI:
			return state->sys->core.repi;
		case DATA_LATCH_REPR:
			return state->sys->core.repr;
		case DATA_LATCH_MEM_ADDR:
			return state->mem_addr;
		case DATA_LATCH_MEM_DATA:
			return state->mem_data;
		case DATA_LATCH_IMM_0:
			return READ_IMM_LATCH_(state, 0, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_LATCH_IMM_1:
			return READ_IMM_LATCH_(state, 1, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_LATCH_IMM_2:
			return READ_IMM_LATCH_(state, 2, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_LATCH_IMM_HML_RM:
			return ((state->decoded_inst.imm_words[2] & 0xff) << 16) | state->decoded_inst.imm_words[1];
		case DATA_LATCH_SFI_1:
			return (state->decoded_inst.imm_words[0] >> 2) & 0x000f;
		case DATA_LATCH_SFI_2:
			return (state->decoded_inst.imm_words[0] >> 8) & 0x000f;
		case DATA_LATCH_RM_1:
			return READ_IMM_LATCH_(state, state->decoded_inst.rm2_offset, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_LATCH_RM_2:
			return READ_IMM_LATCH_(state, state->decoded_inst.rm2_offset + 1, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_LATCH_RM_HML:
			return ((state->decoded_inst.imm_words[state->decoded_inst.rm2_offset + 1] & 0xff) << 16) | state->decoded_inst.imm_words[state->decoded_inst.rm2_offset];
		case DATA_REG_IMM_0_8:
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[0] >> 8) & 0x7, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_REG_IMM_1_8:
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[1] >> 8) & 0x7, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_REG_IMM_1_2:
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[1] >> 2) & 0x7, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_REG_IMM_2_8:
		{
			{
			if (state->decoded_inst.imm_words[2] >= 0xc000) 
			{
				decode_invalid_opcode_(decode_state);
				return 0;
			}
			state->mucode_decoded_buffer.srcs[1].sign_extend = ((state->decoded_inst.imm_words[2] & 0x0800) != 0);
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[2] >> 8) & 0x7, state->decoded_inst.imm_words[2] >> 14);
		}
		case DATA_REG_RM_1_8:
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[state->decoded_inst.rm2_offset] >> 8) & 0x7, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_REG_RM_1_2:
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[state->decoded_inst.rm2_offset] >> 2) & 0x7, state->mucode_decoded_buffer.srcs[0].size);
		case DATA_REG_RM_2_8:
		{
			if (state->decoded_inst.imm_words[state->decoded_inst.rm2_offset + 1] >= 0xc000) 
			{
				decode_invalid_opcode_(decode_state);
				return 0;
			}
			state->mucode_decoded_buffer.srcs[1].sign_extend = ((state->decoded_inst.imm_words[state->decoded_inst.rm2_offset + 1] & 0x0800) != 0);
			return ACCESS_REG_BITS_(state, (state->decoded_inst.imm_words[state->decoded_inst.rm2_offset + 1] >> 8) & 0x7, state->decoded_inst.imm_words[state->decoded_inst.rm2_offset + 1] >> 14);
		}
		default:
			execute_unreachable_();
	}
}

static void
write_data_ (pilot_execute_state *state, data_bus_specifier dest, uint_fast24_t *src)
{
	switch (dest)
	{
		case DATA_ZERO:
		case DATA_SIZE:
			return;
		case DATA_REG_L0:
			state->sys->core.regs[0] &= 0xffff00;
			state->sys->core.regs[0] |= *src & 0xff;
			return;
		case DATA_REG_L1:
			state->sys->core.regs[1] &= 0xffff00;
			state->sys->core.regs[1] |= *src & 0xff;
			return;
		case DATA_REG_L2:
			state->sys->core.regs[2] &= 0xffff00;
			state->sys->core.regs[2] |= *src & 0xff;
			return;
		case DATA_REG_L3:
			state->sys->core.regs[3] &= 0xffff00;
			state->sys->core.regs[3] |= *src & 0xff;
			return;
		case DATA_REG_M0:
			state->sys->core.regs[0] &= 0xff00ff;
			state->sys->core.regs[0] |= ((*src & 0xff) << 8);
			return;
		case DATA_REG_M1:
			state->sys->core.regs[1] &= 0xff00ff;
			state->sys->core.regs[1] |= ((*src & 0xff) << 8);
			return;
		case DATA_REG_M2:
			state->sys->core.regs[2] &= 0xff00ff;
			state->sys->core.regs[2] |= ((*src & 0xff) << 8);
			return;
		case DATA_REG_M3:
			state->sys->core.regs[3] &= 0xff00ff;
			state->sys->core.regs[3] |= ((*src & 0xff) << 8);
			return;
		case DATA_REG_W0:
			state->sys->core.regs[0] &= 0xff0000;
			state->sys->core.regs[0] |= *src & 0xffff;
			return;
		case DATA_REG_W1:
			state->sys->core.regs[1] &= 0xff0000;
			state->sys->core.regs[1] |= *src & 0xffff;
			return;
		case DATA_REG_W2:
			state->sys->core.regs[2] &= 0xff0000;
			state->sys->core.regs[2] |= *src & 0xffff;
			return;
		case DATA_REG_W3:
			state->sys->core.regs[3] &= 0xff0000;
			state->sys->core.regs[3] |= *src & 0xffff;
			return;
		case DATA_REG_W4:
			state->sys->core.regs[4] &= 0xff0000;
			state->sys->core.regs[4] |= *src & 0xffff;
			return;
		case DATA_REG_W5:
			state->sys->core.regs[5] &= 0xff0000;
			state->sys->core.regs[5] |= *src & 0xffff;
			return;
		case DATA_REG_W6:
			state->sys->core.regs[6] &= 0xff0000;
			state->sys->core.regs[6] |= *src & 0xffff;
			return;
		case DATA_REG_W7:
			state->sys->core.regs[7] &= 0xff0000;
			state->sys->core.regs[7] |= *src & 0xffff;
			return;
		case DATA_REG_P0:
			state->sys->core.regs[0] = *src & 0xffffff;
			return;
		case DATA_REG_P1:
			state->sys->core.regs[1] = *src & 0xffffff;
			return;
		case DATA_REG_P2:
			state->sys->core.regs[2] = *src & 0xffffff;
			return;
		case DATA_REG_P3:
			state->sys->core.regs[3] = *src & 0xffffff;
			return;
		case DATA_REG_P4:
			state->sys->core.regs[4] = *src & 0xffffff;
			return;
		case DATA_REG_P5:
			state->sys->core.regs[5] = *src & 0xffffff;
			return;
		case DATA_REG_P6:
			state->sys->core.regs[6] = *src & 0xffffff;
			return;
		case DATA_REG_SP:
			state->sys->core.regs[7] = *src & 0xffffff;
			return;
		case DATA_REG__F:
			state->sys->core.wf &= 0xff00;
			state->sys->core.wf |= *src & 0xff;
			return;
		case DATA_REG__W:
			state->sys->core.wf &= 0x00ff;
			state->sys->core.wf |= ((*src & 0xff) << 8);
			return;
		case DATA_REG_WF:
			state->sys->core.wf = *src & 0xffff;
			return;
		case DATA_REG_PGC:
			state->sys->core.pgc = *src & 0xfffffe;
			return;
		case DATA_LATCH_MEM_ADDR:
			state->mem_addr = *src;
			return;
		case DATA_LATCH_MEM_DATA:
			state->mem_data = *src;
			return;
		case DATA_REG_IMM_0_8:
			state->sys->core.regs[(state->decoded_inst.imm_words[0] >> 8) & 0x7] = *src;
		case DATA_REG_IMM_1_8:
			state->sys->core.regs[(state->decoded_inst.imm_words[1] >> 8) & 0x7] = *src;
		case DATA_REG_IMM_1_2:
			state->sys->core.regs[(state->decoded_inst.imm_words[1] >> 2) & 0x7] = *src;
		case DATA_REG_RM_1_8:
			state->sys->core.regs[(state->decoded_inst.imm_words[state->decoded_inst.rm2_offset] >> 8) & 0x7] = *src;
		case DATA_REG_RM_1_2:
			 state->sys->core.regs[(state->decoded_inst.imm_words[state->decoded_inst.rm2_offset] >> 2) & 0x7] = *src;
		case DATA_REG_IMM_2_8:
		case DATA_REG_RM_2_8:
		case DATA_LATCH_IMM_0:
		case DATA_LATCH_IMM_1:
		case DATA_LATCH_IMM_2:
		case DATA_LATCH_IMM_HML_RM:
		case DATA_LATCH_SFI_1:
		case DATA_LATCH_SFI_2:
		case DATA_LATCH_RM_1:
		case DATA_LATCH_RM_2:
		case DATA_LATCH_RM_HML:
			return;
		default:
			execute_unreachable_();
	}
}

static void
execute_half1_mem_wait_ (pilot_execute_state *state)
{
	if (state->mem_access_waiting)
	{
		if (Pilot_mem_data_wait(state->sys))
		{
			state->mem_access_waiting = FALSE;
			if (state->mem_access_was_read)
			{
				state->mem_data = Pilot_mem_get_data(state->sys);
			}
		}
		else if (state->control->srcs[0].location == DATA_LATCH_MEM_DATA
			|| state->control->srcs[1].location == DATA_LATCH_MEM_DATA
			|| state->control->dest == DATA_LATCH_MEM_DATA)
		{
			return;
		}
	}
	state->execution_phase = EXEC_HALF1_OPERAND_LATCH;
}

static void
execute_half1_mem_prepare_ (pilot_execute_state *state)
{
	if (state->control->mem_latch_ctl == MEM_LATCH_HALF1)
	{
		if (state->control->mem_write_ctl != MEM_READ)
		{
			switch (state->control->mem_write_ctl)
			{
				case MEM_WRITE_FROM_SRC1:
					state->mem_data = state->alu_input_latches[1];
					break;
				case MEM_WRITE_FROM_DEST:
					// MEM_WRITE_FROM_DEST should only be used in cycle 2!
					execute_unreachable_();
					state->mem_data = state->alu_output_latch;
					break;
				case MEM_WRITE_FROM_MDR:
					break;
				default:
					execute_unreachable_();
			}
		}
	}
	state->execution_phase = EXEC_HALF1_MEM_ASSERT;
}

static void
execute_half1_mem_assert_ (pilot_execute_state *state)
{
	if (state->control->mem_latch_ctl == MEM_LATCH_HALF1 && !state->control->mem_access_suppress)
	{
		if (state->control->mem_write_ctl == MEM_READ)
		{
			if (!Pilot_mem_addr_read_assert(state->sys, state->mem_addr))
			{
				return;
			}
			state->mem_access_was_read = TRUE;
		}
		else
		{
			if (!Pilot_mem_addr_write_assert(state->sys, state->mem_addr, state->mem_data))
			{
				return;
			}
		}
		state->mem_access_waiting = TRUE;
	}
	state->execution_phase = EXEC_HALF2_READY;
}

void
pilot_execute_half1 (pilot_execute_state *state)
{
	if (state->execution_phase == EXEC_HALF1_READY)
	{
		state->execution_phase = EXEC_HALF1_MEM_WAIT;
	}
	
	if (state->execution_phase == EXEC_HALF1_MEM_WAIT)
	{
		execute_half1_mem_wait_(state);
	}
	
	if (state->execution_phase == EXEC_HALF1_OPERAND_LATCH)
	{
		state->alu_input_latches[0] = fetch_data_(state, state->control->srcs[0].location);
		state->alu_input_latches[1] = fetch_data_(state, state->control->srcs[1].location);
		state->execution_phase = EXEC_HALF1_MEM_PREPARE;
	}
	
	if (state->execution_phase == EXEC_HALF1_MEM_PREPARE)
	{
		execute_half1_mem_prepare_(state);
	}
	
	if (state->execution_phase == EXEC_HALF1_MEM_ASSERT)
	{
		execute_half1_mem_assert_(state);
	}
}

static inline uint16_t
alu_operate_shifter_ (pilot_execute_state *state, uint16_t operand)
{
	bool inject_bit;
	bool msb_bit;
	bool lsb_bit = operand & 1;
	bool carry_flag = (fetch_data_(state, DATA_REG__F) & F_CARRY) != 0;
	
	if (state->control->srcs[1].size == SIZE_8_BIT)
	{
		msb_bit = (operand & 0x80) != 0;
	}
	else if (state->control->srcs[1].size == SIZE_16_BIT)
	{
		msb_bit = (operand & 0x8000) != 0;
	}
	else
	{
		msb_bit = (operand & 0x800000) != 0;
	}
	
	switch (state->control->shifter_mode)
	{
		case SHIFTER_NONE:
		case SHIFTER_LEFT:
		case SHIFTER_RIGHT_LOGICAL:
			inject_bit = 0;
			break;
		case SHIFTER_LEFT_CARRY:
		case SHIFTER_RIGHT_CARRY:
			inject_bit = carry_flag;
			break;
		case SHIFTER_LEFT_BARREL:
		case SHIFTER_RIGHT_ARITH:
			inject_bit = msb_bit;
			break;
		case SHIFTER_RIGHT_BARREL:
			inject_bit = lsb_bit;
			break;
		default:
			execute_unreachable_();
	}
	
	switch (state->control->shifter_mode)
	{
		case SHIFTER_NONE:
			break;
		case SHIFTER_LEFT:
		case SHIFTER_LEFT_CARRY:
		case SHIFTER_LEFT_BARREL:
			state->alu_shifter_carry_bit = msb_bit ^ state->control->invert_carries;
			operand <<= 1;
			operand |= inject_bit;
			break;
		case SHIFTER_RIGHT_LOGICAL:
		case SHIFTER_RIGHT_ARITH:
		case SHIFTER_RIGHT_CARRY:
		case SHIFTER_RIGHT_BARREL:
			state->alu_shifter_carry_bit = lsb_bit ^ state->control->invert_carries;
			operand >>= 1;
			if (state->control->srcs[1].is_16bit)
			{
				operand |= inject_bit << 15;
			}
			else
			{
				operand |= inject_bit << 7;
			}
			break;
		default:
			execute_unreachable_();
	}
	
	return operand;
}

static inline uint8_t
alu_modify_flags_ (pilot_execute_state *state, uint8_t flags, uint_fast24_t operands[2], uint_fast24_t result, uint_fast24_t carries)
{
	bool alu_carry;
	bool alu_overflow;
	bool alu_zero;
	bool alu_neg;
	uint8_t flag_source_word = 0;
	
	uint_fast24_t alu_parity = result;
	alu_parity = alu_parity ^ alu_parity >> 4;
	alu_parity = alu_parity ^ alu_parity >> 2;
	alu_parity = alu_parity ^ alu_parity >> 1;
	
	if (state->control->srcs[0].size == SIZE_8_BIT)
	{
		alu_carry = (carries & 0x80) != 0;
		alu_neg = (result & 0x80) != 0;
		alu_overflow = ((operands[1] ^ result) & (operands[0] ^ result) & 0x80) != 0;
		alu_zero = (result & 0xff) == 0;
	}
	else if (state->control->srcs[0].size == SIZE_16_BIT)
	{
		alu_carry = (carries & 0x8000) != 0;
		alu_neg = (result & 0x8000) != 0;
		alu_overflow = ((operands[1] ^ result) & (operands[0] ^ result) & 0x8000) != 0;
		alu_zero = (result & 0xffff) == 0;
		alu_parity ^= (alu_parity >> 8);
	}
	else
	{
		alu_carry = (carries & 0x800000) != 0;
		alu_neg = (result & 0x800000) != 0;
		alu_overflow = ((operands[1] ^ result) & (operands[0] ^ result) & 0x800000) != 0;
		alu_zero = (result) == 0;
		alu_parity ^= (alu_parity >> 16);
	}
	alu_carry ^= state->control->invert_carries;
	
	// S - Sign/negative flag
	flag_source_word |= alu_neg << 7;
	// Z - Zero flag
	flag_source_word |= alu_zero << 6;
	// V - Overflow/parity flag
	switch (state->control->flag_v_mode)
	{
		case FLAG_V_NORMAL:
			if (state->control->operation == ALU_ADD)
			{
				// overflow
				flag_source_word |= alu_overflow << 2;
			}
			else
			{
				// parity
				flag_source_word |= alu_parity << 2;
			}
			break;
		case FLAG_V_SHIFTER_CARRY:
			flag_source_word |= (state->alu_shifter_carry_bit != 0) << 2;
			break;
		case FLAG_V_CLEAR:
			break;
		default:
			execute_unreachable_();
	}
	
	// C, X - Carry/borrow flags
	flag_source_word |= alu_carry << 1;
	flag_source_word |= alu_carry;
	
	flags &= ~state->control->flag_write_mask;
	flags |= (flag_source_word & state->control->flag_write_mask);
	
	return flags;
}

static void
execute_half2_result_latch_ (pilot_execute_state *state)
{
	int i;
	uint_fast24_t operands[2];
	
	alu_src_control *src2 = &state->control->srcs[1];
	uint8_t flags = fetch_data_(state, DATA_REG__F);
	bool carry_flag_status = (flags & F_CARRY) != 0;
	for (i = 0; i < 2; i++)
	{
		alu_src_control *src = &state->control->srcs[i];
		operands[i] = state->alu_input_latches[i];
		if (src->size == SIZE_8_BIT)
		{
			operands[i] &= 0xff;
			if (src->sign_extend && (operands[i] & 0x80))
			{
				operands[i] |= 0xffff00;
			}
		}
		else if (src->size == SIZE_16_BIT)
		{
			operands[i] &= 0xffff;
			if (src->sign_extend && (operands[i] & 0x8000))
			{
				operands[i] |= 0xff0000;
			}
		}
	}
	
	if (state->control->src2_add1)
	{
		operands[1] += 1;
	}
	else if (state->control->src2_add_carry)
	{
		operands[1] += carry_flag_status;
	}
	
	if (state->control->src2_negate)
	{
		operands[1] = ~operands[1] + 1;
		if (src2->size == SIZE_8_BIT && !src2->sign_extend)
		{
			operands[1] &= 0xff;
		}
		else if (src2->size == SIZE_16_BIT && !src2->sign_extend)
		{
			operands[1] &= 0xffff;
		}
	}
	
	state->alu_shifter_carry_bit = FALSE;
	operands[1] = alu_operate_shifter_(state, operands[1]);

	switch (state->control->operation)
	{
		case ALU_OFF:
			break;
		case ALU_ADD:
			state->alu_output_latch = operands[0] + operands[1];
			carries = operands[0] ^ operands[1] ^ state->alu_output_latch;
			break;
		case ALU_AND:
			state->alu_output_latch = operands[0] & operands[1];
			carries = 0;
			break;
		case ALU_OR:
			state->alu_output_latch = operands[0] | operands[1];
			carries = 0;
			break;
		case ALU_XOR:
			state->alu_output_latch = operands[0] ^ operands[1];
			carries = 0;
			break;
		default:
			execute_unreachable_();
	}
	
	if ((&state->control->srcs[0].size == SIZE_8_BIT) && (flags & F_DECIMAL) && (carries & 0x08))
	{
		state->alu_output_latch = state->alu_output_latch + 0x10;
		carries = (carries & 0x0f) | ((operands[0] ^ operands[1] ^ state->alu_output_latch) & 0xf0);
	
	if (state->control->operation != ALU_OFF)
	{
		flags = alu_modify_flags_(state, flags, operands, state->alu_output_latch, carries);
	}
	
	state->execution_phase = EXEC_HALF2_MEM_PREPARE;
}

static void
execute_half2_mem_prepare_ (pilot_execute_state *state)
{
	bool carry_flag = (fetch_data_(state, DATA_REG__F) & F_CARRY) != 0;
	
	if (state->control->mem_latch_ctl == MEM_LATCH_HALF2)
	{
		state->mem_addr = state->alu_output_latch;
	}
	if (state->control->mem_latch_ctl >= MEM_LATCH_HALF2)
	{
		if (state->control->mem_write_ctl != MEM_READ)
		{
			switch (state->control->mem_write_ctl)
			{
				case MEM_WRITE_FROM_SRC1:
					// MEM_WRITE_FROM_SRC1 should only be used in cycle 1!
					execute_unreachable_();
					state->mem_data = state->alu_input_latches[1];
					break;
				case MEM_WRITE_FROM_DEST:
					state->mem_data = state->alu_output_latch;
					break;
				case MEM_WRITE_FROM_MDR:
					break;
				default:
					execute_unreachable_();
			}
		}
	}
	
	state->execution_phase = EXEC_HALF2_MEM_ASSERT;
}

static void
execute_half2_mem_assert_ (pilot_execute_state *state)
{
	if (state->control->mem_latch_ctl >= MEM_LATCH_HALF2 && !state->control->mem_access_suppress)
	{
		if (state->control->mem_write_ctl == MEM_READ)
		{
			if (!Pilot_mem_addr_read_assert(
				state->sys, state->mem_addr))
			{
				return;
			}
			state->mem_access_was_read = TRUE;
		}
		else
		{
			if (!Pilot_mem_addr_write_assert(state->sys, state->mem_addr, state->mem_data))
			{
				return;
			}
		}
		state->mem_access_waiting = TRUE;
	}
	state->execution_phase = EXEC_ADVANCE_SEQUENCER;
}

void
pilot_execute_half2 (pilot_execute_state *state)
{
	if (state->execution_phase == EXEC_HALF2_READY)
	{
		state->execution_phase = EXEC_HALF2_RESULT_LATCH;
	}
	
	if (state->execution_phase == EXEC_HALF2_RESULT_LATCH)
	{
		execute_half2_result_latch_(state);
	}
	
	if (state->execution_phase == EXEC_HALF2_MEM_PREPARE)
	{
		execute_half2_mem_prepare_(state);
	}
	
	if (state->execution_phase == EXEC_HALF2_MEM_ASSERT)
	{
		execute_half2_mem_assert_(state);
	}
}

bool
pilot_execute_sequencer_mucode_run (pilot_execute_state *state)
{
	mucode_entry decoded;
	
	decoded = decode_mucode_entry(state->mucode_control);
	state->mucode_control = decoded.next;
	state->mucode_decoded_buffer = decoded.operation;
	state->control = &state->mucode_decoded_buffer;
	
	// Return TRUE if there's another microcode entry to be run
	if (state->mucode_control.entry_idx != MU_NONE)
	{
		return TRUE;
	}
	return FALSE;
}

void
pilot_execute_sequencer_advance (pilot_execute_state *state)
{
	if (state->sequencer_phase == EXEC_SEQ_CORE_OP_EXECUTED)
	{
		if (state->decoded_inst.run_after.entry_idx != MU_NONE)
		{
			state->sequencer_phase = EXEC_SEQ_RUN_AFTER;
			state->mucode_control = state->decoded_inst.run_after;
		}
		else
		{
			state->sequencer_phase = EXEC_SEQ_FINAL_STEPS;
		}
	}

	if (state->sequencer_phase == EXEC_SEQ_FINAL_STEPS)
	{
		state->sequencer_phase = EXEC_SEQ_WAIT_NEXT_INS;
	}
	
	if (state->sequencer_phase == EXEC_SEQ_WAIT_NEXT_INS)
	{
		if (state->sys->interconnects.decoded_inst_semaph)
		{
			state->decoded_inst = *state->sys->interconnects.decoded_inst;
			state->sys->interconnects.decoded_inst_semaph = FALSE;
			state->sequencer_phase = EXEC_SEQ_EVAL_CONTROL;
		}
	}
	
	if (state->sequencer_phase == EXEC_SEQ_EVAL_CONTROL)
	{
		if (state->decoded_inst.override_op.entry_idx != MU_NONE)
		{
			state->sequencer_phase = EXEC_SEQ_OVERRIDE_OP;
			state->mucode_control = state->decoded_inst.override_op;
		}
		else if (state->decoded_inst.run_before.entry_idx != MU_NONE)
		{
			state->sequencer_phase = EXEC_SEQ_RUN_BEFORE;
			state->mucode_control = state->decoded_inst.run_before;
		}
		else
		{
			state->sequencer_phase = EXEC_SEQ_CORE_OP;
		}
	}
	
	if (state->sequencer_phase == EXEC_SEQ_OVERRIDE_OP)
	{
		if (!pilot_execute_sequencer_mucode_run(state))
		{
			state->sequencer_phase = EXEC_SEQ_FINAL_STEPS;
		}
	}
	
	if (state->sequencer_phase == EXEC_SEQ_RUN_BEFORE)
	{
		if (!pilot_execute_sequencer_mucode_run(state))
		{
			state->sequencer_phase = EXEC_SEQ_CORE_OP;
		}
	}
	
	if (state->sequencer_phase == EXEC_SEQ_CORE_OP)
	{
		state->control = &state->decoded_inst.core_op;
		state->sequencer_phase = EXEC_SEQ_CORE_OP_EXECUTED;
	}
	
	if (state->sequencer_phase == EXEC_SEQ_RUN_AFTER)
	{
		if (!pilot_execute_sequencer_mucode_run(state))
		{
			state->sequencer_phase = EXEC_SEQ_FINAL_STEPS;
		}
	}
}


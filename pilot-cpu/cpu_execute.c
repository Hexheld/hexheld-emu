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
	
	uint16_t alu_input_latches[2];
	uint16_t alu_output_latch;
	bool alu_shifter_carry_bit;
	bool alu_half_carry;
	bool a_flag_locked;
	
	// Memory address (16-bit) and data registers for requesting memory accesses
	uint8_t mem_bank;
	uint16_t mem_addr_low;
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

#define ACCESS_REG_(which) \
	state->sys->core.regs_data[ ( state->sys->core.shadow_ctl & DATAREG_ ## which ) != 0 ] . which

static uint_fast16_t
fetch_data_ (pilot_execute_state *state, data_bus_specifier src)
{
	switch (src)
	{
		case DATA_ZERO:
			return 0;
		case DATA_REG__A:
			return ACCESS_REG_(a);
		case DATA_REG__B:
			return ACCESS_REG_(b);
		case DATA_REG__H:
			return ACCESS_REG_(h);
		case DATA_REG__L:
			return ACCESS_REG_(l);
		case DATA_REG__I:
			return ACCESS_REG_(i);
		case DATA_REG__X:
			return ACCESS_REG_(x);
		case DATA_REG__D:
			return ACCESS_REG_(d);
		case DATA_REG__S:
			return ACCESS_REG_(s);
		case DATA_REG_S_:
			return ACCESS_REG_(s) << 8;
		case DATA_REG_AB:
			return ACCESS_REG_(a) << 8 | ACCESS_REG_(b);
		case DATA_REG_HL:
			return ACCESS_REG_(h) << 8 | ACCESS_REG_(l);
		case DATA_REG_IX:
			return ACCESS_REG_(i) << 8 | ACCESS_REG_(x);
		case DATA_REG_DS:
			return ACCESS_REG_(d) << 8 | ACCESS_REG_(s);
		case DATA_REG__C:
			return ACCESS_REG_(c);
		case DATA_REG_SP:
			return state->sys->core.sp;
		case DATA_REG_PC:
			return state->sys->core.pc;
		case DATA_REG__K:
			return state->sys->core.k;
		case DATA_REG__F:
			return ACCESS_REG_(f);
		case DATA_REG_KF:
			return state->sys->core.k << 8 & ACCESS_REG_(f);
		case DATA_REG__E:
			return state->sys->core.int_mask;
		case DATA_LATCH_MEM_ADDR:
			return state->mem_addr_low;
		case DATA_LATCH_MEM_DATA:
			return state->mem_data;
		case DATA_LATCH_IMM_0:
			return state->decoded_inst.imm_words[0];
		case DATA_LATCH_IMM_1:
			return state->decoded_inst.imm_words[1];
		case DATA_LATCH_IMM_2:
			return state->decoded_inst.imm_words[2];
		default:
			execute_unreachable_();
	}
}

static void
write_data_ (pilot_execute_state *state, data_bus_specifier dest, uint16_t *src)
{
	switch (dest)
	{
		case DATA_ZERO:
			return;
		case DATA_REG__A:
			ACCESS_REG_(a) = *src & 0xff;
			return;
		case DATA_REG__B:
			ACCESS_REG_(b) = *src & 0xff;
			return;
		case DATA_REG__H:
			ACCESS_REG_(h) = *src & 0xff;
			return;
		case DATA_REG__L:
			ACCESS_REG_(l) = *src & 0xff;
			return;
		case DATA_REG__I:
			ACCESS_REG_(i) = *src & 0xff;
			return;
		case DATA_REG__X:
			ACCESS_REG_(x) = *src & 0xff;
			return;
		case DATA_REG__D:
			ACCESS_REG_(d) = *src & 0xff;
			return;
		case DATA_REG__S:
			ACCESS_REG_(s) = *src & 0xff;
			return;
		case DATA_REG_S_:
			ACCESS_REG_(s) = *src >> 8;
			return;
		case DATA_REG_AB:
			ACCESS_REG_(a) = *src >> 8;
			ACCESS_REG_(b) = *src & 0xff;
			return;
		case DATA_REG_HL:
			ACCESS_REG_(h) = *src >> 8;
			ACCESS_REG_(l) = *src & 0xff;
			return;
		case DATA_REG_IX:
			ACCESS_REG_(i) = *src >> 8;
			ACCESS_REG_(x) = *src & 0xff;
			return;
		case DATA_REG_DS:
			ACCESS_REG_(d) = *src >> 8;
			ACCESS_REG_(s) = *src & 0xff;
			return;
		case DATA_REG__C:
			ACCESS_REG_(c) = *src & 0xff;
			return;
		case DATA_REG_SP:
			state->sys->core.sp = *src & 0xfffe;
			return;
		case DATA_REG_PC:
			state->sys->core.pc = *src & 0xfffe;
			return;
		case DATA_REG__K:
			state->sys->core.k = *src & 0xff;
			return;
		case DATA_REG__F:
			ACCESS_REG_(f) = *src & 0xff;
			return;
		case DATA_REG_KF:
			state->sys->core.k = *src >> 8;
			ACCESS_REG_(f) = *src & 0xff;
			return;
		case DATA_REG__E:
			state->sys->core.int_mask = *src & 0xff;
			return;
		case DATA_LATCH_MEM_ADDR:
			state->mem_addr_low = *src;
			return;
		case DATA_LATCH_MEM_DATA:
			state->mem_data = *src;
			return;
		case DATA_LATCH_IMM_0:
			state->decoded_inst.imm_words[0] = *src;
			return;
		case DATA_LATCH_IMM_1:
			state->decoded_inst.imm_words[1] = *src;
			return;
		case DATA_LATCH_IMM_2:
			state->decoded_inst.imm_words[2] = *src;
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
	if (state->control->mem_latch_ctl & MEM_LATCH_AT_HALF1_MASK)
	{
		switch (state->control->mem_latch_ctl)
		{
			case MEM_LATCH_HALF1_B0:
				state->mem_bank = 0;
				break;
			case MEM_LATCH_HALF1_B1:
				state->mem_bank = 1;
				break;
			case MEM_LATCH_HALF1_BD:
				state->mem_bank = fetch_data_(state, DATA_REG__D);
				break;
			default:
				execute_unreachable_();
		}
		
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
	if (state->control->mem_latch_ctl & MEM_LATCH_AT_HALF1_MASK
		&& !state->control->mem_access_suppress)
	{
		if (state->control->mem_write_ctl == MEM_READ)
		{
			if (!Pilot_mem_addr_read_assert(
				state->sys, state->mem_bank, state->mem_addr_low))
			{
				return;
			}
			state->mem_access_was_read = TRUE;
		}
		else
		{
			if (!Pilot_mem_addr_write_assert(
				state->sys, state->mem_bank, state->mem_addr_low, state->mem_data))
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

	if (state->control->srcs[1].is_16bit)
	{
		msb_bit = (operand & 0x8000) != 0;
	}
	else
	{
		msb_bit = (operand & 0x80) != 0;	
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

#define MODIFY_FLAG(which_flag, value) \
if ( (value) == TRUE )\
{\
	flags |= which_flag ;\
}\
else\
{\
	flags &= ~which_flag ;\
}
static inline uint8_t
alu_modify_flags_ (pilot_execute_state *state, uint8_t flags, uint16_t operands[2], uint16_t result, uint16_t carries)
{
	bool alu_carry;
	bool alu_half_carry;
	bool alu_overflow;
	bool alu_zero;
	bool alu_neg;
	uint8_t flag_source_word = 0;
	uint16_t alu_parity = result;
	alu_parity = alu_parity ^ alu_parity >> 4;
	alu_parity = alu_parity ^ alu_parity >> 2;
	alu_parity = alu_parity ^ alu_parity >> 1;
	alu_half_carry = (carries & 0x0004) != 0;
	
	if (state->control->srcs[0].is_16bit)
	{
		alu_carry = (carries & 0x8000) != 0;
		alu_neg = (result & 0x8000) != 0;
		alu_overflow = ((operands[1] ^ result) & (operands[0] ^ result) & 0x8000) != 0;
		alu_zero = result == 0;
		alu_parity = alu_parity ^ alu_parity >> 8;
	}
	else
	{
		alu_carry = (carries & 0x80) != 0;
		alu_neg = (result & 0x80) != 0;
		alu_overflow = ((operands[1] ^ result) & (operands[0] ^ result) & 0x80) != 0;
		alu_zero = (result & 0xff) == 0;
	}
	alu_carry ^= state->control->invert_carries;
	alu_half_carry ^= state->control->invert_carries;
	
	// S - Sign/negative flag
	flag_source_word |= alu_neg << 7;
	// Z - Zero flag
	flag_source_word |= alu_zero << 6;
	// I - Master interrupt enable flag
	flag_source_word |= alu_zero << 5;
	// H - Half carry flag
	flag_source_word |= alu_half_carry << 4;
	// A - Segment adjust flag
	flag_source_word |= alu_carry << 3;
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
	// D - Data segment mode flag
	flag_source_word |= (state->control->flag_d != 0) << 1;
	// C - Carry/borrow flag
	flag_source_word |= alu_carry;

	flags &= ~state->control->flag_write_mask;
	flags |= (flag_source_word & state->control->flag_write_mask);
	
	return flags;
}
#undef MODIFY_FLAG

static void
execute_half2_result_latch_ (pilot_execute_state *state)
{
	int i;
	uint16_t operands[2];
	uint16_t carries;
	alu_src_control *src2 = &state->control->srcs[1];
	uint8_t flags = fetch_data_(state, DATA_REG__F);
	bool carry_flag_status = (flags & F_CARRY) != 0;
	for (i = 0; i < 2; i++)
	{
		alu_src_control *src = &state->control->srcs[i];
		operands[i] = state->alu_input_latches[i];
		if (!src->is_16bit)
		{
			operands[i] &= 0xFF;
			if (src->sign_extend && (operands[i] & 0x80))
			{
				operands[i] |= 0xFF00;
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
		if (!src2->is_16bit && !src2->sign_extend)
		{
			operands[1] &= 0xff;
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
	if (state->control->mem_latch_ctl & MEM_LATCH_AT_HALF2_MASK)
	{
		switch (state->control->mem_latch_ctl)
		{
			case MEM_LATCH_HALF2_B0:
				state->mem_bank = 0;
				break;
			case MEM_LATCH_HALF2_BD:
				state->mem_bank = fetch_data_(state, DATA_REG__D);
				break;
			case MEM_LATCH_HALF2_BD_ALUC:
				state->mem_bank = fetch_data_(state, DATA_REG__D) + carry_flag;
				break;
			default:
				execute_unreachable_();
		}
		
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
	if (state->control->mem_latch_ctl & MEM_LATCH_AT_HALF2_MASK
		&& !state->control->mem_access_suppress)
	{
		if (state->control->mem_write_ctl == MEM_READ)
		{
			if (!Pilot_mem_addr_read_assert(
				state->sys, state->mem_bank, state->mem_addr_low))
			{
				return;
			}
			state->mem_access_was_read = TRUE;
		}
		else
		{
			if (!Pilot_mem_addr_write_assert(
				state->sys, state->mem_bank, state->mem_addr_low, state->mem_data))
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
	bool d_flag_set = (fetch_data_(state, DATA_REG__F) & F_DS_MODE) != 0;
	
	decoded = decode_mucode_entry(state->mucode_control, d_flag_set);
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


#include "cpu_regs.h"
#include "cpu_decode.h"
#include "memory.h"
#include <stdint.h>

/*
 * Pipeline stages:
 * 
 * 1. Fetch
 * The fetch stage consists of a 2-word prefetch queue and a 1-word latch connected to the decode stage.
 * 
 * a. First half of cycle
 * - If prefetch queue is not empty and a branch is not signalled from the decode stage:
 *   - If "word ready" is lowered, or "wait" line is not raised:
 *     - Pull word from prefetch queue
 *       - Latch word for decode stage
 *     - Raise "word ready" line, if lowered
 * - Otherwise:
 *   - Lower "word ready" line
 *
 * b. Second half of cycle
 * - If a branch is signalled from decode stage:
 *   - Lower "word ready" line
 *   - Flush prefetch queue
 *   - Set address to predicted PC value from decode stage
 *
 * - If the execute stage isn't trying to read from memory:
 *   - Read next word from next address
 *   - Increment next address (by 2)
 *
 * -----
 *
 * Interconnections fetch->decode:
 * - "Word ready" line - signal to the decode stage that the word latch contains valid data and is ready to be read.
 * - Word latch (16 bits)
 *
 * Interconnections decode->fetch:
 * - "Wait" line - signal from the decode stage that the currently latched word should be held
 * - "Stall" line - signal from the decode stage that a branch misprediction has happened
 * - "Branch" line - signal from the decode stage that a branch has happened and the branch address has been latched
 * - Branch address latch (24 bits)
 *
 * ----
 *
 * 2. Decode
 * The decode stage consists of a combinational logic array that interprets an instruction word and buffers a series of
 * signals for the execute stage, which are latched after the execute stage is done with the last instruction.
 *
 * a. First half of cycle
 * - Read instruction word from fetch stage and decode it
 *   - If immediate operand required, read another word from fetch stage
 *
 *   - If branch instruction is detected, try to predict a branch
 *     - Branches can only be predicted if the branch destination is directly encoded as an immediate value in the
 *       instruction word and/or its following immediate operand
 *     - If branch can be predicted:
 *       - Assume it will be taken
 *       - Latch predicted PC value to fetch stage
 *       - Signal branch to fetch stage
 *     - If branch cannot be predicted:
 *       - Signal stall to fetch stage
 *       - Wait for updated PC value from this instructions's execution stage
 *
 * b. Second half of cycle
 * - If "execute ready" line has been raised:
 *   - If a decoded instruction is available:
 *     - Latch previously decoded signals for execution stage
 *     - Strobe "instruction ready" line
 *   - Else:
 * - Else:
 *   - Keep "wait" line raised
 * 
 * -----
 *
 * Interconnections
 * 
 * -----
 * 
 * 3. Execute
 * The execute stage consists of sequential logic that uses the latched signals from the instruction decoder to perform
 * the instruction.
 * 
 */

// Placeholder
void decode_not_implemented_ ();

// Decodes an RM specifier.
void decode_rm_specifier (pilot_decode_state *state, rm_spec rm, bool is_dest, bool src_is_left, data_size_spec size);

static inline void
decode_inst_branch_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	if ((opcode & 0xff00) == 0xff00)
	{
		// RST
		decode_not_implemented_();
		return;
	}
	if ((opcode & 0xffe0) == 0xfe00)
	{
		// REPI
		decode_not_implemented_();
		return;
	}
	if ((opcode & 0xf800) == 0xf000)
	{
		if (!(opcode & 0xff))
		{
			// REPR
			decode_not_implemented_();
			return;
		}
		else if (opcode & 0x80)
		{
			// DJNZ
			decode_not_implemented_();
			return;
		}
		else
		{
			decode_invalid_opcode_(state);
			return;
		}
	}

	switch (opcode & 0x0700)
	{
		case 0x0000:
			// JP, JR.L
			decode_not_implemented_();
			return;
		case 0x0100:
			// CALL, CR.L
			decode_not_implemented_();
			return;
		case 0x0200:
		{
			switch (opcode & 0x01c0)
			{
				case 0x0000:
					// JP rm24
					decode_not_implemented_();
					return;
				case 0x0040:
					// JEA
					decode_not_implemented_();
					return;
				case 0x0100:
					// CALL rm24
					decode_not_implemented();
					return;
				case 0x0140:
					// CEA
					decode_not_implemented();
					return;
				default:
					break;
			}
		}
	}
	
	decode_invalid_opcode_(state);
	return;
}

static void
decode_inst_bit_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	decode_not_implemented_();
	return;
}

static void
decode_inst_ld_other_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	execute_control_word *core_op = &state->work_regs.core_op;
	core_op->src2_add1 = FALSE;
	core_op->src2_add_carry = FALSE;
	core_op->src2_negate = FALSE;
	core_op->flag_write_mask = 0;
	core_op->flag_v_mode = FLAG_V_NORMAL;

	// left operand is never fetched and is always zero
	core_op->srcs[0].location = DATA_ZERO;
	core_op->srcs[0].size = SIZE_24_BIT;
	core_op->operation = ALU_OR;

	core_op->srcs[1].location = DATA_REG_IMM_0;

	core_op->dest = DATA_REG_IMM_0_8;
	core_op->shifter_mode = SHIFTER_NONE;
	
	if ((opcode & 0x0800) == 0x0000)
	{
		// LD.P r24, hml
		core_op->srcs[1].size = SIZE_24_BIT;
		core_op->srcs[1].sign_extend = FALSE;
		return;
	}
	else
	{
		// LDQ
		core_op->srcs[1].size = SIZE_8_BIT;
		core_op->srcs[1].sign_extend = TRUE;
		return;
	}
}

static inline void
decode_inst_arithlogic_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	uint_fast8_t operation = ((opcode & 0x00c0) >> 6) | ((opcode & 0x1800) >> 9);
	data_size_spec size = ((opcode & 0xc000) >> 14);
	execute_control_word *core_op = &state->work_regs.core_op;
	data_bus_specifier reg_select;
	bool uses_imm = FALSE;_
	
	core_op->shifter_mode = SHIFTER_NONE;
	core_op->src2_add1 = FALSE;
	core_op->flag_v_mode = FLAG_V_NORMAL;
	
	// Decode core_op operation
	switch (operation)
	{
		case 0: case 1: case 2: case 3: case 8: case 9: case 10: case 11:
			// ADD, ADX, SUB, SBX
			core_op->operation = ALU_ADD;
			core_op->src2_add_carry = operation & 1;
			core_op->src2_negate = operation & 2;
			core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY | F_EXTEND;
			break;
		case 4: case 12:
			// AND
			core_op->operation = ALU_AND;
			core_op->src2_add_carry = FALSE;
			core_op->src2_negate = FALSE;
			core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
			break;
		case 5: case 13:
			// XOR
			core_op->operation = ALU_XOR;
			core_op->src2_add_carry = FALSE;
			core_op->src2_negate = FALSE;
			core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
			break;
		case 6: case 14:
			// OR
			core_op->operation = ALU_OR;
			core_op->src2_add_carry = FALSE;
			core_op->src2_negate = FALSE;
			core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
			break;
		case 7:
			// CP
			core_op->operation = ALU_ADD;
			core_op->src2_add_carry = FALSE;
			core_op->src2_negate = TRUE;
			core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
			break;
		case 15:
		{
			uses_imm = TRUE;
			operation = (opcode & 0x0700) >> 16;
			switch(operation)
			{
				case 0: case 1: case 2: case 3:
					// ADD, ADX, SUB, SBX
					core_op->operation = ALU_ADD;
					core_op->src2_add_carry = operation & 1;
					core_op->src2_negate = operation & 2;
					core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY | F_EXTEND;
					break;
				case 4:
					// AND
					core_op->operation = ALU_AND;
					core_op->src2_add_carry = FALSE;
					core_op->src2_negate = FALSE;
					core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
					break;
				case 5:
					// XOR
					core_op->operation = ALU_XOR;
					core_op->src2_add_carry = FALSE;
					core_op->src2_negate = FALSE;
					core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
					break;
				case 6:
					// OR
					core_op->operation = ALU_OR;
					core_op->src2_add_carry = FALSE;
					core_op->src2_negate = FALSE;
					core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
					break;
				case 7:
					// CP
					core_op->operation = ALU_ADD;
					core_op->src2_add_carry = FALSE;
					core_op->src2_negate = TRUE;
					core_op->flag_write_mask = F_NEG | F_ZERO | F_OVERFLOW | F_CARRY;
					break;
			default:
				decode_unreachable_();
			}
		}
		default:
			decode_unreachable_();
	}
	
	// Decode operands
	if (!uses_imm)
	{
		// from RM src
		rm_spec rm = opcode & 0x003f;
		decode_rm_specifier(state, rm, FALSE, FALSE, size);
		state->work_regs.core_op.srcs[1].sign_extend = FALSE;
		
		state->work_regs.core_op.srcs[0].location = DATA_LATCH_IMM_0_8;
		state->work_regs.core_op.srcs[0].size = size;
		state->work_regs.core_op.srcs[0].sign_extend = FALSE;
		return;
	}
	else
	{
		// from immediate src
		rm_spec rm = opcode & 0x003f;
		decode_rm_specifier(state, rm, TRUE, TRUE, size);
		state->work_regs.core_op.srcs[0].sign_extend = FALSE;
		
		state->work_regs.core_op.srcs[1].location = DATA_LATCH_IMM_1;
		state->work_regs.core_op.srcs[1].size = size;
		state->work_regs.core_op.srcs[1].sign_extend = FALSE;
		
		if (operation == 7)
		{
			state->work_regs.core_op.dest = DATA_ZERO;
		}
		return;
	}
}

static void
decode_inst_ld_group_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	execute_control_word *core_op = &state->work_regs.core_op;
	data_size_spec size = ((opcode & 0xc000) >> 14);
	core_op->src2_add1 = FALSE;
	core_op->src2_add_carry = FALSE;
	core_op->src2_negate = FALSE;
	core_op->flag_write_mask = 0;
	core_op->flag_v_mode = FLAG_V_NORMAL;

	// left operand is never fetched and is always zero
	core_op->srcs[0].location = DATA_ZERO;
	core_op->srcs[0].size = size;
	
	core_op->operation = ALU_OR;
	core_op->shifter_mode = SHIFTER_NONE;
	
	if ((opcode & 0x00c0) == 0x00c0)
	{
		// one RM specifier
		rm_spec rm_src = opcode & 0x3f;
		data_bus_specifier reg;

		core_op->srcs[0].size = SIZE_24_BIT;
		core_op->dest = DATA_REG_IMM_0_8;

		if ((opcode & 0x8800) == 0x0800)
		{
			// LDSX
			core_op->srcs[1].sign_extend = TRUE;
		}
		if ((opcode & 0xf800) == 0x9800)
		{
			// LEA with pre-decrement (uses the RM decoder to handle the auto-index)
			rm_spec dummy_rm = (opcode & 0x0f80) >> 12;
			decode_rm_specifier(state, dummy_rm, TRUE, FALSE, size);
		}
		
		// if LEA @-r24, this will technically be treated as the second RM operand, but that operand is never used
		decode_rm_specifier(state, rm_src, FALSE, FALSE, size);
		
		return;
	}
	else
	{
		// LD (two RM specifiers)
		rm_spec rm_src = opcode & 0x3f;
		rm_spec rm_dest = (opcode >> 6) & 0x3f;
		// fetch src (right operand)
		decode_rm_specifier(state, rm_src, FALSE, FALSE, size);
		// write into dest (left operand)
		// does not need to be fetched, hence it's not a source, so don't set src_is_left
		decode_rm_specifier(state, rm_dest, TRUE, FALSE, size);
		return;
	}
	
	decode_unreachable_(state);
	return;
}

static void
decode_inst_other_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	decode_not_implemented_();
	return;
}

static void
decode_inst_ (pilot_decode_state *state)
{
	state->rm_ops = 0;
	uint_fast16_t opcode = state->work_regs.imm_words[0];
	
	if ((opcode & 0xf000) >= 0xe000)
	{
		// Branch instructions
		decode_inst_branch_(state, opcode);
	}
	else if ((opcode & 0xf000) == 0xd000)
	{
		// Bit operations
		decode_inst_bit_(state, opcode);
	}
	else if ((opcode & 0xf000) == 0xc000)
	{
		// Two miscellaneous LD instructions
		decode_inst_ld_other_(state, opcode);
	}
	else if ((opcode & 0x2000) == 0x2000)
	{
		// Arithmetic/logic instructions
		decode_inst_arithlogic_(state, opcode);
	}
	else if ((opcode & 0x3000) == 0x1000)
	{
		// Main group of LD instructions
		decode_inst_ld_group_(state, opcode);
	}
	else if ((opcode & 0x3000) == 0x0000)
	{
		// Everything else
		decode_inst_other_(state, opcode);
	}
}

void
decode_queue_read_word (pilot_decode_state *state)
{
	state->inst_length++;
	state->words_to_read++;
}

void
pilot_decode_half1 (pilot_decode_state *state)
{
	if (state->decoding_phase == DECODER_HALF1_DISPATCH_WAIT)
	{
		bool *decoded_inst_semaph = &state->sys->interconnects.decoded_inst_semaph;
		if (!(*decoded_inst_semaph))
		{
			state->decoding_phase = DECODER_HALF1_READY;
		}
	}
	
	if (state->decoding_phase == DECODER_HALF1_READY)
	{
		state->inst_length = 0;
		state->decoding_phase = DECODER_HALF1_READ_INST_WORD;
	}
	
	if (state->decoding_phase == DECODER_HALF1_READ_INST_WORD)
	{
		bool read_ok = decode_try_read_word_(state);
		if (read_ok)
		{
			state->inst_length++;
			decode_inst_(state);
			state->decoding_phase = DECODER_HALF2_READ_OPERANDS;
		}
	}
}


void
pilot_decode_half2 (pilot_decode_state *state)
{
	if (state->decoding_phase == DECODER_HALF2_READ_OPERANDS)
	{
		if (state->words_to_read > 0)
		{
			bool read_ok = decode_try_read_word_(state);
			if (read_ok)
			{
				state->words_to_read--;
			}
		}
		
		if (state->words_to_read == 0)
		{
			state->decoding_phase = DECODER_HALF2_DISPATCH;
		}
	}
	
	if (state->decoding_phase == DECODER_HALF2_DISPATCH)
	{
		state->work_regs.inst_pgc = state->pgc;
		bool *decoded_inst_semaph = &state->sys->interconnects.decoded_inst_semaph;
		*decoded_inst_semaph = TRUE;
		state->decoding_phase = DECODER_HALF1_DISPATCH_WAIT;
	}
}


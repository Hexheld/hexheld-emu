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

// Runs the invalid opcode exception reporting.
void decode_invalid_opcode_ (pilot_decode_state *state);

// Asserts if an RM specifier is valid.
// NOTE: Special RM specifiers for certain instructions are not checked here and need to be special-case evaluated beforehand.
bool is_rm_valid_ (rm_spec rm);

bool decode_rm_specifier (pilot_decode_state *state, rm_spec rm, bool is_dest, bool src_is_left, data_size_spec size);

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
	if (opcode & 0xf800 == 0xf000)
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
}

static inline void
decode_inst_arithlogic_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	uint_fast8_t operation = ((opcode & 0x00c0) >> 6) | ((opcode & 0x1800) >> 9);
	data_size_spec size = ((opcode & 0x3000) >> 14);
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
		rm_spec rm = opcode & 0x003F;
		decode_rm_specifier(state, rm, FALSE, FALSE, size);

		state->work_regs.core_op.srcs[1].location = reg_select;
		state->work_regs.core_op.srcs[1].size = size;
		state->work_regs.core_op.srcs[1].sign_extend = FALSE;
		
		state->work_regs.core_op.srcs[0].location = DATA_LATCH_IMM_0_8;
		state->work_regs.core_op.srcs[0].size = size;
		state->work_regs.core_op.srcs[0].sign_extend = FALSE;
		return;
	}
	else
	{
		// from immediate src
		rm_spec rm = opcode & 0x003F;
		decode_rm_specifier(state, rm, TRUE, TRUE, size);
		state->work_regs.core_op.srcs[0].size = size;
		state->work_regs.core_op.srcs[0].sign_extend = FALSE;

		state->work_regs.core_op.srcs[1].location = DATA_LATCH_IMM_1;
		state->work_regs.core_op.srcs[1].size = size;
		state->work_regs.core_op.srcs[1].sign_extend = FALSE;
		
		if (operation != 7)
		{
			state->work_regs.core_op.dest = state->work_regs.core_op.srcs[0];
		}
		return;
	}
	
	decode_invalid_opcode_(state);
}

static void
decode_inst_ld_group_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	execute_control_word *core_op = &state->work_regs.core_op;
	data_size_spec size = ((opcode & 0x3000) >> 14);
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

	// currently up to here with rewriting
	
	if (opcode & 0x8800)
	{
		decode_unreachable_();
	}
	
	if ((opcode & 0x7000) == 0x5000)
	{
		// zm specifiers
		uint8_t zm_spec = opcode & 0xff;
		data_bus_specifier reg;
		bool zm_is_dest = !(opcode & 0x0200);
		if (is_16bit)
		{
			 reg = !(opcode & 0x0100) ? DATA_REG_AB : DATA_REG_HL;
		}
		else
		{
			 reg = !(opcode & 0x0100) ? DATA_REG__A : DATA_REG__B;
		}
		
		if (zm_is_dest)
		{
			core_op->srcs[1].location = reg;
			core_op->dest = DATA_LATCH_MEM_DATA;
		}
		else
		{
			core_op->srcs[1].location = DATA_LATCH_MEM_DATA;
			core_op->dest = reg;
		}
		
		decode_zm_specifier(state, zm_spec, zm_is_dest, is_16bit);
		state->work_regs.override_op.reg_select |= (opcode & 0x0100) != 0;
		return;
	}
	if ((opcode & 0x7000) == 0x6000)
	{
		// rm src/dest
		rm_spec rm_src = opcode & 0x1f;
		rm_spec rm_dest = (opcode >> 5) & 0x1f;
		// fetch src (right operand)
		decode_rm_specifier(state, rm_src, FALSE, FALSE, is_16bit);
		// write into dest (left operand)
		// does not need to be fetched, hence it's not a source, so don't set src_is_left
		decode_rm_specifier(state, rm_dest, TRUE, FALSE, is_16bit);
		return;
	}
	if ((opcode & 0x7000) == 0x7000)
	{
		// reg <- imm8
		switch ((opcode >> 8) & 7)
		{
			case 0:
				core_op->dest = DATA_REG__A;
			case 1:
				core_op->dest = DATA_REG__H;
			case 2:
				core_op->dest = DATA_REG__I;
			case 3:
				core_op->dest = DATA_REG__D;
			case 4:
				core_op->dest = DATA_REG__B;
			case 5:
				core_op->dest = DATA_REG__L;
			case 6:
				core_op->dest = DATA_REG__X;
			case 7:
				core_op->dest = DATA_REG__S;
		}
		core_op->srcs[1].location = DATA_LATCH_IMM_0;
		core_op->srcs[1].is_16bit = FALSE;
		core_op->srcs[1].sign_extend = FALSE;
		return;
	}
	
	decode_invalid_opcode_(state);
}

static void
decode_inst_stack_ (pilot_decode_state *state, uint_fast16_t opcode)
{
	(void) state;
	(void) opcode;
	decode_not_implemented_();
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
	core_op->operation = ALU_OR;
	
	core_op->srcs[1].is_16bit = FALSE;
	core_op->srcs[1].sign_extend = FALSE;
	core_op->shifter_mode = SHIFTER_NONE;
	
	if ((opcode & 0xfe00) == 0x4000)
	{
		// ld/add C, imm
		core_op->srcs[1].location = DATA_LATCH_IMM_0;
		core_op->dest = DATA_REG__C;
		return;
	}
	
	decode_invalid_opcode_(state);
}

static void
decode_inst_ (pilot_decode_state *state)
{
	uint_fast16_t opcode = state->work_regs.imm_words[0];
	
	if (opcode & 0x0800)
	{
		// Branch instructions
		decode_inst_branch_(state, opcode);
	}
	else if (opcode & 0x8000)
	{
		// Arithmetic/logic operations
		decode_inst_arithlogic_(state, opcode);
	}
	else if ((opcode & 0x7000) > 0x4000)
	{
		decode_inst_ld_group_(state, opcode);
	}
	else if ((opcode & 0x4200) == 0x4200)
	{
		decode_inst_stack_(state, opcode);
	}
}

void
pilot_queue_read_word (pilot_decode_state *state)
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
		state->work_regs.inst_pc = state->pc;
		state->work_regs.inst_k = state->k;
		bool *decoded_inst_semaph = &state->sys->interconnects.decoded_inst_semaph;
		*decoded_inst_semaph = TRUE;
		state->decoding_phase = DECODER_HALF1_DISPATCH_WAIT;
	}
}


#include "cpu_regs.h"
#include "memory.h"
#include <stdint.h>

static enum {
	// Carry/borrow flag
	F_CARRY   = 0x01,
	// When set, operations with DS ignore S, freeing it for general-purpose use
	F_DS_MODE = 0x02,
	// Overflow/parity flag
	F_OVRFLW  = 0x04,
	// Segment adjust flag
	F_DS_ADJ  = 0x08,
	// Half carry flag
	F_HCARRY  = 0x10,
	// Interrupt enable
	F_INT_EN  = 0x20,
	// Zero flag
	F_ZERO    = 0x40,
	// Sign (negative) flag
	F_NEG     = 0x80
} flag_masks;

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

typedef int_fast8_t bool;

typedef enum
{
	REG8_A = 0,
	REG8_B,
	REG8_H,
	REG8_L,
	REG8_I,
	REG8_X,
	REG8_D,
	REG8_S,
	
	REG8_C,
	REG8_F,
	REG8_E
} reg8_spec;

typedef enum
{
	REG16_AB = 0,
	REG16_A_SX,
	REG16_HL,
	REG16_B_SX,
	REG16_IX,
	REG16_A_ZX,
	REG16_DS,
	REG16_SP
} reg16_spec;

typedef enum
{
	DATA_REG,
	DATA_IND_IMM16,
	DATA_IND_IMM24,
	DATA_IND_REG16,
	DATA_IND_IX_IMM16,
	DATA_IND_SP_IMM16,
} data_location_spec;

typedef enum
{
	IND_ZEROPAGE = 0,
	IND_DS,
	IND_ONEPAGE
} indirect_type_spec;

typedef union
{
	reg8_spec reg8;
	reg16_spec reg16;
} reg_spec;
	

typedef struct
{
	data_location_spec location;
	indirect_type_spec ind_type;
	reg_spec reg;
} argument_spec;

typedef enum
{
	DATA_ZERO = 0,
	DATA_REG__A,
	DATA_REG__B,
	DATA_REG__H,
	DATA_REG__L,
	DATA_REG__I,
	DATA_REG__X,
	DATA_REG__D,
	DATA_REG__S,
	DATA_REG_AB,
	DATA_REG_HL,
	DATA_REG_IX,
	DATA_REG_DS,
	DATA_REG__C,
	DATA_REG_SP,
	DATA_REG_PC,
	DATA_REG__K,
	DATA_REG__F,
	DATA_REG_KF,
	DATA_REG__E,
	DATA_LATCH_MEM_ADDR,
	DATA_LATCH_MEM_DATA,
	DATA_LATCH_IMM_1,
	DATA_LATCH_IMM_2
} data_bus_specifier;

struct alu_src_control {
	data_bus_specifier location;
	bool swap8;
	bool sign_extend;
};

typedef struct
{
	struct alu_src_control srcs[2];
	data_bus_specifier dest;

	enum
	{
		ALU_ADD,
		ALU_AND,
		ALU_OR,
		ALU_XOR
	} operation;
	
	// Shifter control
	enum
	{
		SHIFTER_NONE,
		SHIFTER_LEFT,
		SHIFTER_LEFT_BARREL,
		SHIFTER_RIGHT_LOGICAL,
		SHIFTER_RIGHT_ARITH,
		SHIFTER_RIGHT_BARREL
	} shifter_mode;
	
	// Source transformations
	bool src2_negate;
	bool src2_add1;
	bool src2_add_carry;
	
	// Flag control
	uint_fast8_t flag_write_mask;
	bool flag_v_parity;
	
	// Register swap control
	
	// Memory control
	bool mem_latch_addr;
	bool mem_write;
} execute_control_word;

struct inst_decoded_flags
{
	// Immediate data sources
	uint_fast16_t imm_words[3];
	
	// Core operation word
	execute_control_word core_op;
	
	// Branch flags
	bool branch;
	enum
	{
		COND_LT,         // less than
		COND_GE,         // greater than or equal
		COND_LE,         // less than or equal
		COND_GT,         // greater than
		COND_U_LE,       // unsigned less than or equal
		COND_U_GT,       // unsigned greater than
		COND_Z,          // equal; zero
		COND_NZ,         // not equal; nonzero
		COND_S,          // positive; sign set
		COND_NS,         // negative; sign clear
		COND_V,          // overflow; parity error
		COND_NV,         // not overflow; parity ok
		COND_C,          // carry set; unsigned less than
		COND_NC,         // carry clear; unsigned greater than or equal
		COND_ALWAYS,     // always
		COND_ALWAYS_CALL // always, but used for calls
	} branch_cond;
	enum
	{
		BR_RELATIVE,
		BR_LOCAL,
		BR_D,
		BR_FAR,
		BR_RET,
		BR_RET_FAR
	} branch_dest_type;
};

typedef struct {
	struct inst_decoded_flags work_regs;
	enum
	{
		DECODER_READY,
		DECODER_READ_INST_WORD,
		DECODER_READ_2_EXTRA_WORDS,
		DECODER_READ_EXTRA_WORD
	} decoding_cycle;
} pilot_decode_state;

struct pilot_execute_state {
	struct inst_decoded_flags decoded_inst;
	execute_control_word control;
};

static void decode_read_word_ (pilot_decode_state state);

static void decode_invalid_opcode_ (pilot_decode_state state);

static inline void
decode_inst_branch_ (pilot_decode_state state, uint_fast16_t opcode)
{
	if ((opcode & 0xff00) == 0xff00)
	{
		// RST
		return;
	}
	if ((opcode & 0x0400) == 0x0000)
	{
		// JR, CALLR
		return;
	}

	switch (opcode & 0x0300)
	{
		case 0x0000:
			switch (opcode & 0x00e0)
			{
				case 0x0000:
					// JP, CALL
					return;
				case 0x0080:
					// JPD, CALLD
					return;
				case 0x00c0:
					// JPF, CALLF indirect mem24
					return;
				default:
					break;
			}
		case 0x0100:
			if ((opcode & 0x00ff) == 0x0000)
			{
				// JRL, CALLRL pcr15
				return;
			}
			break;
		case 0x0200:
			// JPF, CALLF imm24
			return;
		case 0x0300:
			switch (opcode & 0x00ff)
			{
				case 0x0000:
					// RET
					return;
				case 0x0080:
					// RETF
					return;
				default:
					if ((opcode & 0xff1f) == 0xef10)
					{
						// RETI
						return;
					}
					break;
			}
	}

	decode_invalid_opcode_(state);
}

static inline void
decode_inst_arithlogic_ (pilot_decode_state state, uint_fast16_t opcode)
{
	if ((opcode & 0x02c0) == 0x0200)
	{
		// from src or to dest
		return;
	}
	if ((opcode & 0x0200) == 0x0000)
	{
		// from immediate
		return;
	}
	
	decode_invalid_opcode_(state);
}

static void
decode_inst_ (pilot_decode_state state)
{
	decode_read_word_(state);
	uint_fast16_t opcode = state.work_regs.imm_words[0];
	
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
}



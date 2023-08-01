#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef int_fast8_t bool;
#define TRUE 1
#define FALSE 0

typedef enum
{
	REG8_L0 = 0,
	REG8_L1,
	REG8_L2,
	REG8_L3,
	REG8_M0,
	REG8_M1,
	REG8_M2,
	REG8_M3,
	
	REG8_W,
	REG8_F
} reg8_spec;

typedef enum
{
	REG16_W0 = 0,
	REG16_W1,
	REG16_W2,
	REG16_W3,
	REG16_W4,
	REG16_W5,
	REG16_W6,
	REG16_W7,
	
	REG16_WF
} reg16_spec;

typedef enum
{
	REG24_P0 = 0,
	REG24_P1,
	REG24_P2,
	REG24_P3,
	REG24_P4,
	REG24_P5,
	REG24_P6,
	REG24_SP
} reg24_spec;

typedef enum
{
	// RM operands
	DATA_REG,
	DATA_IND_REG24_IMM16,
	DATA_IND_REG24,
	DATA_IND_IMM16,
	DATA_IND_IMM24,
	DATA_IND_PGC24_IMM16,
	DATA_IND_PGC24_IMM24,
	
	// register indexed
	DATA_IND_REG24_REG8,
	DATA_IND_REG24_REG8_SX,
	DATA_IND_REG24_REG16,
	DATA_IND_REG24_REG16_SX,
	DATA_IND_REG24_REG24,
	
	// absolute indexed
	DATA_IND_IMM24_REG8,
	DATA_IND_IMM24_REG8_SX,
	DATA_IND_IMM24_REG16,
	DATA_IND_IMM24_REG16_SX,
	DATA_IND_IMM24_REG24,
} data_location_spec;

typedef enum
{
	SIZE_8_BIT = 0,
	SIZE_16_BIT,
	SIZE_24_BIT
} data_size_spec;

typedef union
{
	reg8_spec reg8;
	reg16_spec reg16;
	reg24_spec reg24;
} reg_spec;

typedef uint_fast8_t rm_spec;

#define RM_NULL -1

typedef enum
{
	// no-op
	MU_NONE = 0,
	
	// request memory
	MU_IND_IMM,
	MU_IND_IMM0,
	MU_IND_IMM_RM,
	MU_IND_IMM_WITH_BITS,
	
	MU_IND_REG,
	MU_IND_REG_POST_AUTO,
	MU_IND_REG_AUTO,
	MU_IND_REG_WITH_IMM,
	MU_IND_REG_WITH_BITS,
	MU_IND_PGC_WITH_IMM,
	MU_IND_PGC_WITH_IMM_RM,
	MU_IND_PGC_WITH_HML,
	MU_IND_PGC_WITH_HML_RM,
	
	// post-increment
	MU_POST_AUTOIDX
} mucode_entry_idx;

typedef struct
{
	mucode_entry_idx entry_idx;
	// bit 3: sign extend
	// bit 4: RM operand number
	uint_fast8_t reg_select;
	data_size_spec size;
	bool is_write;
} mucode_entry_spec;

typedef enum
{
	// zero, or n/a
	DATA_ZERO = 0,

	// the current micro-operation size
	DATA_SIZE,
	
	// 8-bit registers
	DATA_REG_L0,
	DATA_REG_L1,
	DATA_REG_L2,
	DATA_REG_L3,
	DATA_REG_M0,
	DATA_REG_M1,
	DATA_REG_M2,
	DATA_REG_M3,
	DATA_REG__F,
	DATA_REG__W,
	
	// 16-bit registers
	DATA_REG_W0,
	DATA_REG_W1,
	DATA_REG_W2,
	DATA_REG_W3,
	DATA_REG_W4,
	DATA_REG_W5,
	DATA_REG_W6,
	DATA_REG_W7,
	DATA_REG_WF,
	
	// 24-bit registers
	DATA_REG_P0,
	DATA_REG_P1,
	DATA_REG_P2,
	DATA_REG_P3,
	DATA_REG_P4,
	DATA_REG_P5,
	DATA_REG_P6,
	DATA_REG_SP,
	DATA_REG_PGC,
	
	// Special registers
	DATA_LATCH_REPI,
	DATA_LATCH_REPR,
	DATA_LATCH_MEM_ADDR,
	DATA_LATCH_MEM_DATA,
	DATA_LATCH_IMM_0,
	DATA_LATCH_IMM_1,
	DATA_LATCH_IMM_2,
	DATA_LATCH_IMM_HML_RM,
	
	// Short Form Immediate bits of each RM operand
	DATA_LATCH_SFI_1,
	DATA_LATCH_SFI_2,
	
	// Second RM operand latches
	DATA_LATCH_RM_1,
	DATA_LATCH_RM_2,
	DATA_LATCH_RM_HML,

	// Registers from a 3-bit value
	DATA_REG_IMM_0_8,
	DATA_REG_IMM_1_8,
	DATA_REG_IMM_1_2,
	DATA_REG_IMM_2_8,
	DATA_REG_RM_1_8,
	DATA_REG_RM_1_2,
	DATA_REG_RM_2_8
} data_bus_specifier;

typedef struct {
	data_bus_specifier location;
	data_size_spec size;
	bool sign_extend;
} alu_src_control;

typedef struct
{
	alu_src_control srcs[2];
	data_bus_specifier dest;

	enum
	{
		ALU_OFF,
		ALU_ADD,
		ALU_AND,
		ALU_OR,
		ALU_XOR
	} operation;
	
	// Source transformations (in order)
	bool src2_add_carry;
	bool src2_negate;
	bool src2_add1;
	
	// Shifter control
	enum
	{
		SHIFTER_NONE,
		SHIFTER_LEFT,
		SHIFTER_LEFT_CARRY,
		SHIFTER_LEFT_BARREL,
		SHIFTER_RIGHT_LOGICAL,
		SHIFTER_RIGHT_ARITH,
		SHIFTER_RIGHT_CARRY,
		SHIFTER_RIGHT_BARREL
	} shifter_mode;
	
	// Flag control
	uint_fast8_t flag_write_mask;
	bool invert_carries;
	enum
	{
		FLAG_C_ALU_CARRY,
		FLAG_C_SHIFTER_CARRY,
	} flag_c_mode;
	
	bool flag_d;
	
	enum
	{
		FLAG_V_NORMAL,
		FLAG_V_SHIFTER_CARRY,
		FLAG_V_CLEAR,
	} flag_v_mode;
	
	// Memory control
	/* Latching an address follows the cycle below:
	 * 1. If memory unit is not ready, execution is blocked with address to be latched being held
	 * 2. Address is latched, data is read or written, takes 1+ cycles
	 * 3. While data is being read or written, 
	 */
	enum
	{
		// Don't latch address
		MEM_NO_LATCH = 0,
		// Latches at first half of cycle, address from ALU src0
		MEM_LATCH_HALF1,
		// Latches at second half of cycle, address from ALU dest
		MEM_LATCH_HALF2,
		// Latches at second half of cycle, address is whatever was left in MAR (used for destination writeback)
		MEM_LATCH_HALF2_MAR,
	} mem_latch_ctl;
	// If set, suppresses memory access assertion if memory is latched in this cycle
	bool mem_access_suppress;
	enum
	{
		MEM_READ = 0,
		// Data is latched from ALU src1
		MEM_WRITE_FROM_SRC1,
		// Data is latched from ALU dest
		MEM_WRITE_FROM_DEST,
		// Data is not latched; whatever was left in MDR is what's written back
		MEM_WRITE_FROM_MDR,
	} mem_write_ctl;

	// The Pilot has a 24-bit internal data bus, but this is reduced by glue logic to 16 bits for any accesses outside the CPU.
	bool mem_size;
} execute_control_word;

typedef struct
{
	execute_control_word operation;
	mucode_entry_spec next;
} mucode_entry;

typedef struct
{
	// Immediate data sources
	uint_fast16_t imm_words[5];
	
	// PGC for this instruction
	uint_fast24_t inst_pgc;
	
	// Sequencer control
	// override_op: if not MU_NONE, overrides the entire execution with itself
	mucode_entry_spec override_op;
	// run_before: if not MU_NONE, is executed before core_op - usually for memory reads
	mucode_entry_spec run_before;
	// core_op: single-cycle execution control word assembled by the decode stage
	execute_control_word core_op;
	// run_after: if not MU_NONE, is executed after core_op - usually for memory writes
	mucode_entry_spec run_after;

	// Register post auto-increment control
	int8_t auto_incr_amount;
	
	// Branch flags
	bool branch;
	enum
	{
		COND_LE = 0,     // less than or equal
		COND_GT,         // greater than
		COND_LT,         // less than
		COND_GE,         // greater than or equal
		COND_U_LE,       // unsigned less than or equal
		COND_U_GT,       // unsigned greater than
		COND_C,          // carry set; unsigned less than
		COND_NC,         // carry clear; unsigned greater than or equal
		COND_M,          // minus; sign set
		COND_P,          // plus; sign clear
		COND_V,          // overflow; parity even
		COND_NV,         // not overflow; parity odd
		COND_Z,          // equal; zero
		COND_NZ,         // not equal; nonzero
		COND_ALWAYS,     // always
		COND_ALWAYS_CALL // always, but used for calls
	} branch_cond;
	
	enum
	{
		BR_RELATIVE_SHORT,
		BR_RELATIVE_LONG,
		BR_LONG,
		BR_RET,
		BR_RET_LONG
	} branch_dest_type;
	
	// Offset of the second RM operand
	uint8_t rm2_offset;
} inst_decoded_flags;

#endif

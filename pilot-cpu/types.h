#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef int_fast8_t bool;
#define TRUE 1
#define FALSE 0

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

typedef uint_fast8_t rm_spec;
typedef uint_fast8_t zm_spec;

#define RM_NULL -1

typedef enum
{
	MU_NONE = 0,
	MU_IND_IMM,
	MU_IND_REG,
	MU_IND_REG_AUTO,
	MU_IND_REG_WITH_IMM,
	MU_IND_WITH_DS,
	MU_IND_WITH_DS_AUTO,
	MU_IND_DS_IX_IMM,

	MU_IND_IMM0,
	MU_IND_REG_WITH_IMM0,
	MU_IND_DS_IX_IMM0,
	
	MU_LD_ZN_IND,
	MU_LD_ZN_IND__DEREFER_,
	MU_LD_ZN_IND__MOV_,
	
	MU_LD_ZN_AUTO_IND,
	MU_LD_ZN_AUTO_IND__AUTO_BUFFER_,
	MU_LD_ZN_AUTO_IND__DEREFER_,
	MU_LD_ZN_AUTO_IND__MOV_,
	MU_LD_ZN_AUTO_IND__AUTO_WRITEBACK_,

	MU_LD_ZN_WITH_DS_IND,
	MU_LD_ZN_WITH_DS_IND__DEREFER_,
	MU_LD_ZN_WITH_DS_IND__DS_CALC_,
	
	MU_LD_ZN_DS_AUTO_IND,
	MU_LD_ZN_DS_AUTO_IND__AUTO_BUFFER_,
	MU_LD_ZN_DS_AUTO_IND__DEREFER_,
	MU_LD_ZN_DS_AUTO_IND__DS_CALC_,
	MU_LD_ZN_DS_AUTO_IND__MOV_,
	MU_LD_ZN_DS_AUTO_IND__AUTO_WRITEBACK_,

	MU_LD_ZN_WITH_IX_IND,

	MU_LD_ZN_DS_IX_IND,

	MU_POST_AUTOIDX,
} mucode_entry_idx;

typedef struct
{
	mucode_entry_idx entry_idx;
	uint_fast8_t reg_select;
	bool bank_select;
	bool is_16bit;
	bool is_write;
} mucode_entry_spec;

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
	DATA_REG_S_,
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
	DATA_LATCH_IMM_0,
	DATA_LATCH_IMM_1,
	DATA_LATCH_IMM_2
} data_bus_specifier;

typedef struct {
	data_bus_specifier location;
	bool is_16bit;
	bool sign_extend;
} alu_src_control;

#define MEM_LATCH_AT_HALF1_MASK 0x10
#define MEM_LATCH_AT_HALF2_MASK 0x20

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
	
	// Source transformations
	bool src2_add1;
	bool src2_add_carry;
	bool src2_negate;
	
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
		FLAG_A_CARRY,
		FLAG_A_OVERFLOW,
	} flag_a_mode;
	// At the end of this cycle, locks further modification of the A flag until after the current instruction is finished
	bool set_flag_a_lock;
	enum
	{
		FLAG_V_NORMAL,
		FLAG_V_SHIFTER_CARRY,
		FLAG_V_CLEAR,
	} flag_v_mode;
	
	// Register swap control
	
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
		// Latches at first half of cycle, address from ALU src0 with bank 0 or 1
		MEM_LATCH_HALF1_B0 = MEM_LATCH_AT_HALF1_MASK | 0,
		MEM_LATCH_HALF1_B1 = MEM_LATCH_AT_HALF1_MASK | 1,
		MEM_LATCH_HALF1_BD = MEM_LATCH_AT_HALF1_MASK | 2,
		// Latches at second half of cycle, address from ALU dest with bank D, D + ALU.c or 0
		MEM_LATCH_HALF2_B0 = MEM_LATCH_AT_HALF2_MASK | 0,
		MEM_LATCH_HALF2_BD = MEM_LATCH_AT_HALF2_MASK | 2,
		MEM_LATCH_HALF2_BD_ALUC = MEM_LATCH_AT_HALF2_MASK | 3,
		// Latches at second half of cycle, address and bank are whatever was left in MAR (used for destination writeback)
		MEM_LATCH_HALF2_MAR = MEM_LATCH_AT_HALF2_MASK | 8,
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
	bool mem_16bit;
} execute_control_word;

typedef struct
{
	execute_control_word operation;
	mucode_entry_spec next;
} mucode_entry;

typedef struct
{
	// Immediate data sources
	uint_fast16_t imm_words[3];
	
	// PC and K for this instruction
	uint16_t inst_pc;
	uint8_t inst_k;
	
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
	enum
	{
		AUTO_INCR_HL,
		AUTO_INCR_IX
	} auto_incr_reg_select;

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
		COND_S,          // negative; sign set
		COND_NS,         // positive; sign clear
		COND_V,          // overflow; parity even
		COND_NV,         // not overflow; parity odd
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
} inst_decoded_flags;

#endif

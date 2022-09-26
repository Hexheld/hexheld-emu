#include <stdint.h>

typedef int_fast8_t bool;
#define TRUE 1
#define FALSE 0

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
} mucode_entry_idx;

typedef struct
{
	mucode_entry_idx entry_idx;
	uint_fast8_t reg_select;
	bool bank_select;
	bool is_16bit;
	bool is_write;
} mucode_entry_spec;

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

struct alu_src_control {
	data_bus_specifier location;
	bool is_16bit;
	bool sign_extend;
};

typedef struct
{
	struct alu_src_control srcs[2];
	data_bus_specifier dest;

	enum
	{
		ALU_OFF,
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
	/* Latching an address follows the cycle below:
	 * 1. If memory unit is not ready, execution is blocked with address to be latched being held
	 * 2. Address is latched, data is read or written, takes 1+ cycles
	 * 3. While data is being read or written, 
	 */
	enum
	{
		// Don't latch address
		MEM_NO_LATCH,
		// Latches at first half of cycle, address from ALU src0 with bank 0 or 1
		MEM_LATCH_HALF1_B0,
		MEM_LATCH_HALF1_B1,
		MEM_LATCH_HALF1_BD,
		// Latches at second half of cycle, address from ALU dest with bank D, D + ALU.c or 0
		MEM_LATCH_HALF2_BD,
		MEM_LATCH_HALF2_B0,
		MEM_LATCH_HALF2_BD_ALUC,
		// Latches at second half of cycle, address is whatever was left in MAR (used for destination writeback)
		MEM_LATCH_HALF2_MAR
	} mem_latch_ctl;
	enum
	{
		MEM_READ,
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

struct inst_decoded_flags
{
	// Immediate data sources
	uint_fast16_t imm_words[3];
	
	// Sequencer control
	mucode_entry_spec run_before;
	execute_control_word core_op;
	mucode_entry_spec run_after;
	
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
};

typedef struct {
	struct inst_decoded_flags work_regs;
	uint16_t inst_pc;
	uint8_t inst_k;
	uint8_t inst_length;
	enum
	{
		DECODER_READY,
		DECODER_READ_INST_WORD,
		DECODER_READ_2_EXTRA_WORDS,
		DECODER_READ_EXTRA_WORD
	} decoding_cycle;
} pilot_decode_state;

void decode_unreachable_ ();

// Reads one word from the fetch unit
void decode_read_word_ (pilot_decode_state *state);

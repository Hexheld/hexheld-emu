#include "cpu_regs.h"
#include "memory.h"

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



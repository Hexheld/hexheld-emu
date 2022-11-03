#ifndef __CPU_REGS_H__
#define __CPU_REGS_H__

#include "types.h"

struct pilot_regs_data
{
	// Accumulators
	uint8_t a, b;
	uint8_t h, l;
	
	// Index register
	uint8_t i, x;
	
	// Data segment register
	uint8_t d, s;
	
	// I/O access register
	uint8_t c;
	
	// Processor flags
	uint8_t f;
};

enum
{
	DATAREG_a = 1 << 0,
	DATAREG_b = 1 << 1,
	DATAREG_h = 1 << 2,
	DATAREG_l = 1 << 3,
	DATAREG_i = 1 << 4,
	DATAREG_x = 1 << 5,
	DATAREG_d = 1 << 6,
	DATAREG_s = 1 << 7,
	DATAREG_c = 1 << 8,
	DATAREG_f = 1 << 9
} reg_shadowctl_bits;

typedef struct {
	struct pilot_regs_data regs_data[2];
	
	// For each register, select which copy of it is visible (0 or 1)
	uint16_t shadow_ctl;
	
	// Stack pointer; bit 0 is always zero
	uint16_t sp;
	
	// Program counter and program bank form a 24-bit address; bit 0 of pc is always zero
	uint16_t pc;
	uint8_t k;
	
	// Processor flags
	uint8_t flags, flags_shadow;
	uint8_t int_mask;
} Pilot_cpu_regs;

const enum
{
	// Carry/borrow flag
	F_CARRY   = 1 << 0,
	// When set, operations with DS ignore S, freeing it for general-purpose use
	F_DS_MODE = 1 << 1,
	// Overflow/parity flag
	F_OVRFLW  = 1 << 2,
	// Segment adjust flag
	F_DS_ADJ  = 1 << 3,
	// Half carry flag
	F_HCARRY  = 1 << 4,
	// Interrupt enable
	F_INT_EN  = 1 << 5,
	// Zero flag
	F_ZERO    = 1 << 6,
	// Sign (negative) flag
	F_NEG     = 1 << 7
} flag_masks;

#endif

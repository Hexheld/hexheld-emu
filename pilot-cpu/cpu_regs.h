#ifndef __CPU_REGS_H__
#define __CPU_REGS_H__

#include "types.h"

typedef struct {
	uint_fast24_t regs[8];
	uint16_t wf;

	// Program countet
	uint_fast24_t pgc;
	
	// Internal states
	uint8_t repi;
	uint8_t repr;
} Pilot_cpu_regs;

const enum
{
	// Extend carry/borrow
	F_EXTEND  = 1 << 0,
	// Decimal flag
	F_DECIMAL = 1 << 1,
	// Overflow/parity flag
	F_OVRFLOW = 1 << 2,
	// Carry/borrow flag
	F_CARRY   = 1 << 3,
	// Zero flag
	F_ZERO    = 1 << 6,
	// Sign (negative) flag
	F_NEG     = 1 << 7,
	// Interrupt request level
	F_IRL     = 0x7 << 8
} flag_masks;

#endif

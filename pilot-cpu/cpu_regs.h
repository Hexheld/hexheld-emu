#ifndef __CPU_REGS_H__
#define __CPU_REGS_H__

#include <stdint.h>

struct pilot_regs_main_
{
	// Accumulators
	uint8_t a, b;
	uint8_t h, l;
	
	// Index register
	uint16_t ix;
	
	// Data segment register
	uint16_t ds;
	
	// I/O access register
	uint8_t c;
};

typedef struct {
	struct pilot_regs_main_ main;
	struct pilot_regs_main_ shadow;
	
	// Stack pointer; bit 0 is always zero
	uint16_t sp;
	
	// Program counter and program bank form a 24-bit address; bit 0 of pc is always zero
	uint16_t pc;
	uint8_t k;
	
	// Processor flags
	uint8_t flags, flags_shadow;
	uint8_t int_mask;
} Pilot_cpu_regs;

#endif

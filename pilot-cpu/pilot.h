#ifndef __PILOT_H__
#define __PILOT_H__

#include <stdint.h>
#include <stddef.h>
#include "cpu_regs.h"

typedef enum {
	MCTL_READY = 0,
	MCTL_MEM_BUSY,
	MCTL_IO_BUSY,
	MCTL_DATA_LATCHED
} Pilot_memctl_state;

typedef struct {
	Pilot_memctl_state state;
	size_t wait_cycles_left;
	uint32_t addr_reg;
	uint16_t data_reg;
} Pilot_memctl;

typedef struct {
	Pilot_cpu_regs cpu;
	Pilot_memctl memctl;
	uint8_t ram[0x7fff];
} Pilot_system;

#endif

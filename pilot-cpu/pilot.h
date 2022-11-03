#ifndef __PILOT_H__
#define __PILOT_H__

#include <stdint.h>
#include <stddef.h>
#include "cpu_regs.h"
#include "cpu_interconnect.h"

typedef enum
{
	MCTL_READY = 0,
	MCTL_MEM_R_BUSY,
	MCTL_IO_R_BUSY,
	MCTL_MEM_W_BUSY,
	MCTL_IO_W_BUSY,
	MCTL_DATA_LATCHED
} Pilot_memctl_state;

typedef struct
{
	Pilot_memctl_state state;
	bool data_valid;
	size_t wait_cycles_left;
	uint32_t addr_reg;
	uint16_t data_reg_in;
	uint16_t data_reg_out;
} Pilot_memctl;

typedef struct
{
	Pilot_cpu_regs core;
	Pilot_memctl memctl;
	pilot_interconnect interconnects;
	uint8_t ram[0x7fff];
} Pilot_system;

#endif

#include "memory.h"
#include <stddef.h>

#define WRAM_START 0x0000
#define WRAM_END   0x7fff
#define VRAM_START 0x8000
#define VRAM_END   0xffff

#define CART_CSMOD_START   0x010000
#define CART_CSMOD_END     0x01ffff
#define CART_ROMONLY_START 0x020000
#define CART_ROMONLY_END   0xffffff

int
io_read (Pilot_system *sys)
{
	return 1;
}

int
mem_read (Pilot_system *sys)
{
	uint32_t addr = sys->memctl.addr_reg;
	if (addr <= WRAM_END)
	{
		addr &= 0x7FFE;
		sys->memctl.data_reg = (uint16_t)sys->ram[addr] + ((uint16_t)sys->ram[addr + 1] << 8);
		return 0;
	}
	else if (addr <= VRAM_END)
	{
		// try to read VRAM
	}
	else if (addr <= CART_CSMOD_END)
	{
		// call cartridge chip select mapper
	}
	else
	{
		// call cartridge mapper
	}
	return 1;
}

Pilot_memctl_state
Pilot_mem_addr_assert (Pilot_system *sys, uint8_t bank, uint16_t addr)
{
	if (sys->memctl.state == MCTL_READY)
	{
		uint32_t actual_address = ((uint32_t)bank << 16) + addr;
		
		sys->memctl.addr_reg = actual_address;
		sys->memctl.state = MCTL_MEM_BUSY;
		return MCTL_READY;
	}

	return sys->memctl.state;
}

int
Pilot_mem_data_wait (Pilot_system *sys)
{
	if (sys->memctl.state == MCTL_DATA_LATCHED)
	{
		return 0;
	}

	return 1;
}

void
Pilot_memctl_tick (Pilot_system *sys)
{
	switch (sys->memctl.state)
	{
		case MCTL_MEM_BUSY:
			if (mem_read(sys) == 0)
			{
				sys->memctl.state = MCTL_DATA_LATCHED;
			}
			break;
		case MCTL_IO_BUSY:
			if (io_read(sys) == 0)
			{
				sys->memctl.state = MCTL_DATA_LATCHED;
			}
			break;
		case MCTL_DATA_LATCHED:
			sys->memctl.state = MCTL_READY;
			break;
		default:
			break;
	}
}

uint16_t
Pilot_mem_read (Pilot_system *sys)
{
	return sys->memctl.data_reg;
}

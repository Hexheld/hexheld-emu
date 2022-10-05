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

bool
io_read (Pilot_system *sys)
{
	return TRUE;
}

bool
mem_read (Pilot_system *sys)
{
	uint32_t addr = sys->memctl.addr_reg;
	if (addr <= WRAM_END)
	{
		addr &= 0x7FFE;
		sys->memctl.data_reg_in = (uint16_t)sys->ram[addr] + ((uint16_t)sys->ram[addr + 1] << 8);
		return TRUE;
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
	return FALSE;
}

/*
 * Memory accesses through the memory controller need to be carried out as such:
 * 
 * Reads:
 * Tick 0: Pilot_mem_addr_read_assert - assert the address to be accessed
 * Tick 1: Pilot_mem_data_wait - wait for the access slot to be conceded
 * Tick 1+n, n >= 0: Pilot_mem_read - read data latch
 * 
 * Writes:
 * Tick 0: Pilot_mem_addr_write_assert
 * 
 */
Pilot_memctl_state
Pilot_mem_addr_read_assert (Pilot_system *sys, uint8_t bank, uint16_t addr)
{
	if (sys->memctl.state == MCTL_READY)
	{
		uint32_t actual_address = ((uint32_t)bank << 16) + addr;
		
		sys->memctl.addr_reg = actual_address;
		sys->memctl.state = MCTL_MEM_R_BUSY;
		return MCTL_READY;
	}

	return sys->memctl.state;
}

Pilot_memctl_state
Pilot_mem_addr_write_assert (Pilot_system *sys, uint8_t bank, uint16_t addr, uint16_t data)
{
	if (sys->memctl.state == MCTL_READY)
	{
		uint32_t actual_address = ((uint32_t)bank << 16) + addr;
		
		sys->memctl.addr_reg = actual_address;
		sys->memctl.data_reg_out = data;
		sys->memctl.state = MCTL_MEM_W_BUSY;
		return MCTL_READY;
	}

	return sys->memctl.state;
}

bool
Pilot_mem_data_wait (Pilot_system *sys)
{
	if (sys->memctl.data_valid)
	{
		return TRUE;
	}

	return FALSE;
}

void
Pilot_memctl_tick (Pilot_system *sys)
{
	sys->memctl.data_valid = FALSE;
	switch (sys->memctl.state)
	{
		case MCTL_MEM_R_BUSY:
			if (mem_read(sys))
			{
				sys->memctl.state = MCTL_READY;
				sys->memctl.data_valid = TRUE;
			}
			break;
		case MCTL_IO_R_BUSY:
			if (io_read(sys))
			{
				sys->memctl.state = MCTL_READY;
				sys->memctl.data_valid = TRUE;
			}
			break;
		default:
			break;
	}
}

uint16_t
Pilot_mem_get_data (Pilot_system *sys)
{
	return sys->memctl.data_reg_in;
}

#include "memory.h"
#include <stddef.h>

#define WRAM_END         0x007fff
#define VRAM_END         0x00ffff
#define CART_CS1_END     0x0fffff
#define CART_CS2_END     0x1fffff
#define CART_ROM_END     0xffdfff
#define TMRAM_END        0xffefff
#define OAM_END          0xfff27f
#define HCIO_START       0xfff300
#define HCIO_END         0xfff3ff
#define HRAM_END         0xffffff

bool
mem_read (Pilot_system *sys)
{
	uint32_t addr = sys->memctl.addr_reg;
	if (addr <= WRAM_END)
	{
		// try to read WRAM
	}
	else if (addr <= VRAM_END)
	{
		// try to read VRAM
	}
	else if (addr <= CART_CS2_END)
	{
		// call cartridge chip select mapper
	}
	else if (addr <= CART_ROM_END)
	{
		// call cartridge mapper
	}
	else if (addr <= TMRAM_END)
	{
		// tilemap RAM
	}
	else if (addr <= OAM_END)
	{
		// sprite attribute RAM
	}
	else if (HCIO_START <= addr && addr <= HCIO_END)
	{
		// memory mapped I/O
	}
	else if (addr <= HRAM_END)
	{
		sys->memctl.data_reg_in = sys->hram[addr] | (sys->hram[addr + 1] << 8);
		return TRUE;
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
Pilot_mem_addr_read_assert (Pilot_system *sys, uint32_t addr)
{
	if (sys->memctl.state == MCTL_READY)
	{
		sys->memctl.addr_reg = addr;
		sys->memctl.state = MCTL_MEM_R_BUSY;
		return MCTL_READY;
	}

	return sys->memctl.state;
}

Pilot_memctl_state
Pilot_mem_addr_write_assert (Pilot_system *sys, uint32_t addr, uint16_t data)
{
	if (sys->memctl.state == MCTL_READY)
	{
		sys->memctl.addr_reg = addr;
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
	if (sys->memctl.state == MCTL_MEM_R_BUSY && mem_read(sys))
	{
		sys->memctl.state = MCTL_READY;
		sys->memctl.data_valid = TRUE;
	}
}

uint16_t
Pilot_mem_get_data (Pilot_system *sys)
{
	return sys->memctl.data_reg_in;
}

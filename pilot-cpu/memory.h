#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include "pilot.h"

void Pilot_memctl_tick (Pilot_system *sys);

Pilot_memctl_state Pilot_mem_addr_read_assert (Pilot_system *sys, uint8_t bank, uint16_t addr);
Pilot_memctl_state Pilot_mem_addr_write_assert (Pilot_system *sys, uint8_t bank, uint16_t addr, uint16_t data);
Pilot_memctl_state Pilot_io_addr_assert (Pilot_system *sys, uint8_t addr);
bool Pilot_mem_data_wait (Pilot_system *sys);
uint16_t Pilot_mem_get_data (Pilot_system *sys);
bool Pilot_io_data_wait (Pilot_system *sys);

uint16_t Pilot_memctl_read (Pilot_system *sys);
void Pilot_memctl_write (Pilot_system *sys, uint16_t data);

#endif

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include "pilot.h"

void Pilot_memctl_tick (Pilot_system *sys);

Pilot_memctl_state Pilot_mem_addr_read_assert (Pilot_system *sys, uint32_t addr);
Pilot_memctl_state Pilot_mem_addr_write_assert (Pilot_system *sys, uint32_t addr, uint16_t data);
bool Pilot_mem_data_wait (Pilot_system *sys);
uint16_t Pilot_mem_get_data (Pilot_system *sys);

uint16_t Pilot_memctl_read (Pilot_system *sys);
void Pilot_memctl_write (Pilot_system *sys, uint16_t data);

#endif

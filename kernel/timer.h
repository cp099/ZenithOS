#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Initialize PIT timer at given frequency (in Hz)
void timer_init(uint32_t frequency);

// Sleep/block for a number of ticks (10ms per tick at 100Hz)
void timer_wait(uint32_t ticks);

// Return system ticks count
uint32_t get_ticks(void);

#endif

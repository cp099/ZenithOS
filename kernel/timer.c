#include "timer.h"
#include "idt.h"
#include <stddef.h>

static uint32_t system_ticks = 0;

// Port output helper
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

// Timer interrupt callback (triggers on IRQ0 / Interrupt 32)
static void timer_callback(registers_t* regs) {
    (void)regs;
    system_ticks++;
}

void timer_init(uint32_t frequency) {
    // Register the IRQ0 callback
    register_interrupt_handler(32, timer_callback);

    // Write command byte (0x36) to command port 0x43
    // Set binary counting, Mode 3 (Square Wave Generator), access LSB then MSB, Channel 0
    outb(0x43, 0x36);

    // Calculate frequency divisor
    uint32_t divisor = 1193180 / frequency;
    if (divisor > 65535) divisor = 65535;

    // Send the frequency divisor bytes to channel 0 data port 0x40
    outb(0x40, (uint8_t)(divisor & 0xFF));          // LSB
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));   // MSB
}

uint32_t get_ticks(void) {
    return system_ticks;
}

void timer_wait(uint32_t ticks) {
    uint32_t eticks = system_ticks + ticks;
    while (system_ticks < eticks) {
        __asm__ volatile("hlt"); // Wait for next interrupt (power save)
    }
}

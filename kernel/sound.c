#include "sound.h"
#include "timer.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void play_beep(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
        uint8_t tmp = inb(0x61) & 0xFC;
        outb(0x61, tmp);
        if (duration_ms > 0) {
            uint32_t ticks = duration_ms / 10;
            if (ticks == 0) ticks = 1;
            timer_wait(ticks);
        }
        return;
    }

    uint32_t div = 1193180 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }

    if (duration_ms > 0) {
        uint32_t ticks = duration_ms / 10;
        if (ticks == 0) ticks = 1;
        timer_wait(ticks);
    }

    tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

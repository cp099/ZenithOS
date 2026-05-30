#include "ata.h"

// Port IO helpers
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %w0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %w1, %w0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Wait for BSY (bit 7) to clear and DRQ (bit 3) to set
static void ata_wait_drq(void) {
    while (1) {
        uint8_t status = inb(0x1F7);
        if (!(status & 0x80) && (status & 0x08)) {
            break;
        }
    }
}

// Wait for BSY (bit 7) to clear
static void ata_wait_bsy(void) {
    while (inb(0x1F7) & 0x80);
}

void ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer) {
    ata_wait_bsy();
    
    // Select drive: 0xF0 specifies Slave Drive on Primary Channel (our zenithos.zfs disk)
    outb(0x1F6, 0xF0 | ((lba >> 24) & 0x0F));
    
    // Write parameters
    outb(0x1F2, count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    
    // Send Read Command (0x20)
    outb(0x1F7, 0x20);

    uint16_t* ptr = (uint16_t*)buffer;
    for (uint8_t s = 0; s < count; s++) {
        ata_wait_drq();
        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            ptr[s * 256 + i] = inw(0x1F0);
        }
    }
}

void ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer) {
    ata_wait_bsy();
    
    // Select drive: 0xF0 (Slave)
    outb(0x1F6, 0xF0 | ((lba >> 24) & 0x0F));
    
    // Write parameters
    outb(0x1F2, count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    
    // Send Write Command (0x30)
    outb(0x1F7, 0x30);

    const uint16_t* ptr = (const uint16_t*)buffer;
    for (uint8_t s = 0; s < count; s++) {
        ata_wait_drq();
        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(0x1F0, ptr[s * 256 + i]);
        }
    }
    
    // Flush cache to ensure write to disk
    outb(0x1F7, 0xE7); // Cache Flush Command
    ata_wait_bsy();
}

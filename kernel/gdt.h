#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// GDT entry structure
struct gdt_entry_struct {
    uint16_t limit_low;           // The lower 16 bits of the limit
    uint16_t base_low;            // The lower 16 bits of the base
    uint8_t  base_middle;         // The next 8 bits of the base
    uint8_t  access;              // Access flags, determine Ring privilege
    uint8_t  granularity;         // Granularity and limit high bits
    uint8_t  base_high;           // The last 8 bits of the base
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

// GDT pointer structure (passed to lgdt)
struct gdt_ptr_struct {
    uint16_t limit;               // Size of the GDT - 1
    uint32_t base;                // Address of the GDT
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

// TSS structure for x86 hardware multitasking/stack switching
struct tss_entry_struct {
    uint32_t link;                // Previous TSS link
    uint32_t esp0;                // Kernel stack pointer (Ring 0)
    uint32_t ss0;                 // Kernel stack segment
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;          // I/O map base address (usually pointing past struct)
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

// Initialize GDT and TSS
void gdt_init(void);

// Set kernel stack pointer for interrupts
void set_kernel_stack(uint32_t stack_phys);

#endif

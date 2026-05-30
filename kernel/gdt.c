#include "gdt.h"
#include <stddef.h>

#define GDT_ENTRIES_COUNT 6

static gdt_entry_t gdt_entries[GDT_ENTRIES_COUNT];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss_entry;

// Set value of a GDT gate
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = ((base >> 16) & 0xFF);
    gdt_entries[num].base_high   = ((base >> 24) & 0xFF);

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F);

    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

// Memory copy helper
static void local_memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0) {
        *ptr++ = (unsigned char)val;
    }
}

// Initialise the GDT and TSS
void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES_COUNT) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 1. Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 2. Kernel Code Segment (0x08)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 3. Kernel Data Segment (0x10)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 4. User Code Segment (0x18 | 3 = 0x1B)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 5. User Data Segment (0x20 | 3 = 0x23)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 6. TSS Descriptor (0x28)
    uint32_t tss_base  = (uint32_t)&tss_entry;
    uint32_t tss_limit = sizeof(tss_entry_t) - 1;
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x00);

    // Initialize TSS entry
    local_memset(&tss_entry, 0, sizeof(tss_entry_t));
    tss_entry.ss0  = 0x10;          // Kernel Data selector
    tss_entry.esp0 = 0x90000;       // Kernel stack bottom (matching stage2.asm)
    tss_entry.iomap_base = sizeof(tss_entry_t); // Disables IO map

    // Reload the GDT and reload segment selectors
    __asm__ volatile(
        "lgdt %0\n\t"               // Load the GDT
        "ljmp $0x08, $1f\n\t"       // Reload CS with 0x08 code segment
        "1:\n\t"
        "mov $0x10, %%ax\n\t"       // Reload data segment registers with 0x10
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "mov $0x28, %%ax\n\t"       // TSS selector index is 0x28
        "ltr %%ax\n\t"              // Load task register
        :
        : "m"(gdt_ptr)
        : "ax", "memory"
    );
}

// Dynamically set kernel stack top (useful during process context switches)
void set_kernel_stack(uint32_t stack_phys) {
    tss_entry.esp0 = stack_phys;
}

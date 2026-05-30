#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT gate entry structure
struct idt_entry_struct {
    uint16_t base_lo;             // Lower 16 bits of handler address
    uint16_t sel;                 // Kernel code segment selector (0x08)
    uint8_t  always0;             // Reserved, must be 0
    uint8_t  flags;               // Type and attributes
    uint16_t base_hi;             // Upper 16 bits of handler address
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

// IDT pointer structure (passed to lidt)
struct idt_ptr_struct {
    uint16_t limit;               // Size of table - 1
    uint32_t base;                // Start address of table
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

// Register state struct pushed during interrupts
struct Registers {
    uint32_t gs, fs, es, ds;      // Segment registers
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t int_no, err_code;    // Pushed manually in assembly stub
    uint32_t eip, cs, eflags, useresp, ss; // Pushed by CPU hardware
};

typedef struct Registers registers_t;

// Initialize IDT and PIC
void idt_init(void);

// Typedef for interrupt handler callbacks
typedef void (*interrupt_handler_t)(registers_t* regs);

// Register a custom callback for an interrupt
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);

#endif

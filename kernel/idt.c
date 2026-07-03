#include "idt.h"
#include "graphics.h"
#include "task.h"
#include <stddef.h>

#define IDT_ENTRIES_COUNT 256

static idt_entry_t idt_entries[IDT_ENTRIES_COUNT];
static idt_ptr_t   idt_ptr;
static interrupt_handler_t interrupt_handlers[IDT_ENTRIES_COUNT];

// Exception assembly stubs declared in interrupts.asm
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

// Hardware IRQ assembly stubs declared in interrupts.asm
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void idt_irq11(void); // renamed to avoid naming collision with symbols
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

// Syscall stub
extern void isr128(void);

// Outb helper function (writes a byte to an I/O port)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

// Inb helper function (reads a byte from an I/O port)
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Memory set helper
static void local_memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0) {
        *ptr++ = (unsigned char)val;
    }
}

// Set individual IDT gate
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_lo = (base & 0xFFFF);
    idt_entries[num].base_hi = ((base >> 16) & 0xFFFF);
    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = flags;
}

// Remap PIC to avoid exception overlap (map IRQ0-7 to 0x20-0x27, IRQ8-15 to 0x28-0x2F)
static void pic_remap(void) {
    outb(0x20, 0x11);           // ICW1 (Init command)
    outb(0xA0, 0x11);
    
    outb(0x21, 0x20);           // ICW2 (PIC1 Vector Offset: 0x20)
    outb(0xA1, 0x28);           // ICW2 (PIC2 Vector Offset: 0x28)
    
    outb(0x21, 0x04);           // ICW3 (Cascade identity master)
    outb(0xA1, 0x02);           // ICW3 (Cascade identity slave)
    
    outb(0x21, 0x01);           // ICW4 (8086 mode)
    outb(0xA1, 0x01);
    
    outb(0x21, 0x00);           // Null mask (enable all IRQs)
    outb(0xA1, 0x00);
}

void idt_init(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES_COUNT) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    local_memset(&idt_entries, 0, sizeof(idt_entry_t) * IDT_ENTRIES_COUNT);
    local_memset(&interrupt_handlers, 0, sizeof(interrupt_handler_t) * IDT_ENTRIES_COUNT);

    // Remap PIC
    pic_remap();

    // 1. Install Exception Gates (Ring 0 Privilege = 0x8E)
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    // 2. Install Hardware IRQ Gates (mapped to interrupts 32-47, Ring 0 Privilege)
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)idt_irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // 3. Install Syscall trap gate (interrupt 128 = 0x80, User Privilege = 0xEE)
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    // Load IDT register
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

void register_interrupt_handler(uint8_t n, interrupt_handler_t handler) {
    interrupt_handlers[n] = handler;
}

// Visual error string formatting
extern void serial_print(const char* str);
extern void serial_print_hex(uint32_t val);

// Visual error string formatting
static void print_error_regs(registers_t* regs) {
    serial_print("\n*** CPU EXCEPTION ***\n");
    serial_print("Interrupt index: 0x");
    serial_print_hex(regs->int_no);
    serial_print("\n");

    print_string_default("\n*** CPU EXCEPTION ***\n");
    print_string_default("Interrupt index: ");
    
    // Quick hex formatter
    char buf[12];
    buf[0] = '0'; buf[1] = 'x';
    uint32_t num = regs->int_no;
    for (int i = 9; i >= 2; i--) {
        uint32_t nib = num & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
        num >>= 4;
    }
    buf[10] = '\n'; buf[11] = '\0';
    print_string_default(buf);

    serial_print("EAX: 0x"); serial_print_hex(regs->eax); serial_print(" ");
    serial_print("EBX: 0x"); serial_print_hex(regs->ebx); serial_print(" ");
    serial_print("ECX: 0x"); serial_print_hex(regs->ecx); serial_print(" ");
    serial_print("EDX: 0x"); serial_print_hex(regs->edx); serial_print("\n");
    serial_print("ESI: 0x"); serial_print_hex(regs->esi); serial_print(" ");
    serial_print("EDI: 0x"); serial_print_hex(regs->edi); serial_print(" ");
    serial_print("ESP: 0x"); serial_print_hex(regs->esp); serial_print(" ");
    serial_print("EBP: 0x"); serial_print_hex(regs->ebp); serial_print("\n");
    serial_print("CS:  0x"); serial_print_hex(regs->cs);  serial_print(" ");
    serial_print("SS:  0x"); serial_print_hex(regs->ss);  serial_print(" ");
    serial_print("EFL: 0x"); serial_print_hex(regs->eflags); serial_print("\n");

    serial_print("EIP: 0x");
    serial_print_hex(regs->eip);
    serial_print("\n");

    print_string_default("EIP: ");
    num = regs->eip;
    for (int i = 9; i >= 2; i--) {
        uint32_t nib = num & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
        num >>= 4;
    }
    print_string_default(buf);
    
    if (regs->int_no == 14) { // Page Fault
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        
        serial_print("Page Fault Address (CR2): 0x");
        serial_print_hex(cr2);
        serial_print("\n");

        serial_print("Error Code: 0x");
        serial_print_hex(regs->err_code);
        serial_print("\n");

        uint32_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        serial_print("CR3 (Page Directory Phys): 0x");
        serial_print_hex(cr3);
        serial_print("\n");

        Task* cur_task = get_current_task();
        if (cur_task) {
            serial_print("Active Task ID: 0x");
            serial_print_hex(cur_task->id);
            serial_print("\n");
            serial_print("Active Task CR3: 0x");
            serial_print_hex(cur_task->cr3);
            serial_print("\n");
            serial_print("Active Task user_esp: 0x");
            serial_print_hex(cur_task->user_esp);
            serial_print("\n");
            serial_print("Active Task kstack: 0x");
            serial_print_hex(cur_task->kstack);
            serial_print("\n");
        }

        serial_print("Call Stack (EBP Walk):\n");
        uint32_t* ebp = (uint32_t*)regs->ebp;
        // The kernel stacks are allocated on the heap or are at boot stack (0x90000)
        while (ebp != NULL && (uint32_t)ebp >= 0x10000 && (uint32_t)ebp < 0x1C00000) {
            uint32_t ret_addr = ebp[1];
            serial_print("  [EBP ");
            serial_print_hex((uint32_t)ebp);
            serial_print("] Return Address: 0x");
            serial_print_hex(ret_addr);
            serial_print("\n");
            ebp = (uint32_t*)ebp[0];
        }

        uint32_t* dir = (uint32_t*)cr3;
        uint32_t dir_idx = cr2 >> 22;
        uint32_t tbl_idx = (cr2 >> 12) & 0x3FF;

        serial_print("PDE Index: 0x");
        serial_print_hex(dir_idx);
        serial_print(" PDE Value: 0x");
        serial_print_hex(dir[dir_idx]);
        serial_print("\n");

        if (dir[dir_idx] & 0x01) { // present
            uint32_t* page_table = (uint32_t*)(dir[dir_idx] & 0xFFFFF000);
            serial_print("PTE Index: 0x");
            serial_print_hex(tbl_idx);
            serial_print(" PTE Value: 0x");
            serial_print_hex(page_table[tbl_idx]);
            serial_print("\n");
        }

        print_string_default("Page Fault Address (CR2): ");
        num = cr2;
        for (int i = 9; i >= 2; i--) {
            uint32_t nib = num & 0xF;
            buf[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
            num >>= 4;
        }
        print_string_default(buf);
    }
}

// Master C-level interrupt dispatcher called by assembly stubs
void handle_interrupt(registers_t* regs) {
    // Send EOI (End of Interrupt) to PIC if it's a hardware interrupt
    if (regs->int_no >= 32 && regs->int_no < 48) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20); // Reset slave PIC
        }
        outb(0x20, 0x20);     // Reset master PIC
    }

    // Call registered handler
    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
    } else {
        // Unhandled interrupts / exceptions
        if (regs->int_no < 32) {
            print_error_regs(regs);
            
            // Check if exception came from Ring 3 (User mode CS is 0x1B)
            if (regs->cs == 0x1B) {
                Task* cur = get_current_task();
                if (cur != NULL && cur->id != 0) {
                    print_string_default("\n[KERNEL] User task crashed. Terminating process.\n");
                    serial_print("\n[KERNEL] User task crashed. Terminating process.\n");
                    
                    extern void task_terminate(Task* task, int exit_code);
                    task_terminate(cur, -1);
                    
                    extern void scheduler_yield(void);
                    scheduler_yield();
                    return; // Should never be reached
                }
            }

            print_string_default("\nSystem Halted due to unhandled processor exception.");
            __asm__ volatile("cli");
            while (1) {
                __asm__ volatile("hlt");
            }
        }
    }
}

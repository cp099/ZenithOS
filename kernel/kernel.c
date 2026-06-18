/* ==========================================================================
   ZenithOS Kernel Main Bootstrapper
   Initializes CPU, Paging, Graphics, Filesystem, and executes Ring 3 Userland.
   ========================================================================== */

#include "graphics.h"
#include "gdt.h"
#include "idt.h"
#include "paging.h"
#include "timer.h"
#include "keyboard.h"
#include "ata.h"
#include "zenithfs.h"
#include "syscall.h"
#include "heap.h"
#include "task.h"
#include <stddef.h>
#include <stdbool.h>

// Forward declarations
void user_program(void);
static void enter_ring3(uint32_t entry_point, uint32_t user_stack);

// --------------------------------------------------------------------------
// COM1 Serial Debugging Driver
// --------------------------------------------------------------------------
#define SERIAL_PORT 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);    // Disable all interrupts
    outb(SERIAL_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_PORT + 1, 0x00);    //                  (hi byte)
    outb(SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(SERIAL_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int is_transmit_empty(void) {
    return inb(SERIAL_PORT + 5) & 0x20;
}

static void serial_write_char(char a) {
    while (is_transmit_empty() == 0);
    outb(SERIAL_PORT, a);
}

void serial_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(str[i]);
    }
}

void serial_print_hex(uint32_t val) {
    char hex[9];
    const char* chars = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        hex[i] = chars[val & 0xF];
        val >>= 4;
    }
    hex[8] = '\0';
    serial_print(hex);
}

// Busy-loop delay for boot stages before interrupts are enabled
static void boot_delay(void) {
    for (volatile int d = 0; d < 12000000; d++);
}

static void test_task(void) {
    while (1) {
        extern void graphics_draw_statusbar(void);
        graphics_draw_statusbar();
        
        Task* cur = get_current_task();
        cur->state = TASK_SLEEPING;
        cur->sleep_ticks = 100; // Sleep 1 second
        scheduler_yield();
    }
}

// --------------------------------------------------------------------------
// Kernel Main Entry Point
// --------------------------------------------------------------------------
__attribute__((section(".text.boot"))) void kernel_main(void) {
    // Initialize stack guard first using CPU timestamp counter (TSC) to ensure all threads share the same random seed
    uint32_t tsc_low;
    __asm__ volatile("rdtsc" : "=a"(tsc_low) : : "edx");
    extern uint32_t __stack_chk_guard;
    __stack_chk_guard = 0xDEADC0DE ^ tsc_low ^ (uint32_t)&__stack_chk_guard;

    // Initialize serial port first for logging
    serial_init();
    serial_print("\n==================================================\n");
    serial_print("          Zenith OS Core Kernel Serial Log        \n");
    serial_print("==================================================\n");

    // Copy and inspect BootInfo written by Stage 2
    struct BootInfo binfo;
    const uint8_t* boot_info_src = (const uint8_t*)0x7000;
    uint8_t* boot_info_dest = (uint8_t*)&binfo;
    for (size_t i = 0; i < sizeof(struct BootInfo); i++) {
        boot_info_dest[i] = boot_info_src[i];
    }

    serial_print("Stage 2 BootInfo structure copied:\n");
    serial_print("  framebuffer: 0x"); serial_print_hex(binfo.framebuffer); serial_print("\n");
    serial_print("  width:       "); serial_print_hex(binfo.width);       serial_print("\n");
    serial_print("  height:      "); serial_print_hex(binfo.height);      serial_print("\n");
    serial_print("  pitch:       "); serial_print_hex(binfo.pitch);       serial_print("\n");
    serial_print("  bpp:         "); serial_print_hex(binfo.bpp);         serial_print("\n");

    // 1. Initialize graphics mode (VESA VBE linear framebuffer)
    serial_print("[+] Initializing graphics mode...\n");
    graphics_init(); // Shows splash screen at 0%
    boot_delay();
    
    // 2. Load custom Ring privilege GDT and Task State Segment
    serial_print("[+] Initializing GDT & TSS...\n");
    graphics_update_progress("Initializing CPU Segment Descriptors (GDT/TSS)...", 15);
    gdt_init();
    boot_delay();
    
    // 3. Load Interrupt Descriptor Table (IDT) and remap PIC
    serial_print("[+] Initializing IDT & Remapping PIC...\n");
    graphics_update_progress("Configuring Interrupt Vectors & PIC controller...", 30);
    idt_init();
    boot_delay();
    
    // 4. Initialize physical memory allocator (PMM) and paging (VMM)
    serial_print("[+] Enabling Paging VMM/PMM...\n");
    graphics_update_progress("Enabling CPU Two-Level Paging (128MB identity mapped)...", 45);
    paging_init();
    boot_delay();

    // 4.5 Initialize kernel heap and multitasking scheduler
    serial_print("[+] Initializing Kernel Heap & Scheduler...\n");
    graphics_update_progress("Initializing Kernel Heap & Task Scheduler...", 50);
    heap_init();
    scheduler_init();
    task_create(test_task, 0);
    boot_delay();
    
    // 5. Initialize timer interrupts (PIT at 100Hz)
    serial_print("[+] Initializing Timer (PIT) at 100Hz...\n");
    graphics_update_progress("Initializing System Timer interrupts (PIT @ 100Hz)...", 60);
    timer_init(100);
    boot_delay();
    
    // 6. Initialize Keyboard interrupt handler
    serial_print("[+] Initializing Keyboard Driver...\n");
    graphics_update_progress("Initializing PS/2 Keyboard layout driver...", 75);
    keyboard_init();
    boot_delay();
    
    // 7. Enable CPU interrupts (essential for timer & keyboard to fire)
    serial_print("[+] Enabling CPU hardware interrupts...\n");
    __asm__ volatile("sti");
    timer_wait(35);
    
    // 8. Probe storage and mount custom ZenithFS filesystem
    serial_print("[+] Probing storage and mounting ZenithFS...\n");
    graphics_update_progress("Detecting Hard Drive and mounting ZenithFS...", 85);
    zenithfs_mount();
    timer_wait(35);
    
    // 9. Register system call interface (int 0x80)
    serial_print("[+] Registering Syscall interface...\n");
    graphics_update_progress("Registering Int 0x80 System Call Interface...", 95);
    syscall_init();
    timer_wait(35);
    
    // 10. Switch CPU to User privilege Ring 3 and execute user program
    serial_print("[+] Loading initial user space process 'sh.bin'...\n");
    graphics_update_progress("Loading Userland Interactive Shell (sh.bin)...", 100);
    
    // Wait for a good amount of time (15 seconds) so the user can see the boot screen
    timer_wait(1500);

    // Load sh.bin to a safe temporary area in kernel space (e.g. 8MB)
    uint8_t* exec_buf = (uint8_t*)0x800000;

    int32_t size = zenithfs_read_file("sh.bin", exec_buf);
    if (size > 0) {
        serial_print("  [+] Loaded sh.bin from disk. Creating private page directory...\n");
        uint32_t* process_dir = (uint32_t*)vmm_create_page_dir();
        
        // Update current task CR3 tracking
        get_current_task()->cr3 = (uint32_t)process_dir;

        // Allocate a window for the shell
        extern void create_window_for_task(struct Task* owner, int w, int h, const char* title);
        create_window_for_task(get_current_task(), 640, 400, "sh.bin");

        // Map user pages starting at 0x40000000 in the private directory (with 16KB extra for BSS/padding)
        uint32_t mem_size = (uint32_t)size + 16384;
        for (uint32_t addr = 0x40000000; addr < 0x40000000 + mem_size; addr += 4096) {
            vmm_map_page_in_dir(process_dir, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }

        // Allocate and map user stack pages (from 0x400F8000 to 0x40100000, 32KB = 8 pages)
        for (uint32_t addr = 0x400F8000; addr < 0x40100000; addr += 4096) {
            vmm_map_page_in_dir(process_dir, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }

        // Switch to the process's page directory before copying code
        __asm__ volatile("mov %0, %%cr3" : : "r"(process_dir));

        // Copy shell code to user virtual memory base (which now resolves via process_dir)
        for (int i = 0; i < size; i++) {
            ((uint8_t*)0x40000000)[i] = exec_buf[i];
        }

        // Zero out the rest of user space memory segment (BSS + padding)
        for (uint32_t i = size; i < mem_size; i++) {
            ((uint8_t*)0x40000000)[i] = 0;
        }

        // Zero out stack memory to prevent garbage access and leak vulnerabilities
        for (uint32_t i = 0; i < 32768; i++) {
            ((uint8_t*)0x400F8000)[i] = 0;
        }
        
        serial_print("  [+] Code at 0x40000000: ");
        for (int i = 0; i < 16; i++) {
            serial_print_hex(((uint8_t*)0x40000000)[i]);
            serial_print(" ");
        }
        serial_print("\n");
        
        // Draw the window manager frame and console workspace
        graphics_draw_frame();
        graphics_clear_console();
        graphics_swap_buffers();
        
        serial_print("  [+] Swapping CPU context to sh.bin in Ring 3...\n");
        enter_ring3(0x40000000, 0x40100000);
    } else {
        serial_print("  [-] Warning: 'sh.bin' not found on ZenithFS. Falling back to internal user program...\n");
        
        // Map user stack pages in master page directory (from 0x400F8000 to 0x40100000, 32KB)
        for (uint32_t addr = 0x400F8000; addr < 0x40100000; addr += 4096) {
            vmm_map_page(addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }
        
        // Make the kernel code page containing user_program user-accessible for the fallback
        uint32_t prog_page = (uint32_t)user_program & 0xFFFFF000;
        vmm_map_page(prog_page, prog_page, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        
        // Draw the window manager frame and console workspace
        graphics_draw_frame();
        graphics_clear_console();
        graphics_swap_buffers();
        
        enter_ring3((uint32_t)user_program, 0x40100000);
    }
    
    // Should never be reached as iret switches code flow
    while(1) {
        __asm__ volatile("hlt");
    }

}

// --------------------------------------------------------------------------
// User Program running in Ring 3 privilege
// --------------------------------------------------------------------------
void user_program(void) {
    const char* welcome_msg = "\n[USER PROCESS] Entered Ring 3 Userland successfully!\n"
                              "[USER PROCESS] Please enter your name: ";
    
    // Syscall SYS_WRITE (0)
    __asm__ volatile(
        "mov $0, %%eax\n\t"     // SYS_WRITE
        "mov %0, %%ebx\n\t"     // Message pointer
        "int $0x80\n\t"
        :
        : "r"(welcome_msg)
        : "eax", "ebx"
    );

    char name_buffer[64];
    // Syscall SYS_READ (1)
    __asm__ volatile(
        "mov $1, %%eax\n\t"     // SYS_READ
        "mov %0, %%ebx\n\t"     // Destination buffer pointer
        "mov $64, %%ecx\n\t"    // Max buffer size
        "int $0x80\n\t"
        :
        : "r"(name_buffer)
        : "eax", "ebx", "ecx"
    );

    const char* greet_msg = "\n[USER PROCESS] Nice to meet you! Sleeping for 150 ticks (1.5 seconds)...\n";
    __asm__ volatile(
        "mov $0, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "int $0x80\n\t"
        :
        : "r"(greet_msg)
        : "eax", "ebx"
    );

    // Syscall SYS_SLEEP (2)
    __asm__ volatile(
        "mov $2, %%eax\n\t"     // SYS_SLEEP
        "mov $150, %%ebx\n\t"   // 150 ticks
        "int $0x80\n\t"
        :
        :
        : "eax", "ebx"
    );

    const char* finished_msg = "[USER PROCESS] Sleep finished. Exiting userland program.\n";
    __asm__ volatile(
        "mov $0, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "int $0x80\n\t"
        :
        : "r"(finished_msg)
        : "eax", "ebx"
    );

    // Syscall SYS_EXIT (3)
    __asm__ volatile(
        "mov $3, %%eax\n\t"     // SYS_EXIT
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

// --------------------------------------------------------------------------
// Transition from Ring 0 to Ring 3 (User mode) using iret stack manipulation
// --------------------------------------------------------------------------
void enter_ring3(uint32_t entry_point, uint32_t user_stack) {
    // Disable interrupts during stack setup
    __asm__ volatile("cli");
    
    // Configure TSS stack pointer to point to kernel stack bottom for interrupt handling
    set_kernel_stack(get_current_task()->kstack);
    
    // Set segment registers to point to User Data Segment selector (0x20 | RPL 3 = 0x23)
    __asm__ volatile(
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        
        "push $0x23\n\t"           // Push SS (User Data segment)
        "push %1\n\t"              // Push ESP (User Stack pointer)
        "pushfl\n\t"               // Push EFLAGS
        "pop %%eax\n\t"
        "or $0x200, %%eax\n\t"     // Enable interrupts in user mode (set IF flag in eflags)
        "push %%eax\n\t"
        "push $0x1B\n\t"           // Push CS (User Code segment 0x18 | RPL 3 = 0x1B)
        "push %0\n\t"              // Push EIP (User program entry point address)
        "iret\n\t"                 // Return from interrupt to User mode
        :
        : "r"(entry_point), "r"(user_stack)
        : "eax", "memory"
    );
}

void user_entry_wrapper(void) {
    Task* cur = get_current_task();
    enter_ring3(0x40000000, cur->user_esp);
}

// Stack protector guard symbols
uint32_t __stack_chk_guard = 0xDEADC0DE;

void __attribute__((noreturn)) __stack_chk_fail(void) {
    serial_print("\n[CRITICAL SECURITY ALERT] Stack Smashing/Overflow Detected! System halted.\n");
    print_string_default("\n[SECURITY PANIC] STACK CORRUPTION DETECTED!\n");
    while (1) {
        __asm__ volatile("cli\n\t"
                         "hlt");
    }
}


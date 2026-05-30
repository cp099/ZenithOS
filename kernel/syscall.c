#include "syscall.h"
#include "graphics.h"
#include "keyboard.h"
#include "timer.h"
#include "zenithfs.h"
#include "paging.h"
#include "task.h"
#include <stddef.h>

extern void serial_print(const char* str);
extern void serial_print_hex(uint32_t val);

// Validate that user-provided pointers reside within the valid User memory space (0MB to 128MB or 0x40000000 to 0x48000000)
bool syscall_verify_pointer(const void* ptr, uint32_t size) {
    uint32_t start = (uint32_t)ptr;
    uint32_t end = start + size;
    
    // Strict User virtual space check (1GB to 1GB + 128MB)
    // Ensures pointers do not point to kernel space (under 1GB) and handle overflow/wrap-around.
    if (start >= 0x40000000 && end <= 0x48000000 && end >= start) {
        return true;
    }
    
    serial_print("  [!] syscall_verify_pointer failed: ptr=0x");
    serial_print_hex(start);
    serial_print(", size=");
    serial_print_hex(size);
    serial_print(", end=0x");
    serial_print_hex(end);
    serial_print("\n");
    return false;
}


// Master syscall dispatcher (triggered on int 0x80 / Interrupt 128)
static void syscall_handler(registers_t* regs) {
    // Re-enable interrupts during system call execution to allow timer/keyboard interrupts to fire
    __asm__ volatile("sti");

    uint32_t syscall_num = regs->eax;
    
    switch (syscall_num) {
        case SYS_WRITE: {
            const char* str = (const char*)regs->ebx;
            // Verify buffer safety (check first byte and search for null terminator)
            uint32_t len = 0;
            if (str != NULL) {
                while (str[len] != '\0' && len < 4096) len++;
            }
            
            if (str == NULL || !syscall_verify_pointer(str, len + 1)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_WRITE!\n");
                regs->eax = -1; // Return error code
            } else {
                print_string_default(str);
                serial_print(str);
                regs->eax = 0;   // Success
            }
            break;
        }
        
        case SYS_READ: {
            char* buffer = (char*)regs->ebx;
            uint32_t max_len = regs->ecx;
            
            if (buffer == NULL || !syscall_verify_pointer(buffer, max_len)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_READ!\n");
                regs->eax = -1;
            } else {
                uint32_t count = 0;
                while (count < max_len - 1) {
                    char c = keyboard_getchar();
                    if (c == '\b') {
                        if (count > 0) {
                            count--;
                            // Backspace echo (erase char visually)
                            print_string_default("\b \b");
                        }
                        continue;
                    }
                    buffer[count++] = c;
                    print_char_default(c); // Echo keypress
                    if (c == '\n') break;
                }
                buffer[count] = '\0';
                regs->eax = count;
            }
            break;
        }
        
        case SYS_SLEEP: {
            uint32_t ticks = regs->ebx;
            timer_wait(ticks);
            regs->eax = 0;
            break;
        }
        
        case SYS_EXIT: {
            print_string_default("\n[USER PROCESS] Exit syscall invoked. Halting user task.\n");
            regs->eax = 0;
            // Loop user thread indefinitely
            while (1) {
                __asm__ volatile("hlt");
            }
            break;
        }

        case SYS_SET_COLOR: {
            uint32_t fg = regs->ebx;
            uint32_t bg = regs->ecx;
            graphics_set_default_colors(fg, bg);
            regs->eax = 0;
            break;
        }

        case SYS_LIST_FILES: {
            zenithfs_list_directory();
            regs->eax = 0;
            break;
        }

        case SYS_EXEC: {
            const char* filename = (const char*)regs->ebx;
            char local_filename[256];
            uint32_t len = 0;
            if (filename != NULL) {
                // Copy character-by-character immediately to prevent TOCTOU/double-fetch
                while (filename[len] != '\0' && len < 255) {
                    local_filename[len] = filename[len];
                    len++;
                }
            }
            local_filename[len] = '\0';
            
            if (filename == NULL || !syscall_verify_pointer(filename, len + 1)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_EXEC!\n");
                regs->eax = -1;
            } else {
                // Buffer at 8MB is safe (below PMM start at 16MB)
                uint8_t* exec_buf = (uint8_t*)0x800000;
                int32_t size = zenithfs_read_file(local_filename, exec_buf);

                if (size <= 0) {
                    regs->eax = -1; // File not found
                } else {
                    Task* cur = get_current_task();
                    
                    // Create private page directory if the task currently shares the kernel directory
                    if (cur->cr3 == vmm_get_kernel_page_dir()) {
                        cur->cr3 = vmm_create_page_dir();
                    }
                    
                    // Clear the old user space pages/tables to prevent leaks
                    vmm_clear_user_space((uint32_t*)cur->cr3);
                    
                    // Map new program pages in the process's page directory (with 16KB extra for BSS/padding safety)
                    uint32_t mem_size = (uint32_t)size + 16384;
                    for (uint32_t addr = 0x40000000; addr < 0x40000000 + mem_size; addr += 4096) {
                        vmm_map_page_in_dir((uint32_t*)cur->cr3, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                    }
                    
                    // Map a private user stack page range (from 0x400F8000 to 0x40100000, 32KB = 8 pages)
                    for (uint32_t addr = 0x400F8000; addr < 0x40100000; addr += 4096) {
                        vmm_map_page_in_dir((uint32_t*)cur->cr3, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                    }

                    // Switch CPU's CR3 register to the process directory context
                    __asm__ volatile("mov %0, %%cr3" : : "r"(cur->cr3));

                    // Copy code into mapped user virtual space 0x40000000
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

                    // Log execution event to serial port
                    serial_print("  [+] SYS_EXEC: Launched ");
                    serial_print(local_filename);
                    serial_print(" (File size: ");
                    serial_print_hex(size);
                    serial_print(" bytes, Memory mapped: ");
                    serial_print_hex(mem_size);
                    serial_print(" bytes)\n");
                    
                    // Drop privileges: any spawned program drops from Root (0) to User (1000)
                    cur->uid = 1000;
                    
                    // Reset registers to execution entry point (0x40000000) and stack top (0x40100000)
                    regs->eip = 0x40000000;
                    regs->useresp = 0x40100000;
                    regs->eax = 0; // Success
                }
            }
            break;
        }

        case SYS_READ_FILE: {
            const char* filename = (const char*)regs->ebx;
            uint8_t* buffer = (uint8_t*)regs->ecx;
            
            char local_filename[256];
            uint32_t len = 0;
            if (filename != NULL) {
                while (filename[len] != '\0' && len < 255) {
                    local_filename[len] = filename[len];
                    len++;
                }
            }
            local_filename[len] = '\0';
            
            if (filename == NULL || !syscall_verify_pointer(filename, len + 1) || buffer == NULL) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_READ_FILE!\n");
                regs->eax = -1;
            } else {
                int32_t file_size = zenithfs_get_file_size(local_filename);
                if (file_size < 0) {
                    regs->eax = -1; // File not found
                } else if (!syscall_verify_pointer(buffer, file_size)) {
                    print_string_default("\n[SYSCALL ERROR] Access Violation: Target buffer bounds exceed User Space!\n");
                    regs->eax = -1;
                } else {
                    int32_t size = zenithfs_read_file(local_filename, buffer);
                    regs->eax = size;
                }
            }
            break;
        }
        
        case SYS_CLEAR: {
            graphics_clear_console();
            regs->eax = 0;
            break;
        }


        case SYS_GETCHAR: {
            uint32_t non_blocking = regs->ebx;
            if (non_blocking == 1) {
                if (keyboard_haschar()) {
                    regs->eax = (uint32_t)keyboard_getchar();
                } else {
                    regs->eax = 0;
                }
            } else {
                regs->eax = (uint32_t)keyboard_getchar();
            }
            break;
        }

        case SYS_SET_CURSOR: {
            uint32_t col = regs->ebx;
            uint32_t row = regs->ecx;
            graphics_set_cursor(col, row);
            regs->eax = 0;
            break;
        }

        case SYS_UPTIME: {
            regs->eax = get_ticks();
            break;
        }

        case SYS_SHUTDOWN: {
            if (get_current_task()->uid != 0) {
                print_string_default("\n[SECURITY ERROR] Access Denied: Privilege SYS_SHUTDOWN required!\n");
                regs->eax = -1;
            } else {
                graphics_draw_shutdown();
                regs->eax = 0;
            }
            break;
        }
 
        case SYS_REBOOT: {
            if (get_current_task()->uid != 0) {
                print_string_default("\n[SECURITY ERROR] Access Denied: Privilege SYS_REBOOT required!\n");
                regs->eax = -1;
            } else {
                graphics_draw_restart();
                regs->eax = 0;
            }
            break;
        }


        default:
            print_string_default("\n[SYSCALL ERROR] Unknown syscall number invoked!\n");
            regs->eax = -1;
            break;
    }

    // Disable interrupts again before returning to the assembly ISR common stub
    __asm__ volatile("cli");
}

void syscall_init(void) {
    // Register the handler for interrupt 128 (0x80)
    register_interrupt_handler(128, syscall_handler);
}

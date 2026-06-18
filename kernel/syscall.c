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

bool syscall_verify_string(const char* str, uint32_t max_len) {
    if (str == NULL) return false;
    uint32_t addr = (uint32_t)str;
    for (uint32_t len = 0; len < max_len; len++) {
        if (addr < 0x40000000 || addr >= 0x48000000) return false;
        if (*(const char*)addr == '\0') return true;
        addr++;
    }
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
            if (!syscall_verify_string(str, 4096)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_WRITE!\n");
                regs->eax = -1;
            } else {
                print_string_default(str);
                serial_print(str);
                graphics_swap_buffers();
                regs->eax = 0;
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
                            graphics_swap_buffers();
                        }
                        continue;
                    }
                    buffer[count++] = c;
                    print_char_default(c); // Echo keypress
                    graphics_swap_buffers();
                    if (c == '\n') break;
                }
                buffer[count] = '\0';
                regs->eax = count;
            }
            break;
        }
        
        case SYS_SLEEP: {
            uint32_t ticks = regs->ebx;
            Task* cur = get_current_task();
            cur->state = TASK_SLEEPING;
            cur->sleep_ticks = ticks;
            regs->eax = 0;
            scheduler_yield();
            break;
        }
        
        case SYS_EXIT: {
            Task* cur = get_current_task();
            
            // Reclaim child task's page directory mappings
            uint32_t* child_dir = (uint32_t*)cur->cr3;
            if (child_dir != (uint32_t*)vmm_get_kernel_page_dir()) {
                // Switch CPU's CR3 back to kernel directory context
                __asm__ volatile("mov %0, %%cr3" : : "r"(vmm_get_kernel_page_dir()));
                
                // Clear user pages and free directory frame
                vmm_clear_user_space(child_dir);
                pmm_free_frame((uint32_t)child_dir);
            }
            
            // Mark task as dead
            cur->state = TASK_DEAD;
            cur->exit_code = (int)regs->ebx; // Store exit code from ebx
            
            // Unblock parent task
            if (cur->parent != NULL) {
                cur->parent->state = TASK_READY;
            }
            
            // Yield CPU context to switch away permanently
            scheduler_yield();
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
            if (!syscall_verify_string((const char*)regs->ebx, 512)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_EXEC!\n");
                regs->eax = -1;
            } else {
                char local_args[512];
                uint32_t len = 0;
                const char* user_args = (const char*)regs->ebx;
                while (user_args[len] != '\0' && len < 511) {
                    local_args[len] = user_args[len];
                    len++;
                }
                local_args[len] = '\0';

                char* argv_strings[32];
                uint32_t argc = 0;
                char* ptr = local_args;
                while (*ptr != '\0' && argc < 32) {
                    while (*ptr == ' ') ptr++;
                    if (*ptr == '\0') break;
                    argv_strings[argc++] = ptr;
                    while (*ptr != '\0' && *ptr != ' ') ptr++;
                    if (*ptr == ' ') {
                        *ptr = '\0';
                        ptr++;
                    }
                }

                if (argc == 0) {
                    regs->eax = -1;
                } else {
                    uint8_t* exec_buf = (uint8_t*)0x800000;
                    int32_t size = zenithfs_read_file(argv_strings[0], exec_buf);

                    if (size <= 0) {
                        regs->eax = -1;
                    } else {
                        Task* cur = get_current_task();

                        extern void user_entry_wrapper(void);
                        Task* child = task_create(user_entry_wrapper, 0);
                        child->parent = cur;
                        child->uid = 1000;

                        uint32_t* child_dir = (uint32_t*)vmm_create_page_dir();
                        child->cr3 = (uint32_t)child_dir;

                        uint32_t mem_size = (uint32_t)size + 16384;
                        for (uint32_t addr = 0x40000000; addr < 0x40000000 + mem_size; addr += 4096) {
                            vmm_map_page_in_dir(child_dir, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                        }

                        for (uint32_t addr = 0x400F8000; addr < 0x40100000; addr += 4096) {
                            vmm_map_page_in_dir(child_dir, addr, pmm_alloc_frame(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                        }

                        uint32_t parent_cr3;
                        __asm__ volatile("mov %%cr3, %0" : "=r"(parent_cr3));
                        __asm__ volatile("mov %0, %%cr3" : : "r"(child_dir));

                        for (int i = 0; i < size; i++) {
                            ((uint8_t*)0x40000000)[i] = exec_buf[i];
                        }
                        for (uint32_t i = size; i < mem_size; i++) {
                            ((uint8_t*)0x40000000)[i] = 0;
                        }
                        for (uint32_t i = 0; i < 32768; i++) {
                            ((uint8_t*)0x400F8000)[i] = 0;
                        }

                        uint32_t esp = 0x40100000;
                        uint32_t argv_ptrs[32];
                        for (int idx = (int)argc - 1; idx >= 0; idx--) {
                            uint32_t arg_len = 0;
                            while (argv_strings[idx][arg_len] != '\0') arg_len++;
                            esp -= (arg_len + 1);
                            char* dest = (char*)esp;
                            for (uint32_t k = 0; k <= arg_len; k++) {
                                dest[k] = argv_strings[idx][k];
                            }
                            argv_ptrs[idx] = esp;
                        }

                        esp &= ~3;

                        esp -= (argc + 1) * sizeof(char*);
                        uint32_t* child_argv_array = (uint32_t*)esp;
                        for (uint32_t idx = 0; idx < argc; idx++) {
                            child_argv_array[idx] = argv_ptrs[idx];
                        }
                        child_argv_array[argc] = 0;

                        uint32_t argv_val = esp;

                        esp -= 4;
                        *(uint32_t*)esp = argv_val;

                        esp -= 4;
                        *(uint32_t*)esp = argc;

                        esp -= 4;
                        *(uint32_t*)esp = 0;

                        child->user_esp = esp;

                        __asm__ volatile("mov %0, %%cr3" : : "r"(parent_cr3));

                        serial_print("  [+] SYS_EXEC: Spawned child ");
                        serial_print(argv_strings[0]);
                        serial_print(" (File size: ");
                        serial_print_hex(size);
                        serial_print(" bytes)\n");

                        cur->state = TASK_BLOCKED;
                        child->state = TASK_READY;

                        regs->eax = 0;
                        scheduler_yield();
                    }
                }
            }
            break;
        }

        case SYS_READ_FILE: {
            const char* filename = (const char*)regs->ebx;
            uint8_t* buffer = (uint8_t*)regs->ecx;
            
            if (!syscall_verify_string(filename, 256) || buffer == NULL) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_READ_FILE!\n");
                regs->eax = -1;
            } else {
                char local_filename[256];
                uint32_t len = 0;
                while (filename[len] != '\0' && len < 255) {
                    local_filename[len] = filename[len];
                    len++;
                }
                local_filename[len] = '\0';
                
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
            graphics_swap_buffers();
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
            graphics_swap_buffers();
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
 
        case SYS_GET_TASKS: {
            TaskInfo* user_buf = (TaskInfo*)regs->ebx;
            uint32_t max_tasks = regs->ecx;
            
            if (user_buf == NULL || !syscall_verify_pointer(user_buf, max_tasks * sizeof(TaskInfo))) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_GET_TASKS!\n");
                regs->eax = -1;
            } else {
                Task* head = get_task_list_head();
                uint32_t count = 0;
                if (head != NULL) {
                    Task* curr = head;
                    do {
                        if (count >= max_tasks) break;
                        
                        TaskInfo info;
                        info.id = curr->id;
                        info.state = curr->state;
                        
                        // Calculate memory size
                        extern uint32_t vmm_get_user_mapped_memory_kb(uint32_t* dir);
                        info.mem_size_kb = vmm_get_user_mapped_memory_kb((uint32_t*)curr->cr3);
                        
                        // Uptime calculation
                        extern uint32_t get_ticks(void);
                        info.uptime = get_ticks() - curr->start_tick;
                        
                        // Copy to user space buffer
                        user_buf[count] = info;
                        count++;
                        
                        curr = curr->next;
                    } while (curr != head);
                }
                regs->eax = count;
            }
            break;
        }

        case SYS_WRITE_FILE: {
            const char* filename = (const char*)regs->ebx;
            const uint8_t* buffer = (const uint8_t*)regs->ecx;
            uint32_t size = regs->edx;
            
            if (!syscall_verify_string(filename, 256) || buffer == NULL || !syscall_verify_pointer(buffer, size)) {
                print_string_default("\n[SYSCALL ERROR] Access Violation: Invalid pointer passed to SYS_WRITE_FILE!\n");
                regs->eax = -1;
            } else {
                char local_filename[256];
                uint32_t len = 0;
                while (filename[len] != '\0' && len < 255) {
                    local_filename[len] = filename[len];
                    len++;
                }
                local_filename[len] = '\0';
                
                extern int32_t zenithfs_write_file(const char* filename, const uint8_t* buffer, uint32_t size);
                regs->eax = zenithfs_write_file(local_filename, buffer, size);
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

        case SYS_SWAP_BUFFERS: {
            graphics_swap_buffers();
            regs->eax = 0;
            break;
        }

        case SYS_SWIPE_TRANSITION: {
            graphics_swipe_transition();
            regs->eax = 0;
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

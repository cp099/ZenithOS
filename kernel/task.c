#include "task.h"
#include "heap.h"
#include "paging.h"

extern void switch_context(uint32_t* old_esp, uint32_t new_esp);

static Task* current_task = NULL;
static Task* task_list_head = NULL;
static uint32_t next_task_id = 0;

static void task_exit(void) {
    // Mark current task as dead
    current_task->state = TASK_DEAD;
    // Yield to let another task run
    scheduler_yield();
    // Should never reach here
    while (1) { __asm__ volatile("hlt"); }
}

static void task_wrapper(void (*entry)(void)) {
    // Enable interrupts when starting a new task
    __asm__ volatile("sti");
    entry();
    task_exit();
}

void scheduler_init(void) {
    // Allocate space for the initial kernel/boot task
    Task* boot_task = (Task*)kmalloc(sizeof(Task));
    boot_task->id = next_task_id++;
    boot_task->esp = 0; // Will be set when switching out of it
    boot_task->kstack = 0x90000; // Boot stack top
    boot_task->cr3 = vmm_get_kernel_page_dir(); // Default kernel directory
    boot_task->uid = 0; // Root user
    boot_task->state = TASK_RUNNING;
    boot_task->sleep_ticks = 0;
    boot_task->next = boot_task; // Circular list
    
    current_task = boot_task;
    task_list_head = boot_task;
}

Task* task_create(void (*entry)(void), uint32_t flags) {
    (void)flags;
    
    Task* new_task = (Task*)kmalloc(sizeof(Task));
    new_task->id = next_task_id++;
    
    // Allocate a 4KB stack
    uint32_t stack_size = 4096;
    void* stack_mem = kmalloc(stack_size);
    new_task->kstack = (uint32_t)stack_mem + stack_size;
    new_task->cr3 = vmm_get_kernel_page_dir(); // Default kernel directory
    new_task->uid = 0; // Kernel thread / Root user by default
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;
    
    // Set up initial stack frame
    uint32_t* stack = (uint32_t*)new_task->kstack;
    
    // Parameter for task_wrapper: entry point
    *(--stack) = (uint32_t)entry;
    // Return address for task_wrapper: task_exit (fallback)
    *(--stack) = (uint32_t)task_exit;
    // Address returned to by switch_context (via ret)
    *(--stack) = (uint32_t)task_wrapper;
    
    // Registers popped by switch_context
    *(--stack) = 0; // ebp
    *(--stack) = 0; // ebx
    *(--stack) = 0; // esi
    *(--stack) = 0; // edi
    
    new_task->esp = (uint32_t)stack;
    
    // Add to task circular list
    new_task->next = current_task->next;
    current_task->next = new_task;
    
    return new_task;
}

void scheduler_yield(void) {
    // Disable interrupts during task selection
    __asm__ volatile("cli");
    
    Task* old_task = current_task;
    Task* next_task = old_task->next;
    
    // Find next ready or running task
    while (next_task->state != TASK_READY && next_task->state != TASK_RUNNING) {
        if (next_task == old_task) {
            // No other ready tasks!
            if (old_task->state == TASK_RUNNING) {
                // Keep running the current task
                __asm__ volatile("sti");
                return;
            } else {
                // System is idle! Just wait for interrupts
                __asm__ volatile("sti\n\t"
                                 "hlt\n\t"
                                 "cli");
            }
        }
        next_task = next_task->next;
    }
    
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;
    current_task = next_task;
    
    // Swap CR3 to restore private page directory context if different
    if (next_task->cr3 != 0) {
        uint32_t current_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        if (current_cr3 != next_task->cr3) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(next_task->cr3));
        }
    }

    // Perform context switch
    switch_context(&old_task->esp, next_task->esp);
    
    // Re-enable interrupts after context switch
    __asm__ volatile("sti");
}

void scheduler_tick(void) {
    // Decrement sleep counters
    Task* curr = task_list_head;
    do {
        if (curr->state == TASK_SLEEPING) {
            if (curr->sleep_ticks > 0) {
                curr->sleep_ticks--;
            }
            if (curr->sleep_ticks == 0) {
                curr->state = TASK_READY;
            }
        }
        curr = curr->next;
    } while (curr != task_list_head);
}

Task* get_current_task(void) {
    return current_task;
}

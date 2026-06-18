#include "task.h"
#include "heap.h"
#include "paging.h"

extern void switch_context(uint32_t* old_esp, uint32_t new_esp);
extern void set_kernel_stack(uint32_t stack_phys);

static Task* current_task = NULL;
static Task* task_list_head = NULL;
static uint32_t next_task_id = 0;
static Task* task_to_reap = NULL;

void reap_dead_tasks(void) {
    if (task_to_reap != NULL) {
        if (task_to_reap->kstack != 0 && task_to_reap->id != 0) {
            kfree((void*)(task_to_reap->kstack - 4096));
        }
        kfree(task_to_reap);
        task_to_reap = NULL;
    }
}

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
    
    // Reap any pending dead tasks since this is a new execution path
    reap_dead_tasks();
    
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
    boot_task->parent = NULL;
    boot_task->exit_code = 0;
    boot_task->user_esp = 0;
    boot_task->start_tick = 0;
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
    new_task->parent = NULL;
    new_task->exit_code = 0;
    new_task->user_esp = 0;
    extern uint32_t get_ticks(void);
    new_task->start_tick = get_ticks();
    
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
    
    // If old_task is dead, mark it to be reaped and remove it from the circular list
    if (old_task->state == TASK_DEAD) {
        task_to_reap = old_task;
        
        // Remove from circular list
        Task* prev = task_list_head;
        while (prev->next != old_task) {
            prev = prev->next;
        }
        prev->next = old_task->next;
        
        if (task_list_head == old_task) {
            if (old_task->next == old_task) {
                task_list_head = NULL;
            } else {
                task_list_head = old_task->next;
            }
        }
    }
    
    // Count active tasks to avoid infinite loops if none are ready/running
    uint32_t active_count = 0;
    if (task_list_head != NULL) {
        Task* t = task_list_head;
        do {
            active_count++;
            t = t->next;
        } while (t != task_list_head);
    }
    
    Task* next_task = old_task->next;
    uint32_t checked = 0;
    
    // Find next ready or running task
    while (next_task->state != TASK_READY && next_task->state != TASK_RUNNING) {
        checked++;
        if (checked > active_count) {
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
                checked = 0; // Reset checks
            }
        }
        next_task = next_task->next;
    }
    
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;
    current_task = next_task;
    
    // Update the GDT TSS ESP0 with next task's kernel stack bottom
    set_kernel_stack(next_task->kstack);
    
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
    
    // Reap any dead tasks since we context switched back to this task
    reap_dead_tasks();
    
    // Re-enable interrupts after context switch
    __asm__ volatile("sti");
}

void scheduler_tick(void) {
    // Decrement sleep counters
    if (task_list_head == NULL) return;
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

Task* get_task_list_head(void) {
    return task_list_head;
}

void task_wake_keyboard_waiters(void) {
    if (task_list_head == NULL) return;
    Task* curr = task_list_head;
    do {
        if (curr->state == TASK_BLOCKED_INPUT) {
            curr->state = TASK_READY;
        }
        curr = curr->next;
    } while (curr != task_list_head);
}

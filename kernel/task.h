#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_SLEEPING 2
#define TASK_DEAD     3

typedef struct Task {
    uint32_t id;
    uint32_t esp;
    uint32_t kstack;
    uint32_t cr3;
    uint32_t uid;
    uint32_t state;
    uint32_t sleep_ticks;
    struct Task* next;
} Task;

void scheduler_init(void);
Task* task_create(void (*entry)(void), uint32_t flags);
void scheduler_yield(void);
void scheduler_tick(void);
Task* get_current_task(void);

#endif

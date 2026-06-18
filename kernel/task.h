#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#define TASK_READY         0
#define TASK_RUNNING       1
#define TASK_SLEEPING      2
#define TASK_DEAD          3
#define TASK_BLOCKED       4
#define TASK_BLOCKED_INPUT 5

struct fs_node;

typedef struct Task {
    uint32_t id;
    uint32_t esp;
    uint32_t kstack;
    uint32_t cr3;
    uint32_t uid;
    uint32_t state;
    uint32_t sleep_ticks;
    struct Task* parent;
    int exit_code;
    uint32_t user_esp;
    uint32_t start_tick;
    struct fs_node* fd_table[16];
    struct Task* next;
} Task;

typedef struct TaskInfo {
    uint32_t id;
    uint32_t state;
    uint32_t mem_size_kb;
    uint32_t uptime;
} TaskInfo;

void scheduler_init(void);
Task* task_create(void (*entry)(void), uint32_t flags);
void scheduler_yield(void);
void scheduler_tick(void);
Task* get_current_task(void);
Task* get_task_list_head(void);
void task_wake_keyboard_waiters(void);
void task_terminate(struct Task* task, int exit_code);

#endif

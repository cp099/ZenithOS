#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --------------------------------------------------------------------------
// ZenithOS System Call Wrappers
// --------------------------------------------------------------------------

// Print a string to the console
void print(const char* str);

// Read keyboard input into buffer (returns number of characters read)
int read(char* buf, int max_len);

// Suspend program for specified timer ticks
void sleep(int ticks);

// Exit program execution and return to kernel
void exit(void);

// Set default console text color (foreground, background)
void set_color(uint32_t fg, uint32_t bg);

// List files inside root directory
void list_files(void);

// Replace current process memory image and execute target file from disk
int exec(const char* filename);

// Read file contents from ZenithFS into buffer
int read_file(const char* filename, char* buffer);

// Clear the console screen
void clear_screen(void);

// Standard input/output and utility extensions
char getchar(void);
char getchar_nonblock(void);
void set_cursor(int col, int row);
int uptime(void);
void shutdown(void);
void restart(void);
void puts(const char* str);
void putchar(char c);
void clear(void);
char* itoa(int val, int base);
int input(char* buf, int len);

// Task states
#define TASK_READY         0
#define TASK_RUNNING       1
#define TASK_SLEEPING      2
#define TASK_DEAD          3
#define TASK_BLOCKED       4
#define TASK_BLOCKED_INPUT 5

// Task structure copied from kernel
typedef struct TaskInfo {
    uint32_t id;
    uint32_t state;
    uint32_t mem_size_kb;
    uint32_t uptime;
} TaskInfo;

int get_tasks(TaskInfo* buf, int max_tasks);
int write_file(const char* filename, const char* buffer, int size);
void swap_buffers(void);
void swipe_transition(void);



// --------------------------------------------------------------------------
// Standard Utility Functions
// --------------------------------------------------------------------------
int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);
char* strtok(char* str, const char* delim);

#endif

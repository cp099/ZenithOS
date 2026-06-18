#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

// Syscall index definitions
#define SYS_WRITE   0
#define SYS_READ    1
#define SYS_SLEEP   2
#define SYS_EXIT    3
#define SYS_SET_COLOR 4
#define SYS_LIST_FILES 5
#define SYS_EXEC    6
#define SYS_READ_FILE 7
#define SYS_CLEAR   8
#define SYS_GETCHAR   9
#define SYS_SET_CURSOR 10
#define SYS_UPTIME    11
#define SYS_SHUTDOWN  12
#define SYS_GET_TASKS 13
#define SYS_REBOOT    14
#define SYS_WRITE_FILE 15
#define SYS_SWAP_BUFFERS 16
#define SYS_SWIPE_TRANSITION 17


// Initialize syscall gate (registers interrupt 0x80 handler)
void syscall_init(void);

// Safe pointer verification routine for Ring 3 buffers
bool syscall_verify_pointer(const void* ptr, uint32_t size);
bool syscall_verify_string(const char* str, uint32_t max_len);

#endif

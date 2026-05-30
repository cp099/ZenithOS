#include "libc.h"

// --------------------------------------------------------------------------
// System Call Assembler Wrappers
// --------------------------------------------------------------------------

void print(const char* str) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(0), "b"(str)
    );
}

int read(char* buf, int max_len) {
    int count;
    __asm__ volatile(
        "int $0x80"
        : "=a"(count)
        : "a"(1), "b"(buf), "c"(max_len)
    );
    return count;
}

void sleep(int ticks) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(2), "b"(ticks)
    );
}

void exit(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(3)
    );
}

void set_color(uint32_t fg, uint32_t bg) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(4), "b"(fg), "c"(bg)
    );
}

void list_files(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(5)
    );
}

int exec(const char* filename) {
    int result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(6), "b"(filename)
    );
    return result;
}

int read_file(const char* filename, char* buffer) {
    int result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(7), "b"(filename), "c"(buffer)
    );
    return result;
}

void clear_screen(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(8)
    );
}

// --------------------------------------------------------------------------
// Standard Utility Implementations
// --------------------------------------------------------------------------

int strlen(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void* memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

// String tokenizer (strtok) using a static token pointer
static char* strtok_saved_str = NULL;

char* strtok(char* str, const char* delim) {
    if (str != NULL) {
        strtok_saved_str = str;
    }
    if (strtok_saved_str == NULL) {
        return NULL;
    }
    
    // Find start of token (skip leading delimiters)
    char* token_start = strtok_saved_str;
    while (*token_start != '\0') {
        bool is_delim = false;
        for (int i = 0; delim[i] != '\0'; i++) {
            if (*token_start == delim[i]) {
                is_delim = true;
                break;
            }
        }
        if (!is_delim) {
            break;
        }
        token_start++;
    }
    
    if (*token_start == '\0') {
        strtok_saved_str = NULL;
        return NULL;
    }
    
    // Find end of token
    char* token_end = token_start;
    while (*token_end != '\0') {
        bool is_delim = false;
        for (int i = 0; delim[i] != '\0'; i++) {
            if (*token_end == delim[i]) {
                is_delim = true;
                break;
            }
        }
        if (is_delim) {
            *token_end = '\0';
            strtok_saved_str = token_end + 1;
            return token_start;
        }
        token_end++;
    }
    
    strtok_saved_str = NULL;
    return token_start;
}

char getchar(void) {
    uint32_t c;
    __asm__ volatile(
        "int $0x80"
        : "=a"(c)
        : "a"(9), "b"(0)
    );
    return (char)c;
}

char getchar_nonblock(void) {
    uint32_t c;
    __asm__ volatile(
        "int $0x80"
        : "=a"(c)
        : "a"(9), "b"(1)
    );
    return (char)c;
}

void set_cursor(int col, int row) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(10), "b"(col), "c"(row)
    );
}

int uptime(void) {
    int ticks;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ticks)
        : "a"(11)
    );
    return ticks;
}

void shutdown(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(12)
    );
}

void restart(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(14)
    );
}

int get_tasks(TaskInfo* buf, int max_tasks) {
    int count;
    __asm__ volatile(
        "int $0x80"
        : "=a"(count)
        : "a"(13), "b"(buf), "c"(max_tasks)
    );
    return count;
}

void puts(const char* str) {
    print(str);
}

void putchar(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    print(buf);
}

void clear(void) {
    clear_screen();
}

int input(char* buf, int len) {
    return read(buf, len);
}

char* itoa(int val, int base) {

    static char buf[32];
    int i = 30;
    buf[31] = '\0';
    
    bool negative = false;
    if (val < 0 && base == 10) {
        negative = true;
        val = -val;
    }
    
    unsigned int uval = (unsigned int)val;
    if (uval == 0) {
        buf[i--] = '0';
    } else {
        while (uval > 0) {
            unsigned int rem = uval % base;
            buf[i--] = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
            uval /= base;
        }
    }
    
    if (negative) {
        buf[i--] = '-';
    }
    
    return &buf[i + 1];
}

// Userland stack protection symbols
uint32_t __stack_chk_guard = 0x5ECA1DAE;

void __attribute__((noreturn)) __stack_chk_fail(void) {
    print("\n[USER PROCESS SECURITY ALERT] Stack Smashing/Overflow Detected! Terminating process.\n");
    // Call SYS_EXIT (syscall index 3)
    __asm__ volatile(
        "mov $3, %%eax\n\t"
        "int $0x80"
        :
        :
        : "eax"
    );
    while (1) {
        __asm__ volatile("hlt");
    }
}



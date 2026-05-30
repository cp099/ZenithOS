#include "libc.h"

// --------------------------------------------------------------------------
// System Call Assembler Wrappers
// --------------------------------------------------------------------------

void print(const char* str) {
    __asm__ volatile(
        "mov $0, %%eax\n\t"     // SYS_WRITE = 0
        "mov %0, %%ebx\n\t"     // string pointer
        "int $0x80\n\t"
        :
        : "r"(str)
        : "eax", "ebx"
    );
}

int read(char* buf, int max_len) {
    int count;
    __asm__ volatile(
        "mov $1, %%eax\n\t"     // SYS_READ = 1
        "mov %1, %%ebx\n\t"     // buffer pointer
        "mov %2, %%ecx\n\t"     // max length
        "int $0x80\n\t"
        "mov %%eax, %0\n\t"     // save count
        : "=r"(count)
        : "r"(buf), "r"(max_len)
        : "eax", "ebx", "ecx"
    );
    return count;
}

void sleep(int ticks) {
    __asm__ volatile(
        "mov $2, %%eax\n\t"     // SYS_SLEEP = 2
        "mov %0, %%ebx\n\t"     // ticks
        "int $0x80\n\t"
        :
        : "r"(ticks)
        : "eax", "ebx"
    );
}

void exit(void) {
    __asm__ volatile(
        "mov $3, %%eax\n\t"     // SYS_EXIT = 3
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

void set_color(uint32_t fg, uint32_t bg) {
    __asm__ volatile(
        "mov $4, %%eax\n\t"     // SYS_SET_COLOR = 4
        "mov %0, %%ebx\n\t"     // foreground
        "mov %1, %%ecx\n\t"     // background
        "int $0x80\n\t"
        :
        : "r"(fg), "r"(bg)
        : "eax", "ebx", "ecx"
    );
}

void list_files(void) {
    __asm__ volatile(
        "mov $5, %%eax\n\t"     // SYS_LIST_FILES = 5
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

int exec(const char* filename) {
    int result;
    __asm__ volatile(
        "mov $6, %%eax\n\t"     // SYS_EXEC = 6
        "mov %1, %%ebx\n\t"     // filename
        "int $0x80\n\t"
        "mov %%eax, %0\n\t"
        : "=r"(result)
        : "r"(filename)
        : "eax", "ebx"
    );
    return result;
}

int read_file(const char* filename, char* buffer) {
    int result;
    __asm__ volatile(
        "mov $7, %%eax\n\t"     // SYS_READ_FILE = 7
        "mov %1, %%ebx\n\t"     // filename
        "mov %2, %%ecx\n\t"     // buffer
        "int $0x80\n\t"
        "mov %%eax, %0\n\t"
        : "=r"(result)
        : "r"(filename), "r"(buffer)
        : "eax", "ebx", "ecx"
    );
    return result;
}

void clear_screen(void) {
    __asm__ volatile(
        "mov $8, %%eax\n\t"     // SYS_CLEAR = 8
        "int $0x80\n\t"
        :
        :
        : "eax"
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
    char c;
    __asm__ volatile(
        "mov $9, %%eax\n\t"
        "mov $0, %%ebx\n\t"
        "int $0x80\n\t"
        "mov %%al, %0\n\t"
        : "=r"(c)
        :
        : "eax", "ebx"
    );
    return c;
}

char getchar_nonblock(void) {
    char c;
    __asm__ volatile(
        "mov $9, %%eax\n\t"
        "mov $1, %%ebx\n\t"
        "int $0x80\n\t"
        "mov %%al, %0\n\t"
        : "=r"(c)
        :
        : "eax", "ebx"
    );
    return c;
}

void set_cursor(int col, int row) {
    __asm__ volatile(
        "mov $10, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "mov %1, %%ecx\n\t"
        "int $0x80\n\t"
        :
        : "r"(col), "r"(row)
        : "eax", "ebx", "ecx"
    );
}

int uptime(void) {
    int ticks;
    __asm__ volatile(
        "mov $11, %%eax\n\t"
        "int $0x80\n\t"
        "mov %%eax, %0\n\t"
        : "=r"(ticks)
        :
        : "eax"
    );
    return ticks;
}

void shutdown(void) {
    __asm__ volatile(
        "mov $12, %%eax\n\t"
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
}

void restart(void) {
    __asm__ volatile(
        "mov $13, %%eax\n\t"
        "int $0x80\n\t"
        :
        :
        : "eax"
    );
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


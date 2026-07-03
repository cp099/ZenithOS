#include "keyboard.h"
#include "idt.h"
#include "graphics.h"
#include "task.h"

// I/O Port reader helper
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

// Scancode translation tables (US English layout)
static const char kbd_us_lower[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', /* Backspace, Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  /* Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, /* Left Shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, /* Right Shift */
  '*',   0, /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1-F10 Keys */
    0,  /* Num Lock */
    0,  /* Scroll Lock */
    0,  /* Home Key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* End Key */
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0, 0, 0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys undefined */
};

static const char kbd_us_upper[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', /* Backspace, Tab */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  /* Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, /* Left Shift */
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, /* Right Shift */
  '*',   0, /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1-F10 Keys */
    0,  /* Num Lock */
    0,  /* Scroll Lock */
    0,  /* Home Key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* End Key */
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0, 0, 0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys undefined */
};

// Keyboard state tracking
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

// Simple Ring Buffer for keyboard input
#define BUFFER_SIZE 256
static char char_buffer[BUFFER_SIZE];
static uint32_t buffer_head = 0;
static uint32_t buffer_tail = 0;

static void buffer_write(char c) {
    uint32_t next = (buffer_head + 1) % BUFFER_SIZE;
    if (next != buffer_tail) {
        char_buffer[buffer_head] = c;
        buffer_head = next;
    }
}

static char buffer_read(void) {
    if (buffer_head == buffer_tail) return 0;
    char c = char_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    return c;
}

static bool buffer_has_data(void) {
    return buffer_head != buffer_tail;
}

extern void serial_print(const char* str);
extern void serial_print_hex(uint32_t val);

static bool extended_key = false;

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }

    // Check for key release (Break codes have bit 7 set)
    if (scancode & 0x80) {
        uint8_t key = scancode & 0x7F;
        if (key == 0x2A || key == 0x36) {
            shift_pressed = false;
        } else if (key == 0x1D) {
            ctrl_pressed = false;
        } else if (key == 0x38) {
            alt_pressed = false;
        }
        extended_key = false;
    } else {
        // Key press (Make codes)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
        } else if (scancode == 0x1D) {
            ctrl_pressed = true;
        } else if (scancode == 0x38) {
            alt_pressed = true;
        } else if (scancode == 0x3A) {
            caps_lock = !caps_lock; // Toggle Caps Lock
        } else {
            // Check for Ctrl+C
            if (ctrl_pressed && scancode == 0x2E) {
                Task* cur = get_current_task();
                if (cur != NULL && cur->uid != 0) {
                    cur->state = TASK_DEAD;
                    if (cur->parent != NULL) {
                        cur->parent->state = TASK_READY;
                    }
                    extended_key = false;
                    scheduler_yield();
                    return; // Should not reach here
                }
            }

            char c = 0;
            if (extended_key) {
                if (scancode == 0x4B) {       // Left Arrow
                    c = 1;
                } else if (scancode == 0x4D) { // Right Arrow
                    c = 2;
                } else if (scancode == 0x48) { // Up Arrow
                    c = 3;
                } else if (scancode == 0x50) { // Down Arrow
                    c = 4;
                }
                extended_key = false;
            } else {
                // Translate standard keypress using XOR for shift and caps lock
                bool upper = shift_pressed ^ caps_lock;
                if (scancode < 128) {
                    c = upper ? kbd_us_upper[scancode] : kbd_us_lower[scancode];
                }
            }

            if (c != 0) {
                buffer_write(c);
                task_wake_keyboard_waiters();
            }
        }
    }
}

void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;

    // Enable first PS/2 port (keyboard)
    while (inb(0x64) & 0x02); // Wait for PS/2 input buffer to be empty
    outb(0x64, 0xAE);

    // Drain keyboard buffer
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    // Register IRQ1 handler
    register_interrupt_handler(33, keyboard_callback);
}

bool keyboard_haschar(void) {
    return buffer_has_data();
}

char keyboard_getchar(void) {
    while (1) {
        __asm__ volatile("cli");
        if (keyboard_haschar()) {
            char c = buffer_read();
            __asm__ volatile("sti");
            return c;
        }
        Task* cur = get_current_task();
        cur->state = TASK_BLOCKED_INPUT;
        scheduler_yield(); // scheduler_yield will sti before returning
    }
}

void keyboard_inject_string(const char* str) {
    uint32_t eflags;
    __asm__ volatile("pushfl; pop %0; cli" : "=r"(eflags));
    for (int i = 0; str[i] != '\0'; i++) {
        buffer_write(str[i]);
    }
    extern void task_wake_keyboard_waiters(void);
    task_wake_keyboard_waiters();
    __asm__ volatile("push %0; popfl" : : "r"(eflags));
}

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

// Keyboard interrupt handler (IRQ1 / Interrupt 33)
static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

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
            // Translate standard keypress using XOR for shift and caps lock
            bool upper = shift_pressed ^ caps_lock;
            char c = upper ? kbd_us_upper[scancode] : kbd_us_lower[scancode];
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

    // Register IRQ1 handler
    register_interrupt_handler(33, keyboard_callback);
}

bool keyboard_haschar(void) {
    return buffer_has_data();
}

char keyboard_getchar(void) {
    while (!buffer_has_data()) {
        Task* cur = get_current_task();
        cur->state = TASK_BLOCKED_INPUT;
        scheduler_yield();
    }
    return buffer_read();
}

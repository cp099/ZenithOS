#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Initialize keyboard driver and register interrupt handler
void keyboard_init(void);

// Read a character from the keyboard buffer (blocking)
char keyboard_getchar(void);

// Check if a character is available in the keyboard buffer
bool keyboard_haschar(void);

#endif

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

#define CHAR_WIDTH    12
#define CHAR_HEIGHT   24

#define MARGIN_LEFT   48
#define MARGIN_TOP    48
#define MARGIN_RIGHT  48
#define MARGIN_BOTTOM 48

struct BootInfo {
    uint32_t framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} __attribute__((packed));

void graphics_init(void);
void graphics_set_default_colors(uint32_t fg, uint32_t bg);
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_circle(int xc, int yc, int r, uint32_t color);
void draw_filled_circle(int xc, int yc, int r, uint32_t color);
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void draw_rounded_rect_outline(int x, int y, int w, int h, int r, uint32_t color);
void graphics_clear(uint32_t color);
void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void print_string(const char* str, uint32_t fg, uint32_t bg);
void print_char(char c, uint32_t fg, uint32_t bg);
void print_string_default(const char* str);
void print_char_default(char c);

void graphics_set_cursor(uint32_t col, uint32_t row);
void graphics_draw_frame(void);
void graphics_clear_console(void);

void graphics_draw_splash(void);
void graphics_update_progress(const char* status, uint32_t percentage);
void graphics_draw_shutdown(void);
void graphics_draw_restart(void);
void graphics_draw_statusbar(void);

#endif



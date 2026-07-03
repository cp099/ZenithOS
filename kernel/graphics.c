#include "graphics.h"
#include "font.h"
#include "timer.h"
#include "task.h"
#include "heap.h"

static struct BootInfo boot_info;
static volatile uint8_t* fb = NULL;
static uint32_t terminal_col = 0;
static uint32_t terminal_row = 0;

static uint32_t backbuffer[1280 * 1024];


#define COLOR_DEFAULT_FG 0x002C2825 // Warm Charcoal/Ebony
#define COLOR_DEFAULT_BG 0x00FAF7F2 // Soft Warm Beige base

static uint32_t current_fg = COLOR_DEFAULT_FG;
static uint32_t current_bg = COLOR_DEFAULT_BG;

void graphics_set_default_colors(uint32_t fg, uint32_t bg) {
    current_fg = fg;
    current_bg = bg;
}

// Simple memory copy
static void* local_memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

void graphics_draw_gradient(uint32_t start_color, uint32_t end_color) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    uint32_t mid_color = 0xECE6DB; // Cozy warm sand middle stop
    
    uint8_t r1 = (start_color >> 16) & 0xFF;
    uint8_t g1 = (start_color >> 8) & 0xFF;
    uint8_t b1 = start_color & 0xFF;
    
    uint8_t r2 = (mid_color >> 16) & 0xFF;
    uint8_t g2 = (mid_color >> 8) & 0xFF;
    uint8_t b2 = mid_color & 0xFF;
    
    uint8_t r3 = (end_color >> 16) & 0xFF;
    uint8_t g3 = (end_color >> 8) & 0xFF;
    uint8_t b3 = end_color & 0xFF;
    
    for (uint32_t y = 0; y < height; y++) {
        uint8_t r, g, b;
        if (y < height / 2) {
            uint32_t dy = y * 2;
            r = r1 + ((int32_t)(r2 - r1) * (int32_t)dy) / (int32_t)height;
            g = g1 + ((int32_t)(g2 - g1) * (int32_t)dy) / (int32_t)height;
            b = b1 + ((int32_t)(b2 - b1) * (int32_t)dy) / (int32_t)height;
        } else {
            uint32_t dy = (y - height / 2) * 2;
            r = r2 + ((int32_t)(r3 - r2) * (int32_t)dy) / (int32_t)height;
            g = g2 + ((int32_t)(g3 - g2) * (int32_t)dy) / (int32_t)height;
            b = b2 + ((int32_t)(b3 - b2) * (int32_t)dy) / (int32_t)height;
        }
        uint32_t color = (r << 16) | (g << 8) | b;
        
        uint32_t* line_bb = &backbuffer[y * width];
        for (uint32_t x = 0; x < width; x++) {
            line_bb[x] = color;
        }
    }
}

void graphics_init(void) {
    // Copy the BootInfo block written by Stage 2 at 0x7000
    local_memcpy(&boot_info, (const void*)0x7000, sizeof(struct BootInfo));
    fb = (volatile uint8_t*)(uintptr_t)boot_info.framebuffer;
    
    // Show splash screen on startup
    graphics_draw_splash();
}


void graphics_draw_frame(void) {
    // 1. Draw modern light theme parchment wallpaper gradient
    graphics_draw_gradient(0xF4EFE6, 0xEAE3D8);
    
    // 2. Center console card container dimensions
    uint32_t container_x = MARGIN_LEFT - 8;
    uint32_t container_y = MARGIN_TOP - 8;
    uint32_t container_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT + 16;
    uint32_t container_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM + 16;
    
    // Draw layered Drop Shadows (soft warm sand ambient shadows)
    draw_rounded_rect(container_x + 12, container_y + 12, container_w, container_h, 12, 0xEAE3D8);
    draw_rounded_rect(container_x + 8, container_y + 8, container_w, container_h, 12, 0xD8CFC0);
    draw_rounded_rect(container_x + 4, container_y + 4, container_w, container_h, 12, 0xC4B5A3);
    
    // Draw main console workspace card panel (glassmorphic warm cream base)
    extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
    draw_rounded_rect_alpha(container_x, container_y, container_w, container_h, 12, 0xFFFFFF, 225);
    
    // Draw subtle border around the console workspace
    draw_rounded_rect_outline(container_x, container_y, container_w, container_h, 12, 0xC4B5A3);
}

void graphics_clear_console(void) {
    uint32_t work_x = MARGIN_LEFT;
    uint32_t work_y = MARGIN_TOP;
    uint32_t work_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT;
    uint32_t work_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM;
    draw_rect(work_x, work_y, work_w, work_h, current_bg);
    terminal_col = 0;
    terminal_row = 0;
    
    // Draw initial cursor at top left of workspace
    draw_rect(MARGIN_LEFT, MARGIN_TOP + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_fg);
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= boot_info.width || y >= boot_info.height) return;
    backbuffer[y * boot_info.width + x] = color;
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void draw_circle(int xc, int yc, int r, uint32_t color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        draw_pixel(xc + x, yc + y, color);
        draw_pixel(xc - x, yc + y, color);
        draw_pixel(xc + x, yc - y, color);
        draw_pixel(xc - x, yc - y, color);
        draw_pixel(xc + y, yc + x, color);
        draw_pixel(xc - y, yc + x, color);
        draw_pixel(xc + y, yc - x, color);
        draw_pixel(xc - y, yc - x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void draw_filled_circle(int xc, int yc, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                draw_pixel(xc + x, yc + y, color);
            }
        }
    }
}

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) {
        draw_rect(x, y, w, h, color);
        return;
    }
    if (2 * r > w) r = w / 2;
    if (2 * r > h) r = h / 2;

    draw_filled_circle(x + r, y + r, r, color);
    draw_filled_circle(x + w - r - 1, y + r, r, color);
    draw_filled_circle(x + r, y + h - r - 1, r, color);
    draw_filled_circle(x + w - r - 1, y + h - r - 1, r, color);

    draw_rect(x, y + r, w, h - 2 * r, color);
    draw_rect(x + r, y, w - 2 * r, r, color);
    draw_rect(x + r, y + h - r, w - 2 * r, r, color);
}

void draw_rounded_rect_outline(int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) {
        draw_rect(x, y, w, 1, color);
        draw_rect(x, y + h - 1, w, 1, color);
        draw_rect(x, y, 1, h, color);
        draw_rect(x + w - 1, y, 1, h, color);
        return;
    }
    if (2 * r > w) r = w / 2;
    if (2 * r > h) r = h / 2;

    draw_rect(x + r, y, w - 2 * r, 1, color);
    draw_rect(x + r, y + h - 1, w - 2 * r, 1, color);
    draw_rect(x, y + r, 1, h - 2 * r, color);
    draw_rect(x + w - 1, y + r, 1, h - 2 * r, color);

    for (int dy = 0; dy <= r; dy++) {
        for (int dx = 0; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 >= (r - 1) * (r - 1) && d2 <= r * r) {
                draw_pixel(x + r - dx, y + r - dy, color);
                draw_pixel(x + w - r - 1 + dx, y + r - dy, color);
                draw_pixel(x + r - dx, y + h - r - 1 + dy, color);
                draw_pixel(x + w - r - 1 + dx, y + h - r - 1 + dy, color);
            }
        }
    }
}

void graphics_clear(uint32_t color) {
    uint32_t total_pixels = boot_info.width * boot_info.height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        backbuffer[i] = color;
    }
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    for (uint32_t row_idx = 0; row_idx < 16; row_idx++) {
        uint8_t row_data = font_bitmap[(uint8_t)c][row_idx];
        for (uint32_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            int active = (row_data & (0x80 >> bit_idx)) != 0;
            if (active) {
                draw_pixel(x + bit_idx, y + row_idx, fg);
            } else if (bg != 0xFFFFFFFF) {
                draw_pixel(x + bit_idx, y + row_idx, bg);
            }
        }
    }
}

static void graphics_scroll(void) {
    uint32_t char_height = CHAR_HEIGHT;
    uint32_t width = boot_info.width;
    
    uint32_t work_x = MARGIN_LEFT;
    uint32_t work_y = MARGIN_TOP;
    uint32_t work_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT;
    uint32_t work_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM;
    
    // Copy only within the terminal workspace margins in the backbuffer
    for (uint32_t y = 0; y < work_h - char_height; y++) {
        uint32_t dst_y = work_y + y;
        uint32_t src_y = work_y + y + char_height;
        
        uint32_t* dst_row = &backbuffer[dst_y * width + work_x];
        uint32_t* src_row = &backbuffer[src_y * width + work_x];
        
        for (uint32_t i = 0; i < work_w; i++) {
            dst_row[i] = src_row[i];
        }
    }
    
    // Clear bottom row of console workspace in the backbuffer
    draw_rect(work_x, work_y + work_h - char_height, work_w, char_height, current_bg);
    
    terminal_row = (work_h / char_height) - 1;
}
void print_char(char c, uint32_t fg, uint32_t bg) {

    uint32_t max_cols = (boot_info.width - MARGIN_LEFT - MARGIN_RIGHT) / CHAR_WIDTH;
    uint32_t max_rows = (boot_info.height - MARGIN_TOP - MARGIN_BOTTOM) / CHAR_HEIGHT;
    
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, bg);

    if (c == '\b' || c == 127) {
        // Clear cursor line at the old position
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, bg);
        if (terminal_col > 0) {
            terminal_col--;
            // Erase the character at the new cursor position with background color
            draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT, bg);
        }
        // Draw the cursor line at the new position
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, fg);
        return;
    }
    
    if (c == '\n') {
        terminal_col = 0;
        if (++terminal_row >= max_rows) {
            graphics_scroll();
        }
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, fg);
        return;
    }
    
    if (c == '\r') {
        terminal_col = 0;
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, fg);
        return;
    }

    if (c == '\t') {
        terminal_col = (terminal_col + 4) & ~3;
        if (terminal_col >= max_cols) {
            terminal_col = 0;
            if (++terminal_row >= max_rows) {
                graphics_scroll();
            }
        }
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, fg);
        return;
    }
    
    draw_char(c, MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT, fg, bg);
    
    if (++terminal_col >= max_cols) {
        terminal_col = 0;
        if (++terminal_row >= max_rows) {
            graphics_scroll();
        }
    }

    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, fg);
}

void print_string(const char* str, uint32_t fg, uint32_t bg) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        print_char(str[i], fg, bg);
    }
}

void print_char_default(char c) {
    print_char(c, current_fg, current_bg);
}

void print_string_default(const char* str) {
    print_string(str, current_fg, current_bg);
}

void graphics_set_cursor(uint32_t col, uint32_t row) {
    uint32_t max_cols = (boot_info.width - MARGIN_LEFT - MARGIN_RIGHT) / CHAR_WIDTH;
    uint32_t max_rows = (boot_info.height - MARGIN_TOP - MARGIN_BOTTOM) / CHAR_HEIGHT;
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_bg);
    if (col < max_cols) terminal_col = col;
    if (row < max_rows) terminal_row = row;
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_fg);
}

void graphics_draw_splash(void) {
    // 1. Clear screen to modern warm beige base
    graphics_clear(0xFAF7F2);
    
    // 2. Draw refined ASCII Microchip Logo centered at y = boot_info.height / 4
    uint32_t start_y = boot_info.height / 4;
    uint32_t start_x = (boot_info.width - 30 * CHAR_WIDTH) / 2;
    
    const char* logo[] = {
        "         |   |   |   |        ",
        "       .-+---+---+---+-.      ",
        "     --|  ___________  |--    ",
        "     --| [  _______  ] |--    ",
        "     --| [ |  ___  | ] |--    ",
        "     --| [ | [ _ ] | ] |--    ",
        "     --| [ |  \\_/  | ] |--    ",
        "     --| [ |_______| ] |--    ",
        "     --| [___________] |--    ",
        "       '-+---+---+---+-'      ",
        "         |   |   |   |        "
    };
    
    for (int r = 0; r < 11; r++) {
        for (int c = 0; logo[r][c] != '\0'; c++) {
            if (logo[r][c] != ' ') {
                draw_char(logo[r][c], start_x + c * CHAR_WIDTH, start_y + r * CHAR_HEIGHT, 0x10B981, 0xFAF7F2);
            }
        }
    }
    
    // Draw centered brand text "ZenithOS" in large, polished ASCII block text made of corresponding letters
    uint32_t text_y = start_y + 11 * CHAR_HEIGHT + 24;
    uint32_t text_x = (boot_info.width - 62 * CHAR_WIDTH) / 2;
    
    const char* brand_ascii[] = {
        "ZZZZZZ    eeee   nnnn      ii    t     h       OOOO    SSSSS",
        "    ZZ   e    e  n   n           t     hhhh   OO  OO  SS    ",
        "   ZZ    eeeeee  n   n     ii  ttttt   h   h  OO  OO   SSSS ",
        "  ZZ     e       n   n     ii    t     h   h  OO  OO      SS",
        "ZZZZZZ    eeee   n   n     ii    t     h   h   OOOO   SSSSS "
    };
    
    for (int r = 0; r < 5; r++) {
        for (int c = 0; brand_ascii[r][c] != '\0'; c++) {
            if (brand_ascii[r][c] != ' ') {
                draw_char(brand_ascii[r][c], text_x + c * CHAR_WIDTH, text_y + r * CHAR_HEIGHT, 0x2C2825, 0xFAF7F2);
            }
        }
    }
    
    // 3. Draw loading status box centered below logo
    uint32_t box_w = 600;
    uint32_t box_h = 45;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = text_y + 5 * CHAR_HEIGHT + 40;
    
    // Draw box border (Sand outline)
    draw_rect(box_x, box_y, box_w, 2, 0xC4B5A3);
    draw_rect(box_x, box_y, 2, box_h, 0xC4B5A3);
    draw_rect(box_x, box_y + box_h - 2, box_w, 2, 0xC4B5A3);
    draw_rect(box_x + box_w - 2, box_y, 2, box_h, 0xC4B5A3);
    
    graphics_update_progress("Initializing Zenith OS kernel...", 0);
}
 
void graphics_update_progress(const char* status, uint32_t percentage) {
    uint32_t box_w = 600;
    uint32_t box_h = 45;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height / 4) + 16 * CHAR_HEIGHT + 64;

    // Clear status text area (y = box_y - 40 to box_y - 20)
    uint32_t status_len = 0;
    while (status[status_len]) status_len++;
    uint32_t status_x = boot_info.width / 2 - (status_len * CHAR_WIDTH) / 2;
    draw_rect(box_x - 100, box_y - 40, box_w + 200, 20, 0xFAF7F2);
    
    for (uint32_t i = 0; i < status_len; i++) {
        draw_char(status[i], status_x + i * CHAR_WIDTH, box_y - 38, 0x5F5850, 0xFAF7F2);
    }
    
    // Fill the progress bar inside the box
    uint32_t bar_max_w = box_w - 8;
    uint32_t bar_w = (bar_max_w * percentage) / 100;
    
    draw_rect(box_x + 4, box_y + 4, bar_w, box_h - 8, 0x10B981);
    draw_rect(box_x + 4 + bar_w, box_y + 4, bar_max_w - bar_w, box_h - 8, 0xEAE3D8);
    
    // Print percentage text centered below progress box
    char percent_str[8];
    percent_str[0] = '0' + (percentage / 100);
    percent_str[1] = '0' + ((percentage % 100) / 10);
    percent_str[2] = '0' + (percentage % 10);
    percent_str[3] = '%';
    percent_str[4] = '\0';
    
    char* print_str = percent_str;
    if (print_str[0] == '0') {
        print_str++;
        if (print_str[0] == '0') {
            print_str++;
        }
    }
    
    uint32_t p_len = 0;
    while (print_str[p_len]) p_len++;
    uint32_t p_x = boot_info.width / 2 - (p_len * CHAR_WIDTH) / 2;
    draw_rect(p_x - 20, box_y + box_h + 15, p_len * CHAR_WIDTH + 40, 20, 0xFAF7F2);
    for (uint32_t i = 0; i < p_len; i++) {
        draw_char(print_str[i], p_x + i * CHAR_WIDTH, box_y + box_h + 17, 0x10B981, 0xFAF7F2);
    }
    graphics_swap_buffers();
}

static inline void outb_local(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb_local(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw_local(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %w0, %w1" : : "a"(val), "Nd"(port));
}

void graphics_draw_shutdown(void) {
    // 1. Clear screen to modern gradient
    graphics_draw_gradient(0xFAF7F2, 0xEAE3D8);
    
    // 2. Draw styled box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Drop shadow (cozy warm grey)
    draw_rounded_rect(box_x + 12, box_y + 12, box_w, box_h, 16, 0xD8CFC0);
    
    // Cozy white card panel
    draw_rounded_rect(box_x, box_y, box_w, box_h, 16, 0xFFFFFF);
    
    // Terracotta warning border (rounded)
    draw_rounded_rect_outline(box_x, box_y, box_w, box_h, 16, 0x7C2D12);
    
    // Centered messages
    const char* m1 = "ZENITH OPERATING SYSTEM";
    const char* m2 = "SYSTEM SHUTDOWN";
    const char* m3 = "It is now safe to turn off your computer.";
    
    uint32_t len1 = 0; while (m1[len1]) len1++;
    uint32_t len2 = 0; while (m2[len2]) len2++;
    uint32_t len3 = 0; while (m3[len3]) len3++;
    
    uint32_t x1 = box_x + (box_w - len1 * CHAR_WIDTH) / 2;
    uint32_t x2 = box_x + (box_w - len2 * CHAR_WIDTH) / 2;
    uint32_t x3 = box_x + (box_w - len3 * CHAR_WIDTH) / 2;
    
    uint32_t y1 = box_y + 40;
    uint32_t y2 = box_y + 90;
    uint32_t y3 = box_y + 140;
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0x5F5850, 0xFFFFFF);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0x7C2D12, 0xFFFFFF);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0x15803D, 0xFFFFFF);
    
    graphics_swap_buffers();
    
    // Perform QEMU ACPI poweroff
    timer_wait(1000);
    outw_local(0x604, 0x2000);
    outw_local(0xB004, 0x2000);
    outw_local(0x4004, 0x3400);
}
 
void graphics_draw_restart(void) {
    // 1. Clear screen to modern gradient
    graphics_draw_gradient(0xFAF7F2, 0xEAE3D8);
    
    // 2. Draw styled box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Drop shadow (cozy warm grey)
    draw_rounded_rect(box_x + 12, box_y + 12, box_w, box_h, 16, 0xD8CFC0);
    
    // Cozy white card panel
    draw_rounded_rect(box_x, box_y, box_w, box_h, 16, 0xFFFFFF);
    
    // Deep Teal restart border (rounded)
    draw_rounded_rect_outline(box_x, box_y, box_w, box_h, 16, 0x0F766E);
    
    // Centered messages
    const char* m1 = "ZENITH OPERATING SYSTEM";
    const char* m2 = "SYSTEM RESTART";
    const char* m3 = "Rebooting computer. Please wait...";
    
    uint32_t len1 = 0; while (m1[len1]) len1++;
    uint32_t len2 = 0; while (m2[len2]) len2++;
    uint32_t len3 = 0; while (m3[len3]) len3++;
    
    uint32_t x1 = box_x + (box_w - len1 * CHAR_WIDTH) / 2;
    uint32_t x2 = box_x + (box_w - len2 * CHAR_WIDTH) / 2;
    uint32_t x3 = box_x + (box_w - len3 * CHAR_WIDTH) / 2;
    
    uint32_t y1 = box_y + 40;
    uint32_t y2 = box_y + 90;
    uint32_t y3 = box_y + 140;
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0x5F5850, 0xFFFFFF);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0x0F766E, 0xFFFFFF);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0x2C2825, 0xFFFFFF);
    
    graphics_swap_buffers();
    
    // Reboot via 8042 Keyboard Controller
    timer_wait(1000);
    while (inb_local(0x64) & 0x02);
    outb_local(0x64, 0xFE);
    while (1) { __asm__ volatile("hlt"); }
}

static void kernel_itoa(int n, char* s) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    
    // Reverse
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = s[j];
        s[j] = s[k];
        s[k] = temp;
    }
}

static void draw_string_at(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        draw_char(str[i], x + i * CHAR_WIDTH, y, fg, bg);
    }
}




void graphics_draw_statusbar(void) {
    uint32_t bar_y = boot_info.height - 36;
    uint32_t bar_h = 30;
    uint32_t bar_x = MARGIN_LEFT;
    uint32_t bar_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT;
    
    // Renders a sleek, modern bar
    draw_rounded_rect(bar_x, bar_y, bar_w, bar_h, 8, 0x12131A);
    draw_rounded_rect_outline(bar_x, bar_y, bar_w, bar_h, 8, 0x2D2E3D);
    
    // Get uptime
    uint32_t seconds = get_ticks() / 100;
    
    // Active tasks count (interrupt-safe)
    uint32_t eflags;
    __asm__ volatile("pushfl; pop %0; cli" : "=r"(eflags));
    Task* head = get_task_list_head();
    uint32_t task_count = 0;
    if (head != NULL) {
        Task* curr = head;
        do {
            task_count++;
            curr = curr->next;
        } while (curr != head);
    }
    __asm__ volatile("push %0; popfl" : : "r"(eflags));
    
    // Memory usage
    extern uint32_t vmm_get_allocated_memory_mb(void);
    uint32_t mem_mb = vmm_get_allocated_memory_mb();
    
    // Format strings
    char upt_str[32] = "Uptime: ";
    char num_buf[16];
    kernel_itoa(seconds, num_buf);
    int len = 8;
    for (int i = 0; num_buf[i] != '\0'; i++) upt_str[len++] = num_buf[i];
    upt_str[len++] = 's';
    upt_str[len] = '\0';
    
    char task_str[32] = "Tasks: ";
    kernel_itoa(task_count, num_buf);
    len = 7;
    for (int i = 0; num_buf[i] != '\0'; i++) task_str[len++] = num_buf[i];
    task_str[len] = '\0';
    
    char mem_str[64] = "Memory: ";
    kernel_itoa(mem_mb, num_buf);
    len = 8;
    for (int i = 0; num_buf[i] != '\0'; i++) mem_str[len++] = num_buf[i];
    const char* suffix = " MB / 128 MB";
    for (int i = 0; suffix[i] != '\0'; i++) mem_str[len++] = suffix[i];
    mem_str[len] = '\0';
    
    // Draw Icons
    // 1. Clock Icon (circle with hands)
    int clock_cx = bar_x + 13;
    int clock_cy = bar_y + 15;
    draw_circle(clock_cx, clock_cy, 7, 0x00E5FF);
    draw_pixel(clock_cx, clock_cy, 0x00E5FF);
    draw_pixel(clock_cx, clock_cy - 1, 0x00E5FF);
    draw_pixel(clock_cx, clock_cy - 2, 0x00E5FF);
    draw_pixel(clock_cx, clock_cy - 3, 0x00E5FF);
    draw_pixel(clock_cx, clock_cy - 4, 0x00E5FF);
    draw_pixel(clock_cx + 1, clock_cy, 0x00E5FF);
    draw_pixel(clock_cx + 2, clock_cy, 0x00E5FF);
    draw_pixel(clock_cx + 3, clock_cy, 0x00E5FF);

    // 2. Tasks Silhouette Icon
    int task_cx = bar_x + (bar_w / 2) - 80;
    int task_cy = bar_y + 15;
    draw_filled_circle(task_cx, task_cy - 3, 3, 0xE2E8F0);
    draw_rect(task_cx - 5, task_cy + 1, 11, 4, 0xE2E8F0);
    draw_pixel(task_cx - 6, task_cy + 3, 0xE2E8F0);
    draw_pixel(task_cx - 6, task_cy + 4, 0xE2E8F0);
    draw_pixel(task_cx + 6, task_cy + 3, 0xE2E8F0);
    draw_pixel(task_cx + 6, task_cy + 4, 0xE2E8F0);

    // 3. RAM Icon
    int ram_cx = bar_x + bar_w - 300;
    int ram_cy = bar_y + 15;
    draw_rect(ram_cx - 6, ram_cy - 5, 13, 1, 0x10B981);
    draw_rect(ram_cx - 6, ram_cy + 5, 13, 1, 0x10B981);
    draw_rect(ram_cx - 6, ram_cy - 5, 1, 11, 0x10B981);
    draw_rect(ram_cx + 6, ram_cy - 5, 1, 11, 0x10B981);
    // Left pins
    draw_pixel(ram_cx - 8, ram_cy - 3, 0x10B981); draw_pixel(ram_cx - 7, ram_cy - 3, 0x10B981);
    draw_pixel(ram_cx - 8, ram_cy - 1, 0x10B981); draw_pixel(ram_cx - 7, ram_cy - 1, 0x10B981);
    draw_pixel(ram_cx - 8, ram_cy + 1, 0x10B981); draw_pixel(ram_cx - 7, ram_cy + 1, 0x10B981);
    draw_pixel(ram_cx - 8, ram_cy + 3, 0x10B981); draw_pixel(ram_cx - 7, ram_cy + 3, 0x10B981);
    // Right pins
    draw_pixel(ram_cx + 7, ram_cy - 3, 0x10B981); draw_pixel(ram_cx + 8, ram_cy - 3, 0x10B981);
    draw_pixel(ram_cx + 7, ram_cy - 1, 0x10B981); draw_pixel(ram_cx + 8, ram_cy - 1, 0x10B981);
    draw_pixel(ram_cx + 7, ram_cy + 1, 0x10B981); draw_pixel(ram_cx + 8, ram_cy + 1, 0x10B981);
    draw_pixel(ram_cx + 7, ram_cy + 3, 0x10B981); draw_pixel(ram_cx + 8, ram_cy + 3, 0x10B981);

    // Draw text inside status bar (y = bar_y + 3)
    uint32_t text_y = bar_y + 3;
    draw_string_at(upt_str, bar_x + 28, text_y, 0x00E5FF, 0x12131A);
    draw_string_at(task_str, bar_x + (bar_w / 2) - 60, text_y, 0xE2E8F0, 0x12131A);
    draw_string_at(mem_str, bar_x + bar_w - 280, text_y, 0x10B981, 0x12131A);
    
    graphics_swap_buffers();
}

void graphics_swap_buffers(void) {
    if (fb == NULL) return;
    
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    uint32_t pitch = boot_info.pitch;
    
    if (boot_info.bpp == 32) {
        for (uint32_t y = 0; y < height; y++) {
            uint32_t* dest = (uint32_t*)(fb + y * pitch);
            uint32_t* src = &backbuffer[y * width];
            for (uint32_t x = 0; x < width; x++) {
                dest[x] = src[x];
            }
        }
    } else if (boot_info.bpp == 24) {
        for (uint32_t y = 0; y < height; y++) {
            uint8_t* dest = (uint8_t*)(fb + y * pitch);
            uint32_t* src = &backbuffer[y * width];
            for (uint32_t x = 0; x < width; x++) {
                uint32_t color = src[x];
                uint32_t offset = x * 3;
                dest[offset] = color & 0xFF;
                dest[offset + 1] = (color >> 8) & 0xFF;
                dest[offset + 2] = (color >> 16) & 0xFF;
            }
        }
    }
}

uint32_t* get_backbuffer_ptr(void) {
    return backbuffer;
}

uint32_t get_screen_width(void) {
    return boot_info.width;
}

uint32_t get_screen_height(void) {
    return boot_info.height;
}

void graphics_swipe_transition(void) {
    if (fb == NULL) return;
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    uint32_t pitch = boot_info.pitch;
    
    for (int step = 0; step < 25; step++) {
        uint32_t x_start = (step * width) / 25;
        uint32_t x_end = ((step + 1) * width) / 25;
        if (x_end > width) x_end = width;
        
        if (boot_info.bpp == 32) {
            for (uint32_t y = 0; y < height; y++) {
                uint32_t* dest = (uint32_t*)(fb + y * pitch);
                uint32_t* src = &backbuffer[y * width];
                for (uint32_t x = x_start; x < x_end; x++) {
                    dest[x] = src[x];
                }
            }
        } else if (boot_info.bpp == 24) {
            for (uint32_t y = 0; y < height; y++) {
                uint8_t* dest = (uint8_t*)(fb + y * pitch);
                uint32_t* src = &backbuffer[y * width];
                for (uint32_t x = x_start; x < x_end; x++) {
                    uint32_t color = src[x];
                    uint32_t offset = x * 3;
                    dest[offset] = color & 0xFF;
                    dest[offset + 1] = (color >> 8) & 0xFF;
                    dest[offset + 2] = (color >> 16) & 0xFF;
                }
            }
        }
        timer_wait(1);
    }
}

void draw_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint8_t alpha) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    uint8_t r_src = (color >> 16) & 0xFF;
    uint8_t g_src = (color >> 8) & 0xFF;
    uint8_t b_src = color & 0xFF;
    
    for (uint32_t i = 0; i < h; i++) {
        uint32_t py = y + i;
        if (py >= height) continue;
        for (uint32_t j = 0; j < w; j++) {
            uint32_t px = x + j;
            if (px >= width) continue;
            
            uint32_t dst_color = backbuffer[py * width + px];
            uint8_t r_dst = (dst_color >> 16) & 0xFF;
            uint8_t g_dst = (dst_color >> 8) & 0xFF;
            uint8_t b_dst = dst_color & 0xFF;
            
            uint8_t r_out = (r_src * alpha + r_dst * (255 - alpha)) / 255;
            uint8_t g_out = (g_src * alpha + g_dst * (255 - alpha)) / 255;
            uint8_t b_out = (b_src * alpha + b_dst * (255 - alpha)) / 255;
            
            backbuffer[py * width + px] = (r_out << 16) | (g_out << 8) | b_out;
        }
    }
}

void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    uint8_t r_src = (color >> 16) & 0xFF;
    uint8_t g_src = (color >> 8) & 0xFF;
    uint8_t b_src = color & 0xFF;
    
    int cx1 = x + r, cy1 = y + r;
    int cx2 = x + w - r - 1, cy2 = y + r;
    int cx3 = x + r, cy3 = y + h - r - 1;
    int cx4 = x + w - r - 1, cy4 = y + h - r - 1;
    
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= (int)height) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= (int)width) continue;
            
            bool draw = true;
            if (px < cx1 && py < cy1) {
                if ((px - cx1)*(px - cx1) + (py - cy1)*(py - cy1) > r*r) draw = false;
            } else if (px > cx2 && py < cy2) {
                if ((px - cx2)*(px - cx2) + (py - cy2)*(py - cy2) > r*r) draw = false;
            } else if (px < cx3 && py > cy3) {
                if ((px - cx3)*(px - cx3) + (py - cy3)*(py - cy3) > r*r) draw = false;
            } else if (px > cx4 && py > cy4) {
                if ((px - cx4)*(px - cx4) + (py - cy4)*(py - cy4) > r*r) draw = false;
            }
            
            if (draw) {
                uint32_t dst_color = backbuffer[py * width + px];
                uint8_t r_dst = (dst_color >> 16) & 0xFF;
                uint8_t g_dst = (dst_color >> 8) & 0xFF;
                uint8_t b_dst = dst_color & 0xFF;
                
                uint8_t r_out = (r_src * alpha + r_dst * (255 - alpha)) / 255;
                uint8_t g_out = (g_src * alpha + g_dst * (255 - alpha)) / 255;
                uint8_t b_out = (b_src * alpha + b_dst * (255 - alpha)) / 255;
                
                backbuffer[py * width + px] = (r_out << 16) | (g_out << 8) | b_out;
            }
        }
    }
}

void graphics_toggle_launcher(void) {}

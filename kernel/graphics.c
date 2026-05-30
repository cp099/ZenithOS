#include "graphics.h"
#include "font.h"
#include "timer.h"

static struct BootInfo boot_info;
static volatile uint8_t* fb = NULL;
static uint32_t terminal_col = 0;
static uint32_t terminal_row = 0;

// Color schemes
#define COLOR_DEFAULT_FG 0x00E0E0E0 // Light Grey
#define COLOR_DEFAULT_BG 0x000F0F14 // Deep Dark Blue-Black

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

void graphics_init(void) {
    // Copy the BootInfo block written by Stage 2 at 0x7000
    local_memcpy(&boot_info, (const void*)0x7000, sizeof(struct BootInfo));
    fb = (volatile uint8_t*)(uintptr_t)boot_info.framebuffer;
    
    // Show splash screen on startup
    graphics_draw_splash();
}


void graphics_draw_frame(void) {
    // 1. Fill entire screen with desktop background (Slate Purple: 0x6E5F80)
    graphics_clear(0x6E5F80);
    
    // 2. Draw the console window container border.
    // The window covers x from (MARGIN_LEFT - 4) to (width - MARGIN_RIGHT + 4)
    // and y from (MARGIN_TOP - 24) to (height - MARGIN_BOTTOM + 4)
    uint32_t win_x = MARGIN_LEFT - 4;
    uint32_t win_y = MARGIN_TOP - 24;
    uint32_t win_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT + 8;
    uint32_t win_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM + 28;
    
    // Draw outer bevel: Top-left light gray, Bottom-right dark gray
    draw_rect(win_x, win_y, win_w, 2, 0xCCCCCC);
    draw_rect(win_x, win_y, 2, win_h, 0xCCCCCC);
    draw_rect(win_x, win_y + win_h - 2, win_w, 2, 0x555555);
    draw_rect(win_x + win_w - 2, win_y, 2, win_h, 0x555555);
    
    // 3. Draw Title Bar: Filled rectangle (Charcoal Black: 0x1F1F24)
    uint32_t title_x = win_x + 2;
    uint32_t title_y = win_y + 2;
    uint32_t title_w = win_w - 4;
    uint32_t title_h = 20;
    draw_rect(title_x, title_y, title_w, title_h, 0x1F1F24);
    
    // Draw a small decorative title border line under title bar
    draw_rect(title_x, title_y + title_h, title_w, 1, 0x555555);
    
    // Draw centered title text "Zenith Operating System v1.0" in title bar
    const char* title_text = "Zenith Operating System v1.0";
    uint32_t text_len = 0;
    while (title_text[text_len]) text_len++;
    uint32_t text_x = title_x + (title_w - text_len * CHAR_WIDTH) / 2;
    uint32_t text_y = title_y + (title_h - CHAR_HEIGHT) / 2;
    
    for (uint32_t i = 0; i < text_len; i++) {
        draw_char(title_text[i], text_x + i * CHAR_WIDTH, text_y, 0xFFFFFF, 0x1F1F24);
    }
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
    
    if (boot_info.bpp == 32) {
        uint32_t* pixel = (uint32_t*)(fb + y * boot_info.pitch + x * 4);
        *pixel = color;
    } else if (boot_info.bpp == 24) {
        uint8_t* pixel = (uint8_t*)(fb + y * boot_info.pitch + x * 3);
        pixel[0] = color & 0xFF;         // Blue
        pixel[1] = (color >> 8) & 0xFF;  // Green
        pixel[2] = (color >> 16) & 0xFF; // Red
    }
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void graphics_clear(uint32_t color) {
    if (boot_info.bpp == 32) {
        uint32_t total_pixels = boot_info.width * boot_info.height;
        uint32_t* pixel_fb = (uint32_t*)fb;
        for (uint32_t i = 0; i < total_pixels; i++) {
            pixel_fb[i] = color;
        }
    } else {
        for (uint32_t y = 0; y < boot_info.height; y++) {
            for (uint32_t x = 0; x < boot_info.width; x++) {
                draw_pixel(x, y, color);
            }
        }
    }
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if ((uint8_t)c < 32 || (uint8_t)c > 127) c = 32;
    
    for (int row = 0; row < CHAR_HEIGHT; row++) {
        int sy = (row * 8) / CHAR_HEIGHT;
        uint8_t bits = font_bitmap[(int)c][sy];
        for (int col = 0; col < CHAR_WIDTH; col++) {
            int sx = (col * 8) / CHAR_WIDTH;
            
            // Check if current pixel is set
            bool is_set = (bits & (0x80 >> sx)) != 0;
            
            // Bold/smooth interpolation: if the next pixel is set and we're bridging a fractional gap,
            // draw it as set. This thickens the text so it doesn't look thin or pixelated when upscaled.
            if (!is_set && sx < 7) {
                if ((bits & (0x80 >> (sx + 1))) != 0 && (col * 8) % CHAR_WIDTH != 0) {
                    is_set = true;
                }
            }
            
            if (is_set) {
                draw_pixel(x + col, y + row, fg);
            } else {
                draw_pixel(x + col, y + row, bg);
            }
        }
    }
}

static void graphics_scroll(void) {
    uint32_t row_bytes = boot_info.pitch;
    uint32_t char_height = CHAR_HEIGHT;
    
    uint32_t work_x = MARGIN_LEFT;
    uint32_t work_y = MARGIN_TOP;
    uint32_t work_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT;
    uint32_t work_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM;
    
    uint32_t bytes_per_pixel = boot_info.bpp / 8;
    
    // Copy only within the terminal workspace margins
    for (uint32_t y = 0; y < work_h - char_height; y++) {
        uint32_t dst_y = work_y + y;
        uint32_t src_y = work_y + y + char_height;
        
        uint8_t* dst_ptr = (uint8_t*)(fb + dst_y * row_bytes + work_x * bytes_per_pixel);
        const uint8_t* src_ptr = (const uint8_t*)(fb + src_y * row_bytes + work_x * bytes_per_pixel);
        
        for (uint32_t i = 0; i < work_w * bytes_per_pixel; i++) {
            dst_ptr[i] = src_ptr[i];
        }
    }
    
    // Clear bottom row of console workspace
    draw_rect(work_x, work_y + work_h - char_height, work_w, char_height, current_bg);
    
    terminal_row = (work_h / char_height) - 1;
}

void print_char(char c, uint32_t fg, uint32_t bg) {
    uint32_t max_cols = (boot_info.width - MARGIN_LEFT - MARGIN_RIGHT) / CHAR_WIDTH;
    uint32_t max_rows = (boot_info.height - MARGIN_TOP - MARGIN_BOTTOM) / CHAR_HEIGHT;
    
    // Erase cursor at current position
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, bg);

    if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
        }
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
    
    // Erase cursor at current position
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_bg);
    
    if (col < max_cols) terminal_col = col;
    if (row < max_rows) terminal_row = row;
    
    // Draw cursor at new position
    draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_fg);
}

void graphics_draw_splash(void) {
    // 1. Clear screen to dark slate purple background
    graphics_clear(0x1B1822);
    
    // 2. Draw styled ASCII Logo centered at y = boot_info.height / 3
    uint32_t start_y = boot_info.height / 3;
    uint32_t start_x = (boot_info.width - 64 * CHAR_WIDTH) / 2;
    
    const char* logo[] = {
        " ZZZZZZ  EEEEEE  NN  NN  IIIIII  TTTTTT  HH  HH    OOOO    SSSS ",
        "     ZZ  EE      NNN NN    II      TT    HH  HH   OO  OO  SS    ",
        "   ZZZ   EEEE    ######    II      TT    ######   OO  OO   SSSS ",
        "  ZZ     EE      NN ###    II      TT    HH  HH   OO  OO      SS",
        " ZZZZZZ  EEEEEE  NN  NN  IIIIII    TT    HH  HH    OOOO    SSSS "
    };
    
    for (int r = 0; r < 5; r++) {
        for (int c = 0; logo[r][c] != '\0'; c++) {
            if (logo[r][c] != ' ') {
                draw_char(logo[r][c], start_x + c * CHAR_WIDTH, start_y + r * CHAR_HEIGHT, 0x00E5FF, 0x1B1822);
            }
        }
    }
    
    // 3. Draw loading status box centered at y = start_y + 6 * CHAR_HEIGHT + 40
    uint32_t box_w = 500;
    uint32_t box_h = 45;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = start_y + 6 * CHAR_HEIGHT + 40;
    
    // Draw box border (Charcoal outline)
    draw_rect(box_x, box_y, box_w, 2, 0x44414D);
    draw_rect(box_x, box_y, 2, box_h, 0x44414D);
    draw_rect(box_x, box_y + box_h - 2, box_w, 2, 0x44414D);
    draw_rect(box_x + box_w - 2, box_y, 2, box_h, 0x44414D);
    
    graphics_update_progress("Initializing ZenithOS kernel...", 0);
}

void graphics_update_progress(const char* status, uint32_t percentage) {
    uint32_t box_w = 500;
    uint32_t box_h = 45;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height / 3) + 6 * CHAR_HEIGHT + 40;
    
    // Clear status text area (y = box_y - 40 to box_y - 20)
    uint32_t status_len = 0;
    while (status[status_len]) status_len++;
    uint32_t status_x = boot_info.width / 2 - (status_len * CHAR_WIDTH) / 2;
    draw_rect(box_x - 100, box_y - 40, box_w + 200, 20, 0x1B1822);
    
    for (uint32_t i = 0; i < status_len; i++) {
        draw_char(status[i], status_x + i * CHAR_WIDTH, box_y - 38, 0xE0E0E0, 0x1B1822);
    }
    
    // Fill the progress bar inside the box
    uint32_t bar_max_w = box_w - 8;
    uint32_t bar_w = (bar_max_w * percentage) / 100;
    
    draw_rect(box_x + 4, box_y + 4, bar_w, box_h - 8, 0x00E5FF);
    draw_rect(box_x + 4 + bar_w, box_y + 4, bar_max_w - bar_w, box_h - 8, 0x2A2735);
    
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
    draw_rect(p_x - 10, box_y + box_h + 15, p_len * CHAR_WIDTH + 20, 20, 0x1B1822);
    for (uint32_t i = 0; i < p_len; i++) {
        draw_char(print_str[i], p_x + i * CHAR_WIDTH, box_y + box_h + 17, 0x00E5FF, 0x1B1822);
    }
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
    // 1. Clear screen to dark slate purple background
    graphics_clear(0x1B1822);
    
    // 2. Draw styled beveled warning box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Charcoal box with warning orange outline
    draw_rect(box_x, box_y, box_w, box_h, 0x1F1F24);
    
    // Draw bevel borders (3px)
    draw_rect(box_x, box_y, box_w, 3, 0xFF8000); // Top
    draw_rect(box_x, box_y, 3, box_h, 0xFFFF00); // Left
    draw_rect(box_x, box_y + box_h - 3, box_w, 3, 0xFF8000); // Bottom
    draw_rect(box_x + box_w - 3, box_y, 3, box_h, 0xFFFFFF); // Right
    
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
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0xFFFFFF, 0x1F1F24);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0xFF8000, 0x1F1F24);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0x00FF33, 0x1F1F24);
    
    // Perform QEMU ACPI poweroff
    timer_wait(1000);
    outw_local(0x604, 0x2000);
    outw_local(0xB004, 0x2000);
    outw_local(0x4004, 0x3400);
}

void graphics_draw_restart(void) {
    // 1. Clear screen to dark slate purple background
    graphics_clear(0x1B1822);
    
    // 2. Draw styled beveled box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Charcoal box with system blue outline
    draw_rect(box_x, box_y, box_w, box_h, 0x1F1F24);
    
    // Draw bevel borders (3px)
    draw_rect(box_x, box_y, box_w, 3, 0x00E5FF); // Top
    draw_rect(box_x, box_y, 3, box_h, 0x00E5FF); // Left
    draw_rect(box_x, box_y + box_h - 3, box_w, 3, 0x00E5FF); // Bottom
    draw_rect(box_x + box_w - 3, box_y, 3, box_h, 0x00E5FF); // Right
    
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
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0xFFFFFF, 0x1F1F24);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0x00E5FF, 0x1F1F24);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0xE0E0E0, 0x1F1F24);
    
    // Reboot via 8042 Keyboard Controller
    timer_wait(1000);
    while (inb_local(0x64) & 0x02);
    outb_local(0x64, 0xFE);
    while (1) { __asm__ volatile("hlt"); }
}



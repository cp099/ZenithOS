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

void graphics_draw_gradient(uint32_t start_color, uint32_t end_color) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    // We introduce a middle color stop to make the cosmic gradient look more modern and multi-toned
    uint32_t mid_color = 0x241435; // Vibrant Cosmic Violet-Indigo
    
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
        
        if (boot_info.bpp == 32) {
            uint32_t* line_fb = (uint32_t*)(fb + y * boot_info.pitch);
            for (uint32_t x = 0; x < width; x++) {
                line_fb[x] = color;
            }
        } else {
            for (uint32_t x = 0; x < width; x++) {
                draw_pixel(x, y, color);
            }
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
    // 1. Draw modern cosmic wallpaper gradient (Galaxy indigo to violet)
    graphics_draw_gradient(0x070B19, 0x160A26);
    
    // 2. Center console card container dimensions
    uint32_t container_x = MARGIN_LEFT - 8;
    uint32_t container_y = MARGIN_TOP - 8;
    uint32_t container_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT + 16;
    uint32_t container_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM + 16;
    
    // Draw layered Drop Shadows
    draw_rounded_rect(container_x + 12, container_y + 12, container_w, container_h, 12, 0x030305);
    draw_rounded_rect(container_x + 8, container_y + 8, container_w, container_h, 12, 0x050508);
    draw_rounded_rect(container_x + 4, container_y + 4, container_w, container_h, 12, 0x08080C);
    
    // Draw main console workspace card panel (carbon base)
    draw_rounded_rect(container_x, container_y, container_w, container_h, 12, 0x07080B);
    
    // Draw subtle border around the console workspace
    draw_rounded_rect_outline(container_x, container_y, container_w, container_h, 12, 0x2D2E3D);
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
    
    uint8_t r_fg = (fg >> 16) & 0xFF;
    uint8_t g_fg = (fg >> 8) & 0xFF;
    uint8_t b_fg = fg & 0xFF;
    
    for (int row = 0; row < CHAR_HEIGHT; row++) {
        // Map row 0..CHAR_HEIGHT-1 to 0..7 range in fixed point (8.8)
        uint32_t v_fp = (row * 7 * 256) / (CHAR_HEIGHT - 1);
        uint32_t y_low = v_fp >> 8;
        uint32_t y_high = y_low + 1;
        if (y_high > 7) y_high = 7;
        uint32_t weight_y = v_fp & 0xFF;
        
        for (int col = 0; col < CHAR_WIDTH; col++) {
            // Map col 0..CHAR_WIDTH-1 to 0..7 range in fixed point (8.8)
            uint32_t u_fp = (col * 7 * 256) / (CHAR_WIDTH - 1);
            uint32_t x_low = u_fp >> 8;
            uint32_t x_high = x_low + 1;
            if (x_high > 7) x_high = 7;
            uint32_t weight_x = u_fp & 0xFF;
            
            // Read 4 binary pixel values from 8x8 font bitmap
            uint32_t val_00 = (font_bitmap[(int)c][y_low] & (0x80 >> x_low)) ? 255 : 0;
            uint32_t val_10 = (font_bitmap[(int)c][y_low] & (0x80 >> x_high)) ? 255 : 0;
            uint32_t val_01 = (font_bitmap[(int)c][y_high] & (0x80 >> x_low)) ? 255 : 0;
            uint32_t val_11 = (font_bitmap[(int)c][y_high] & (0x80 >> x_high)) ? 255 : 0;
            
            // Bilinear interpolation
            uint32_t val_0 = val_00 + (((int32_t)val_10 - (int32_t)val_00) * (int32_t)weight_x >> 8);
            uint32_t val_1 = val_01 + (((int32_t)val_11 - (int32_t)val_01) * (int32_t)weight_x >> 8);
            uint32_t intensity = val_0 + (((int32_t)val_1 - (int32_t)val_0) * (int32_t)weight_y >> 8);
            
            if (intensity > 0) {
                uint32_t actual_bg = bg;
                if (bg == 0xFFFFFFFF) {
                    // Read background from framebuffer
                    if (boot_info.bpp == 32) {
                        actual_bg = *(volatile uint32_t*)(fb + (y + row) * boot_info.pitch + (x + col) * 4);
                    } else {
                        actual_bg = 0x07080B; // fallback
                    }
                }
                
                // Blend colors based on intensity
                uint8_t r_bg = (actual_bg >> 16) & 0xFF;
                uint8_t g_bg = (actual_bg >> 8) & 0xFF;
                uint8_t b_bg = actual_bg & 0xFF;
                
                uint8_t r = r_bg + (((int32_t)r_fg - (int32_t)r_bg) * (int32_t)intensity >> 8);
                uint8_t g = g_bg + (((int32_t)g_fg - (int32_t)g_bg) * (int32_t)intensity >> 8);
                uint8_t b = b_bg + (((int32_t)b_fg - (int32_t)b_bg) * (int32_t)intensity >> 8);
                
                draw_pixel(x + col, y + row, (r << 16) | (g << 8) | b);
            } else if (bg != 0xFFFFFFFF) {
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
    uint32_t start_x = (boot_info.width - 68 * CHAR_WIDTH) / 2;
    
    const char* logo[] = {
        " ZZZZZZ  EEEEEE  NN  NN  IIIIII  TTTTTT  HH  HH        OOOO    SSSS ",
        "     ZZ  EE      NNN NN    II      TT    HH  HH       OO  OO  SS    ",
        "   ZZZ   EEEE    ######    II      TT    ######       OO  OO   SSSS ",
        "  ZZ     EE      NN ###    II      TT    HH  HH       OO  OO      SS",
        " ZZZZZZ  EEEEEE  NN  NN  IIIIII    TT    HH  HH        OOOO    SSSS "
    };
    
    for (int r = 0; r < 5; r++) {
        for (int c = 0; logo[r][c] != '\0'; c++) {
            if (logo[r][c] != ' ') {
                draw_char(logo[r][c], start_x + c * CHAR_WIDTH, start_y + r * CHAR_HEIGHT, 0x00E5FF, 0x1B1822);
            }
        }
    }
    
    // 3. Draw loading status box centered at y = start_y + 6 * CHAR_HEIGHT + 40
    uint32_t box_w = 600;
    uint32_t box_h = 45;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = start_y + 6 * CHAR_HEIGHT + 40;
    
    // Draw box border (Charcoal outline)
    draw_rect(box_x, box_y, box_w, 2, 0x44414D);
    draw_rect(box_x, box_y, 2, box_h, 0x44414D);
    draw_rect(box_x, box_y + box_h - 2, box_w, 2, 0x44414D);
    draw_rect(box_x + box_w - 2, box_y, 2, box_h, 0x44414D);
    
    graphics_update_progress("Initializing Zenith OS kernel...", 0);
}
 
void graphics_update_progress(const char* status, uint32_t percentage) {
    uint32_t box_w = 600;
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
    draw_rect(p_x - 20, box_y + box_h + 15, p_len * CHAR_WIDTH + 40, 20, 0x1B1822);
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
    // 1. Clear screen to modern gradient
    graphics_draw_gradient(0x070B19, 0x160A26);
    
    // 2. Draw styled box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Drop shadow
    draw_rounded_rect(box_x + 12, box_y + 12, box_w, box_h, 16, 0x05060A);
    
    // Charcoal box
    draw_rounded_rect(box_x, box_y, box_w, box_h, 16, 0x12131A);
    
    // Warning red border (rounded)
    draw_rounded_rect_outline(box_x, box_y, box_w, box_h, 16, 0xEF4444);
    
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
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0xE2E8F0, 0x12131A);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0xEF4444, 0x12131A);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0x10B981, 0x12131A);
    
    // Perform QEMU ACPI poweroff
    timer_wait(1000);
    outw_local(0x604, 0x2000);
    outw_local(0xB004, 0x2000);
    outw_local(0x4004, 0x3400);
}
 
void graphics_draw_restart(void) {
    // 1. Clear screen to modern gradient
    graphics_draw_gradient(0x070B19, 0x160A26);
    
    // 2. Draw styled box in the center
    uint32_t box_w = 600;
    uint32_t box_h = 220;
    uint32_t box_x = (boot_info.width - box_w) / 2;
    uint32_t box_y = (boot_info.height - box_h) / 2;
    
    // Drop shadow
    draw_rounded_rect(box_x + 12, box_y + 12, box_w, box_h, 16, 0x05060A);
    
    // Charcoal box
    draw_rounded_rect(box_x, box_y, box_w, box_h, 16, 0x12131A);
    
    // System cyber cyan border (rounded)
    draw_rounded_rect_outline(box_x, box_y, box_w, box_h, 16, 0x00E5FF);
    
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
    
    for (uint32_t i = 0; i < len1; i++) draw_char(m1[i], x1 + i * CHAR_WIDTH, y1, 0xE2E8F0, 0x12131A);
    for (uint32_t i = 0; i < len2; i++) draw_char(m2[i], x2 + i * CHAR_WIDTH, y2, 0x00E5FF, 0x12131A);
    for (uint32_t i = 0; i < len3; i++) draw_char(m3[i], x3 + i * CHAR_WIDTH, y3, 0xE0E0E0, 0x12131A);
    
    // Reboot via 8042 Keyboard Controller
    timer_wait(1000);
    while (inb_local(0x64) & 0x02);
    outb_local(0x64, 0xFE);
    while (1) { __asm__ volatile("hlt"); }
}



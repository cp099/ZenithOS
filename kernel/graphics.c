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

static bool compositor_active = false;
static Window windows[MAX_WINDOWS] = {0};
static bool mouse_button_down = false;
static int drag_window_idx = -1;


static int mouse_x = 0;
static int mouse_y = 0;
static uint32_t cursor_saved_bg[12 * 20];

static bool launcher_visible = false;
static uint32_t launcher_saved_bg[600 * 180];

static void graphics_save_cursor_bg(void);
static void graphics_restore_cursor_bg(void);

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
    
    mouse_x = boot_info.width / 2;
    mouse_y = boot_info.height / 2;
    
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
    
    // Draw main console workspace card panel (glassmorphic carbon base)
    extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
    draw_rounded_rect_alpha(container_x, container_y, container_w, container_h, 12, 0x07080B, 180);
    
    // Draw subtle border around the console workspace
    draw_rounded_rect_outline(container_x, container_y, container_w, container_h, 12, 0x2D2E3D);
}

void graphics_clear_console(void) {
    Task* cur = get_current_task();
    Window* win = NULL;
    if (cur != NULL) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].active && windows[i].owner == cur) {
                win = &windows[i];
                break;
            }
        }
    }
    if (win != NULL) {
        for (int i = 0; i < win->width * win->height; i++) {
            win->buffer[i] = 0x0C0C12;
        }
        win->terminal_col = 0;
        win->terminal_row = 0;
    } else {
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
                draw_pixel(x + bit_idx * 2,     y + row_idx * 2,     fg);
                draw_pixel(x + bit_idx * 2 + 1, y + row_idx * 2,     fg);
                draw_pixel(x + bit_idx * 2,     y + row_idx * 2 + 1, fg);
                draw_pixel(x + bit_idx * 2 + 1, y + row_idx * 2 + 1, fg);
            } else if (bg != 0xFFFFFFFF) {
                draw_pixel(x + bit_idx * 2,     y + row_idx * 2,     bg);
                draw_pixel(x + bit_idx * 2 + 1, y + row_idx * 2,     bg);
                draw_pixel(x + bit_idx * 2,     y + row_idx * 2 + 1, bg);
                draw_pixel(x + bit_idx * 2 + 1, y + row_idx * 2 + 1, bg);
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

void draw_char_in_window_buffer(uint32_t* buffer, int win_width, char c, int x, int y, uint32_t fg, uint32_t bg) {
    for (uint32_t row_idx = 0; row_idx < 16; row_idx++) {
        uint8_t row_data = font_bitmap[(uint8_t)c][row_idx];
        for (uint32_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            int active = (row_data & (0x80 >> bit_idx)) != 0;
            int px = x + bit_idx;
            int py = y + row_idx;
            uint32_t color = active ? fg : bg;
            if (color != 0xFFFFFFFF) {
                buffer[py * win_width + px] = color;
            }
        }
    }
}

static void draw_string_at_1x(const char* str, int x, int y, uint32_t fg, uint32_t bg) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        for (uint32_t row_idx = 0; row_idx < 16; row_idx++) {
            uint8_t row_data = font_bitmap[(uint8_t)str[i]][row_idx];
            for (uint32_t bit_idx = 0; bit_idx < 8; bit_idx++) {
                int active = (row_data & (0x80 >> bit_idx)) != 0;
                int px = x + i * 8 + bit_idx;
                int py = y + row_idx;
                uint32_t color = active ? fg : bg;
                if (color != 0xFFFFFFFF) {
                    draw_pixel(px, py, color);
                }
            }
        }
    }
}

void print_char(char c, uint32_t fg, uint32_t bg) {
    Task* cur = get_current_task();
    Window* win = NULL;
    if (cur != NULL) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].active && windows[i].owner == cur) {
                win = &windows[i];
                break;
            }
        }
    }

    if (win != NULL) {
        uint32_t max_cols = win->width / 8;
        uint32_t max_rows = win->height / 16;
        
        if (c == '\b') {
            if (win->terminal_col > 0) {
                win->terminal_col--;
            }
            draw_char_in_window_buffer(win->buffer, win->width, ' ', win->terminal_col * 8, win->terminal_row * 16, fg, bg);
            return;
        }
        
        if (c == '\n') {
            win->terminal_col = 0;
            if (++win->terminal_row >= max_rows) {
                for (int y = 0; y < (int)(win->height - 16); y++) {
                    for (int x = 0; x < win->width; x++) {
                        win->buffer[y * win->width + x] = win->buffer[(y + 16) * win->width + x];
                    }
                }
                for (int y = win->height - 16; y < win->height; y++) {
                    for (int x = 0; x < win->width; x++) {
                        win->buffer[y * win->width + x] = bg;
                    }
                }
                win->terminal_row = max_rows - 1;
            }
            return;
        }
        
        if (c == '\r') {
            win->terminal_col = 0;
            return;
        }
        
        if (c == '\t') {
            win->terminal_col = (win->terminal_col + 4) & ~3;
            if (win->terminal_col >= max_cols) {
                win->terminal_col = 0;
                if (++win->terminal_row >= max_rows) {
                    for (int y = 0; y < (int)(win->height - 16); y++) {
                        for (int x = 0; x < win->width; x++) {
                            win->buffer[y * win->width + x] = win->buffer[(y + 16) * win->width + x];
                        }
                    }
                    for (int y = win->height - 16; y < win->height; y++) {
                        for (int x = 0; x < win->width; x++) {
                            win->buffer[y * win->width + x] = bg;
                        }
                    }
                    win->terminal_row = max_rows - 1;
                }
            }
            return;
        }
        
        draw_char_in_window_buffer(win->buffer, win->width, c, win->terminal_col * 8, win->terminal_row * 16, fg, bg);
        
        if (++win->terminal_col >= max_cols) {
            win->terminal_col = 0;
            if (++win->terminal_row >= max_rows) {
                for (int y = 0; y < (int)(win->height - 16); y++) {
                    for (int x = 0; x < win->width; x++) {
                        win->buffer[y * win->width + x] = win->buffer[(y + 16) * win->width + x];
                    }
                }
                for (int y = win->height - 16; y < win->height; y++) {
                    for (int x = 0; x < win->width; x++) {
                        win->buffer[y * win->width + x] = bg;
                    }
                }
                win->terminal_row = max_rows - 1;
            }
        }
    } else {
        uint32_t max_cols = (boot_info.width - MARGIN_LEFT - MARGIN_RIGHT) / CHAR_WIDTH;
        uint32_t max_rows = (boot_info.height - MARGIN_TOP - MARGIN_BOTTOM) / CHAR_HEIGHT;
        
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
    Task* cur = get_current_task();
    Window* win = NULL;
    if (cur != NULL) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (windows[i].active && windows[i].owner == cur) {
                win = &windows[i];
                break;
            }
        }
    }
    if (win != NULL) {
        uint32_t max_cols = win->width / 8;
        uint32_t max_rows = win->height / 16;
        if (col < max_cols) win->terminal_col = col;
        if (row < max_rows) win->terminal_row = row;
    } else {
        uint32_t max_cols = (boot_info.width - MARGIN_LEFT - MARGIN_RIGHT) / CHAR_WIDTH;
        uint32_t max_rows = (boot_info.height - MARGIN_TOP - MARGIN_BOTTOM) / CHAR_HEIGHT;
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_bg);
        if (col < max_cols) terminal_col = col;
        if (row < max_rows) terminal_row = row;
        draw_rect(MARGIN_LEFT + terminal_col * CHAR_WIDTH, MARGIN_TOP + terminal_row * CHAR_HEIGHT + (CHAR_HEIGHT - 1), CHAR_WIDTH, 1, current_fg);
    }
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
    
    graphics_swap_buffers();
    
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
    
    // Active tasks count
    Task* head = get_task_list_head();
    uint32_t task_count = 0;
    if (head != NULL) {
        Task* curr = head;
        do {
            task_count++;
            curr = curr->next;
        } while (curr != head);
    }
    
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
    
    if (compositor_active) {
        // Redraw desktop background
        // 1. Wallpaper
        graphics_draw_gradient(0x070B19, 0x160A26);
        
        // 2. Empty container
        uint32_t container_x = MARGIN_LEFT - 8;
        uint32_t container_y = MARGIN_TOP - 8;
        uint32_t container_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT + 16;
        uint32_t container_h = boot_info.height - MARGIN_TOP - MARGIN_BOTTOM + 16;
        draw_rounded_rect(container_x + 12, container_y + 12, container_w, container_h, 12, 0x030305);
        draw_rounded_rect(container_x + 8, container_y + 8, container_w, container_h, 12, 0x050508);
        draw_rounded_rect(container_x + 4, container_y + 4, container_w, container_h, 12, 0x08080C);
        extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
        draw_rounded_rect_alpha(container_x, container_y, container_w, container_h, 12, 0x07080B, 180);
        draw_rounded_rect_outline(container_x, container_y, container_w, container_h, 12, 0x2D2E3D);
        
        // 3. Status bar
        {
            uint32_t bar_y = boot_info.height - 36;
            uint32_t bar_h = 30;
            uint32_t bar_x = MARGIN_LEFT;
            uint32_t bar_w = boot_info.width - MARGIN_LEFT - MARGIN_RIGHT;
            draw_rounded_rect(bar_x, bar_y, bar_w, bar_h, 8, 0x12131A);
            draw_rounded_rect_outline(bar_x, bar_y, bar_w, bar_h, 8, 0x2D2E3D);
            uint32_t seconds = get_ticks() / 100;
            Task* head = get_task_list_head();
            uint32_t task_count = 0;
            if (head != NULL) {
                Task* curr = head;
                do {
                    task_count++;
                    curr = curr->next;
                } while (curr != head);
            }
            extern uint32_t vmm_get_allocated_memory_mb(void);
            uint32_t mem_mb = vmm_get_allocated_memory_mb();
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
            int task_cx = bar_x + (bar_w / 2) - 80;
            int task_cy = bar_y + 15;
            draw_filled_circle(task_cx, task_cy - 3, 3, 0xE2E8F0);
            draw_rect(task_cx - 5, task_cy + 1, 11, 4, 0xE2E8F0);
            draw_pixel(task_cx - 6, task_cy + 3, 0xE2E8F0);
            draw_pixel(task_cx - 6, task_cy + 4, 0xE2E8F0);
            draw_pixel(task_cx + 6, task_cy + 3, 0xE2E8F0);
            draw_pixel(task_cx + 6, task_cy + 4, 0xE2E8F0);
            int ram_cx = bar_x + bar_w - 300;
            int ram_cy = bar_y + 15;
            draw_rect(ram_cx - 6, ram_cy - 5, 13, 1, 0x10B981);
            draw_rect(ram_cx - 6, ram_cy + 5, 13, 1, 0x10B981);
            draw_rect(ram_cx - 6, ram_cy - 5, 1, 11, 0x10B981);
            draw_rect(ram_cx + 6, ram_cy - 5, 1, 11, 0x10B981);
            draw_pixel(ram_cx - 8, ram_cy - 3, 0x10B981); draw_pixel(ram_cx - 7, ram_cy - 3, 0x10B981);
            draw_pixel(ram_cx - 8, ram_cy - 1, 0x10B981); draw_pixel(ram_cx - 7, ram_cy - 1, 0x10B981);
            draw_pixel(ram_cx - 8, ram_cy + 1, 0x10B981); draw_pixel(ram_cx - 7, ram_cy + 1, 0x10B981);
            draw_pixel(ram_cx - 8, ram_cy + 3, 0x10B981); draw_pixel(ram_cx - 7, ram_cy + 3, 0x10B981);
            draw_pixel(ram_cx + 7, ram_cy - 3, 0x10B981); draw_pixel(ram_cx + 8, ram_cy - 3, 0x10B981);
            draw_pixel(ram_cx + 7, ram_cy - 1, 0x10B981); draw_pixel(ram_cx + 8, ram_cy - 1, 0x10B981);
            draw_pixel(ram_cx + 7, ram_cy + 1, 0x10B981); draw_pixel(ram_cx + 8, ram_cy + 1, 0x10B981);
            draw_pixel(ram_cx + 7, ram_cy + 3, 0x10B981); draw_pixel(ram_cx + 8, ram_cy + 3, 0x10B981);
            uint32_t text_y = bar_y + 3;
            draw_string_at(upt_str, bar_x + 28, text_y, 0x00E5FF, 0x12131A);
            draw_string_at(task_str, bar_x + (bar_w / 2) - 60, text_y, 0xE2E8F0, 0x12131A);
            draw_string_at(mem_str, bar_x + bar_w - 280, text_y, 0x10B981, 0x12131A);
        }
        
        // 4. Draw windows
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (!windows[i].active) continue;
            
            draw_rect(windows[i].x + 4, windows[i].y - 20, windows[i].width, windows[i].height + 24, 0x050508);
            
            uint32_t border_color = (i == MAX_WINDOWS - 1) ? 0x00E5FF : 0x2D2E3D;
            draw_rect(windows[i].x - 1, windows[i].y - 25, windows[i].width + 2, windows[i].height + 26, border_color);
            
            draw_rect(windows[i].x, windows[i].y - 24, windows[i].width, 24, 0x12131A);
            
            draw_string_at_1x(windows[i].title, windows[i].x + 8, windows[i].y - 20, 0xE2E8F0, 0xFFFFFFFF);
            
            int cx = windows[i].x + windows[i].width - 22;
            int cy = windows[i].y - 20;
            draw_rect(cx, cy, 16, 16, 0xEF4444);
            draw_pixel(cx + 4, cy + 4, 0xFFFFFF); draw_pixel(cx + 11, cy + 4, 0xFFFFFF);
            draw_pixel(cx + 5, cy + 5, 0xFFFFFF); draw_pixel(cx + 10, cy + 5, 0xFFFFFF);
            draw_pixel(cx + 6, cy + 6, 0xFFFFFF); draw_pixel(cx + 9, cy + 6, 0xFFFFFF);
            draw_pixel(cx + 7, cy + 7, 0xFFFFFF); draw_pixel(cx + 8, cy + 7, 0xFFFFFF);
            draw_pixel(cx + 7, cy + 8, 0xFFFFFF); draw_pixel(cx + 8, cy + 8, 0xFFFFFF);
            draw_pixel(cx + 6, cy + 9, 0xFFFFFF); draw_pixel(cx + 9, cy + 9, 0xFFFFFF);
            draw_pixel(cx + 5, cy + 10, 0xFFFFFF); draw_pixel(cx + 10, cy + 10, 0xFFFFFF);
            draw_pixel(cx + 4, cy + 11, 0xFFFFFF); draw_pixel(cx + 11, cy + 11, 0xFFFFFF);
            
            for (int y = 0; y < windows[i].height; y++) {
                int dest_y = windows[i].y + y;
                if (dest_y < 0 || dest_y >= (int)boot_info.height) continue;
                for (int x = 0; x < windows[i].width; x++) {
                    int dest_x = windows[i].x + x;
                    if (dest_x < 0 || dest_x >= (int)boot_info.width) continue;
                    backbuffer[dest_y * boot_info.width + dest_x] = windows[i].buffer[y * windows[i].width + x];
                }
            }
        }
    }
    
    graphics_save_cursor_bg();
    draw_mouse_cursor();
    
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
    
    graphics_restore_cursor_bg();
}

void create_window_for_task(struct Task* owner, int w, int h, const char* title) {
    int slot = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;
    
    windows[slot].active = 1;
    windows[slot].owner = owner;
    windows[slot].width = w;
    windows[slot].height = h;
    windows[slot].x = (boot_info.width - w) / 2 + slot * 20;
    windows[slot].y = (boot_info.height - h) / 2 + slot * 20 - 20;
    
    if (windows[slot].x < (int)MARGIN_LEFT) windows[slot].x = MARGIN_LEFT;
    if (windows[slot].y < (int)MARGIN_TOP) windows[slot].y = MARGIN_TOP;
    
    int len = 0;
    while (title[len] != '\0' && len < 63) {
        windows[slot].title[len] = title[len];
        len++;
    }
    windows[slot].title[len] = '\0';
    
    windows[slot].buffer = (uint32_t*)kmalloc(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        windows[slot].buffer[i] = 0x0C0C12;
    }
    
    windows[slot].terminal_col = 0;
    windows[slot].terminal_row = 0;
    
    if (slot < MAX_WINDOWS - 1) {
        Window temp = windows[slot];
        for (int j = slot; j < MAX_WINDOWS - 1; j++) {
            windows[j] = windows[j + 1];
        }
        windows[MAX_WINDOWS - 1] = temp;
    }
    
    compositor_active = true;
}

void destroy_window_for_task(struct Task* owner) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].active && windows[i].owner == owner) {
            windows[i].active = 0;
            if (windows[i].buffer) {
                kfree(windows[i].buffer);
                windows[i].buffer = NULL;
            }
            windows[i].owner = NULL;
        }
    }
    
    bool any_active = false;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].active) {
            any_active = true;
            break;
        }
    }
    if (!any_active) {
        compositor_active = false;
    }
}

void graphics_toggle_mouse_button(void) {
    if (!mouse_button_down) {
        mouse_button_down = true;
        drag_window_idx = -1;
        
        for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
            if (!windows[i].active) continue;
            
            int title_x = windows[i].x;
            int title_y = windows[i].y - 24;
            int title_w = windows[i].width;
            int title_h = 24;
            
            int close_x = windows[i].x + windows[i].width - 22;
            int close_y = windows[i].y - 20;
            int close_w = 16;
            int close_h = 16;
            
            if (mouse_x >= close_x && mouse_x < close_x + close_w &&
                mouse_y >= close_y && mouse_y < close_y + close_h) {
                struct Task* owner = windows[i].owner;
                if (owner) {
                    extern void task_terminate(struct Task* task, int exit_code);
                    task_terminate(owner, 0);
                }
                mouse_button_down = false;
                break;
            }
            
            if (mouse_x >= title_x && mouse_x < title_x + title_w &&
                mouse_y >= title_y && mouse_y < title_y + title_h) {
                drag_window_idx = i;
                
                if (i < MAX_WINDOWS - 1) {
                    Window temp = windows[i];
                    for (int j = i; j < MAX_WINDOWS - 1; j++) {
                        windows[j] = windows[j + 1];
                    }
                    windows[MAX_WINDOWS - 1] = temp;
                    drag_window_idx = MAX_WINDOWS - 1;
                }
                break;
            }
        }
    } else {
        mouse_button_down = false;
        drag_window_idx = -1;
    }
    
    graphics_swap_buffers();
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

static void graphics_save_cursor_bg(void) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    for (int y = 0; y < 20; y++) {
        int py = mouse_y + y;
        for (int x = 0; x < 12; x++) {
            int px = mouse_x + x;
            if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                cursor_saved_bg[y * 12 + x] = backbuffer[py * width + px];
            } else {
                cursor_saved_bg[y * 12 + x] = current_bg;
            }
        }
    }
}

static void graphics_restore_cursor_bg(void) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    for (int y = 0; y < 20; y++) {
        int py = mouse_y + y;
        for (int x = 0; x < 12; x++) {
            int px = mouse_x + x;
            if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                backbuffer[py * width + px] = cursor_saved_bg[y * 12 + x];
            }
        }
    }
}

void draw_mouse_cursor(void) {
    static const char* mouse_cursor[20] = {
        "B           ",
        "BB          ",
        "BCB         ",
        "BCCB        ",
        "BCCCB       ",
        "BCCCCB      ",
        "BCCCCCB     ",
        "BCCCCCCB    ",
        "BCCCCCCB    ",
        "BCCBBBBBB   ",
        "BCB  BCB    ",
        "BB   BCB    ",
        "     BCB    ",
        "      B     ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            "
    };
    
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    for (int y = 0; y < 20; y++) {
        int py = mouse_y + y;
        if (py < 0 || py >= (int)height) continue;
        for (int x = 0; x < 12; x++) {
            int px = mouse_x + x;
            if (px < 0 || px >= (int)width) continue;
            
            char pixel_type = mouse_cursor[y][x];
            if (pixel_type == 'B') {
                backbuffer[py * width + px] = 0x000000;
            } else if (pixel_type == 'C') {
                backbuffer[py * width + px] = mouse_button_down ? 0xFFFF00 : 0x00E5FF;
            }
        }
    }
}

void graphics_move_mouse(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > (int)boot_info.width - 12) mouse_x = boot_info.width - 12;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > (int)boot_info.height - 20) mouse_y = boot_info.height - 20;
    
    if (mouse_button_down && drag_window_idx != -1) {
        windows[drag_window_idx].x += dx;
        windows[drag_window_idx].y += dy;
        
        if (windows[drag_window_idx].x < -windows[drag_window_idx].width + 20)
            windows[drag_window_idx].x = -windows[drag_window_idx].width + 20;
        if (windows[drag_window_idx].x > (int)boot_info.width - 20)
            windows[drag_window_idx].x = (int)boot_info.width - 20;
        if (windows[drag_window_idx].y < 0)
            windows[drag_window_idx].y = 0;
        if (windows[drag_window_idx].y > (int)boot_info.height - 40)
            windows[drag_window_idx].y = (int)boot_info.height - 40;
    }
    
    graphics_swap_buffers();
}

static void draw_launcher_menu_at(int lx, int ly) {
    draw_rounded_rect_alpha(lx, ly, 600, 180, 16, 0x12131A, 220);
    draw_rounded_rect_outline(lx, ly, 600, 180, 16, 0x00E5FF);
    
    draw_string_at("ZENITH SYSTEM LAUNCHER", lx + 180, ly + 15, 0x00E5FF, 0xFFFFFFFF);
    
    int card_w = 160;
    int card_h = 100;
    int card_y = ly + 50;
    
    // Card 1: Shell
    int c1_x = lx + 30;
    draw_rounded_rect(c1_x, card_y, card_w, card_h, 8, 0x1B1822);
    draw_rounded_rect_outline(c1_x, card_y, card_w, card_h, 8, 0x2D2E3D);
    draw_pixel(c1_x + 30, card_y + 20, 0x00E5FF);
    draw_pixel(c1_x + 31, card_y + 21, 0x00E5FF);
    draw_pixel(c1_x + 32, card_y + 22, 0x00E5FF);
    draw_pixel(c1_x + 31, card_y + 23, 0x00E5FF);
    draw_pixel(c1_x + 30, card_y + 24, 0x00E5FF);
    draw_rect(c1_x + 36, card_y + 24, 8, 2, 0x00E5FF);
    draw_string_at("sh.bin", c1_x + (card_w - 6 * CHAR_WIDTH)/2, card_y + 60, 0xE2E8F0, 0xFFFFFFFF);
    
    // Card 2: Calc
    int c2_x = lx + 220;
    draw_rounded_rect(c2_x, card_y, card_w, card_h, 8, 0x1B1822);
    draw_rounded_rect_outline(c2_x, card_y, card_w, card_h, 8, 0x2D2E3D);
    draw_rect(c2_x + 30, card_y + 22, 6, 2, 0x10B981);
    draw_rect(c2_x + 32, card_y + 20, 2, 6, 0x10B981);
    draw_rect(c2_x + 44, card_y + 21, 6, 2, 0x10B981);
    draw_rect(c2_x + 44, card_y + 24, 6, 2, 0x10B981);
    draw_string_at("calc.bin", c2_x + (card_w - 8 * CHAR_WIDTH)/2, card_y + 60, 0xE2E8F0, 0xFFFFFFFF);
    
    // Card 3: Blaster
    int c3_x = lx + 410;
    draw_rounded_rect(c3_x, card_y, card_w, card_h, 8, 0x1B1822);
    draw_rounded_rect_outline(c3_x, card_y, card_w, card_h, 8, 0x2D2E3D);
    draw_pixel(c3_x + 35, card_y + 18, 0xEF4444);
    draw_rect(c3_x + 34, card_y + 19, 3, 4, 0xE2E8F0);
    draw_rect(c3_x + 32, card_y + 22, 7, 2, 0xE2E8F0);
    draw_pixel(c3_x + 31, card_y + 24, 0x00E5FF);
    draw_pixel(c3_x + 39, card_y + 24, 0x00E5FF);
    draw_string_at("blaster", c3_x + (card_w - 7 * CHAR_WIDTH)/2, card_y + 60, 0xE2E8F0, 0xFFFFFFFF);
}

void graphics_toggle_launcher(void) {
    uint32_t width = boot_info.width;
    uint32_t height = boot_info.height;
    
    int target_x = (width - 600) / 2;
    int target_y = height - 36 - 180 - 10;
    
    if (!launcher_visible) {
        for (int y = 0; y < 180; y++) {
            int py = target_y + y;
            for (int x = 0; x < 600; x++) {
                int px = target_x + x;
                launcher_saved_bg[y * 600 + x] = backbuffer[py * width + px];
            }
        }
        
        launcher_visible = true;
        
        for (int step = 1; step <= 10; step++) {
            for (int y = 0; y < 180; y++) {
                int py = target_y + y;
                for (int x = 0; x < 600; x++) {
                    int px = target_x + x;
                    backbuffer[py * width + px] = launcher_saved_bg[y * 600 + x];
                }
            }
            
            int current_y = height - ((height - target_y) * step) / 10;
            draw_launcher_menu_at(target_x, current_y);
            graphics_swap_buffers();
            timer_wait(2);
        }
    } else {
        for (int step = 9; step >= 0; step--) {
            for (int y = 0; y < 180; y++) {
                int py = target_y + y;
                for (int x = 0; x < 600; x++) {
                    int px = target_x + x;
                    backbuffer[py * width + px] = launcher_saved_bg[y * 600 + x];
                }
            }
            
            int current_y = height - ((height - target_y) * step) / 10;
            if (step > 0) {
                draw_launcher_menu_at(target_x, current_y);
            }
            graphics_swap_buffers();
            timer_wait(2);
        }
        
        launcher_visible = false;
    }
}

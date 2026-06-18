#include "vfs.h"
#include "task.h"
#include "keyboard.h"
#include "graphics.h"
#include "zenithfs.h"
#include "heap.h"

// Forward declarations of standard helpers
static void local_memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
}

static int local_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void local_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

// --------------------------------------------------------------------------
// /dev/keyboard Driver Node
// --------------------------------------------------------------------------
static int keyboard_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    uint32_t count = 0;
    while (count < size) {
        char c = keyboard_getchar();
        if (c == '\b') {
            if (count > 0) {
                count--;
                print_char_default('\b');
                graphics_swap_buffers();
            }
            continue;
        }
        buffer[count++] = c;
        print_char_default(c);
        graphics_swap_buffers();
        if (c == '\n') break;
    }
    return count;
}

fs_node_t* vfs_create_keyboard_node(void) {
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    local_strcpy(node->name, "/dev/keyboard");
    node->flags = 1; // Device
    node->size = 0;
    node->inode = 0;
    node->offset = 0;
    node->read = keyboard_read;
    node->write = NULL;
    node->open = NULL;
    node->close = NULL;
    return node;
}

// --------------------------------------------------------------------------
// /dev/stdout and /dev/stderr Console Nodes
// --------------------------------------------------------------------------
static int console_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        print_char_default((char)buffer[i]);
    }
    return size;
}

fs_node_t* vfs_create_console_node(void) {
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    local_strcpy(node->name, "/dev/console");
    node->flags = 1; // Device
    node->size = 0;
    node->inode = 0;
    node->offset = 0;
    node->read = NULL;
    node->write = console_write;
    node->open = NULL;
    node->close = NULL;
    return node;
}

// --------------------------------------------------------------------------
// /dev/fb VESA Framebuffer Node
// --------------------------------------------------------------------------
extern uint32_t* get_backbuffer_ptr(void); // Declared in graphics.h / graphics.c
extern uint32_t get_screen_width(void);
extern uint32_t get_screen_height(void);

static int fb_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    uint32_t* bb = get_backbuffer_ptr();
    uint32_t width = get_screen_width();
    uint32_t height = get_screen_height();
    uint32_t fb_size = width * height * 4;
    
    if (offset >= fb_size) return 0;
    if (offset + size > fb_size) {
        size = fb_size - offset;
    }
    
    uint8_t* bb_bytes = (uint8_t*)bb;
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = bb_bytes[offset + i];
    }
    return size;
}

static int fb_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    uint32_t* bb = get_backbuffer_ptr();
    uint32_t width = get_screen_width();
    uint32_t height = get_screen_height();
    uint32_t fb_size = width * height * 4;
    
    if (offset >= fb_size) return 0;
    if (offset + size > fb_size) {
        size = fb_size - offset;
    }
    
    uint8_t* bb_bytes = (uint8_t*)bb;
    for (uint32_t i = 0; i < size; i++) {
        bb_bytes[offset + i] = buffer[i];
    }
    return size;
}

static fs_node_t* vfs_create_fb_node(void) {
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    local_strcpy(node->name, "/dev/fb");
    node->flags = 1; // Device
    node->size = get_screen_width() * get_screen_height() * 4;
    node->inode = 0;
    node->offset = 0;
    node->read = fb_read;
    node->write = fb_write;
    node->open = NULL;
    node->close = NULL;
    return node;
}

// --------------------------------------------------------------------------
// ZenithFS File Wrapper Nodes
// --------------------------------------------------------------------------
static int zfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) {
        size = node->size - offset;
    }
    if (node->inode == 0) return 0;
    
    uint8_t* file_buf = (uint8_t*)node->inode;
    local_memcpy(buffer, file_buf + offset, size);
    return size;
}

static int zfs_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset + size > node->size) {
        uint32_t new_size = offset + size;
        uint8_t* new_buf = (uint8_t*)kmalloc(new_size);
        if (node->size > 0 && node->inode != 0) {
            local_memcpy(new_buf, (void*)node->inode, node->size);
            kfree((void*)node->inode);
        }
        node->inode = (uint32_t)new_buf;
        node->size = new_size;
    }
    
    uint8_t* file_buf = (uint8_t*)node->inode;
    local_memcpy(file_buf + offset, buffer, size);
    return size;
}

static void zfs_close(fs_node_t* node) {
    if (node->inode != 0 && node->size > 0) {
        // Write file back to ZenithFS disk
        zenithfs_write_file(node->name, (const uint8_t*)node->inode, node->size);
        kfree((void*)node->inode);
    }
    kfree(node);
}

// --------------------------------------------------------------------------
// VFS Master API
// --------------------------------------------------------------------------
void vfs_init(void) {
    // Nothing special needed globally as nodes are instantiated on open
}

int vfs_open(const char* filename, int flags) {
    (void)flags;
    Task* cur = get_current_task();
    if (!cur) return -1;
    
    // Find free file descriptor slot
    int fd = -1;
    for (int i = 0; i < 16; i++) {
        if (cur->fd_table[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1; // Out of FDs
    
    fs_node_t* node = NULL;
    
    // Check dev paths
    if (local_strcmp(filename, "/dev/keyboard") == 0) {
        node = vfs_create_keyboard_node();
    } else if (local_strcmp(filename, "/dev/fb") == 0) {
        node = vfs_create_fb_node();
    } else if (local_strcmp(filename, "/dev/stdout") == 0 || local_strcmp(filename, "/dev/stderr") == 0) {
        node = vfs_create_console_node();
    } else {
        // Assume ZenithFS file
        // If path has a leading slash, strip it since ZenithFS files are flat
        const char* zfs_name = filename;
        if (filename[0] == '/') {
            zfs_name = filename + 1;
        }
        
        node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (!node) return -1;
        
        local_strcpy(node->name, zfs_name);
        node->flags = 2; // File
        node->offset = 0;
        node->read = zfs_read;
        node->write = zfs_write;
        node->open = NULL;
        node->close = zfs_close;
        
        int32_t fsize = zenithfs_get_file_size(zfs_name);
        if (fsize >= 0) {
            node->size = fsize;
            if (fsize > 0) {
                node->inode = (uint32_t)kmalloc(fsize);
                zenithfs_read_file(zfs_name, (uint8_t*)node->inode);
            } else {
                node->inode = 0;
            }
        } else {
            // New file creation
            node->size = 0;
            node->inode = 0;
        }
    }
    
    if (!node) return -1;
    
    cur->fd_table[fd] = node;
    return fd;
}

int vfs_close(int fd) {
    Task* cur = get_current_task();
    if (!cur || fd < 0 || fd >= 16) return -1;
    
    fs_node_t* node = cur->fd_table[fd];
    if (!node) return -1;
    
    if (node->close) {
        node->close(node);
    } else {
        kfree(node);
    }
    
    cur->fd_table[fd] = NULL;
    return 0;
}

int vfs_read(int fd, uint8_t* buffer, uint32_t size) {
    Task* cur = get_current_task();
    if (!cur || fd < 0 || fd >= 16) return -1;
    
    fs_node_t* node = cur->fd_table[fd];
    if (!node || !node->read) return -1;
    
    int bytes = node->read(node, node->offset, size, buffer);
    if (bytes > 0) {
        node->offset += bytes;
    }
    return bytes;
}

int vfs_write(int fd, uint8_t* buffer, uint32_t size) {
    Task* cur = get_current_task();
    if (!cur || fd < 0 || fd >= 16) return -1;
    
    fs_node_t* node = cur->fd_table[fd];
    if (!node || !node->write) return -1;
    
    int bytes = node->write(node, node->offset, size, buffer);
    if (bytes > 0) {
        node->offset += bytes;
    }
    return bytes;
}

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t size;
    uint32_t inode;
    uint32_t offset;
    int (*read)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    int (*write)(struct fs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    void (*open)(struct fs_node* node);
    void (*close)(struct fs_node* node);
} fs_node_t;

void vfs_init(void);
int vfs_open(const char* filename, int flags);
int vfs_close(int fd);
int vfs_read(int fd, uint8_t* buffer, uint32_t size);
int vfs_write(int fd, uint8_t* buffer, uint32_t size);

fs_node_t* vfs_create_keyboard_node(void);
fs_node_t* vfs_create_console_node(void);

#endif

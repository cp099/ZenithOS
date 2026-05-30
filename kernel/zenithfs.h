#ifndef ZENITHFS_H
#define ZENITHFS_H

#include <stdint.h>
#include <stdbool.h>

#define ZFS_MAGIC           "ZNTH"
#define ZFS_BLOCK_SIZE      512
#define ZFS_MAX_FILENAME    60

// File types (mode)
#define ZFS_TYPE_FREE       0
#define ZFS_TYPE_FILE       1
#define ZFS_TYPE_DIR        2

// ZenithFS Superblock layout (Sector 0)
struct zfs_superblock {
    char     magic[4];            // "ZNTH"
    uint32_t block_size;          // Typically 512
    uint32_t num_blocks;          // Total blocks in drive
    uint32_t num_inodes;          // Total inodes
    uint32_t inode_bitmap_block;  // Inode bitmap block index
    uint32_t block_bitmap_block;  // Block allocation bitmap block index
    uint32_t inode_table_block;   // Inode table block start index
    uint32_t data_start_block;    // Data blocks start index
} __attribute__((packed));

typedef struct zfs_superblock zfs_superblock_t;

// ZenithFS Inode layout (64 bytes)
struct zfs_inode {
    uint32_t mode;                // Type (file, dir, free)
    uint32_t size;                // Size in bytes
    uint32_t blocks_count;        // Block count allocated
    uint32_t direct[12];          // 12 direct block pointers
    uint32_t indirect;            // 1 indirect block pointer (points to block of pointers)
} __attribute__((packed));

typedef struct zfs_inode zfs_inode_t;

// ZenithFS Directory Entry (64 bytes)
struct zfs_dirent {
    char     name[ZFS_MAX_FILENAME]; // Null-terminated filename
    uint32_t inode_num;           // Inode index mapping
} __attribute__((packed));

typedef struct zfs_dirent zfs_dirent_t;

// Initialize and mount ZenithFS from primary slave drive
bool zenithfs_mount(void);

// Read entire file content into buffer (returns bytes read, or -1 on error)
int32_t zenithfs_read_file(const char* filename, uint8_t* buffer);

// Print all filenames in root directory to console
void zenithfs_list_directory(void);

#endif

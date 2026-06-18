#include "zenithfs.h"
#include "ata.h"
#include "graphics.h"
#include <stddef.h>

static zfs_superblock_t sb;
static bool mounted = false;

// String copy helper
static void local_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

// String comparison helper
static int local_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Memory zero helper
static void local_memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0) {
        *ptr++ = (unsigned char)val;
    }
}

// Memory copy helper
static void* local_memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

// Read an inode from disk
static bool read_inode(uint32_t num, zfs_inode_t* inode) {
    if (!mounted || num >= sb.num_inodes) return false;
    
    // 8 inodes per 512-byte sector
    uint32_t sector = sb.inode_table_block + (num / 8);
    uint32_t offset = (num % 8) * sizeof(zfs_inode_t);
    
    uint8_t buffer[512];
    ata_read_sectors(sector, 1, buffer);
    
    // Copy the 64-byte inode
    local_memcpy(inode, buffer + offset, sizeof(zfs_inode_t));
    return true;
}

// Write an inode to disk
static bool write_inode(uint32_t num, const zfs_inode_t* inode) {
    if (!mounted || num >= sb.num_inodes) return false;
    
    uint32_t sector = sb.inode_table_block + (num / 8);
    uint32_t offset = (num % 8) * sizeof(zfs_inode_t);
    
    uint8_t buffer[512];
    ata_read_sectors(sector, 1, buffer);
    
    local_memcpy(buffer + offset, inode, sizeof(zfs_inode_t));
    ata_write_sectors(sector, 1, buffer);
    return true;
}

// Automatic formatting of an empty disk with a dummy file system
static void format_dummy_filesystem(void) {
    print_string_default("  [!] Magic \"ZNTH\" not found. Formatting ZenithFS...\n");
    
    // Initialize Superblock
    sb.magic[0] = 'Z'; sb.magic[1] = 'N'; sb.magic[2] = 'T'; sb.magic[3] = 'H';
    sb.block_size = ZFS_BLOCK_SIZE;
    sb.num_blocks = 20480;       // 10MB
    sb.num_inodes = 64;
    sb.inode_bitmap_block = 1;
    sb.block_bitmap_block = 2;
    sb.inode_table_block = 3;    // Inode table: blocks 3 to 10 (8 blocks for 64 inodes)
    sb.data_start_block = 11;    // Data blocks start at block 11
    
    // Temporarily set mounted to true to write GDT structures
    mounted = true;
    
    // Write superblock to LBA 0
    uint8_t sector_buf[512];
    local_memset(sector_buf, 0, 512);
    local_memcpy(sector_buf, &sb, sizeof(zfs_superblock_t));
    ata_write_sectors(0, 1, sector_buf);
    
    // Setup Root Directory Inode (Inode 0)
    zfs_inode_t root_inode;
    local_memset(&root_inode, 0, sizeof(root_inode));
    root_inode.mode = ZFS_TYPE_DIR;
    root_inode.size = sizeof(zfs_dirent_t); // Size of 1 directory entry (64 bytes)
    root_inode.blocks_count = 1;
    root_inode.direct[0] = 11;    // Data in Block 11
    write_inode(0, &root_inode);
    
    // Setup hello.txt Inode (Inode 1)
    zfs_inode_t hello_inode;
    local_memset(&hello_inode, 0, sizeof(hello_inode));
    hello_inode.mode = ZFS_TYPE_FILE;
    hello_inode.size = 32;       // Text size
    hello_inode.blocks_count = 1;
    hello_inode.direct[0] = 12;   // Data in Block 12
    write_inode(1, &hello_inode);
    
    // Setup Root Directory Contents (Block 11)
    local_memset(sector_buf, 0, 512);
    zfs_dirent_t* dirent = (zfs_dirent_t*)sector_buf;
    local_strcpy(dirent[0].name, "hello.txt");
    dirent[0].inode_num = 1;
    ata_write_sectors(11, 1, sector_buf);
    
    // Setup hello.txt Contents (Block 12)
    local_memset(sector_buf, 0, 512);
    local_memcpy(sector_buf, "Hello from ZenithFS File System!", 32);
    ata_write_sectors(12, 1, sector_buf);
    
    print_string_default("  [+] ZenithFS formatting completed.\n");
}

bool zenithfs_mount(void) {
    uint8_t buffer[512];
    ata_read_sectors(0, 1, buffer);
    
    local_memcpy(&sb, buffer, sizeof(zfs_superblock_t));
    
    // Verify magic signature
    if (sb.magic[0] == 'Z' && sb.magic[1] == 'N' && sb.magic[2] == 'T' && sb.magic[3] == 'H') {
        mounted = true;
    } else {
        mounted = true; // Set temporarily for formatting
        format_dummy_filesystem();
        mounted = true;
    }
    
    return mounted;
}


void zenithfs_list_directory(void) {
    if (!mounted) return;
    
    zfs_inode_t root_inode;
    if (!read_inode(0, &root_inode)) {
        print_string_default("Error: Failed to read root directory inode.\n");
        return;
    }
    
    print_string_default("Directory listing for / :\n");
    
    uint8_t buffer[512];
    // Loop through root directory data blocks
    for (uint32_t i = 0; i < 12; i++) {
        if (root_inode.direct[i] == 0) break;
        
        ata_read_sectors(root_inode.direct[i], 1, buffer);
        zfs_dirent_t* dirents = (zfs_dirent_t*)buffer;
        
        // 8 directory entries per block
        for (int d = 0; d < 8; d++) {
            if (dirents[d].inode_num != 0) {
                print_string_default("  - ");
                print_string_default(dirents[d].name);
                print_string_default("  [Inode ");
                
                // Print Inode Number correctly
                uint32_t val = dirents[d].inode_num;
                char ibuf[12];
                int idx = 0;
                if (val == 0) {
                    ibuf[idx++] = '0';
                } else {
                    char temp[12];
                    int t_idx = 0;
                    while (val > 0) {
                        temp[t_idx++] = '0' + (val % 10);
                        val /= 10;
                    }
                    while (t_idx > 0) {
                        ibuf[idx++] = temp[--t_idx];
                    }
                }
                ibuf[idx++] = ']';
                ibuf[idx++] = '\n';
                ibuf[idx] = '\0';
                print_string_default(ibuf);
            }
        }
    }
}

int32_t zenithfs_read_file(const char* filename, uint8_t* buffer) {
    if (!mounted) return -1;
    
    zfs_inode_t root_inode;
    if (!read_inode(0, &root_inode)) return -1;
    
    uint8_t sector_buf[512];
    uint32_t target_inode_num = 0xFFFFFFFF;
    
    // 1. Search directory entries for filename
    for (uint32_t i = 0; i < 12; i++) {
        if (root_inode.direct[i] == 0) break;
        
        ata_read_sectors(root_inode.direct[i], 1, sector_buf);
        zfs_dirent_t* dirents = (zfs_dirent_t*)sector_buf;
        
        for (int d = 0; d < 8; d++) {
            if (dirents[d].inode_num != 0 && local_strcmp(dirents[d].name, filename) == 0) {
                target_inode_num = dirents[d].inode_num;
                break;
            }
        }
        if (target_inode_num != 0xFFFFFFFF) break;
    }
    
    if (target_inode_num == 0xFFFFFFFF) {
        return -1; // File not found
    }
    
    // 2. Read file inode
    zfs_inode_t file_inode;
    if (!read_inode(target_inode_num, &file_inode)) return -1;
    
    // 3. Read data blocks into buffer
    uint32_t bytes_to_read = file_inode.size;
    uint32_t bytes_read = 0;
    
    for (uint32_t i = 0; i < 12; i++) {
        if (file_inode.direct[i] == 0 || bytes_read >= bytes_to_read) break;
        
        uint8_t temp[512];
        ata_read_sectors(file_inode.direct[i], 1, temp);
        
        uint32_t chunk = (bytes_to_read - bytes_read > 512) ? 512 : (bytes_to_read - bytes_read);
        local_memcpy(buffer + bytes_read, temp, chunk);
        bytes_read += chunk;
    }
    
    // Read from indirect block if needed
    if (bytes_read < bytes_to_read && file_inode.indirect != 0) {
        uint32_t indirect_block[128];
        ata_read_sectors(file_inode.indirect, 1, (uint8_t*)indirect_block);
        
        for (uint32_t i = 0; i < 128; i++) {
            if (indirect_block[i] == 0 || bytes_read >= bytes_to_read) break;
            
            uint8_t temp[512];
            ata_read_sectors(indirect_block[i], 1, temp);
            
            uint32_t chunk = (bytes_to_read - bytes_read > 512) ? 512 : (bytes_to_read - bytes_read);
            local_memcpy(buffer + bytes_read, temp, chunk);
            bytes_read += chunk;
        }
    }
    
    return bytes_read;
}

int32_t zenithfs_get_file_size(const char* filename) {
    if (!mounted) return -1;
    
    zfs_inode_t root_inode;
    if (!read_inode(0, &root_inode)) return -1;
    
    uint8_t sector_buf[512];
    uint32_t target_inode_num = 0xFFFFFFFF;
    
    // 1. Search directory entries for filename
    for (uint32_t i = 0; i < 12; i++) {
        if (root_inode.direct[i] == 0) break;
        
        ata_read_sectors(root_inode.direct[i], 1, sector_buf);
        zfs_dirent_t* dirents = (zfs_dirent_t*)sector_buf;
        
        for (int d = 0; d < 8; d++) {
            if (dirents[d].inode_num != 0 && local_strcmp(dirents[d].name, filename) == 0) {
                target_inode_num = dirents[d].inode_num;
                break;
            }
        }
        if (target_inode_num != 0xFFFFFFFF) break;
    }
    
    if (target_inode_num == 0xFFFFFFFF) {
        return -1; // File not found
    }
    
    // 2. Read file inode
    zfs_inode_t file_inode;
    if (!read_inode(target_inode_num, &file_inode)) return -1;
    
    return (int32_t)file_inode.size;
}

static void delete_file_if_exists(const char* filename) {
    zfs_inode_t root_inode;
    if (!read_inode(0, &root_inode)) return;
    
    uint8_t sector_buf[512];
    for (uint32_t i = 0; i < 12; i++) {
        if (root_inode.direct[i] == 0) break;
        
        ata_read_sectors(root_inode.direct[i], 1, sector_buf);
        zfs_dirent_t* dirents = (zfs_dirent_t*)sector_buf;
        
        for (int d = 0; d < 8; d++) {
            if (dirents[d].inode_num != 0 && local_strcmp(dirents[d].name, filename) == 0) {
                uint32_t inode_num = dirents[d].inode_num;
                zfs_inode_t file_inode;
                if (read_inode(inode_num, &file_inode)) {
                    // Free blocks in block bitmap
                    uint8_t block_bitmap[512];
                    ata_read_sectors(sb.block_bitmap_block, 1, block_bitmap);
                    
                    // Free direct blocks
                    for (uint32_t b = 0; b < 12; b++) {
                        uint32_t blk = file_inode.direct[b];
                        if (blk != 0 && blk < 4096) {
                            block_bitmap[blk / 8] &= ~(1 << (blk % 8));
                        }
                    }
                    
                    // Free indirect block and its pointed blocks
                    if (file_inode.indirect != 0 && file_inode.indirect < 4096) {
                        uint32_t indirect_block[128];
                        ata_read_sectors(file_inode.indirect, 1, (uint8_t*)indirect_block);
                        for (uint32_t b = 0; b < 128; b++) {
                            uint32_t blk = indirect_block[b];
                            if (blk != 0 && blk < 4096) {
                                block_bitmap[blk / 8] &= ~(1 << (blk % 8));
                            }
                        }
                        // Free the indirect block itself
                        uint32_t blk = file_inode.indirect;
                        block_bitmap[blk / 8] &= ~(1 << (blk % 8));
                    }
                    
                    // Write back block bitmap
                    ata_write_sectors(sb.block_bitmap_block, 1, block_bitmap);
                    
                    // Free inode in inode bitmap
                    uint8_t inode_bitmap[512];
                    ata_read_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
                    inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
                    ata_write_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
                    
                    // Clear inode table entry
                    local_memset(&file_inode, 0, sizeof(zfs_inode_t));
                    write_inode(inode_num, &file_inode);
                }
                
                // Clear directory entry
                dirents[d].inode_num = 0;
                local_memset(dirents[d].name, 0, ZFS_MAX_FILENAME);
                ata_write_sectors(root_inode.direct[i], 1, sector_buf);
                
                // Decrease root inode size
                if (read_inode(0, &root_inode)) {
                    if (root_inode.size >= 64) {
                        root_inode.size -= 64;
                        write_inode(0, &root_inode);
                    }
                }
                return;
            }
        }
    }
}

static int get_free_inode(void) {
    uint8_t bitmap[512];
    ata_read_sectors(sb.inode_bitmap_block, 1, bitmap);
    for (uint32_t i = 0; i < sb.num_inodes; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            ata_write_sectors(sb.inode_bitmap_block, 1, bitmap);
            return (int)i;
        }
    }
    return -1;
}

static int get_free_blocks(uint32_t* blocks, uint32_t count) {
    uint8_t bitmap[512];
    ata_read_sectors(sb.block_bitmap_block, 1, bitmap);
    uint32_t found = 0;
    uint32_t limit = sb.num_blocks < 4096 ? sb.num_blocks : 4096;
    for (uint32_t i = 12; i < limit; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            blocks[found++] = i;
            if (found == count) {
                ata_write_sectors(sb.block_bitmap_block, 1, bitmap);
                return 0; // Success
            }
        }
    }
    return -1; // Out of blocks
}

int32_t zenithfs_write_file(const char* filename, const uint8_t* buffer, uint32_t size) {
    if (!mounted) return -1;
    if (size > 140 * 512) return -1; // Max size supported (70KB)
    
    // 1. Delete if it exists to reuse/overwrite cleanly
    delete_file_if_exists(filename);
    
    // 2. Calculate blocks needed
    uint32_t blocks_needed = (size + 511) / 512;
    uint32_t total_blocks_to_allocate = blocks_needed;
    if (blocks_needed > 12) {
        total_blocks_to_allocate += 1;
    }
    
    // 3. Find a free inode
    int inode_num = get_free_inode();
    if (inode_num < 0) return -1;
    
    // 4. Find free blocks
    uint32_t allocated_blocks[141];
    if (total_blocks_to_allocate > 0) {
        if (get_free_blocks(allocated_blocks, total_blocks_to_allocate) < 0) {
            // Rollback inode allocation
            uint8_t inode_bitmap[512];
            ata_read_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
            inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
            ata_write_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
            return -1;
        }
    }
    
    // 5. Write data to blocks
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t block = allocated_blocks[i];
        uint8_t chunk[512];
        local_memset(chunk, 0, 512);
        
        uint32_t bytes_to_copy = size - (i * 512);
        if (bytes_to_copy > 512) bytes_to_copy = 512;
        local_memcpy(chunk, buffer + (i * 512), bytes_to_copy);
        
        ata_write_sectors(block, 1, chunk);
    }
    
    // 6. Handle indirect block if needed
    uint32_t indirect_block_num = 0;
    if (blocks_needed > 12) {
        indirect_block_num = allocated_blocks[total_blocks_to_allocate - 1];
        uint32_t indirect_ptrs[128];
        local_memset(indirect_ptrs, 0, sizeof(indirect_ptrs));
        
        for (uint32_t i = 12; i < blocks_needed; i++) {
            indirect_ptrs[i - 12] = allocated_blocks[i];
        }
        
        ata_write_sectors(indirect_block_num, 1, (uint8_t*)indirect_ptrs);
    }
    
    // 7. Write inode
    zfs_inode_t inode;
    local_memset(&inode, 0, sizeof(zfs_inode_t));
    inode.mode = ZFS_TYPE_FILE;
    inode.size = size;
    inode.blocks_count = total_blocks_to_allocate;
    for (uint32_t i = 0; i < 12; i++) {
        if (i < blocks_needed) {
            inode.direct[i] = allocated_blocks[i];
        } else {
            inode.direct[i] = 0;
        }
    }
    inode.indirect = indirect_block_num;
    write_inode(inode_num, &inode);
    
    // 8. Add directory entry
    zfs_inode_t root_inode;
    if (!read_inode(0, &root_inode)) return -1;
    
    bool added = false;
    uint8_t sector_buf[512];
    for (uint32_t i = 0; i < 12; i++) {
        if (root_inode.direct[i] == 0) break;
        
        ata_read_sectors(root_inode.direct[i], 1, sector_buf);
        zfs_dirent_t* dirents = (zfs_dirent_t*)sector_buf;
        
        for (int d = 0; d < 8; d++) {
            if (dirents[d].inode_num == 0) {
                local_memset(dirents[d].name, 0, ZFS_MAX_FILENAME);
                local_strcpy(dirents[d].name, filename);
                dirents[d].inode_num = inode_num;
                ata_write_sectors(root_inode.direct[i], 1, sector_buf);
                added = true;
                break;
            }
        }
        if (added) break;
    }
    
    if (!added) {
        // Root directory full! Reclaim everything
        // (Free inode)
        uint8_t inode_bitmap[512];
        ata_read_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
        inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
        ata_write_sectors(sb.inode_bitmap_block, 1, inode_bitmap);
        
        // (Free blocks)
        uint8_t block_bitmap[512];
        ata_read_sectors(sb.block_bitmap_block, 1, block_bitmap);
        for (uint32_t i = 0; i < total_blocks_to_allocate; i++) {
            uint32_t blk = allocated_blocks[i];
            block_bitmap[blk / 8] &= ~(1 << (blk % 8));
        }
        ata_write_sectors(sb.block_bitmap_block, 1, block_bitmap);
        return -1;
    }
    
    // 9. Update root directory inode size
    if (read_inode(0, &root_inode)) {
        root_inode.size += 64;
        write_inode(0, &root_inode);
    }
    
    return (int32_t)size;
}



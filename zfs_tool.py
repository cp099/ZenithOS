#!/usr/bin/env python3
import sys
import os
import struct

BLOCK_SIZE = 512
NUM_BLOCKS = 20480  # 10MB
NUM_INODES = 64

# ZenithFS types
ZFS_TYPE_FREE = 0
ZFS_TYPE_FILE = 1
ZFS_TYPE_DIR  = 2

# Disk Layout
SUPERBLOCK_BLOCK = 0
INODE_BITMAP_BLOCK = 1
BLOCK_BITMAP_BLOCK = 2
INODE_TABLE_BLOCK = 3
DATA_START_BLOCK = 11

def format_disk(disk_path):
    print(f"Formatting {disk_path} with ZenithFS...")
    # Create a clean 10MB image
    with open(disk_path, "wb") as f:
        f.write(b"\x00" * (NUM_BLOCKS * BLOCK_SIZE))
    
    # Write Superblock (Sector 0)
    # layout: magic(4s), block_size(I), num_blocks(I), num_inodes(I),
    #         inode_bitmap(I), block_bitmap(I), inode_table(I), data_start(I)
    sb_data = struct.pack("<4sIIIIIII", b"ZNTH", BLOCK_SIZE, NUM_BLOCKS, NUM_INODES,
                          INODE_BITMAP_BLOCK, BLOCK_BITMAP_BLOCK, INODE_TABLE_BLOCK, DATA_START_BLOCK)
    
    # Write Inode Bitmap (Sector 1) - mark Inode 0 as used (1)
    inode_bitmap = bytearray(512)
    inode_bitmap[0] = 0x01  # Inode 0 (Root Dir) is used
    
    # Write Block Bitmap (Sector 2) - mark Blocks 0 to 11 as used (bits 0 to 11 set to 1)
    block_bitmap = bytearray(512)
    # block 11 is root directory data, blocks 0 to 10 are metadata
    # 12 blocks used -> bits 0..11 set -> 0x0FFF in LSB
    block_bitmap[0] = 0xFF
    block_bitmap[1] = 0x0F
    
    # Write Root Directory Inode (Inode index 0 in Sector 3)
    # layout: mode(I), size(I), blocks_count(I), direct(12I), indirect(I)
    # Root dir size is initially 0 directory entries, direct[0] = 11
    root_inode = struct.pack("<III12II", ZFS_TYPE_DIR, 0, 1,
                             11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    
    # Write to disk
    with open(disk_path, "r+b") as f:
        # Superblock
        f.seek(SUPERBLOCK_BLOCK * BLOCK_SIZE)
        f.write(sb_data)
        # Inode Bitmap
        f.seek(INODE_BITMAP_BLOCK * BLOCK_SIZE)
        f.write(inode_bitmap)
        # Block Bitmap
        f.seek(BLOCK_BITMAP_BLOCK * BLOCK_SIZE)
        f.write(block_bitmap)
        # Inode Table (Inode 0 starts at beginning of table)
        f.seek(INODE_TABLE_BLOCK * BLOCK_SIZE)
        f.write(root_inode)
        
    print("Formatting complete. Root directory initialized.")

def get_free_inode(f):
    f.seek(INODE_BITMAP_BLOCK * BLOCK_SIZE)
    bitmap = bytearray(f.read(BLOCK_SIZE))
    for i in range(NUM_INODES):
        byte_idx = i // 8
        bit_idx = i % 8
        if not (bitmap[byte_idx] & (1 << bit_idx)):
            # Mark as used
            bitmap[byte_idx] |= (1 << bit_idx)
            f.seek(INODE_BITMAP_BLOCK * BLOCK_SIZE)
            f.write(bitmap)
            return i
    raise Exception("Out of inodes!")

def get_free_blocks(f, count):
    f.seek(BLOCK_BITMAP_BLOCK * BLOCK_SIZE)
    bitmap = bytearray(f.read(BLOCK_SIZE))
    blocks = []
    # Start looking from block 12 (data start block is 11, 11 is used for root dir)
    for i in range(12, NUM_BLOCKS):
        byte_idx = i // 8
        bit_idx = i % 8
        if not (bitmap[byte_idx] & (1 << bit_idx)):
            bitmap[byte_idx] |= (1 << bit_idx)
            blocks.append(i)
            if len(blocks) == count:
                f.seek(BLOCK_BITMAP_BLOCK * BLOCK_SIZE)
                f.write(bitmap)
                return blocks
    raise Exception("Out of data blocks!")

def add_file(disk_path, host_file, zfs_name):
    if len(zfs_name) >= 60:
        raise Exception("Filename too long!")
    
    with open(host_file, "rb") as hf:
        data = hf.read()
    
    size = len(data)
    blocks_needed = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
    if blocks_needed > 140:
        raise Exception("File too large! Max 140 blocks (70KB) supported with single indirect addressing.")
    
    total_blocks_to_allocate = blocks_needed
    if blocks_needed > 12:
        total_blocks_to_allocate += 1  # 1 extra block for the indirect pointers table
        
    print(f"Adding file {host_file} as {zfs_name} (size: {size} bytes, blocks: {blocks_needed}, allocated: {total_blocks_to_allocate})...")
    
    with open(disk_path, "r+b") as f:
        # Get a free inode
        inode_num = get_free_inode(f)
        
        # Get free blocks
        blocks = get_free_blocks(f, total_blocks_to_allocate) if total_blocks_to_allocate > 0 else []
        
        # Write data to blocks
        data_blocks = blocks[:blocks_needed]
        for i, block in enumerate(data_blocks):
            f.seek(block * BLOCK_SIZE)
            chunk = data[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            # Pad chunk to 512 bytes
            if len(chunk) < BLOCK_SIZE:
                chunk += b"\x00" * (BLOCK_SIZE - len(chunk))
            f.write(chunk)
            
        indirect_block_num = 0
        if blocks_needed > 12:
            # The last block in the list is the indirect block
            indirect_block_num = blocks[-1]
            # Fill it with the extra block numbers (beyond direct[12])
            indirect_ptrs = data_blocks[12:]
            # Pad to 128 elements with zeros
            indirect_ptrs += [0] * (128 - len(indirect_ptrs))
            # Pack as 128 unsigned ints
            indirect_data = struct.pack("<128I", *indirect_ptrs)
            f.seek(indirect_block_num * BLOCK_SIZE)
            f.write(indirect_data)
            
        # Write Inode entry
        # direct pointers list must have 12 entries
        direct_ptrs = data_blocks[:12]
        direct = direct_ptrs + [0] * (12 - len(direct_ptrs))
        inode_data = struct.pack("<III12II", ZFS_TYPE_FILE, size, total_blocks_to_allocate, *direct, indirect_block_num)
        
        f.seek(INODE_TABLE_BLOCK * BLOCK_SIZE + inode_num * 64)
        f.write(inode_data)

        
        # Add entry to root directory directory structure (block 11)
        f.seek(11 * BLOCK_SIZE)
        dirents_data = f.read(BLOCK_SIZE)
        # Read all dirents (8 entries of 64 bytes each)
        dirents = []
        for i in range(8):
            entry = dirents_data[i*64 : (i+1)*64]
            name, inode = struct.unpack("<60sI", entry)
            # Decode name, strip nulls
            name = name.split(b"\x00")[0].decode("ascii", errors="ignore")
            dirents.append((name, inode))
            
        # Find empty entry
        free_slot = -1
        for idx, (name, inode) in enumerate(dirents):
            if inode == 0:
                free_slot = idx
                break
                
        if free_slot == -1:
            raise Exception("Root directory is full (max 8 files)!")
            
        # Encode new entry
        name_bytes = zfs_name.encode("ascii")
        name_bytes = name_bytes + b"\x00" * (60 - len(name_bytes))
        new_entry = struct.pack("<60sI", name_bytes, inode_num)
        
        f.seek(11 * BLOCK_SIZE + free_slot * 64)
        f.write(new_entry)
        
        # Update Root Directory Inode (increase size by 64 bytes)
        # Read Root Inode
        f.seek(INODE_TABLE_BLOCK * BLOCK_SIZE)
        root_inode_data = f.read(64)
        mode, root_size, blocks_count, *rest = struct.unpack("<III12II", root_inode_data)
        root_size += 64
        updated_root_inode = struct.pack("<III12II", mode, root_size, blocks_count, *rest)
        f.seek(INODE_TABLE_BLOCK * BLOCK_SIZE)
        f.write(updated_root_inode)
        
    print(f"File successfully added at Inode {inode_num}.")

def list_files(disk_path):
    print(f"Listing files in {disk_path}:")
    with open(disk_path, "rb") as f:
        f.seek(11 * BLOCK_SIZE)
        dirents_data = f.read(BLOCK_SIZE)
        for i in range(8):
            entry = dirents_data[i*64 : (i+1)*64]
            name_bytes, inode = struct.unpack("<60sI", entry)
            if inode != 0:
                name = name_bytes.split(b"\x00")[0].decode("ascii", errors="ignore")
                # Read inode size
                f.seek(INODE_TABLE_BLOCK * BLOCK_SIZE + inode * 64)
                inode_data = f.read(12)
                mode, size, blocks_count = struct.unpack("<III", inode_data)
                print(f"  - {name} (Inode: {inode}, Size: {size} bytes, Blocks: {blocks_count})")

def main():
    if len(sys.argv) < 3:
        print("Usage:")
        print("  zfs_tool.py <disk_image> format")
        print("  zfs_tool.py <disk_image> add <host_file> <zfs_name>")
        print("  zfs_tool.py <disk_image> ls")
        sys.exit(1)
        
    disk_path = sys.argv[1]
    cmd = sys.argv[2]
    
    if cmd == "format":
        format_disk(disk_path)
    elif cmd == "add":
        if len(sys.argv) < 5:
            print("Usage: zfs_tool.py <disk_image> add <host_file> <zfs_name>")
            sys.exit(1)
        host_file = sys.argv[3]
        zfs_name = sys.argv[4]
        add_file(disk_path, host_file, zfs_name)
    elif cmd == "ls":
        list_files(disk_path)
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)

if __name__ == "__main__":
    main()

#include "paging.h"
#include "graphics.h"
#include <stddef.h>

// We support 128MB of physical memory (32,768 pages of 4KB)
#define MEM_SIZE_MAX    (128 * 1024 * 1024)
#define PAGE_SIZE       4096
#define TOTAL_PAGES     (MEM_SIZE_MAX / PAGE_SIZE)
#define BITMAP_SIZE     (TOTAL_PAGES / 32)

// Physical memory page allocation bitmap
static uint32_t pmm_bitmap[BITMAP_SIZE];

// Statically allocate Page Directory and the first 32 Page Tables (to identity map 128MB)
__attribute__((aligned(4096))) static uint32_t page_directory[1024];
__attribute__((aligned(4096))) static uint32_t identity_page_tables[32][1024];


// Bitmap helpers
static inline void bitmap_set(uint32_t page_index) {
    pmm_bitmap[page_index / 32] |= (1 << (page_index % 32));
}

static inline void bitmap_clear(uint32_t page_index) {
    pmm_bitmap[page_index / 32] &= ~(1 << (page_index % 32));
}

static inline bool bitmap_test(uint32_t page_index) {
    return (pmm_bitmap[page_index / 32] & (1 << (page_index % 32))) != 0;
}

// Memory copy helper
static void local_memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) {
        *d++ = *s++;
    }
}

// Allocate a physical frame (above 16MB to avoid kernel overwrite)
uint32_t pmm_alloc_frame(void) {
    // Start searching from page index 4096 (16MB mark)
    for (uint32_t i = 4096; i < TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return i * PAGE_SIZE;
        }
    }
    // Out of memory!
    print_string_default("\nCRITICAL: Physical Memory Manager - Out of page frames!");
    while(1) { __asm__ volatile("hlt"); }
    return 0;
}

void pmm_free_frame(uint32_t frame_phys) {
    uint32_t page_index = frame_phys / PAGE_SIZE;
    if (page_index < TOTAL_PAGES) {
        bitmap_clear(page_index);
    }
}

// Map a page virtual to physical in the directory
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    uint32_t dir_idx = virt_addr >> 22;
    uint32_t tbl_idx = (virt_addr >> 12) & 0x3FF;
    
    // Check if page table is present in the directory
    if (!(page_directory[dir_idx] & PAGE_PRESENT)) {
        // Allocate a page table from physical memory (will be inside identity mapped 16MB)
        uint32_t pt_phys = pmm_alloc_frame();
        uint32_t* pt_virt = (uint32_t*)pt_phys;
        
        // Zero out the page table entries
        for (int i = 0; i < 1024; i++) {
            pt_virt[i] = 0;
        }
        
        // Link page table in directory
        page_directory[dir_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | flags;
    }
    
    // Extract physical address of the page table
    uint32_t* page_table = (uint32_t*)(page_directory[dir_idx] & 0xFFFFF000);
    
    // Set page table entry
    page_table[tbl_idx] = (phys_addr & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITE | flags;
}

void paging_init(void) {
    // 1. Initialize PMM bitmap
    // Mark all frames as used (1)
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    
    // Free frames from 16MB (page index 4096) up to 128MB
    for (uint32_t i = 4096; i < TOTAL_PAGES; i++) {
        bitmap_clear(i);
    }
    
    // 2. Setup VMM page directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0; // Set to not present
    }
    
    // 3. Identity map first 128MB of RAM (indices 0 to 31 in directory)
    for (uint32_t table = 0; table < 32; table++) {
        for (uint32_t entry = 0; entry < 1024; entry++) {
            uint32_t phys_addr = (table * 4 * 1024 * 1024) + (entry * PAGE_SIZE);
            // Present, Writable, User-accessible
            identity_page_tables[table][entry] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
        }
        // Link table in directory
        page_directory[table] = ((uint32_t)&identity_page_tables[table]) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    
    // 4. Map the physical framebuffer pages to make them writable
    struct BootInfo binfo;
    local_memcpy(&binfo, (const void*)0x7000, sizeof(struct BootInfo));
    
    uint32_t fb_start = binfo.framebuffer;
    uint32_t fb_size = binfo.pitch * binfo.height;
    uint32_t fb_end = fb_start + fb_size;
    
    // Map VESA framebuffer page frames (identity mapped)
    for (uint32_t addr = fb_start; addr < fb_end; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    
    // 5. Enable hardware paging on CPU
    __asm__ volatile(
        "mov %0, %%cr3\n\t"         // Write page directory physical address to CR3
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t" // Set PG bit (bit 31) in CR0
        "mov %%eax, %%cr0\n\t"
        :
        : "r"(&page_directory)
        : "eax", "memory"
    );
}

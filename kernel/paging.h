#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>

// Page Directory and Page Table flags
#define PAGE_PRESENT  0x1
#define PAGE_WRITE    0x2
#define PAGE_USER     0x4

// Initialize PMM and VMM paging
void paging_init(void);

// Allocate a single physical page frame (returns physical address)
uint32_t pmm_alloc_frame(void);

// Free a physical page frame
void pmm_free_frame(uint32_t frame_phys);

// Map a virtual page to a physical page frame in the active page directory
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

// Create a new private page directory and copy kernel space mappings
uint32_t vmm_create_page_dir(void);

// Map a page inside a specific page directory
void vmm_map_page_in_dir(uint32_t* dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

// Free all user space pages and tables inside a page directory
void vmm_clear_user_space(uint32_t* dir);

// Get physical address of the master kernel page directory
uint32_t vmm_get_kernel_page_dir(void);

#endif


#include "heap.h"
#include <stdbool.h>

#define HEAP_START 0xC00000
#define HEAP_SIZE  0x1000000 // 16MB

struct Block {
    size_t size;
    bool is_free;
    struct Block* next;
};

static struct Block* free_list = (struct Block*)HEAP_START;

void heap_init(void) {
    free_list->size = HEAP_SIZE - sizeof(struct Block);
    free_list->is_free = true;
    free_list->next = NULL;
}


void* kmalloc(size_t size) {
    // Align size to 4 bytes
    size = (size + 3) & ~3;

    struct Block* curr = free_list;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            // Can we split?
            if (curr->size >= size + sizeof(struct Block) + 4) {
                struct Block* next_block = (struct Block*)((uintptr_t)curr + sizeof(struct Block) + size);
                next_block->size = curr->size - size - sizeof(struct Block);
                next_block->is_free = true;
                next_block->next = curr->next;

                curr->size = size;
                curr->is_free = false;
                curr->next = next_block;
            } else {
                curr->is_free = false;
            }
            return (void*)((uintptr_t)curr + sizeof(struct Block));
        }
        curr = curr->next;
    }
    return NULL; // Out of memory
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    struct Block* block = (struct Block*)((uintptr_t)ptr - sizeof(struct Block));
    block->is_free = true;

    // Merge consecutive free blocks
    struct Block* curr = free_list;
    while (curr != NULL) {
        if (curr->is_free && curr->next != NULL && curr->next->is_free) {
            curr->size += sizeof(struct Block) + curr->next->size;
            curr->next = curr->next->next;
            // Retry on the merged block to merge further
            continue;
        }
        curr = curr->next;
    }
}

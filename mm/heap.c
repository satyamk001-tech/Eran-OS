#include "heap.h"

typedef struct block_header {
    size_t size;                // Size of usable memory in bytes
    int is_free;                // 1 if free, 0 if allocated
    struct block_header *next;  // Pointer to next block in heap list
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

static block_header_t *heap_start = 0;
static size_t heap_total_size = 0;

void heap_init(uint32_t start_addr, size_t size) {
    heap_start = (block_header_t *)start_addr;
    heap_total_size = size;

    heap_start->size = size - HEADER_SIZE;
    heap_start->is_free = 1;
    heap_start->next = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    // Align size to 8 bytes
    size = (size + 7) & ~7;

    block_header_t *curr = heap_start;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Check if block can be split
            if (curr->size >= size + HEADER_SIZE + 16) {
                block_header_t *new_block = (block_header_t *)((uint8_t *)curr + HEADER_SIZE + size);
                new_block->size = curr->size - size - HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            return (void *)((uint8_t *)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }
    return 0; // Out of heap memory
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *header = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    header->is_free = 1;

    // Coalesce adjacent free blocks
    block_header_t *curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

size_t heap_get_used(void) {
    size_t used = 0;
    block_header_t *curr = heap_start;
    while (curr) {
        if (!curr->is_free) {
            used += curr->size + HEADER_SIZE;
        }
        curr = curr->next;
    }
    return used;
}

size_t heap_get_free(void) {
    size_t free_mem = 0;
    block_header_t *curr = heap_start;
    while (curr) {
        if (curr->is_free) {
            free_mem += curr->size;
        }
        curr = curr->next;
    }
    return free_mem;
}

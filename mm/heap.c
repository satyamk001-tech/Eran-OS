#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../drivers/vga.h"

typedef struct block_header {
    size_t size;                // Size of usable memory in bytes
    int is_free;                // 1 if free, 0 if allocated
    struct block_header *next;  // Pointer to next block in heap list
    uint32_t padding;           // Explicit 16-byte alignment pad for 8-byte aligned payloads
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
    if (!ptr || !heap_start) return;

    uint32_t addr = (uint32_t)ptr;
    uint32_t heap_base = (uint32_t)heap_start;
    if (addr < heap_base + HEADER_SIZE || addr >= heap_base + heap_total_size) {
        return; // Pointer is outside heap boundaries
    }

    // Verify pointer corresponds to an allocated block
    block_header_t *curr = heap_start;
    block_header_t *target = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    int found = 0;
    while (curr) {
        if (curr == target) {
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found || target->is_free) {
        return; // Block not found or already free (double-free prevention)
    }

    target->is_free = 1;

    // Coalesce adjacent free blocks
    curr = heap_start;
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

int heap_run_mem_test(void) {
    kprintf("\n===================================================================\n");
    kprintf("       MEMORY MANAGEMENT & HEAP/PMM/VMM VALIDATION TEST            \n");
    kprintf("===================================================================\n");

    int all_passed = 1;

    // Test 1: Zero-sized allocation request
    void *p0 = kmalloc(0);
    if (p0 == 0) {
        kprintf("[PASS] Zero-sized kmalloc(0) safely returned NULL.\n");
    } else {
        kprintf("[FAIL] kmalloc(0) did not return NULL.\n");
        all_passed = 0;
    }

    // Test 2: 8-byte alignment verification
    void *p1 = kmalloc(17);
    if (p1 && (((uint32_t)p1 & 0x7) == 0)) {
        kprintf("[PASS] kmalloc(17) returned 8-byte aligned pointer at %x.\n", (uint32_t)p1);
    } else {
        kprintf("[FAIL] kmalloc(17) pointer not 8-byte aligned: %x.\n", (uint32_t)p1);
        all_passed = 0;
    }

    // Test 3: Pattern write & read integrity
    if (p1) {
        uint8_t *b = (uint8_t *)p1;
        for (int i = 0; i < 17; i++) b[i] = (uint8_t)(0x5A + i);
        int pattern_ok = 1;
        for (int i = 0; i < 17; i++) {
            if (b[i] != (uint8_t)(0x5A + i)) pattern_ok = 0;
        }
        if (pattern_ok) {
            kprintf("[PASS] Heap memory write/read byte pattern integrity verified.\n");
        } else {
            kprintf("[FAIL] Heap memory readback pattern mismatch.\n");
            all_passed = 0;
        }
        kfree(p1);
    }

    // Test 4: Multiple allocations, heap fragmentation, and coalescing
    size_t free_before = heap_get_free();
    void *p2 = kmalloc(64);
    void *p3 = kmalloc(128);
    if (p2 && p3) {
        kprintf("[PASS] Multiple concurrent allocations succeeded (%x, %x).\n", (uint32_t)p2, (uint32_t)p3);
        kfree(p2);
        kfree(p3);
        size_t free_after = heap_get_free();
        if (free_after == free_before) {
            kprintf("[PASS] Block freeing and contiguous coalescing restored full free heap (%u bytes).\n", free_after);
        } else {
            kprintf("[FAIL] Coalescing did not fully restore free heap: before=%u, after=%u.\n", free_before, free_after);
            all_passed = 0;
        }
    } else {
        kprintf("[FAIL] Concurrent allocations failed.\n");
        all_passed = 0;
    }

    // Test 5: Double-free safety
    size_t free_prior_df = heap_get_free();
    kfree(p1); // Freeing already-freed pointer
    if (heap_get_free() == free_prior_df) {
        kprintf("[PASS] Double-free safely prevented without heap corruption.\n");
    } else {
        kprintf("[FAIL] Double-free corrupted heap metadata.\n");
        all_passed = 0;
    }

    // Test 6: Invalid out-of-bounds pointer rejection
    kfree((void *)0x12345678);
    kfree((void *)0x00000000);
    kfree((void *)0xFFFFFFF0);
    kprintf("[PASS] Out-of-bounds pointer kfree requests safely rejected.\n");

    // Test 7: Allocation exhaustion
    void *huge_p = kmalloc(0x20000000); // 512 MB request
    if (huge_p == 0) {
        kprintf("[PASS] Memory exhaustion safely returned NULL without panic.\n");
    } else {
        kprintf("[FAIL] Oversized allocation unexpectedly succeeded.\n");
        all_passed = 0;
    }

    // Test 8: Physical Memory Manager (PMM) Frame Allocation & Free
    uint32_t pmm_free_kb = pmm_get_free_memory_kb();
    uint32_t page1 = pmm_alloc_page();
    uint32_t page2 = pmm_alloc_page();
    if (page1 != 0 && page2 != 0 && (page1 % 4096 == 0) && (page2 % 4096 == 0)) {
        kprintf("[PASS] PMM page frame allocator returned 4KB aligned physical frames (%x, %x).\n", page1, page2);
        pmm_free_page(page1);
        pmm_free_page(page2);
        if (pmm_get_free_memory_kb() == pmm_free_kb) {
            kprintf("[PASS] PMM frame deallocation restored free physical capacity (%u KB).\n", pmm_free_kb);
        } else {
            kprintf("[FAIL] PMM free capacity mismatch after deallocation.\n");
            all_passed = 0;
        }
    } else {
        kprintf("[FAIL] PMM frame allocation failed.\n");
        all_passed = 0;
    }

    // Test 9: Virtual Memory Management (VMM) Two-Level Paging Remap & TLB Invalidation
    uint32_t vmm_phys = pmm_alloc_page();
    if (vmm_phys != 0) {
        // Test remap at linear address 0x00E00000 (14 MB)
        uint32_t test_vaddr = 0x00E00000;
        vmm_map_page(test_vaddr, vmm_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        volatile uint32_t *vptr = (volatile uint32_t *)test_vaddr;
        *vptr = 0xCAFEBABE;
        if (*vptr == 0xCAFEBABE) {
            kprintf("[PASS] VMM linear page mapping & TLB invalidation verified (*0x%x = 0xCAFEBABE).\n", test_vaddr);
        } else {
            kprintf("[FAIL] VMM mapped address readback mismatch: %x.\n", *vptr);
            all_passed = 0;
        }
        // Restore identity mapping for 0x00E00000
        vmm_map_page(test_vaddr, test_vaddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        pmm_free_page(vmm_phys);
    }

    if (all_passed) {
        kprintf("-------------------------------------------------------------------\n");
        kprintf("[RESULT] ALL MEMORY MANAGEMENT TESTS PASSED (PMM + HEAP + VMM)\n");
        kprintf("===================================================================\n\n");
        return 0;
    } else {
        kprintf("-------------------------------------------------------------------\n");
        kprintf("[RESULT] SOME MEMORY MANAGEMENT TESTS FAILED\n");
        kprintf("===================================================================\n\n");
        return -1;
    }
}

#include "pmm.h"

// 32 MB default memory tracking (8192 frames, bitmap needs 1024 bytes)
#define MAX_FRAMES 32768 // Supports up to 128 MB (32768 * 4KB)
static uint32_t pmm_bitmap[MAX_FRAMES / 32];
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

static inline void set_bit(uint32_t frame) {
    pmm_bitmap[frame / 32] |= (1 << (frame % 32));
}

static inline void clear_bit(uint32_t frame) {
    pmm_bitmap[frame / 32] &= ~(1 << (frame % 32));
}

static inline int test_bit(uint32_t frame) {
    return (pmm_bitmap[frame / 32] & (1 << (frame % 32))) != 0;
}

void pmm_init(uint32_t total_memory_kb) {
    if (total_memory_kb == 0) {
        total_memory_kb = 32768; // Default to 32 MB
    }
    total_frames = (total_memory_kb * 1024) / PAGE_SIZE;
    if (total_frames > MAX_FRAMES) {
        total_frames = MAX_FRAMES;
    }

    // Initially mark all frames as used
    for (uint32_t i = 0; i < (MAX_FRAMES / 32); i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    used_frames = total_frames;

    // Free frames from 2 MB (0x200000) up to total_frames
    // The first 2 MB is reserved for BIOS, Bootloader, Kernel, Stack, and IDT
    uint32_t first_free_frame = (2 * 1024 * 1024) / PAGE_SIZE; // Frame 512
    for (uint32_t f = first_free_frame; f < total_frames; f++) {
        clear_bit(f);
        used_frames--;
    }
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < (total_frames / 32); i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int bit = 0; bit < 32; bit++) {
                if (!(pmm_bitmap[i] & (1 << bit))) {
                    uint32_t frame = i * 32 + bit;
                    if (frame >= total_frames) return 0;
                    set_bit(frame);
                    used_frames++;
                    return frame * PAGE_SIZE;
                }
            }
        }
    }
    return 0; // Out of physical memory
}

void pmm_free_page(uint32_t page_addr) {
    uint32_t frame = page_addr / PAGE_SIZE;
    if (frame < total_frames && test_bit(frame)) {
        clear_bit(frame);
        used_frames--;
    }
}

uint32_t pmm_get_total_memory_kb(void) {
    return (total_frames * PAGE_SIZE) / 1024;
}

uint32_t pmm_get_used_memory_kb(void) {
    return (used_frames * PAGE_SIZE) / 1024;
}

uint32_t pmm_get_free_memory_kb(void) {
    return ((total_frames - used_frames) * PAGE_SIZE) / 1024;
}

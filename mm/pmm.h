#ifndef ERAN_MM_PMM_H
#define ERAN_MM_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

void pmm_init(uint32_t total_memory_kb);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t page_addr);
uint32_t pmm_get_total_memory_kb(void);
uint32_t pmm_get_free_memory_kb(void);
uint32_t pmm_get_used_memory_kb(void);

#endif /* ERAN_MM_PMM_H */

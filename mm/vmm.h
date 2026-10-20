#ifndef ERAN_MM_VMM_H
#define ERAN_MM_VMM_H

#include <stdint.h>
#include <stddef.h>
#include "../kernel/idt.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

void vmm_init(void);
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void vmm_unmap_page(uint32_t virt);
void vmm_page_fault_handler(registers_t *regs);
void vmm_print_info(void);

#endif /* ERAN_MM_VMM_H */

#ifndef ERAN_MM_HEAP_H
#define ERAN_MM_HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init(uint32_t start_addr, size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_get_used(void);
size_t heap_get_free(void);
int heap_run_mem_test(void);

#endif /* ERAN_MM_HEAP_H */

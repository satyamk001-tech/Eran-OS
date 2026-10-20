#ifndef ERAN_PROCESS_H
#define ERAN_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "../kernel/idt.h"

typedef enum {
    PROCESS_READY      = 0,
    PROCESS_RUNNING    = 1,
    PROCESS_BLOCKED    = 2,
    PROCESS_TERMINATED = 3
} process_state_t;

typedef struct process {
    uint32_t pid;
    char name[32];
    process_state_t state;
    uint32_t esp;               // Saved stack pointer
    uint32_t stack_base;        // Allocated stack memory
    uint32_t total_ticks;       // Total CPU ticks consumed
    struct process *next;
} process_t;

void process_init(void);
process_t *process_create(const char *name, void (*entry_point)(void));
void process_yield(void);
void process_exit(int code);
process_t *process_get_current(void);
void process_schedule(registers_t *regs);
void process_list(void);

#endif /* ERAN_PROCESS_H */

#include "process.h"
#include "../mm/heap.h"
#include "../kernel/gdt.h"
#include "../drivers/vga.h"

#define STACK_SIZE 4096

static process_t *process_queue = 0;
static process_t *current_process = 0;
static uint32_t next_pid = 0;

static void mystrcpy(char *dest, const char *src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void process_init(void) {
    // Initialize PID 0: Main Kernel / Interactive Shell Task
    process_t *main_proc = (process_t *)kmalloc(sizeof(process_t));
    main_proc->pid = next_pid++;
    mystrcpy(main_proc->name, "kernel_main", sizeof(main_proc->name));
    main_proc->state = PROCESS_RUNNING;
    main_proc->esp = 0;
    main_proc->stack_base = 0x90000 - STACK_SIZE;
    main_proc->total_ticks = 0;
    main_proc->next = 0;

    process_queue = main_proc;
    current_process = main_proc;
}

process_t *process_create(const char *name, void (*entry_point)(void)) {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (!proc) return 0;

    void *stack = kmalloc(STACK_SIZE);
    if (!stack) {
        kfree(proc);
        return 0;
    }

    proc->pid = next_pid++;
    mystrcpy(proc->name, name, sizeof(proc->name));
    proc->state = PROCESS_READY;
    proc->stack_base = (uint32_t)stack;
    proc->total_ticks = 0;

    // Format top of stack for context entry
    // Stack grows downwards from (stack + STACK_SIZE)
    uint32_t *sp = (uint32_t *)((uint8_t *)stack + STACK_SIZE);

    // Initial CPU interrupt return frame: EFLAGS, CS, EIP
    *(--sp) = 0x00000202;                // EFLAGS (IF=1, interrupts enabled)
    *(--sp) = 0x08;                      // CS (Kernel Code Segment)
    *(--sp) = (uint32_t)entry_point;     // EIP (Process Entry Function)

    // Initial pusha frame: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    *(--sp) = 0; // EAX
    *(--sp) = 0; // ECX
    *(--sp) = 0; // EDX
    *(--sp) = 0; // EBX
    --sp;
    *sp = (uint32_t)sp; // ESP
    *(--sp) = 0; // EBP
    *(--sp) = 0; // ESI
    *(--sp) = 0; // EDI

    // Data Segment descriptor
    *(--sp) = 0x10; // DS

    proc->esp = (uint32_t)sp;

    // Append to process list
    proc->next = 0;
    process_t *tail = process_queue;
    while (tail && tail->next) {
        tail = tail->next;
    }
    if (tail) {
        tail->next = proc;
    } else {
        process_queue = proc;
    }

    return proc;
}

process_t *process_get_current(void) {
    return current_process;
}

void process_yield(void) {
    __asm__ volatile ("int $0x20"); // Trigger timer interrupt (vector 32 = IRQ 0)
}

void process_exit(int code) {
    (void)code;
    if (current_process) {
        current_process->state = PROCESS_TERMINATED;
    }
    process_yield();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void process_schedule(registers_t *regs) {
    if (!current_process || !process_queue) return;

    current_process->total_ticks++;

    // Round-robin selection of next ready process
    process_t *next = current_process->next ? current_process->next : process_queue;
    while (next && next != current_process) {
        if (next->state == PROCESS_READY) {
            break;
        }
        next = next->next ? next->next : process_queue;
    }

    if (!next || next == current_process || next->state != PROCESS_READY) {
        return; // No other ready process
    }

    // Switch from current_process to next
    if (current_process->state == PROCESS_RUNNING) {
        current_process->state = PROCESS_READY;
    }
    current_process->esp = (uint32_t)regs;

    next->state = PROCESS_RUNNING;
    current_process = next;

    // Update TSS kernel stack for user-mode transitions
    tss_set_stack(next->stack_base + STACK_SIZE);
}

void process_list(void) {
    kprintf("===================================================================\n");
    kprintf(" PID   PROCESS NAME             STATE          CPU TICKS           \n");
    kprintf("===================================================================\n");
    process_t *curr = process_queue;
    while (curr) {
        const char *state_str = "READY";
        if (curr->state == PROCESS_RUNNING) state_str = "RUNNING";
        else if (curr->state == PROCESS_BLOCKED) state_str = "BLOCKED";
        else if (curr->state == PROCESS_TERMINATED) state_str = "TERMINATED";

        kprintf(" %d     %s\t\t%s\t\t%u\n", curr->pid, curr->name, state_str, curr->total_ticks);
        curr = curr->next;
    }
    kprintf("===================================================================\n");
}

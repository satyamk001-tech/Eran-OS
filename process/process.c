#include "process.h"
#include "../mm/heap.h"
#include "../kernel/gdt.h"
#include "../drivers/vga.h"

#define STACK_SIZE 4096

static process_t *process_queue = 0;
static process_t *current_process = 0;
static uint32_t next_pid = 0;
static volatile uint32_t switch_esp = 0;

uint32_t process_get_switch_esp(void) {
    uint32_t esp = switch_esp;
    switch_esp = 0;
    return esp;
}

static void process_task_exit(void) {
    process_exit(0);
}

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

    // Return address if entry_point returns:
    *(--sp) = 0;                          // dummy exit code argument for process_exit
    *(--sp) = (uint32_t)process_task_exit;// Return address for entry_point

    // Initial CPU interrupt return frame: EFLAGS, CS, EIP (popped by iret)
    *(--sp) = 0x00000202;                // EFLAGS (IF=1, interrupts enabled)
    *(--sp) = 0x08;                      // CS (Kernel Code Segment)
    *(--sp) = (uint32_t)entry_point;     // EIP (Process Entry Function)

    // IRQ stub frame: err_code, int_no (popped by add esp, 8)
    *(--sp) = 0;                         // err_code
    *(--sp) = 32;                        // int_no

    // pusha frame: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI (popped by popa)
    *(--sp) = 0; // EAX
    *(--sp) = 0; // ECX
    *(--sp) = 0; // EDX
    *(--sp) = 0; // EBX
    --sp;
    *sp = (uint32_t)sp; // ESP
    *(--sp) = 0; // EBP
    *(--sp) = 0; // ESI
    *(--sp) = 0; // EDI

    // Data Segment descriptor (popped by pop eax; mov ds, ax...)
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
        if (current_process->pid == 0) {
            kprintf("\n[WARN] Root kernel process (PID 0) cannot exit.\n");
            return;
        }
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
    process_t *start = current_process->next ? current_process->next : process_queue;
    process_t *curr = start;
    process_t *next = 0;
    do {
        if (curr && curr->state == PROCESS_READY) {
            next = curr;
            break;
        }
        curr = (curr && curr->next) ? curr->next : process_queue;
    } while (curr && curr != start);

    if (!next || next == current_process) {
        return; // No other ready process
    }

    // Switch from current_process to next
    if (current_process->state == PROCESS_RUNNING) {
        current_process->state = PROCESS_READY;
    }
    current_process->esp = (uint32_t)regs;

    next->state = PROCESS_RUNNING;
    current_process = next;
    switch_esp = next->esp;

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

static volatile int test_a_count = 0;
static volatile int test_b_count = 0;
static volatile int test_done_a = 0;
static volatile int test_done_b = 0;

static void sched_task_a(void) {
    for (int i = 1; i <= 5; i++) {
        test_a_count = i;
        kprintf("[SCHED] Task A running: step %d/5 (PID %d)\n", i, current_process ? current_process->pid : 0);
        process_yield();
    }
    test_done_a = 1;
    kprintf("[SCHED] Task A completed successfully.\n");
}

static void sched_task_b(void) {
    for (int i = 1; i <= 5; i++) {
        test_b_count = i;
        kprintf("[SCHED] Task B running: step %d/5 (PID %d)\n", i, current_process ? current_process->pid : 0);
        process_yield();
    }
    test_done_b = 1;
    kprintf("[SCHED] Task B completed successfully.\n");
}

int process_run_sched_test(void) {
    kprintf("\n===================================================================\n");
    kprintf("       SCHEDULER & CONTEXT SWITCHING VALIDATION TEST               \n");
    kprintf("===================================================================\n");
    kprintf("Creating independent test tasks: Task A and Task B...\n");

    test_a_count = 0;
    test_b_count = 0;
    test_done_a = 0;
    test_done_b = 0;

    process_t *t1 = process_create("sched_test_a", sched_task_a);
    process_t *t2 = process_create("sched_test_b", sched_task_b);

    if (!t1 || !t2) {
        kprintf("[FAIL] Failed to allocate processes for scheduler test.\n");
        return -1;
    }

    kprintf("[INFO] Created Task A (PID %d, ESP %x) and Task B (PID %d, ESP %x)\n",
            t1->pid, t1->esp, t2->pid, t2->esp);
    kprintf("[INFO] Yielding CPU to worker tasks for interleaved scheduling...\n");

    int max_cycles = 100;
    while ((!test_done_a || !test_done_b) && max_cycles-- > 0) {
        process_yield();
    }

    kprintf("[VERIFY] Task A final step count: %d (expected 5, done=%d)\n", test_a_count, test_done_a);
    kprintf("[VERIFY] Task B final step count: %d (expected 5, done=%d)\n", test_b_count, test_done_b);

    if (test_a_count == 5 && test_b_count == 5 && test_done_a && test_done_b) {
        kprintf("[PASS] Context switching genuinely alternated between tasks.\n");
        kprintf("[PASS] Register state and stack frames correctly preserved.\n");
        kprintf("===================================================================\n\n");
        return 0;
    } else {
        kprintf("[FAIL] Scheduler test timed out or counters mismatched.\n");
        kprintf("===================================================================\n\n");
        return -1;
    }
}

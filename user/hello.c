/* ==============================================================================
 * Eran OS - Sample User Mode Standalone Executable
 * Runs completely unprivileged in Ring 3 via INT 0x80 System Calls
 * ============================================================================== */

static const char msg[] = "\n[USER ELF] Hello from standalone Ring 3 User Space ELF binary!\n[USER ELF] Successfully invoked INT 0x80 SYS_WRITE!\n\n";

void _start(void) {
    // 1. Invoke SYS_WRITE (Syscall 2)
    __asm__ volatile (
        "mov $2, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80\n"
        : : "r"(msg) : "eax", "ebx"
    );

    // 2. Invoke SYS_EXIT (Syscall 1)
    __asm__ volatile (
        "mov $1, %%eax\n"
        "mov $0, %%ebx\n"
        "int $0x80\n"
        : : : "eax", "ebx"
    );

    // Loop if exit returned
    for (;;) {
        __asm__ volatile ("int $0x80" : : "a"(0)); // SYS_YIELD
    }
}

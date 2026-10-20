#include "syscall.h"
#include "idt.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"
#include "../hal/pci.h"
#include "../process/process.h"

static void syscall_handler(registers_t *regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_YIELD:
            process_yield();
            regs->eax = 0;
            break;

        case SYS_EXIT:
            process_exit((int)regs->ebx);
            regs->eax = 0;
            break;

        case SYS_WRITE: {
            const char *str = (const char *)regs->ebx;
            if (str) {
                vga_puts(str);
                regs->eax = 0;
            } else {
                regs->eax = -1;
            }
            break;
        }

        case SYS_GETPID: {
            process_t *curr = process_get_current();
            regs->eax = curr ? curr->pid : 0;
            break;
        }

        case SYS_UPTIME:
            regs->eax = timer_get_uptime_seconds();
            break;

        case SYS_DEVINFO:
            regs->eax = pci_get_device_count();
            break;

        default:
            kprintf("syscall: unknown system call %u\n", syscall_num);
            regs->eax = -1;
            break;
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}

int32_t sys_yield(void) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_YIELD));
    return ret;
}

int32_t sys_exit(int code) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_EXIT), "b"(code));
    return ret;
}

int32_t sys_write(const char *str) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_WRITE), "b"(str));
    return ret;
}

int32_t sys_getpid(void) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_GETPID));
    return ret;
}

int32_t sys_uptime(void) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_UPTIME));
    return ret;
}

int32_t sys_devinfo(void) {
    int32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(SYS_DEVINFO));
    return ret;
}

#ifndef ERAN_KERNEL_SYSCALL_H
#define ERAN_KERNEL_SYSCALL_H

#include <stdint.h>

#define SYS_YIELD    0
#define SYS_EXIT     1
#define SYS_WRITE    2
#define SYS_READ     3
#define SYS_GETPID   4
#define SYS_UPTIME   5
#define SYS_DEVINFO  6

void syscall_init(void);

// User-space invocation wrappers
int32_t sys_yield(void);
int32_t sys_exit(int code);
int32_t sys_write(const char *str);
int32_t sys_getpid(void);
int32_t sys_uptime(void);
int32_t sys_devinfo(void);

#endif /* ERAN_KERNEL_SYSCALL_H */

#ifndef ERAN_KERNEL_SHELL_H
#define ERAN_KERNEL_SHELL_H

void shell_init(void);
void shell_run_prompt(void);
void shell_execute(const char *cmdline);

#endif /* ERAN_KERNEL_SHELL_H */

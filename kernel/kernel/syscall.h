#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/types.h>

#define SYSCALL_MAX 64

typedef struct {
    void* handler;
} syscall_entry_t;

void syscall_init(void);
void syscall_register(int num, void* handler);
void syscall_dispatch(void);

long kenux_syscall_entry(int nr, long a1, long a2, long a3, long a4, long a5, long a6);

#endif
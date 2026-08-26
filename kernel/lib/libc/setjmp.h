#ifndef LIBC_SETJMP_H
#define LIBC_SETJMP_H

#include <arch/types.h>

typedef struct {
    unsigned long ebx;
    unsigned long esi;
    unsigned long edi;
    unsigned long ebp;
    unsigned long esp;
    unsigned long eip;
} jmp_buf[1];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif

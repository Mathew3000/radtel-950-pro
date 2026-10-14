/*
 * Minimal newlib hook. vsnprintf in newlib-nano can allocate, so it needs
 * _sbrk. The heap grows from the linker's `end` symbol and stops short of the
 * stack reserve the linker script sets aside below _estack.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern char end;
extern char _estack;
extern char _Min_Stack_Size;

void *_sbrk(ptrdiff_t incr)
{
    static char *brk = &end;
    char *limit = &_estack - (uintptr_t)&_Min_Stack_Size;
    char *prev = brk;

    if (brk + incr > limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    brk += incr;
    return prev;
}

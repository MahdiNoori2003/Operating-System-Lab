#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

static int get_digital_root(int n)
{
    if (n < 0)
    {
        return -1;
    }

    while (n > 9)
    {
        int sum = 0;
        while (n != 0)
        {
            sum += n % 10;
            n /= 10;
        }
        n = sum;
    }
    return n;
}

int sys_find_digital_root(void)
{
    int result = get_digital_root(myproc()->tf->ebx);
    return result;
}

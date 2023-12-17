#include "types.h"
#include "user.h"

#define INVALID_INPUT_ERROR "input must be non negative!\n"
#define FEW_ARGUMANTS_ERROR "few arguements.needed one integer provided zero\n"

int find_digital_root_syscall(int n)
{
    int prev_ebx;
    asm volatile(
        "movl %%ebx, %0\n\t"
        "movl %1, %%ebx"
        : "=r"(prev_ebx)
        : "r"(n));
    int result = find_digital_root();
    asm volatile(
        "movl %0, %%ebx" ::"r"(prev_ebx));
    return result;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf(2, FEW_ARGUMANTS_ERROR);
        exit();
    }

    int n = (*argv[1] == '-') ? -atoi(argv[1] + 1) : atoi(argv[1]);

    int result = find_digital_root_syscall(n);
    if (result == -1)
    {
        printf(2, INVALID_INPUT_ERROR);
    }
    else
    {
        printf(1, "%d\n", result);
    }

    exit();
}

#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "prioritylock.h"

struct prioritylock pl;

int sys_init_prioritylock(void)
{
    initprioritylock(&pl, "priority_lock");
    return 0;
}

int sys_acquire_prioritylock(void)
{
    acquirepriority(&pl);
    return 0;
}

int sys_release_prioritylock(void)
{
    releasepriority(&pl);
    return 0;
}

#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_get_uncle_count(void)
{
  int pid;
  if (argint(0, &pid) < 0)
    return -1;
  return count_uncles(pid);
}

int sys_get_process_lifetime(void)
{
  int pid;
  if (argint(0, &pid) < 0)
    return -1;
  return calc_process_lifetime(pid);
}

int sys_change_queue(void)
{
  int queue_number, pid;
  if (argint(0, &pid) < 0 || argint(1, &queue_number) < 0)
    return -1;

  if (queue_number < RR || queue_number > BJF)
    return -1;

  return change_queue(pid, queue_number);
}

int sys_set_bjf_params_for_process(void)
{
  int pid;
  float priority_ratio, arrival_time_ratio, executed_cycle_ratio, process_size_ratio;
  if (argint(0, &pid) < 0 ||
      argfloat(1, &priority_ratio) < 0 ||
      argfloat(2, &arrival_time_ratio) < 0 ||
      argfloat(3, &executed_cycle_ratio) < 0 ||
      argfloat(4, &process_size_ratio) < 0)
  {
    return -1;
  }

  return set_bjf_params_for_process(pid, priority_ratio, arrival_time_ratio, executed_cycle_ratio, process_size_ratio);
}

int sys_set_bjf_params_for_system(void)
{
  float priority_ratio, arrival_time_ratio, executed_cycle_ratio, process_size_ratio;
  if (argfloat(0, &priority_ratio) < 0 ||
      argfloat(1, &arrival_time_ratio) < 0 ||
      argfloat(2, &executed_cycle_ratio) < 0 ||
      argfloat(3, &process_size_ratio) < 0)
  {
    return -1;
  }

  set_bjf_params_for_system(priority_ratio, arrival_time_ratio, executed_cycle_ratio, process_size_ratio);
  return 0;
}

int sys_print_process_info(void)
{
  print_process_info();
  return 0;
}

int sys_set_bjf_priority(void)
{
  int pid, priority;
  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;

  if (priority < BJF_PRIORITY_MIN || priority > BJF_PRIORITY_MAX)
    return -1;

  return set_bjf_priority(pid, priority);
}

int sys_print_syscall_count()
{
  for (int i = 0; i < NCPU; i++)
  {
    cprintf("cpu number %d has run %d systemcalls\n", cpus[i].apicid, cpus[i].syscall_count);
  }

  cprintf("total number of system calls are %d\n", syscall_count_total);
  return 0;
}

int sys_reset_syscall_count()
{
  reset_syscall_count();
  return 0;
}

void *sys_open_sharedmem()
{
  int id;
  char *pointer;

  if (argint(0, &id) < 0)
    return -1;

  pointer = (char *)shm_open(id);

  if (pointer == (char *)-1)
  {
    cprintf("Failed to open shared memory region!\n");
  }
  return pointer;
}

int sys_close_sharedmem()
{
  int id;

  if (argint(0, &id) < 0)
    return -1;

  int res = shm_close(id);
  if (res == -1)
  {
    cprintf("Failed to close shared memory region!\n");
  }
  return res;
}
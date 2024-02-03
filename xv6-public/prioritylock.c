#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "prioritylock.h"

void initprioritylock(struct prioritylock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
  lk->queue = 0;
  initlock(&lk->slk, "spin lock");
}

static void print_prioritylock_queue(struct prioritylock *lk)
{
  struct node *q = lk->queue;
  if (q != 0)
  {
    cprintf("the queue is : ");

    while (q != 0)
    {
      cprintf("%d ", q->priority);
      q = q->next;
    }
    cprintf("\n");
  }
}

void acquirepriority(struct prioritylock *lk)
{
  acquire(&lk->slk);
  if (lk->locked)
  {
    struct proc *cur_proc = myproc();
    struct node *p = (struct node *)kalloc();
    p->next = 0;
    p->priority = cur_proc->pid;
    p->process = cur_proc;

    struct node *q = lk->queue;
    if (q == 0 || p->priority > q->priority)
    {
      lk->queue = p;
      p->next = q;
    }
    else
    {
      while (q->next != 0 && p->priority <= q->next->priority)
      {
        q = q->next;
      }
      p->next = q->next;
      q->next = p;
    }
    sleep(cur_proc, &lk->slk);
  }
  else
  {
    lk->locked = 1;
    lk->pid = myproc()->pid;
  }
  release(&lk->slk);
}

void releasepriority(struct prioritylock *lk)
{
  if (lk->pid != myproc()->pid || !lk->locked)
  {
    cprintf("the process %d does not own the lock to release it\n", myproc()->pid);
    return;
  }

  acquire(&lk->slk);

  if (lk->queue != 0)
  {
    struct node *p = lk->queue;
    // print_prioritylock_queue(lk);
    lk->queue = p->next;
    p->process->state = RUNNABLE;
    lk->pid = p->process->pid;
    wakeup(p->process);
    kfree((char *)p);
  }
  else
  {
    lk->locked = 0;
    lk->pid = 0;
  }

  release(&lk->slk);
}

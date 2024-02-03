#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "shm.h"

#define MIN_RANK 1000000000

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  memset(&p->bjf_info, 0, sizeof(p->bjf_info));

  p->bjf_info.priority = BJF_PRIORITY_DEFAULT;
  p->bjf_info.priority_ratio = 1;
  p->bjf_info.arrival_time_ratio = 1;
  p->bjf_info.executed_cycle_ratio = 1;
  p->bjf_info.process_size_ratio = 1;

  for (int i = 0; i < MAX_SHARED_PAGES; i++)
  {
    p->shared_addresses[i] = -1;
  }

  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  release(&ptable.lock);
  change_queue(p->pid, UNSET);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  acquire(&tickslock);
  np->bjf_info.arrival_time = ticks;
  np->last_run = ticks;
  np->last_in_lcfs = ticks;
  release(&tickslock);

  release(&ptable.lock);
  cmostime(&np->init_time);
  change_queue(pid, UNSET);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
int str_cmp(char *s1, char *s2)
{
  if (strlen(s1) != strlen(s2))
  {
    return 0;
  }
  else
  {
    for (int i = 0; i < strlen(s1); i++)
    {
      if (s1[i] != s2[i])
      {
        return 0;
      }
    }
    return 1;
  }
}

void scheduler(void)
{
  struct proc *p;
  struct proc *last_scheduled_rr = &ptable.proc[NPROC - 1];
  struct proc *last_scheduled_lcfs = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  for (;;)
  {
    for (int i = 0; i < NPROC; i++)
    {
      if (str_cmp(ptable.proc[i].name, "proc_info") || str_cmp(ptable.proc[i].name, "sh"))
      {
        if (ptable.proc[i].state == RUNNABLE)
          change_queue(ptable.proc[i].pid, RR);
      }
    }
    // Enable interrupts on this processor.
    sti();
    acquire(&ptable.lock);
    int found = 0;

    for (int i = 0; i < NQUEUES; i++)
    {
      switch (i)
      {
      case 0:
        p = roundrobin(last_scheduled_rr);
        last_scheduled_rr = p ? p : last_scheduled_rr;
        break;
      case 1:
        p = lcfs(last_scheduled_lcfs);
        last_scheduled_lcfs = p ? p : last_scheduled_lcfs;
        break;
      case 2:
        p = bjf();
        break;
      default:
        break;
      }
      if (p)
      {
        found = 1;
        break;
      }
    }
    if (!found)
    {
      release(&ptable.lock);
      continue;
    }

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    p->last_run = ticks;
    p->bjf_info.executed_cycle += 0.1f;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int count_uncles(int pid)
{
  int p_index = -1;
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].pid == pid)
    {
      p_index = i;
      break;
    }
  }
  if (p_index == -1)
  {
    return -1;
  }
  int count = 0;
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[p_index].parent->parent->pid == ptable.proc[i].parent->pid &&
        ptable.proc[i].parent->state != UNUSED)
    {
      count++;
    }
  }

  if (count == 0)
  {
    return -1;
  }
  else
  {
    return count - 1;
  }
}

static int calc_diff_till_now(struct rtcdate input_time)
{
  struct rtcdate now;
  cmostime(&now);
  uint start_seconds = input_time.hour * 3600 + input_time.minute * 60 + input_time.second;
  uint end_seconds = now.hour * 3600 + now.minute * 60 + now.second;

  uint start_timestamp = input_time.year * 31536000 + input_time.month * 2592000 + input_time.day * 86400 + start_seconds;
  uint end_timestamp = now.year * 31536000 + now.month * 2592000 + now.day * 86400 + end_seconds;

  uint difference = end_timestamp - start_timestamp;
  return difference;
}

int calc_process_lifetime(int pid)
{
  int p_index = -1;
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].pid == pid)
    {
      p_index = i;
      break;
    }
  }

  if (p_index == -1)
  {
    return -1;
  }
  int count = calc_diff_till_now(ptable.proc[p_index].init_time);
  return count;
}

void age_proc(int uptime_ticks)
{
  acquire(&ptable.lock);

  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].state == RUNNABLE && ptable.proc[i].queue != RR)
    {
      if (uptime_ticks - ptable.proc[i].last_run > CHANGE_QUEUE_THRESHOLD)
      {
        release(&ptable.lock);
        change_queue(ptable.proc[i].pid, RR);
        acquire(&ptable.lock);
      }
    }
  }
  release(&ptable.lock);
}

int change_queue(int pid, int new_queue)
{
  int old_queue = -1;

  if (new_queue == UNSET)
  {
    if (pid == 1 || pid == 2)
    {
      new_queue = RR;
    }
    else if (pid > 1)
    {
      new_queue = LCFS;
    }
    else
    {
      return old_queue;
    }
  }

  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].pid == pid)
    {
      old_queue = ptable.proc[i].queue;
      if (old_queue == new_queue)
      {
        release(&ptable.lock);
        return -1;
      }
      ptable.proc[i].queue = new_queue;

      if (new_queue == LCFS)
      {
        ptable.proc[i].last_in_lcfs = ticks;
      }
      release(&ptable.lock);
      return old_queue;
    }
  }

  release(&ptable.lock);
  return old_queue;
}

struct proc *roundrobin(struct proc *last_scheduled_rr)
{
  struct proc *p = last_scheduled_rr;

  for (;;)
  {
    p++;
    if (p >= &ptable.proc[NPROC])
      p = ptable.proc;

    if (p->state == RUNNABLE && p->queue == RR)
      return p;

    if (p == last_scheduled_rr)
      return 0;
  }
}

struct proc *lcfs(struct proc *last_scheduled_lcfs)
{
  struct proc *result = 0;
  int max_arrival_time = -1;

  if (last_scheduled_lcfs != 0 && last_scheduled_lcfs->state == RUNNABLE)
  {
    return last_scheduled_lcfs;
  }

  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].state == RUNNABLE && ptable.proc[i].queue == LCFS)
    {
      if (ptable.proc[i].last_in_lcfs >= max_arrival_time)
      {
        max_arrival_time = ptable.proc[i].last_in_lcfs;
        result = &ptable.proc[i];
      }
    }
  }
  return result;
}

static float bjfrank(struct proc *p)
{
  return p->bjf_info.priority * p->bjf_info.priority_ratio + p->bjf_info.arrival_time * p->bjf_info.arrival_time_ratio +
         p->bjf_info.executed_cycle * p->bjf_info.executed_cycle_ratio + p->sz * p->bjf_info.process_size_ratio;
}

struct proc *bjf()
{
  struct proc *result = 0;
  float min_rank = MIN_RANK;

  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].state == RUNNABLE && ptable.proc[i].queue == BJF)
    {
      float rank = bjfrank(&ptable.proc[i]);
      if (result == 0 || rank < min_rank)
      {
        result = &ptable.proc[i];
        min_rank = rank;
      }
    }
  }
  return result;
}

int set_bjf_params_for_process(int pid, float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio)
{
  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].pid == pid)
    {
      ptable.proc[i].bjf_info.priority_ratio = priority_ratio;
      ptable.proc[i].bjf_info.arrival_time_ratio = arrival_time_ratio;
      ptable.proc[i].bjf_info.executed_cycle_ratio = executed_cycles_ratio;
      ptable.proc[i].bjf_info.process_size_ratio = process_size_ratio;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void set_bjf_params_for_system(float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio)
{
  acquire(&ptable.lock);

  for (int i = 0; i < NPROC; i++)
  {
    ptable.proc[i].bjf_info.priority_ratio = priority_ratio;
    ptable.proc[i].bjf_info.arrival_time_ratio = arrival_time_ratio;
    ptable.proc[i].bjf_info.executed_cycle_ratio = executed_cycles_ratio;
    ptable.proc[i].bjf_info.process_size_ratio = process_size_ratio;
  }
  release(&ptable.lock);
}

int set_bjf_priority(int pid, int priority)
{
  acquire(&ptable.lock);

  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].pid == pid)
    {
      ptable.proc[i].bjf_info.priority = priority;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

static int count_digits(int num)
{
  if (num == 0)
    return 1;
  int count = 0;
  while (num)
  {
    num /= 10;
    ++count;
  }
  return count;
}

static void spacer(int count)
{
  for (int i = 0; i < count; ++i)
    cprintf(" ");
}

void print_process_info()
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleeping",
      [RUNNABLE] "runnable",
      [RUNNING] "running",
      [ZOMBIE] "zombie"};

  static int columns[] = {16, 8, 9, 8, 8, 8, 8, 9, 8, 8, 8, 8};
  cprintf("Process_Name    PID     State    Queue   Cycle   Arrival Priority R_Prty  R_Arvl  R_Exec  R_Size  Rank\n"
          "------------------------------------------------------------------------------------------------------\n");
  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i].state == UNUSED)
      continue;

    const char *state;
    if (ptable.proc[i].state >= 0 && ptable.proc[i].state < NELEM(states) && states[ptable.proc[i].state])
      state = states[ptable.proc[i].state];
    else
      state = "???";

    cprintf("%s", ptable.proc[i].name);
    spacer(columns[0] - strlen(ptable.proc[i].name));

    cprintf("%d", ptable.proc[i].pid);
    spacer(columns[1] - count_digits(ptable.proc[i].pid));

    cprintf("%s", state);
    spacer(columns[2] - strlen(state));

    cprintf("%d", (ptable.proc[i].queue));
    spacer(columns[3] - count_digits(ptable.proc[i].queue));

    cprintf("%d", (int)ptable.proc[i].bjf_info.executed_cycle);
    spacer(columns[4] - count_digits((int)ptable.proc[i].bjf_info.executed_cycle));

    cprintf("%d", ptable.proc[i].bjf_info.arrival_time);
    spacer(columns[5] - count_digits(ptable.proc[i].bjf_info.arrival_time));

    cprintf("%d", ptable.proc[i].bjf_info.priority);
    spacer(columns[6] - count_digits(ptable.proc[i].bjf_info.priority));

    cprintf("%d", (int)ptable.proc[i].bjf_info.priority_ratio);
    spacer(columns[7] - count_digits((int)ptable.proc[i].bjf_info.priority_ratio));

    cprintf("%d", (int)ptable.proc[i].bjf_info.arrival_time_ratio);
    spacer(columns[8] - count_digits((int)ptable.proc[i].bjf_info.arrival_time_ratio));

    cprintf("%d", (int)ptable.proc[i].bjf_info.executed_cycle_ratio);
    spacer(columns[9] - count_digits((int)ptable.proc[i].bjf_info.executed_cycle_ratio));

    cprintf("%d", (int)ptable.proc[i].bjf_info.process_size_ratio);
    spacer(columns[10] - count_digits((int)ptable.proc[i].bjf_info.process_size_ratio));

    cprintf("%d", (int)bjfrank(&ptable.proc[i]));
    if (i != NPROC - 1)
      cprintf("\n");
  }
  release(&ptable.lock);
}

void reset_syscall_count(void)
{
  cli();
  for (int i = 0; i < ncpu; i++)
    cpus[i].syscall_count = 0;
  sti();
  syscall_count_total = 0;
  __sync_synchronize();
}

// shared memory definition
struct shared_memory shm;

// shared memory initialize function
void shm_init()
{
  for (int i = 0; i < MAX_SHARED_PAGES; i++)
  {
    shm.shared_pages[i].id = -1;
    shm.shared_pages[i].frame = (char *)0;
  }

  initlock(&shm.slk, "spin lock");
}

// open the shared memory region
void *shm_open(int id)
{
  struct proc *proc = myproc();
  pde_t *pgdir = proc->pgdir;
  int page_index = -1;

  acquire(&shm.slk);

  for (int i = 0; i < MAX_SHARED_PAGES; i++)
  {
    if (shm.shared_pages[i].id == id)
    {
      page_index = i;
      break;
    }
  }

  if (page_index == -1)
  {
    int free_index = -1;
    for (int i = 0; i < MAX_SHARED_PAGES; i++)
    {
      if (shm.shared_pages[i].id == -1)
      {
        free_index = i;
        break;
      }
    }

    if (free_index == -1)
    {
      release(&shm.slk);
      return (char *)-1;
    }

    char *page;
    page = kalloc();
    memset(page, 0, PGSIZE);
    mappages(pgdir, (void *)PGROUNDUP(proc->sz), PGSIZE, V2P(page), PTE_W | PTE_U);
    proc->shared_addresses[free_index] = PGROUNDUP(proc->sz);
    shm.shared_pages[free_index].frame = page;
    shm.shared_pages[free_index].ref_count++;
    shm.shared_pages[free_index].id = id;
    release(&shm.slk);
    return (void *)proc->shared_addresses[free_index];
  }
  else
  {
    if (proc->shared_addresses[page_index] != -1)
    {
      release(&shm.slk);
      return (void *)-1;
    }

    mappages(pgdir, (void *)PGROUNDUP(proc->sz), PGSIZE, V2P(shm.shared_pages[page_index].frame), PTE_W | PTE_U);
    proc->shared_addresses[page_index] = PGROUNDUP(proc->sz);
    shm.shared_pages[page_index].ref_count++;
    release(&shm.slk);
    return (void *)proc->shared_addresses[page_index];
  }
}

// close the shared memory region
int shm_close(int id)
{
  struct proc *proc = myproc();

  int page_index = -1;
  acquire(&shm.slk);

  for (int i = 0; i < MAX_SHARED_PAGES; i++)
  {
    if (shm.shared_pages[i].id == id)
    {
      page_index = i;
      break;
    }
  }

  if (page_index == -1 || proc->shared_addresses[page_index] == -1)
  {
    release(&shm.slk);
    return -1;
  }

  uint a = PGROUNDUP(proc->shared_addresses[page_index]);
  pte_t *pte = walkpgdir(proc->pgdir, (char *)a, 0);

  if (!pte)
    a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
  else if ((*pte & PTE_P) != 0)
  {
    *pte = 0;
  }

  proc->shared_addresses[page_index] = -1;

  shm.shared_pages[page_index].ref_count--;
  if (shm.shared_pages[page_index].ref_count == 0)
  {
    kfree(shm.shared_pages[page_index].frame);
    shm.shared_pages[page_index].id = -1;
    shm.shared_pages[page_index].frame = (void *)0;
  }

  release(&shm.slk);
  return 0;
}
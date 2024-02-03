#include "date.h"

#define CHANGE_QUEUE_THRESHOLD 8000
#define NQUEUES 3
#define BJF_PRIORITY_MIN 1
#define BJF_PRIORITY_MAX 5
#define BJF_PRIORITY_DEFAULT 3
#define MAX_SHARED_PAGES 16

// Per-CPU state
struct cpu
{
  uchar apicid;              // Local APIC ID
  struct context *scheduler; // swtch() here to enter scheduler
  struct taskstate ts;       // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS]; // x86 global descriptor table
  volatile uint started;     // Has the CPU started?
  int ncli;                  // Depth of pushcli nesting.
  int intena;                // Were interrupts enabled before pushcli?
  struct proc *proc;         // The process running on this cpu or null
  uint syscall_count;
};

extern struct cpu cpus[NCPU];
extern int ncpu;
extern uint syscall_count_total;

// PAGEBREAK: 17
//  Saved registers for kernel context switches.
//  Don't need to save all the segment registers (%cs, etc),
//  because they are constant across kernel contexts.
//  Don't need to save %eax, %ecx, %edx, because the
//  x86 convention is that the caller has saved them.
//  Contexts are stored at the bottom of the stack they
//  describe; the stack pointer is the address of the context.
//  The layout of the context matches the layout of the stack in swtch.S
//  at the "Switch stacks" comment. Switch doesn't save eip explicitly,
//  but it is on the stack and allocproc() manipulates it.
struct context
{
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate
{
  UNUSED,
  EMBRYO,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE
};

enum MLFQ
{
  UNSET,
  RR,
  LCFS,
  BJF
};

struct bjf_info
{
  int priority;
  int arrival_time;
  float priority_ratio;
  float arrival_time_ratio;
  float executed_cycle;
  float executed_cycle_ratio;
  float process_size_ratio;
};

// Per-process state
struct proc
{
  uint sz;                    // Size of process memory (bytes)
  pde_t *pgdir;               // Page table
  char *kstack;               // Bottom of kernel stack for this process
  enum procstate state;       // Process state
  int pid;                    // Process ID
  struct proc *parent;        // Parent process
  struct trapframe *tf;       // Trap frame for current syscall
  struct context *context;    // swtch() here to run process
  void *chan;                 // If non-zero, sleeping on chan
  int killed;                 // If non-zero, have been killed
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (debugging)
  struct rtcdate init_time;
  struct bjf_info bjf_info;
  enum MLFQ queue;
  int last_run;
  int last_in_lcfs;
  int shared_addresses[MAX_SHARED_PAGES];
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

int count_uncles(int pid);
int calc_process_lifetime(int pid);
void age_proc(int uptime_ticks);
int change_queue(int pid, int new_queue);
struct proc *roundrobin(struct proc *last_scheduled_rr);
struct proc *lcfs(struct proc *last_scheduled_lcfs);
struct proc *bjf();
void print_process_info();
void set_bjf_params_for_system(float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio);
int set_bjf_params_for_process(int pid, float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio);
int set_bjf_priority(int pid, int priority);
void reset_syscall_count(void);
void *shm_open(int id);
int shm_close(int id);
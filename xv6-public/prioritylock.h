#include "spinlock.h"

struct node
{
  uint priority;
  struct node *next;
  struct proc *process;
};

struct prioritylock
{
  uint locked;         // Is the lock held?
  struct spinlock slk; // spinlock protecting this priority lock
  char *name;          // Name of lock.
  int pid;             // Process holding lock
  struct node *queue;  // queue
};

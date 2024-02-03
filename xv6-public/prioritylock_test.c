#include "types.h"
#include "user.h"

#define NCHILD 10

void process_function(int i)
{
  acquire_prioritylock();
  printf(1, "Process %d acquired the lock.\n", getpid());
  sleep(500);

  printf(1, "Process %d released the lock.\n", getpid());
  release_prioritylock();
  exit();
}

int main()
{
  init_prioritylock();
  for (int i = 0; i < NCHILD; i++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed.\n");
      exit();
    }
    else if (pid == 0)
    {
      process_function(i);
    }
  }
  for (int i = 0; i < 10; i++)
  {
    wait();
  }
  exit();
}
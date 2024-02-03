#include "types.h"
#include "user.h"

#define NUM_CHILDREN 4
#define SHM_ID 1

int main(int argc, char const *argv[])
{

    char *shared_mem = (char *)open_sharedmem(SHM_ID);
    char *value = (char *)shared_mem;
    *value = 0;

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Error forking child process.\n");
            exit();
        }
        else if (pid == 0)
        {
            char *shared_mem = (char *)open_sharedmem(1);
            acquire_prioritylock();
            char *value = (char *)shared_mem;
            *value += 1;
            printf(1, "Child proc with pid %d shared memory value is : %d\n", getpid(), *value);
            release_prioritylock();
            close_sharedmem(SHM_ID);
            exit();
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        wait();
    }

    printf(1, "Final shared memory value is : %d\n", *value);
    close_sharedmem(SHM_ID);
    exit();
}
#include "types.h"
#include "user.h"

void test()
{
    int forkpid = fork();
    if (forkpid > 0)
    {
        wait();
    }
    else if (forkpid == 0)
    {

        while (1)
        {
            if (get_process_lifetime(getpid()) >= 10)
            {
                printf(1, "child process lifetime is %d\n", get_process_lifetime(getpid()));
                kill(forkpid);
                break;
            }
        }
        exit();
    }
    else
    {
        printf(2, "Failed to create process.\n");
    }
}

int main(int argc, char *argv[])
{
    int forkpid = fork();
    if (forkpid > 0)
    {
        wait();
    }
    else if (forkpid == 0)
    {
        test();
        sleep(500);

        printf(1, "parent process lifetime is %d\n", get_process_lifetime(getpid()));
        exit();
    }
    else
    {
        printf(2, "Failed to create process.\n");
    }
    exit();
}
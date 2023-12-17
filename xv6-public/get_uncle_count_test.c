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
        printf(1, "the number of uncles is %d\n", get_uncle_count(getpid()));
    }
    else
    {
        printf(2, "Failed to create 3rd process.\n");
    }
    exit();
}

int main(int argc, char *argv[])
{
    int pid1 = fork();

    if (pid1 < 0)
    {
        printf(2, "Fork failed\n");
        exit();
    }

    if (pid1 == 0)
    {
        sleep(50);
    }
    else
    {
        int pid2 = fork();

        if (pid2 < 0)
        {
            printf(2, "Fork failed\n");
            exit();
        }

        if (pid2 == 0)
        {
            sleep(50);
        }
        else
        {

            int pid3 = fork();

            if (pid3 < 0)
            {
                printf(2, "Fork failed\n");
                exit();
            }

            if (pid3 == 0)
            {
                test();
            }
        }
        wait();
        wait();
        wait();
    }
    exit();
}
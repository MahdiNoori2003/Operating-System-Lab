#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NUM_PROCESSES 4

char *strcat(const char *str1, const char *str2)
{
    int dest_len = strlen(str1) + strlen(str2);
    int i;
    int j = 0;
    char *dest = (char *)malloc(dest_len + 1);

    for (i = 0; str1[i] != '\0'; i++)
    {
        dest[i] = str1[i];
    }

    for (j = 0; str2[j] != '\0'; j++)
    {
        dest[i] = str2[j];
        i++;
    }

    dest[dest_len] = '\0';

    return dest;
}

void itoa(int num, char *str)
{
    int i = 0;
    int is_negative = 0;

    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }

    if (num == 0)
    {
        str[i++] = '0';
    }
    else
    {
        while (num != 0)
        {
            str[i++] = '0' + (num % 10);
            num /= 10;
        }
    }

    if (is_negative)
    {
        str[i++] = '-';
    }
    int j;
    int len = i;
    for (j = 0; j < len / 2; j++)
    {
        char temp = str[j];
        str[j] = str[len - j - 1];
        str[len - j - 1] = temp;
    }

    str[i] = '\0';
}

void write_text(const char *text)
{
    char stringify_pid[20];
    itoa(getpid(), stringify_pid);
    int fd = open(strcat(strcat("output", stringify_pid), ".txt"), O_WRONLY | O_CREATE);
    if (fd < 0)
    {
        printf(2, "Failed to open file for writing\n");
        exit();
    }

    write(fd, text, strlen(text));
    close(fd);
}

int main()
{
    int i;
    int pid;

    for (i = 0; i < NUM_PROCESSES; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(2, "Fork failed\n");
            exit();
        }
        else if (pid == 0)
        {
            char stringify_pid[20];
            itoa(getpid(), stringify_pid);
            char* message = strcat("Hello from Process ", stringify_pid);
            message = strcat(message, "\n");
            write_text(message);
            exit();
        }
    }

    for (i = 0; i < NUM_PROCESSES; i++)
    {
        wait();
    }

    print_syscall_count();

    exit();
}
#include "types.h"
#include "user.h"

void help()
{
    printf(1, "usage: command <inputs>\n");
    printf(1, "list of commands:\n");
    printf(1, "    info\n");
    printf(1, "    change_queue <pid> <new_queue>\n");
    printf(1, "    set_bjf_process <pid> <priority_ratio> <arrival_time_ratio> <executed_cycles_ratio> <process_size_ratio>\n");
    printf(1, "    set_bjf_system <priority_ratio> <arrival_time_ratio> <executed_cycles_ratio> <process_size_ratio>\n");
}

void set_queue(int pid, int new_queue)
{
    if (pid < 1)
        printf(1, "pid cannot be less than 1\n");
    if (new_queue < 1 || new_queue > 3)
        printf(1, "new queue number should be in range [1, 3]\n");
    int result = change_queue(pid, new_queue);
    if (result < 1)
        printf(1, "error in changing queue\n");
    else
        printf(1, "your process with id %d has changed queue from: %d -> %d\n", pid, result, new_queue);
}

void set_bjf_process_params(int pid, float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio)
{
    if (pid < 1)
        printf(1, "pid cannot be less than 1\n");
    if (priority_ratio < 0 || arrival_time_ratio < 0 || executed_cycles_ratio < 0 || process_size_ratio < 0)
        printf(1, "ratios can not be less than 0\n");
    int result = set_bjf_params_for_process(pid, priority_ratio, arrival_time_ratio, executed_cycles_ratio, process_size_ratio);
    if (result < 0)
        printf(1, "error in setting bjf process parameters\n");
    else
        printf(1, "the process with id %d has changed bjf parameters successfully\n", pid);
}

void set_bjf_system_params(float priority_ratio, float arrival_time_ratio, float executed_cycles_ratio, float process_size_ratio)
{
    if (priority_ratio < 0 || arrival_time_ratio < 0 || executed_cycles_ratio < 0 || process_size_ratio < 0)
        printf(1, "ratios can not be less than 0\n");
    set_bjf_params_for_system(priority_ratio, arrival_time_ratio, executed_cycles_ratio, process_size_ratio);

    printf(1, "bjf parameters for system has changed successfully\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        help();
    }
    else if (argc == 2)
    {
        if (!strcmp(argv[1], "info"))
            print_process_info();
        else
            help();
    }
    else if (argc == 4)
    {
        if (!strcmp(argv[1], "change_queue"))
            set_queue(atoi(argv[2]), atoi(argv[3]));
        else
            help();
    }
    else if (argc == 6)
    {
        if (!strcmp(argv[1], "set_bjf_system"))

            set_bjf_system_params(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        else
            help();
    }
    else if (argc == 7)
    {
        if (!strcmp(argv[1], "set_bjf_process"))
            set_bjf_process_params(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        else
            help();
    }
    else
    {
        help();
    }
    exit();
}
#include "types.h"
#include "user.h"

int main(int argc, char const *argv[])
{
    int pid = getpid();
    printf(1, "Current Process id is %d\n", pid);
    exit();
}

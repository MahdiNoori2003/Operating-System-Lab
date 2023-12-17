#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "stat.h"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf(1, "2 args required. provided %d\n", argc);
        exit();
    }
    char *src = argv[1];
    char *dst = argv[2];

    if (copy_file(src, dst) < 0)
    {
        printf(1, "uanble to copy file\n");
        exit();
    }

    printf(1, "file %s copied to %s successfuly\n", src, dst);
    exit();
}
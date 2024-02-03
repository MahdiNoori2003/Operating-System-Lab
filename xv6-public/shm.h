#define MAX_SHARED_PAGES 16

struct shared_page
{
    int id;
    uint ref_count;
    char *frame;
};

struct shared_memory
{
    struct shared_page shared_pages[MAX_SHARED_PAGES];
    struct spinlock slk;
};

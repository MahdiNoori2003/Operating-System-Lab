#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(find_digital_root)
SYSCALL(get_uncle_count)
SYSCALL(get_process_lifetime)
SYSCALL(copy_file)
SYSCALL(change_queue)
SYSCALL(set_bjf_params_for_process)
SYSCALL(set_bjf_params_for_system)
SYSCALL(print_process_info)
SYSCALL(set_bjf_priority)
SYSCALL(init_prioritylock)
SYSCALL(acquire_prioritylock)
SYSCALL(release_prioritylock)
SYSCALL(print_syscall_count)
SYSCALL(reset_syscall_count)
SYSCALL(open_sharedmem)
SYSCALL(close_sharedmem)


#include <types.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <syscall.h>

void sys__exit(int code) {
    struct addrspace *as = proc_getas();

    as_destroy(as);

    thread_exit();

    (void) code;    // TODO: Handle return value
}

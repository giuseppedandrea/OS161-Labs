#include <types.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <syscall.h>
#include <current.h>
#include <synch.h>

void sys__exit(int code) {
#if OPT_WAITPID
    struct proc *proc = curproc;
    struct thread *thread = curthread;

    proc->p_returncode = code;

    proc_remthread(thread);
    KASSERT(thread->t_proc == NULL);

    V(proc->p_sem);
#else
    struct addrspace *as = proc_getas();

    as_destroy(as);

    (void)code;
#endif

    thread_exit();
}

pid_t sys_waitpid(pid_t pid, userptr_t returncode, int flags) {
#if OPT_WAITPID
    struct proc *proc;
    int rc;

    proc = proc_by_pid(pid);
    if (proc == NULL) {
        return -1;
    }

    rc = proc_wait(proc);
    if (returncode != NULL) {
        *(int *)returncode = rc;
    }

    (void)flags;        // Not handled

    return pid;
#else
    (void)pid;
    (void)returncode;
    (void)flags;
    return -1;
#endif
}

pid_t sys_getpid(void) {
#if OPT_WAITPID
    KASSERT(curproc != NULL);

    return curproc->p_pid;
#else
    return -1;
#endif
}

#include <types.h>
#include <kern/errno.h>
#include <proc.h>
#include <mips/trapframe.h>
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

#if OPT_FORK
static
void
forked_process_thread(void *tf, unsigned long junk)
{
    struct trapframe *childtf = (struct trapframe *)tf;

    (void)junk;

    enter_forked_process(childtf);

    panic("enter_forked_process returned\n");
}
#endif

pid_t sys_fork(struct trapframe *tf) {
#if OPT_FORK
    struct proc *childproc;
    struct trapframe *childtf;
    int ret;

    KASSERT(curproc != NULL);

    childproc = proc_create_runprogram(curproc->p_name);
    if (childproc == NULL) {
        return -ENOMEM;
    }

    ret = as_copy(curproc->p_addrspace, &(childproc->p_addrspace));
    if (ret != 0) {
        kprintf("as_copy failed: %s\n", strerror(ret));
        proc_destroy(childproc);
        return -ret;
    }

    // TODO: linking parent/child, so that child terminated on parent exit

    childtf = kmalloc(sizeof(struct trapframe));
    if (childtf == NULL) {
        proc_destroy(childproc);
        return -ENOMEM;
    }
    memcpy(childtf, tf, sizeof(struct trapframe));

    ret = thread_fork(childproc->p_name /* thread name */,
            childproc /* new process */,
            forked_process_thread /* thread function */,
            (void *)childtf /* thread arg */,
            (unsigned long)0 /* thread arg */);
    if (ret) {
        kprintf("thread_fork failed: %s\n", strerror(ret));
        proc_destroy(childproc);
        kfree(childtf);
        return -ret;
    }

    return childproc->p_pid;
#else
    (void)tf;
    return -1;
#endif
}

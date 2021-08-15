#include <types.h>
#include <kern/errno.h>
#include <proc.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <addrspace.h>
#include <syscall.h>
#include <current.h>
#include <synch.h>

void
sys__exit(int code)
{
#if OPT_WAITPID
    struct proc *proc = curproc;
    struct thread *thread = curthread;

    proc->p_returncode = code;

    proc_remthread(thread);
    KASSERT(thread->t_proc == NULL);

    V(proc->p_sem);

    thread_exit();
#else
    (void)code;         // Return Code not handled

    as_destroy(curproc->p_addrspace);

    thread_exit();
#endif
}

pid_t
sys_waitpid(pid_t pid, userptr_t returncode, int flags)
{
#if OPT_WAITPID
    struct proc *proc;
    int result;

    proc = proc_by_pid(pid);
    if (proc == NULL) {
        return -ECHILD;
    }

    result = proc_wait(proc);
    if (returncode != NULL) {
        *(int *)returncode = result;
    }

    (void)flags;        // Flags not handled

    return pid;
#else
    (void)pid;
    (void)returncode;
    (void)flags;
    return -ENOSYS;
#endif
}

pid_t
sys_getpid(void)
{
#if OPT_WAITPID
    KASSERT(curproc != NULL);

    return curproc->p_pid;
#else
    return -ENOSYS;
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

pid_t
sys_fork(struct trapframe *tf)
{
#if OPT_FORK
    struct proc *childproc;
    struct trapframe *childtf;
    int result;

    KASSERT(curproc != NULL);

    childproc = proc_create_runprogram(curproc->p_name);
    if (childproc == NULL) {
        return -ENOMEM;
    }

    result = as_copy(curproc->p_addrspace, &(childproc->p_addrspace));
    if (result != 0) {
        kprintf("as_copy failed: %s\n", strerror(result));
        proc_destroy(childproc);
        return -ENOMEM;
    }

    proc_filetable_copy(curproc, childproc);

    // TODO: linking parent/child, so that child terminated on parent exit

    childtf = kmalloc(sizeof(struct trapframe));
    if (childtf == NULL) {
        proc_destroy(childproc);
        return -ENOMEM;
    }
    memcpy(childtf, tf, sizeof(struct trapframe));

    result = thread_fork(childproc->p_name /* thread name */,
                childproc /* new process */,
                forked_process_thread /* thread function */,
                (void *)childtf /* thread arg */,
                (unsigned long)0 /* thread arg */);
    if (result != 0) {
        kprintf("thread_fork failed: %s\n", strerror(result));
        proc_destroy(childproc);
        kfree(childtf);
        return -ENOMEM;
    }

    return childproc->p_pid;
#else
    (void)tf;
    return -ENOSYS;
#endif
}

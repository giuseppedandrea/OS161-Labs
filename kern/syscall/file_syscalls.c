#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <lib.h>
#include <uio.h>
#include <vnode.h>
#include <copyinout.h>
#include <proc.h>
#include <current.h>
#include <vfs.h>
#include <limits.h>
#include <syscall.h>

#if OPT_FILE
/* Max number of system-wide open files */
#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

/* System-wide open file table */
struct openfile system_filetable[SYSTEM_OPEN_MAX];
#endif

int
sys_open(const_userptr_t pathname, int flags, mode_t mode, int *errp)
{
#if OPT_FILE
    struct vnode *vn;
    struct openfile *of;
    size_t i;
    int fd, result;

    result = vfs_open((char *)pathname, flags, mode, &vn);
    if (result != 0) {
        *errp = result;
        return -1;
    }

    for (i = 0, of = NULL; i < SYSTEM_OPEN_MAX; i++) {
        if (system_filetable[i].vn == NULL) {
            of = &system_filetable[i];
            of->vn = vn;
            if (flags & O_APPEND) {
                of->offset = 0;     // TODO: handle offset with append
            } else {
                of->offset = 0;
            }
            of->count = 1;
            break;
        }
    }
    if (of == NULL) {
        vfs_close(vn);
        *errp = ENFILE;
        return -1;
    }

    for (fd = STDERR_FILENO + 1; fd < OPEN_MAX; fd++) {
        if (curproc->p_filetable[fd] == NULL) {
            curproc->p_filetable[fd] = of;
            break;
        }
    }
    if (fd >= OPEN_MAX) {
        vfs_close(vn);
        *errp = EMFILE;
        return -1;
    }

    return fd;
#else
    (void)pathname;
    (void)flags;
    (void)mode;
    *errp = ENOSYS;
    return -1;
#endif
}

int
sys_close(int fd, int *errp)
{
#if OPT_FILE
    struct openfile *of;
    struct vnode *vn;

    if ((fd < 0) || (fd >= OPEN_MAX)) {
        *errp = EBADF;
        return -1;
    }

    of = curproc->p_filetable[fd];
    if (of == NULL) {
        *errp = EBADF;
        return -1;
    }

    curproc->p_filetable[fd] = NULL;

    if (of->count > 1) {
        of->count--;
        return 0;
    }

    vn = of->vn;
    of->vn = NULL;

    vfs_close(vn);

    return 0;
#else
    (void)fd;
    *errp = ENOSYS;
    return -1;
#endif
}

#if OPT_FILE
static
ssize_t
file_read(int fd, userptr_t buf_ptr, size_t size, int *errp)
{
    struct openfile *of;
    struct vnode *vn;
    char *kbuf;
    struct iovec iov;
    struct uio ku;
    int result;
    ssize_t nread;

    of = curproc->p_filetable[fd];
    if (of == NULL) {
        *errp = EBADF;
        return -1;
    }

    vn = of->vn;
    if (vn == NULL) {
        *errp = EBADF;
        return -1;
    }

    kbuf = kmalloc(size * sizeof(char));
    if (kbuf == NULL) {
        *errp = ENOMEM;
        return -1;
    }

    uio_kinit(&iov, &ku, kbuf, size, of->offset, UIO_READ);
    result = VOP_READ(of->vn, &ku);
    if (result != 0) {
        *errp = result;
        return -1;
    }

    of->offset = ku.uio_offset;

    nread = size - ku.uio_resid;

    copyout(kbuf, buf_ptr, nread);

    kfree(kbuf);

    return nread;
}
#endif

ssize_t
sys_read(int fd, userptr_t buf_ptr, size_t size, int *errp)
{
    size_t i;
    char *buf = (char *)buf_ptr;

    if ((fd < 0) || (fd >= OPEN_MAX)) {
        *errp = EBADF;
        return -1;
    }

    if (buf == NULL) {
        *errp = EFAULT;
        return -1;
    }

    if (fd != STDIN_FILENO) {
#if OPT_FILE
        return file_read(fd, buf_ptr, size, errp);
#else
        *errp = ENOSYS;
        return -1;
#endif
    }

    for (i = 0; i < size; i++) {
        buf[i] = getch();
        if (buf[i] < 0) {
            return i;
        }
    }

    return (ssize_t)size;
}

#if OPT_FILE
static
ssize_t
file_write(int fd, const_userptr_t buf_ptr, size_t size, int *errp)
{
    struct openfile *of;
    struct vnode *vn;
    char *kbuf;
    struct iovec iov;
    struct uio ku;
    int result;
    ssize_t nwrite;

    of = curproc->p_filetable[fd];
    if (of == NULL) {
        *errp = EBADF;
        return -1;
    }

    vn = of->vn;
    if (vn == NULL) {
        *errp = EBADF;
        return -1;
    }

    kbuf = kmalloc(size * sizeof(char));
    if (kbuf == NULL) {
        *errp = ENOMEM;
        return -1;
    }

    copyin(buf_ptr, kbuf, size);

    uio_kinit(&iov, &ku, kbuf, size, of->offset, UIO_WRITE);
    result = VOP_WRITE(of->vn, &ku);
    if (result != 0) {
        *errp = result;
        return -1;
    }

    kfree(kbuf);

    of->offset = ku.uio_offset;

    nwrite = size - ku.uio_resid;

    return nwrite;
}
#endif

ssize_t
sys_write(int fd, const_userptr_t buf_ptr, size_t size, int *errp)
{
    size_t i;
    const char *buf = (const char *)buf_ptr;

    if ((fd < 0) || (fd >= OPEN_MAX)) {
        *errp = EBADF;
        return -1;
    }

    if (buf == NULL) {
        *errp = EFAULT;
        return -1;
    }

    if ((fd != STDOUT_FILENO) && (fd != STDERR_FILENO)) {
#if OPT_FILE
        return file_write(fd, buf_ptr, size, errp);
#else
        *errp = ENOSYS;
        return -1;
#endif
    }

    for (i = 0; i < size; i++) {
        putch(buf[i]);
    }

    return (ssize_t)size;
}

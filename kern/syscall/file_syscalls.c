#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <syscall.h>

ssize_t
sys_write(int fd, const_userptr_t buf_ptr, size_t size)
{
    size_t i;
    const char *buf = (const char *)buf_ptr;

    if ((fd != STDOUT_FILENO) && (fd != STDERR_FILENO)) {
        return -ENOSYS;
    }

    if (buf == NULL) {
        return -EFAULT;
    }

    for (i = 0; i < size; i++) {
        putch(buf[i]);
    }

    return (ssize_t)size;
}

ssize_t
sys_read(int fd, userptr_t buf_ptr, size_t size)
{
    size_t i;
    char *buf = (char *)buf_ptr;

    if (fd != STDIN_FILENO) {
        return -ENOSYS;
    }

    if (buf == NULL) {
        return -EFAULT;
    }

    for (i = 0; i < size; i++) {
        buf[i] = getch();
        if (buf[i] < 0) {
            return i;
        }
    }

    return (ssize_t)size;
}

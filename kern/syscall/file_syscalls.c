#include <types.h>
#include <kern/unistd.h>
#include <lib.h>
#include <syscall.h>

ssize_t sys_write(int fd, const_userptr_t buf_ptr, size_t size) {
    size_t i;
    const char *p = (const char *)buf_ptr;

    if ((fd != STDOUT_FILENO) && (fd != STDERR_FILENO)) {
        return -1;
    }

    if (p == NULL) {
        return -1;
    }

    for (i = 0; i < size; i++) {
        putch(p[i]);
    }

    return (ssize_t)size;
}

ssize_t sys_read(int fd, userptr_t buf_ptr, size_t size) {
    size_t i;
    char *p = (char *)buf_ptr;

    if (fd != STDIN_FILENO) {
        return -1;
    }

    if (p == NULL) {
        return -1;
    }

    for (i = 0; i < size; i++) {
        p[i] = getch();
        if (p[i] < 0) {
            return i;
        }
    }

    return (ssize_t)size;
}

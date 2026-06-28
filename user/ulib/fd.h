#ifndef ULIB_FD_H_
#define ULIB_FD_H_

#include <stddef.h>
#include <stdint.h>
#include "syscall.h"

#define FD_MAX 64

/* Called by crt0 with values from the PAB auxv. */
void fd_init(uint32_t caps_base, uint32_t caps_count);

/* Open a name via the kernel namespace, returning an integer fd. */
int fd_open(const char *path, uint32_t flags);

int fd_read(int fd, void *buf, size_t n);
int fd_write(int fd, const void *buf, size_t n);
int fd_close(int fd);

/* Get the raw cap handle for a given fd (0 if fd is invalid). */
cap_handle_t fd_get_cap(int fd);

#endif /* ULIB_FD_H_ */

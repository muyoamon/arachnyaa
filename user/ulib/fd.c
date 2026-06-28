#include "fd.h"
#include "syscall.h"

static cap_handle_t fd_table[FD_MAX];

void fd_init(uint32_t caps_base, uint32_t caps_count) {
  if (caps_count > 3u) caps_count = 3u;
  for (uint32_t i = 0; i < caps_count; i++)
    fd_table[i] = sys_cap_get(caps_base + i);
}

int fd_open(const char *path, uint32_t flags) {
  cap_handle_t cap = sys_open(path, flags);
  if (cap == 0) return -1;
  for (int i = 3; i < FD_MAX; i++) {
    if (fd_table[i] == 0) {
      fd_table[i] = cap;
      return i;
    }
  }
  sys_cap_close(cap);
  return -1;
}

int fd_read(int fd, void *buf, size_t n) {
  if (fd < 0 || fd >= FD_MAX || fd_table[fd] == 0) return -1;
  return sys_read(fd_table[fd], buf, n);
}

int fd_write(int fd, const void *buf, size_t n) {
  if (fd < 0 || fd >= FD_MAX || fd_table[fd] == 0) return -1;
  return sys_write(fd_table[fd], buf, (uint32_t)n);
}

int fd_close(int fd) {
  if (fd < 0 || fd >= FD_MAX || fd_table[fd] == 0) return -1;
  sys_cap_close(fd_table[fd]);
  fd_table[fd] = 0;
  return 0;
}

cap_handle_t fd_get_cap(int fd) {
  if (fd < 0 || fd >= FD_MAX) return 0;
  return fd_table[fd];
}

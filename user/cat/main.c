#include "../ulib/fd.h"
#include "../ulib/string.h"

int main(int argc, char **argv) {
  char buf[256];
  int n;

  if (argc <= 1) {
    /* Read from stdin until EOF. */
    while ((n = fd_read(0, buf, sizeof(buf))) > 0)
      fd_write(1, buf, (size_t)n);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    int fd = fd_open(argv[i], 0);
    if (fd < 0) {
      fd_write(2, "cat: cannot open: ", 18);
      fd_write(2, argv[i], strlen(argv[i]));
      fd_write(2, "\n", 1);
      continue;
    }
    while ((n = fd_read(fd, buf, sizeof(buf))) > 0)
      fd_write(1, buf, (size_t)n);
    fd_close(fd);
  }
  return 0;
}

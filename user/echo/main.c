#include "../ulib/fd.h"
#include "../ulib/string.h"

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    fd_write(1, argv[i], strlen(argv[i]));
    fd_write(1, (i + 1 < argc) ? " " : "\n", 1);
  }
  if (argc <= 1)
    fd_write(1, "\n", 1);
  return 0;
}

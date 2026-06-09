#include <stdint.h>
#include "syscall.h"
#include "string.h"

static void dbgprint(const char *s) {
  while (*s) sys_putc(*s++);
}

void _start(void) {
  static const char path[] = "log:stdout";
  static const char msg[]  = "Hello from log-client via IPC\n";

  dbgprint("Hello from log-client via putc\n");

  dbgprint("[LOGCLIENT] calling sys_open\n");
  cap_handle_t h = sys_open(path, 0);
  if (h == 0) sys_exit(1);

  dbgprint("[LOGCLIENT] calling sys_write\n");
  int rc = sys_write(h, msg, sizeof(msg) - 1);
  (void)rc;

  dbgprint("[LOGCLIENT] calling sys_read\n");
  char buf[256];
  memset(buf, 0, sizeof(buf));
  rc = sys_read(h, buf, sizeof(buf));
  dbgprint("[LOGCLIENT] read result:\n");
  dbgprint(buf);

  dbgprint("[LOGPRINT] closing the handle\n");
  rc = sys_close(h);
  if (rc) {
    dbgprint("[LOGPRINT] fail to close handle\n");
    sys_exit(rc);
  }

  sys_exit(0);
}

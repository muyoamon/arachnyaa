#include "../ulib/fd.h"
#include "../ulib/string.h"
#include "../ulib/syscall.h"
#include "../vfs/fs_proto.h"

int main(int argc, char **argv) {
  const char *path = (argc > 1) ? argv[1] : "/";

  cap_handle_t dh = sys_open(path, FS_O_DIRECTORY);
  if (dh == 0) {
    fd_write(2, "ls: cannot open: ", 17);
    fd_write(2, path, strlen(path));
    fd_write(2, "\n", 1);
    return 1;
  }

  for (uint32_t idx = 0; ; idx++) {
    sys_ipc_msg_t req, rep;
    memset(&req, 0, sizeof(req));
    memset(&rep,  0, sizeof(rep));
    req.opcode    = FS_OP_READDIR;
    req.num_bytes = sizeof(uint32_t);
    memcpy(req.data, &idx, sizeof(uint32_t));

    int rc = sys_call(dh, &req, &rep);
    if (rc != 0 || rep.num_bytes == 0) break;

    const fs_dirent_t *de = (const fs_dirent_t *)rep.data;
    fd_write(1, de->name, strlen(de->name));
    if (de->type == FS_TYPE_DIR)
      fd_write(1, "/", 1);
    fd_write(1, "\n", 1);
  }

  sys_cap_close(dh);
  return 0;
}

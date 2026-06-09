#include <stddef.h>
#include <stdint.h>
#include "syscall.h"
#include "string.h"

/* Pre-installed bootstrap endpoint cap: type=ENDPOINT, gen=1, index=0 */
#define BOOTSTRAP_LOG_HANDLER \
  (((uint64_t)KOBJ_ENDPOINT << 56) | ((uint64_t)1 << 32) | 0u)

static void dbgwrite(const char *str) {
  while (*str) sys_putc(*str++);
}

static void puts_raw(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++) sys_putc(buf[i]);
}

/* Returns 1 if buf[0..len-1] equals the null-terminated literal. */
static int streq_bytes(const char *buf, size_t len, const char *lit) {
  size_t i = 0;
  while (lit[i] != '\0') {
    if (i >= len || buf[i] != lit[i]) return 0;
    i++;
  }
  return i == len;
}

static void bind_log_protocol(void) {
  static const char protocol[] = "log";
  sys_ns_bind(protocol, BOOTSTRAP_LOG_HANDLER,
              KOP_OPEN | KOP_WRITE | KOP_CLOSE | KOP_READ);
}

static void spawn_log_client(void) {
  sys_proc_arg_t arg;
  pid_t pid = 0;

  memset(&arg, 0, sizeof(arg));
  arg.flags = SYS_PROG_F_BOOTMODULE;
  arg.module_name = "log-client";
  arg.argv0 = "log-client";

  sys_spawn(&arg, &pid, 0);
  (void)pid;
}

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  sys_open_reply_t open_reply;

  memset(&reply, 0, sizeof(reply));
  memset(&open_reply, 0, sizeof(open_reply));

  if (streq_bytes((const char *)req->data, req->num_bytes, "stdout") ||
      streq_bytes((const char *)req->data, req->num_bytes, "console")) {
    reply.object_id = 1;
    open_reply.allowed_ops = KOP_WRITE | KOP_CLOSE | KOP_READ;
    reply.num_bytes = sizeof(open_reply);
    memcpy(reply.data, &open_reply, sizeof(open_reply));
  } else {
    reply.object_id = 0;
    reply.num_bytes = 0;
  }

  sys_reply(&reply);
}

static void handle_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t written = 0;

  memset(&reply, 0, sizeof(reply));

  if (req->object_id == 1) {
    puts_raw((const char *)req->data, req->num_bytes);
    written = req->num_bytes;
  }

  reply.num_bytes = sizeof(written);
  memcpy(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

static void handle_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  static const char str[] = "Hello from reading on initd.\n";
  size_t nbyte_to_read = *(const size_t *)req->data;

  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(str) > nbyte_to_read ? (uint32_t)nbyte_to_read : (uint32_t)sizeof(str);
  memcpy(reply.data, str, reply.num_bytes);
  sys_reply(&reply);
}

static void handle_close(const sys_ipc_msg_t *req) {
  (void)req;
  dbgwrite("Received close request!\n");
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

static void handle_unknown(void) {
  sys_ipc_msg_t reply;
  uint32_t rc = 0;

  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(rc);
  memcpy(reply.data, &rc, sizeof(rc));
  sys_reply(&reply);
}

static void server_loop(void) {
  dbgwrite("Starting initd server!\n");
  for (;;) {
    sys_ipc_msg_t req;
    int err;

    memset(&req, 0, sizeof(req));
    err = sys_recv(BOOTSTRAP_LOG_HANDLER, &req);
    if (err != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:  handle_open(&req);  break;
    case IPC_OP_WRITE: handle_write(&req); break;
    case IPC_OP_READ:  handle_read(&req);  break;
    case IPC_OP_CLOSE: handle_close(&req); break;
    default:           handle_unknown();   break;
    }
  }
}

void _start(void) {
  bind_log_protocol();
  spawn_log_client();
  server_loop();
  sys_exit(0);
}

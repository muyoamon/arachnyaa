/*
 * shell — interactive command shell
 *
 * Opens tty:0 for terminal I/O.  Command syntax:
 *   [proto:]name [args...]
 *
 * If no ':' in the first token, each prefix in $PATH is tried in order.
 * Default $PATH = "bm".
 *
 * Builtins: exit, set VAR VALUE, unset VAR
 */

#include <stddef.h>
#include <stdint.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"

#define LINE_MAX   240
#define MAX_TOKENS  16
#define MAX_VARS    16

/* ---- variable store ---- */

static char var_names[MAX_VARS][32];
static char var_vals[MAX_VARS][64];
static int  var_count = 0;

static const char *var_get(const char *name) {
  for (int i = 0; i < var_count; i++)
    if (strcmp(var_names[i], name) == 0)
      return var_vals[i];
  return NULL;
}

static void var_set(const char *name, const char *val) {
  for (int i = 0; i < var_count; i++) {
    if (strcmp(var_names[i], name) == 0) {
      strncpy(var_vals[i], val, sizeof(var_vals[i]) - 1);
      var_vals[i][sizeof(var_vals[i]) - 1] = '\0';
      return;
    }
  }
  if (var_count < MAX_VARS) {
    strncpy(var_names[var_count], name, sizeof(var_names[var_count]) - 1);
    var_names[var_count][sizeof(var_names[var_count]) - 1] = '\0';
    strncpy(var_vals[var_count], val, sizeof(var_vals[var_count]) - 1);
    var_vals[var_count][sizeof(var_vals[var_count]) - 1] = '\0';
    var_count++;
  }
}

static void var_unset(const char *name) {
  for (int i = 0; i < var_count; i++) {
    if (strcmp(var_names[i], name) == 0) {
      for (int j = i; j < var_count - 1; j++) {
        memcpy(var_names[j], var_names[j + 1], sizeof(var_names[j]));
        memcpy(var_vals[j],  var_vals[j + 1],  sizeof(var_vals[j]));
      }
      var_count--;
      return;
    }
  }
}

/* ---- terminal helpers ---- */

static cap_handle_t g_tty;

static void tty_write(const char *s, size_t len) {
  if (g_tty && len > 0)
    sys_write(g_tty, s, len);
}

static void tty_puts(const char *s) {
  tty_write(s, strlen(s));
}

static int tty_readline(char *buf, size_t cap) {
  return sys_read(g_tty, buf, cap - 1u);
}

/* ---- tokenizer ---- */

static int tokenize(char *line, size_t len,
                    char *tokens[], int max_tokens) {
  int count = 0;
  size_t i = 0;
  while (i < len && count < max_tokens) {
    while (i < len &&
           (line[i] == ' ' || line[i] == '\t' ||
            line[i] == '\r' || line[i] == '\n'))
      i++;
    if (i >= len) break;
    tokens[count++] = &line[i];
    while (i < len &&
           line[i] != ' ' && line[i] != '\t' &&
           line[i] != '\r' && line[i] != '\n')
      i++;
    if (i < len) line[i++] = '\0';
  }
  return count;
}

static int has_colon(const char *s) {
  while (*s) if (*s++ == ':') return 1;
  return 0;
}

/* ---- entry ---- */

void _start(void) {
  var_set("PATH", "bm");

  g_tty = sys_open("tty:0", 0);
  if (g_tty == 0)
    sys_exit(1);

  // clear the terminal 
  tty_write("\033[2J\033[H", 7);


  tty_puts("arachnyaa shell\n");

  for (;;) {
    tty_puts("$ ");

    char line[LINE_MAX];
    memset(line, 0, sizeof(line));
    int n = tty_readline(line, sizeof(line));
    if (n <= 0) continue;
    if (n < LINE_MAX) line[n] = '\0';

    char *tokens[MAX_TOKENS];
    int tc = tokenize(line, (size_t)n, tokens, MAX_TOKENS);
    if (tc == 0) continue;

    /* Builtins */
    if (strcmp(tokens[0], "exit") == 0)
      sys_exit(0);

    if (strcmp(tokens[0], "clear") == 0) {
      tty_write("\033[2J\033[H", 7);
      continue;
    }

    if (strcmp(tokens[0], "set") == 0) {
      if (tc >= 3) var_set(tokens[1], tokens[2]);
      continue;
    }

    if (strcmp(tokens[0], "unset") == 0) {
      if (tc >= 2) var_unset(tokens[1]);
      continue;
    }

    /* Resolve command to an exec handle */
    cap_handle_t exec_h = 0;

    if (has_colon(tokens[0])) {
      // tty_puts(tokens[0]);
      exec_h = (cap_handle_t)sys_open(tokens[0], 0);
    } else {
      const char *path = var_get("PATH");
      if (!path) path = "bm";
      char path_buf[128];
      strncpy(path_buf, path, sizeof(path_buf) - 1);
      path_buf[sizeof(path_buf) - 1] = '\0';

      char *p = path_buf;
      while (*p && exec_h == 0) {
        char *end = p;
        while (*end && *end != ' ') end++;
        int more = (*end == ' ');
        if (more) *end = '\0';

        char resolved[96];
        size_t plen = strlen(p);
        size_t clen = strlen(tokens[0]);
        if (plen + 1u + clen < sizeof(resolved)) {
          memcpy(resolved, p, plen);
          resolved[plen] = ':';
          memcpy(resolved + plen + 1u, tokens[0], clen + 1u);
          exec_h = (cap_handle_t)sys_open(resolved, 0);
        }

        p = more ? end + 1 : end;
      }
    }

    if (exec_h == 0) {
      tty_puts("not found: ");
      tty_puts(tokens[0]);
      tty_puts("\n");
      continue;
    }

    /* Send IPC_OP_EXEC; pass tty as stdin/stdout/stderr */
    sys_ipc_msg_t exec_req, exec_rep;
    memset(&exec_req, 0, sizeof(exec_req));
    memset(&exec_rep, 0, sizeof(exec_rep));
    exec_req.opcode      = IPC_OP_EXEC;
    exec_req.num_handles = 3;
    exec_req.handles[0]  = g_tty;
    exec_req.handles[1]  = g_tty;
    exec_req.handles[2]  = g_tty;

    size_t argv0_len = strlen(tokens[0]) + 1u;
    exec_req.num_bytes = (uint32_t)argv0_len;
    memcpy(exec_req.data, tokens[0], argv0_len);

    int rc = sys_call(exec_h, &exec_req, &exec_rep);
    sys_cap_close(exec_h);

    if (rc != 0 || exec_rep.handles[0] == 0) {
      tty_puts("exec failed\n");
      continue;
    }

    cap_handle_t watch_cap = exec_rep.handles[0];
    sys_proc_wait(watch_cap);
    sys_cap_close(watch_cap);
  }
}

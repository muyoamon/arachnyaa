/*
 * shell — interactive command shell
 *
 * Opens tty:0 for terminal I/O.  Command syntax:
 *   [proto:]name [args...]
 *
 * Bare names (no ':') resolve via the default namespace ("") binding.
 *
 * Builtins: exit, set VAR VALUE, unset VAR, clear
 */

#include <stddef.h>
#include <stdint.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "../procd/proc_args.h"
#include "../vfs/fs_proto.h"

#define LINE_MAX    240
#define MAX_TOKENS   32
#define MAX_VARS     16
#define MAX_SEGMENTS  8

/* ---- variable store ---- */

static char var_names[MAX_VARS][32];
static char var_vals[MAX_VARS][64];
static int  var_count = 0;

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

/* ---- entry ---- */

void _start(void) {

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

    /* ---- pipeline detection ---- */

    /* Segment boundaries: seg_start[i] = index of first token in segment i. */
    int seg_start[MAX_SEGMENTS + 1];
    int nseg = 0;
    seg_start[0] = 0;
    for (int i = 0; i < tc; i++) {
      if (tokens[i][0] == '|' && tokens[i][1] == '\0') {
        if (nseg + 1 >= MAX_SEGMENTS) break;
        tokens[i] = NULL; /* mark as separator */
        seg_start[++nseg] = i + 1;
      }
    }
    nseg++; /* total segment count */
    seg_start[nseg] = tc;

    /* Create pipes between adjacent segments. */
    cap_handle_t pipe_r[MAX_SEGMENTS - 1];
    cap_handle_t pipe_w[MAX_SEGMENTS - 1];
    int npipes = nseg - 1;
    int pipe_ok = 1;
    for (int i = 0; i < npipes; i++) {
      if (sys_pipe(&pipe_r[i], &pipe_w[i]) != 0) {
        pipe_ok = 0;
        npipes = i;
        break;
      }
    }

    if (!pipe_ok) {
      for (int i = 0; i < npipes; i++) {
        sys_cap_close(pipe_r[i]);
        sys_cap_close(pipe_w[i]);
      }
      tty_puts("pipe failed\n");
      continue;
    }

    /* Execute each segment. */
    cap_handle_t watch_caps[MAX_SEGMENTS];
    int nwatch = 0;

    for (int s = 0; s < nseg; s++) {
      int start = seg_start[s];
      int end   = seg_start[s + 1];
      /* Find last non-NULL token for arg count. */
      int slen = 0;
      for (int i = start; i < end; i++) {
        if (tokens[i]) slen++;
      }
      if (slen == 0) continue;

      /* Determine stdio caps for this segment. */
      cap_handle_t seg_stdin  = (s == 0)        ? g_tty : pipe_r[s - 1];
      cap_handle_t seg_stdout = (s == nseg - 1) ? g_tty : pipe_w[s];

      /* Scan for < and > redirections; null out operator + filename tokens. */
      cap_handle_t redir_in = 0, redir_out = 0;
      for (int i = start; i < end; i++) {
        if (!tokens[i]) continue;
        int is_in = (tokens[i][0] == '<' && tokens[i][1] == '\0');
        int is_out = (tokens[i][0] == '>' && tokens[i][1] == '\0');
        if (!is_in && !is_out) continue;

        /* Find the filename: next non-NULL token. */
        int j = i + 1;
        while (j < end && !tokens[j]) j++;
        if (j >= end) { tty_puts("syntax error\n"); break; }

        uint32_t flags = is_in ? FS_O_RDONLY
                                : (FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC);
        cap_handle_t fh = sys_open(tokens[j], flags);
        if (fh == 0) {
          tty_puts("cannot open: ");
          tty_puts(tokens[j]);
          tty_puts("\n");
        } else if (is_in) {
          if (redir_in) sys_cap_close(redir_in);
          redir_in = fh;
          seg_stdin = fh;
        } else {
          if (redir_out) sys_cap_close(redir_out);
          redir_out = fh;
          seg_stdout = fh;
        }
        tokens[i] = NULL;
        tokens[j] = NULL;
        i = j;
      }

      /* Find command: first non-NULL token in segment after redirect scan. */
      int cmd_idx = start;
      while (cmd_idx < end && !tokens[cmd_idx]) cmd_idx++;
      if (cmd_idx >= end) {
        if (redir_in)  sys_cap_close(redir_in);
        if (redir_out) sys_cap_close(redir_out);
        continue;
      }

      cap_handle_t exec_h = sys_open(tokens[cmd_idx], FS_O_EXEC);
      if (exec_h == 0) {
        tty_puts("not found: ");
        tty_puts(tokens[cmd_idx]);
        tty_puts("\n");
        if (redir_in)  sys_cap_close(redir_in);
        if (redir_out) sys_cap_close(redir_out);
        continue;
      }

      sys_ipc_msg_t exec_req, exec_rep;
      memset(&exec_req, 0, sizeof(exec_req));
      memset(&exec_rep, 0, sizeof(exec_rep));
      exec_req.opcode      = IPC_OP_EXEC;
      exec_req.num_handles = 3;
      exec_req.handles[0]  = seg_stdin;
      exec_req.handles[1]  = seg_stdout;
      exec_req.handles[2]  = g_tty;

      {
        proc_exec_args_t *pea = (proc_exec_args_t *)exec_req.data;
        uint8_t *blob = pea->blobs;
        uint32_t blob_used = 0;
        for (int i = start; i < end; i++) {
          if (!tokens[i]) continue;
          size_t tlen = strlen(tokens[i]) + 1u;
          if (blob_used + tlen > PROC_EXEC_ARGS_BLOB_MAX) break;
          memcpy(blob + blob_used, tokens[i], tlen);
          blob_used += (uint32_t)tlen;
        }
        pea->argv_bytes = blob_used;
        pea->envp_bytes = 0;
        exec_req.num_bytes = PROC_EXEC_ARGS_HDR_SIZE + blob_used;
      }

      int rc = sys_call(exec_h, &exec_req, &exec_rep);
      sys_cap_close(exec_h);
      if (redir_in)  sys_cap_close(redir_in);
      if (redir_out) sys_cap_close(redir_out);

      if (rc == 0 && exec_rep.handles[0] != 0) {
        if (nwatch < MAX_SEGMENTS)
          watch_caps[nwatch++] = exec_rep.handles[0];
        else
          sys_cap_close(exec_rep.handles[0]);
      } else {
        tty_puts("exec failed: ");
        tty_puts(tokens[start]);
        tty_puts("\n");
      }
    }

    /* Close shell's copies of all pipe ends so writers can signal EOF. */
    for (int i = 0; i < npipes; i++) {
      sys_cap_close(pipe_r[i]);
      sys_cap_close(pipe_w[i]);
    }

    /* Wait for all pipeline stages. */
    for (int i = 0; i < nwatch; i++) {
      sys_proc_wait(watch_caps[i]);
      sys_cap_close(watch_caps[i]);
    }
  }
}

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "kb.h"
#include "fb.h"
#include "vterm.h"

/* Object-ID constants */
#define FOCUS_OID      1u
#define VTERM_OID_BASE 0x100u
#define MAX_VTERMS     4

/* Global vterm state */
static vterm_t  vterms[MAX_VTERMS];
static uint32_t vterm_count  = 0;
static int      focused_vterm = -1;

/* ---- forward declarations ---- */
static void server_loop(cap_handle_t ep);
static void handle_irq(const sys_ipc_msg_t *req);
static void handle_open(const sys_ipc_msg_t *req);
static void handle_focus(const sys_ipc_msg_t *req);
static void dispatch_vterm(uint32_t vi, const sys_ipc_msg_t *req);

/* ---- helper: test if buf[0..len-1] equals null-terminated literal ---- */
static int streq(const char *buf, size_t len, const char *lit) {
    size_t i = 0;
    while (lit[i] != '\0') {
        if (i >= len || buf[i] != lit[i]) return 0;
        i++;
    }
    return (i == len);
}

/* ---- helper: test if buf starts with null-terminated prefix ---- */
static int starts_with(const char *buf, size_t len, const char *prefix) {
    size_t plen = strlen(prefix);
    if (len < plen) return 0;
    return memcmp(buf, prefix, plen) == 0;
}

/* ---- IRQ handler: translate scancode, feed to focused vterm ---- */
static void handle_irq(const sys_ipc_msg_t *req) {
    uint8_t scancode = (uint8_t)req->data[0];
    keyevent_t k = kb_translate(scancode);

    if (k.ch == 0 || focused_vterm < 0) return;

    vterm_t *vt = &vterms[focused_vterm];
    vterm_input(vt, k);
    fb_blit(vt->cells);
    fb_set_cursor(vt->cx, vt->cy);

    if (vt->line_ready && vt->has_pending_read) {
        char buf[VT_LINE_MAX];
        uint32_t n = vterm_consume_line(vt, buf, sizeof(buf));
        sys_ipc_msg_t rep;
        memset(&rep, 0, sizeof(rep));
        rep.num_bytes = n;
        memcpy(rep.data, buf, n);
        sys_reply_to(vt->defer_token, &rep);
        vt->has_pending_read = false;
    }
}

/* ---- OPEN handler: allocate / attach vterm or return focus handle ---- */
static void handle_open(const sys_ipc_msg_t *req) {
    sys_ipc_msg_t rep;
    memset(&rep, 0, sizeof(rep));

    const char *data = (const char *)req->data;
    size_t len = (size_t)req->num_bytes;

    if (streq(data, len, "tty:focus")) {
        /* Focus handle */
        rep.object_id = FOCUS_OID;
        uint32_t ops = KOP_WRITE | KOP_CLOSE;
        rep.num_bytes = sizeof(ops);
        memcpy(rep.data, &ops, sizeof(ops));
    } else if (starts_with(data, len, "tty:") && len == 4) {
        /* New vterm (empty path) */
        if (vterm_count < MAX_VTERMS) {
            uint32_t idx = vterm_count++;
            vterm_init(&vterms[idx], fb_attr(VGA_LGRAY, VGA_BLACK));
            rep.object_id = VTERM_OID_BASE + (uint64_t)idx;
            uint32_t ops = KOP_READ | KOP_WRITE | KOP_CLOSE;
            rep.num_bytes = sizeof(ops);
            memcpy(rep.data, &ops, sizeof(ops));
        }
        /* else: leave rep zeroed — OPEN failed */
    } else if (starts_with(data, len, "tty:") && len == 5 &&
               data[4] >= '0' && data[4] <= '9') {
        /* Attach to existing vterm N */
        uint32_t n = (uint32_t)(data[4] - '0');
        if (n < vterm_count) {
            rep.object_id = VTERM_OID_BASE + (uint64_t)n;
            uint32_t ops = KOP_READ | KOP_WRITE | KOP_CLOSE;
            rep.num_bytes = sizeof(ops);
            memcpy(rep.data, &ops, sizeof(ops));
        }
    }

    sys_reply(&rep);
}

/* ---- Focus-object WRITE handler: switch focused vterm ---- */
static void handle_focus(const sys_ipc_msg_t *req) {
    sys_ipc_msg_t rep;
    memset(&rep, 0, sizeof(rep));

    if (req->opcode == IPC_OP_WRITE && req->num_bytes >= 4) {
        uint32_t n;
        memcpy(&n, req->data, 4);
        if (n < vterm_count) {
            focused_vterm = (int)n;
            fb_blit(vterms[focused_vterm].cells);
            fb_set_cursor(vterms[focused_vterm].cx, vterms[focused_vterm].cy);
        }
    }
    sys_reply(&rep);
}

/* ---- Per-vterm IPC dispatcher ---- */
static void dispatch_vterm(uint32_t vi, const sys_ipc_msg_t *req) {
    sys_ipc_msg_t rep;
    memset(&rep, 0, sizeof(rep));

    vterm_t *vt = &vterms[vi];

    switch (req->opcode) {
    case IPC_OP_WRITE: {
        for (uint32_t i = 0; i < req->num_bytes; i++)
            vterm_putchar(vt, (char)req->data[i]);
        if ((int)vi == focused_vterm) {
            fb_blit(vt->cells);
            fb_set_cursor(vt->cx, vt->cy);
        }
        uint32_t written = req->num_bytes;
        rep.num_bytes = sizeof(written);
        memcpy(rep.data, &written, sizeof(written));
        sys_reply(&rep);
        break;
    }
    case IPC_OP_READ: {
        if (vt->line_ready) {
            char buf[VT_LINE_MAX];
            uint32_t n = vterm_consume_line(vt, buf, sizeof(buf));
            rep.num_bytes = n;
            memcpy(rep.data, buf, n);
            sys_reply(&rep);
        } else {
            /* Defer: do not reply now; store token and wait for key input */
            vt->defer_token       = sys_defer_call();
            vt->has_pending_read  = true;
            /* No sys_reply — handle_irq will call sys_reply_to later */
        }
        break;
    }
    case IPC_OP_CLOSE:
        sys_reply(&rep);
        break;
    default:
        sys_reply(&rep);
        break;
    }
}

/* ---- Server loop ---- */
static void server_loop(cap_handle_t ep) {
    sys_ipc_msg_t req, reply;
    for (;;) {
        memset(&req, 0, sizeof(req));
        int err = sys_recv(ep, &req);
        if (err) continue;

        if (req.opcode == IPC_OP_NOTIFY) {
            handle_irq(&req);
            memset(&reply, 0, sizeof(reply));
            sys_reply(&reply);
            continue;
        }

        if (req.opcode == IPC_OP_OPEN) {
            handle_open(&req);
            continue;
        }

        if (req.object_id == FOCUS_OID) {
            handle_focus(&req);
            continue;
        }

        uint32_t vi = (uint32_t)(req.object_id - VTERM_OID_BASE);
        if (vi < vterm_count) {
            dispatch_vterm(vi, &req);
        } else {
            memset(&reply, 0, sizeof(reply));
            sys_reply(&reply);
        }
    }
}

/* ---- Entry point ---- */
void _start(void) {
    cap_handle_t ep = sys_bootstrap_cap(0);

    cap_handle_t kbd_irq = sys_irq_claim(1);
    sys_irq_notify(kbd_irq, ep);

    fb_init();
    kb_init();

    /* Pre-create vterm 0 */
    vterm_init(&vterms[0], fb_attr(VGA_LGRAY, VGA_BLACK));
    vterm_count   = 1;
    focused_vterm = 0;
    fb_clear(fb_attr(VGA_LGRAY, VGA_BLACK));

    server_loop(ep);
    sys_exit(0);
}

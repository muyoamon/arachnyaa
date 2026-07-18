#include "kernel/pipe.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "mm/kheap.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include <lib/string.h>
#include <stdatomic.h>

static void _pipe_write_release(kobj_t *obj) {
  pipe_close_write((pipe_buf_t *)obj->payload);
}

static void _pipe_read_release(kobj_t *obj) {
  pipe_close_read((pipe_buf_t *)obj->payload);
}

static kobj_ops_t _pipe_write_ops = { .release = _pipe_write_release };
static kobj_ops_t _pipe_read_ops  = { .release = _pipe_read_release  };

static void pipe_buf_put(pipe_buf_t *p) {
  if (atomic_fetch_sub(&p->refcnt, 1) == 1)
    kfree(p);
}

int pipe_create(kobj_t **out_read, kobj_t **out_write) {
  pipe_buf_t *buf = kmalloc(sizeof(pipe_buf_t));
  if (!buf) return KERR_NOMEM;
  memset(buf, 0, sizeof(*buf));
  atomic_store(&buf->refcnt, 2);

  kobj_t *robj = kobj_create();
  kobj_t *wobj = kobj_create();
  if (!robj || !wobj) {
    if (robj) kobj_put(robj);
    if (wobj) kobj_put(wobj);
    kfree(buf);
    return KERR_NOMEM;
  }

  robj->type = KOBJ_PIPE;
  robj->ops  = &_pipe_read_ops;
  robj->payload = buf;

  wobj->type = KOBJ_PIPE;
  wobj->ops  = &_pipe_write_ops;
  wobj->payload = buf;

  *out_read  = robj;
  *out_write = wobj;
  return KERR_OK;
}

void pipe_close_write(pipe_buf_t *p) {
  p->write_closed = 1;
  thread_t *reader = p->blocked_reader;
  if (reader) {
    p->blocked_reader = NULL;
    reader->state = T_READY;
    scheduler_add(reader);
  }
  pipe_buf_put(p);
}

void pipe_close_read(pipe_buf_t *p) {
  p->read_closed = 1;
  thread_t *writer = p->blocked_writer;
  if (writer) {
    p->blocked_writer = NULL;
    writer->state = T_READY;
    scheduler_add(writer);
  }
  pipe_buf_put(p);
}

int pipe_read(pipe_buf_t *p, void *buf, uint32_t nbytes) {
  while (p->count == 0) {
    if (p->write_closed) return 0;
    thread_t *t = scheduler_get_current();
    p->blocked_reader = t;
    t->state = T_BLOCKED;
    scheduler_reschedule();
    p->blocked_reader = NULL;
    if (p->read_closed) return -KERR_IO;
  }

  uint32_t n = (nbytes < p->count) ? nbytes : p->count;
  for (uint32_t i = 0; i < n; i++) {
    ((uint8_t *)buf)[i] = p->buf[p->head];
    p->head = (p->head + 1) % PIPE_BUF_SIZE;
  }
  p->count -= n;

  thread_t *writer = p->blocked_writer;
  if (writer) {
    p->blocked_writer = NULL;
    writer->state = T_READY;
    scheduler_add(writer);
  }
  return (int)n;
}

int pipe_write(pipe_buf_t *p, const void *buf, uint32_t nbytes) {
  if (p->read_closed) return -KERR_IO;

  uint32_t written = 0;
  while (written < nbytes) {
    while (p->count == PIPE_BUF_SIZE) {
      if (p->read_closed) return written ? (int)written : -KERR_IO;
      thread_t *t = scheduler_get_current();
      p->blocked_writer = t;
      t->state = T_BLOCKED;
      scheduler_reschedule();
      p->blocked_writer = NULL;
    }

    uint32_t space = PIPE_BUF_SIZE - p->count;
    uint32_t chunk = (nbytes - written < space) ? nbytes - written : space;
    for (uint32_t i = 0; i < chunk; i++) {
      uint32_t pos = (p->head + p->count) % PIPE_BUF_SIZE;
      p->buf[pos] = ((const uint8_t *)buf)[written + i];
      p->count++;
    }
    written += chunk;

    thread_t *reader = p->blocked_reader;
    if (reader) {
      p->blocked_reader = NULL;
      reader->state = T_READY;
      scheduler_add(reader);
    }
  }
  return (int)written;
}

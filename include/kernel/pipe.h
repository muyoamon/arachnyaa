#ifndef ARACHNYAA_KERNEL_PIPE_H_
#define ARACHNYAA_KERNEL_PIPE_H_

#include <process/thread.h>
#include <kernel/kobj.h>
#include <stdatomic.h>
#include <stdint.h>

#define PIPE_BUF_SIZE 4096

typedef struct {
  uint8_t  buf[PIPE_BUF_SIZE];
  uint32_t head;
  uint32_t count;
  uint8_t  write_closed;
  uint8_t  read_closed;
  thread_t *blocked_reader;
  thread_t *blocked_writer;
  atomic_uint refcnt;
} pipe_buf_t;

/* Create a pipe: two kobj_t* (read end, write end) sharing a pipe_buf_t. */
int pipe_create(kobj_t **out_read, kobj_t **out_write);

/* Blocking read; returns bytes read, 0 on EOF, negative on error. */
int pipe_read(pipe_buf_t *p, void *buf, uint32_t nbytes);

/* Blocking write; returns bytes written, negative on error. */
int pipe_write(pipe_buf_t *p, const void *buf, uint32_t nbytes);

/* Called by kobj ops->release of write/read end respectively. */
void pipe_close_write(pipe_buf_t *p);
void pipe_close_read(pipe_buf_t *p);

#endif /* ARACHNYAA_KERNEL_PIPE_H_ */

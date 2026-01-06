#include "process/rq.h"
#include "lib/stddef.h"
#include <process/thread.h>


void rq_init(rq_t *rq) {
  if (!rq) return;
  rq->head = rq->tail = NULL;
  rq->count = 0;
}

void rq_push(rq_t *rq, thread_t *t) {
  if (!t || !rq) return;
  t->next = NULL;
  if (!rq->tail) rq->head = rq->tail = t;
  else rq->tail = rq->tail->next = t;
  rq->count++;
}

thread_t* rq_pop(rq_t *rq) {
  if (!rq) return NULL;
  thread_t *t = rq->head;
  if (t) {
    rq->head = t->next;
    if (!rq->head) rq->tail = NULL;
    rq->count--;
  }
  return t;
}

thread_t *rq_peek(rq_t *rq) {
  if (!rq) return NULL;
  thread_t *t = rq->head;
  return t;
}

bool rq_remove(rq_t *rq, tid_t tid) {
  if (!rq) return NULL;
  thread_t *prev = NULL;
  thread_t *curr = rq->head;

  while (curr) {
    if (curr->tid == tid) {
      if (prev) prev->next = curr->next;
      else rq->head = curr->next;

      if (curr == rq->tail)
        rq->tail = prev;

      rq->count--;
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}

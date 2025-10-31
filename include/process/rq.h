// ArachnyaaOS - runqueue header
#ifndef ARACHNYAA_PROCESS_RQ_H_
#define ARACHNYAA_PROCESS_RQ_H_

#include <process/thread.h>
#include <stdbool.h>

typedef struct rq {
  thread_t *head;
  thread_t *tail;
  int count;
} rq_t;



void rq_init(rq_t *rq);

void rq_push(rq_t *rq, thread_t *t);

thread_t* rq_pop(rq_t *rq);

thread_t *rq_peek(rq_t *rq);

bool rq_remove(rq_t *rq, tid_t tid);



#endif // ARACHNYAA_PROCESS_RQ_H_

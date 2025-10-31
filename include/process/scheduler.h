// ArachyaaOS -- Scheduler
#ifndef ARACHNYAA_PROCESS_SCHEDULER_H_
#define ARACHNYAA_PROCESS_SCHEDULER_H_

#include "process/rq.h"
#include "process/thread.h"

#define SCHED_PRIO_LEVELS 4
#define SCHED_TIME_SLICE_MS 10

typedef struct scheduler {
  rq_t rq[SCHED_PRIO_LEVELS];
  int time_slice_ms;
} scheduler_t;

/**
 * @brief Initialize scheduler.
 *
 * @param[in] time_slice_ms millisecond before attempt reschedule.
 */
void scheduler_init(int time_slice_ms);

/**
 * @brief Yield current thread.
 *
 */
void scheduler_yield(void);

/**
 * @brief Pop the next suitable thread to run.
 *
 * @return Pointer to thread object.
 */
thread_t *scheduler_pick_next(void);

/**
 * @brief Add thread to scheduler.
 *
 * @param[in] t Pointer to thread object.
 */
void scheduler_add(thread_t *t);

/**
 * @brief Remove thread from scheduler.
 *
 * @param[in] tid Thread ID.
 */
void scheduler_remove(tid_t tid);

/**
 * @brief Reschedule the thread. Running the new thread.
 *
 */
void scheduler_reschedule(void);

/**
 * @brief Switch to new thread.
 *
 * @param[in] new_thread Pointer to new thread.
 */
void scheduler_switch(thread_t *new_thread);

/**
 * @brief Get the current runnning thread.
 *
 * @return Pointer to the running thread.
 */
thread_t *scheduler_get_current();

#endif // ARACHNYAA_PROCESS_SCHEDULER_H_

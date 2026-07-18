#ifndef ARACHNYAA_SYS_PIPE_H_
#define ARACHNYAA_SYS_PIPE_H_

#include <kernel/cap.h>

typedef struct {
  cap_handle_t read_cap;
  cap_handle_t write_cap;
} sys_pipe_result_t;

int sys_pipe(sys_pipe_result_t *out);

#endif /* ARACHNYAA_SYS_PIPE_H_ */

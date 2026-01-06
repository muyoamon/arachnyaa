#ifndef ARACHNYAA_KERNEL_ERROR_H_
#define ARACHNYAA_KERNEL_ERROR_H_

typedef enum {
  KERR_OK = 0,          // success
  KERR_UNKNOWN,         // unknown failure
  KERR_NOMEM,           // out of memory
  KERR_INVAL,           // invalid argument
  KERR_NOTFOUND,        // resource not found
  KERR_BUSY,            // resource busy
  KERR_ACCESS,          // access denied
  KERR_PERM,            // invalid permission
  KERR_NOSPACE,         // out of space
  KERR_IO,              // I/O error
  KERR_FAULT,           // invalid memory access
  KERR_INTERRUPTED,     // operation interrupted
  KERR_UNSUPPORTED,     // operation not supported
  KERR_DEADLOCK,        // would cause deadlock
  KERR_TIMEOUT,         // operation timed out
  KERR_STALE,
  KERR_RANGE,
} kerror_t;

#define RET_IF_ERR(func, act) { \
  kerror_t err = func;          \
  if (err) {                    \
    act;                        \
    return err;                 \
  }                             \
}                               \


#endif // ARACHNYAA_KERNEL_ERROR_H_

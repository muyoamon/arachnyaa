#ifndef ARACHNYAA_KERNEL_NAMESPACE_H_
#define ARACHNYAA_KERNEL_NAMESPACE_H_

#include "kernel/error.h"
#include "kernel/protocol.h"
#include <stdint.h>

struct kobj;

typedef struct {
  char protocol[PROCESS_PROTOCOL_NAME_MAX];
  struct kobj *handler;
  uint32_t declared_ops;
} ns_entry_t;

typedef struct {
  ns_entry_t entries[PROCESS_NAMESPACE_CAPACITY];
  uint32_t count;
} process_namespace_t;

void process_namespace_init(process_namespace_t *ns);
void process_namespace_destroy(process_namespace_t *ns);
int process_namespace_inherit(process_namespace_t *dst,
                              const process_namespace_t *src);
int process_namespace_bind(process_namespace_t *ns, const char *protocol,
                           struct kobj *handler, uint32_t declared_ops);
const ns_entry_t *process_namespace_lookup(const process_namespace_t *ns,
                                           const char *protocol);

#endif // ARACHNYAA_KERNEL_NAMESPACE_H_

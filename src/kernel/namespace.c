#include "kernel/namespace.h"
#include "kernel/kobj.h"
#include "lib/string.h"

static int contains_colon(const char *protocol) {
  for (size_t i = 0; protocol[i] != '\0'; i++) {
    if (protocol[i] == ':') return 1;
  }
  return 0;
}

void process_namespace_init(process_namespace_t *ns) {
  memset(ns, 0, sizeof(*ns));
}

void process_namespace_destroy(process_namespace_t *ns) {
  for (uint32_t i = 0; i < ns->count; i++) {
    if (ns->entries[i].handler) {
      kobj_put(ns->entries[i].handler);
    }
  }
  memset(ns, 0, sizeof(*ns));
}

int process_namespace_inherit(process_namespace_t *dst,
                              const process_namespace_t *src) {
  process_namespace_init(dst);
  dst->count = src->count;
  for (uint32_t i = 0; i < src->count; i++) {
    dst->entries[i] = src->entries[i];
    if (dst->entries[i].handler) {
      kobj_get(dst->entries[i].handler);
    }
  }
  return 0;
}

int process_namespace_bind(process_namespace_t *ns, const char *protocol,
                           kobj_t *handler, uint32_t declared_ops) {
  if (contains_colon(protocol)) {
    return KERR_INVAL;
  }
  size_t len = strlen(protocol);
  if (len >= PROCESS_PROTOCOL_NAME_MAX) {
    return KERR_INVAL;
  }
  if (!handler) {
    return KERR_INVAL;
  }

  for (uint32_t i = 0; i < ns->count; i++) {
    if (strcmp(ns->entries[i].protocol, protocol) == 0) {
      if (ns->entries[i].handler) {
        kobj_put(ns->entries[i].handler);
      }
      memset(ns->entries[i].protocol, 0, sizeof(ns->entries[i].protocol));
      strcpy(ns->entries[i].protocol, protocol);
      ns->entries[i].handler = handler;
      ns->entries[i].declared_ops = declared_ops;
      kobj_get(handler);
      return 0;
    }
  }

  if (ns->count >= PROCESS_NAMESPACE_CAPACITY) {
    return KERR_NOSPACE;
  }

  memset(ns->entries[ns->count].protocol, 0,
         sizeof(ns->entries[ns->count].protocol));
  strcpy(ns->entries[ns->count].protocol, protocol);
  ns->entries[ns->count].handler = handler;
  ns->entries[ns->count].declared_ops = declared_ops;
  kobj_get(handler);
  ns->count++;
  return 0;
}

const ns_entry_t *process_namespace_lookup(const process_namespace_t *ns,
                                           const char *protocol) {
  for (uint32_t i = 0; i < ns->count; i++) {
    if (strcmp(ns->entries[i].protocol, protocol) == 0) {
      return &ns->entries[i];
    }
  }
  return NULL;
}

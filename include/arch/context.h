#ifndef ARACHNYAA_ARCH_CONTEXT_H_
#define ARACHNYAA_ARCH_CONTEXT_H_

#include <lib/stddef.h>

typedef struct arch_context arch_context_t;

arch_context_t *arch_context_init(void (*entry)(void *), void *stack_ptr);

void arch_context_free(arch_context_t* ctx);

void arch_context_switch(arch_context_t *old_ctx, arch_context_t *new_ctx);

void arch_context_first_switch(arch_context_t *init_ctx);

#endif // ARACHNYAA_ARCH_CONTEXT_H_

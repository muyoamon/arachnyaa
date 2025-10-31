#include <arch/x86/context.h>
#include <arch/context.h>
#include <mm/kheap.h>
#include <stdint.h>

arch_context_t  *arch_context_init(void (*entry)(void *), void *stack_ptr) {
  arch_context_t *ctx = kmalloc(sizeof(struct arch_context));
  if (!ctx) return NULL;
  ctx->ebp = 0;
  ctx->ebx = 0;
  ctx->edi = 0;
  ctx->esi = 0;
  ctx->eip = (uint32_t)entry;
  ctx->esp = (uint32_t)(stack_ptr);
  return ctx;
}

void arch_context_free(arch_context_t *ctx) {
  kfree(ctx);
}



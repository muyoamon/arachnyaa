#ifndef ARACHNYAA_KERNEL_USER_H_
#define ARACHNYAA_KERNEL_USER_H_

//
// 
//


#include "arch/user.h"
#include "arch/x86/defs.h"
#include <stdint.h>

static inline void user_enter(uintptr_t entry, uintptr_t user_stack) {
  // generic wrapper for kernel/task layer
  arch_enter_user_mode(entry, user_stack, USER_DS, USER_CS);
}

#endif // ARACHNYAA_KERNEL_USER_H_

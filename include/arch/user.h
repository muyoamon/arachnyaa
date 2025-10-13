#ifndef ARACHNYAA_ARCH_USER_H_
#define ARACHNYAA_ARCH_USER_H_

#include <stdint.h>
/**
 * @brief Switch to user mode.
 *
 * @param[in] entry User entry point (EIP).
 * @param[in] user_stack_top Top of user stack (ESP).
 * @param[in] user_ds User data segment selector (optional, ignored on some arch).
 * @param[in] user_cs User code segment selector (optional, ignored on some arch).
 */
void arch_enter_user_mode(uintptr_t entry, uintptr_t user_stack_top,
                          uint16_t user_ds, uint16_t user_cs);

#endif // ARACHNYAA_ARCH_USER_H_

#ifndef _ARACHNYAA_STDDEF_H
#define _ARACHNYAA_STDDEF_H

/*
 * stddef.h - Minimal standard definitions for Arachnyaa OS
 */

/* Define NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

/*
 * For our i686 (32-bit) target, size_t is an unsigned 32-bit integer.
 * 'unsigned int' is typically 32 bits on 32-bit targets.
 */
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

/*
 * ptrdiff_t is the signed integer type of the result of subtracting
 * two pointers. It's usually a signed 32-bit int on our target.
 */
#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#endif

#endif // _ARACHNYAA_STDDEF_H

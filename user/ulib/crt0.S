/*
 * crt0.S — process entry point for user-space utilities.
 *
 * At entry esp points at the PAB written by elfloader:
 *   [esp+0]  argc
 *   [esp+4]  argv[0] ... argv[argc-1]  NULL
 *            envp[0] ... envp[M-1]     NULL
 *            proc_auxv_t entries       {AT_NULL, 0}
 *
 * Scans auxv for AT_CAPS_BASE / AT_CAPS_COUNT, calls fd_init,
 * then calls main(argc, argv, envp), then executes SYS_EXIT.
 */

.section .text
.global _start
_start:
    /* ---- argc, argv, envp ---- */
    movl  (%esp),  %eax          /* argc */
    leal  4(%esp), %ecx          /* argv  = &esp[1]       */
    leal  4(%ecx,%eax,4), %edx   /* envp  = &argv[argc+1] */

    /* ---- scan past envp NULL to reach auxv ---- */
    movl  %edx,   %esi
.scan_envp:
    movl  (%esi), %ecx
    addl  $4,     %esi
    testl %ecx,   %ecx
    jnz   .scan_envp
    /* esi = &auxv[0] */

    /* ---- scan auxv for AT_CAPS_BASE (1) and AT_CAPS_COUNT (2) ---- */
    xorl  %edi, %edi             /* caps_base  = 0 */
    xorl  %ebx, %ebx             /* caps_count = 0 */
.scan_auxv:
    movl  (%esi),  %ecx          /* type */
    testl %ecx,    %ecx
    jz    .auxv_done             /* AT_NULL */
    cmpl  $1,      %ecx          /* AT_CAPS_BASE */
    je    .got_caps_base
    cmpl  $2,      %ecx          /* AT_CAPS_COUNT */
    je    .got_caps_count
    addl  $8,      %esi
    jmp   .scan_auxv
.got_caps_base:
    movl  4(%esi), %edi
    addl  $8,      %esi
    jmp   .scan_auxv
.got_caps_count:
    movl  4(%esi), %ebx
    addl  $8,      %esi
    jmp   .scan_auxv
.auxv_done:

    /* ---- fd_init(caps_base, caps_count) ---- */
    pushl %ebx                   /* caps_count */
    pushl %edi                   /* caps_base  */
    call  fd_init
    addl  $8, %esp
    /* esp is restored to original value (pointing at argc) */

    /* ---- recompute argc/argv/envp (fd_init may clobber ecx/edx) ---- */
    movl  (%esp),  %eax
    leal  4(%esp), %ecx
    leal  4(%ecx,%eax,4), %edx

    /* ---- main(argc, argv, envp) ---- */
    pushl %edx
    pushl %ecx
    pushl %eax
    call  main

    /* ---- sys_exit(return_value) — SYS_EXIT=0, arg1 in ebx ---- */
    movl  %eax, %ebx
    xorl  %eax, %eax
    int   $0x80
1:  jmp   1b

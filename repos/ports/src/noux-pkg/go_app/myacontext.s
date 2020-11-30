/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */
#if defined(__x86_64__)
.globl _setmcontext
_setmcontext:
        movq    16(%rdi), %rsi
        movq    24(%rdi), %rdx
        movq    32(%rdi), %rcx
        movq    40(%rdi), %r8
        movq    48(%rdi), %r9
        movq    56(%rdi), %rax
        movq    64(%rdi), %rbx
        movq    72(%rdi), %rbp
        movq    80(%rdi), %r10
        movq    88(%rdi), %r11
        movq    96(%rdi), %r12
        movq    104(%rdi), %r13
        movq    112(%rdi), %r14
        movq    120(%rdi), %r15
        movq    184(%rdi), %rsp
        pushq   160(%rdi)       /* new %eip */
        movq    8(%rdi), %rdi

        ret

.globl _getmcontext
_getmcontext:
        movq    %rdi, 8(%rdi)
        movq    %rsi, 16(%rdi)
        movq    %rdx, 24(%rdi)
        movq    %rcx, 32(%rdi)
        movq    %r8, 40(%rdi)
        movq    %r9, 48(%rdi)
        movq    $0, 56(%rdi)    /* %rax */
        movq    %rbx, 64(%rdi)
        movq    %r10, 80(%rdi)
        movq    %r11, 88(%rdi)
        movq    %r12, 96(%rdi)
        movq    %r13, 104(%rdi)
        movq    %r14, 112(%rdi)
        movq    %r15, 120(%rdi)
// we need to take rbp rsp rip from caller, not our owm
// FIXME: this works ONLY with frame pointer in rbp (-fno-omit-stackpointer)
// ! probably will not work in case of async signals
// ! (structure below could be broken if someone interrupt current thread
// ! and use its stack)
// current (rsp) point to return address to immediate caller, not to getcontext caller
// ! if we come from setcontext - anything after call to _getmcontext
// ! (inside getcontext) will NOT be executed
// current structure of stack content:
// (rsp):       return adress to caller
// -8(rsp):     rpb (saved frame pointer) of caller
// (-8(rsp)):   rpb (saved frame pointer) of getcontext caller
//      !need to save as rbp into mcontext
// 8(-8(rsp)):  return address to getcontext caller
//      !need to save as rip into mcontext
// &16(-8(rsp)):rsp of getcontext caller after return from getcontext
//      !need to save as rip into mcontext
// during getcontext we allow standart way of regs restoration;
// in case of setcontext it will jump diretly to getcontext caller
        movq    -8(%rsp), %rax  /* saved %rbp of getcontext; shoud be non 0! */
        movq    (%rax), %rcx
        movq    %rcx, 72(%rdi)  /* new saved rpb of caller, save it */

        movq    8(%rax), %rcx
        movq    %rcx, 160(%rdi) /* new rip of caller, save it */

        leaq    16(%rax), %rcx  /* %rsp should point to prev stack cell */
        movq    %rcx, 184(%rdi) /* in this case we override current ret adress */

        movq    32(%rdi), %rcx  /* restore temp reg */
        movq    56(%rdi), %rax  /* restore return value */
// we dont need to restore other regs (like rpb/r15) in setcontext case
// - its already taken from mcontext
// they should NOT be modified in the getcontext head before _getmcontext call
        ret     /* to getcontext to restore regs; setcontext do not appears here */
#endif

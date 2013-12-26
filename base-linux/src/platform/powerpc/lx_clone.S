/*
 * \brief  Linux clone() binding
 * \author Josef Soentgen
 * \date   2013-12-27
 *
 * based on glibc-2.11.3/sysdeps/unix/sysv/linux/powerpc/powerpc32/clone.S
 */

#define SYS_clone 120
#define SYS_exit 1
#define SYS_getpid 20

#define CLONE_VM      0x00000100
#define CLONE_THREAD  0x00010000

#define __ASSUME_FIXED_CLONE_SYSCALL 1

	.text
	.globl lx_clone
	.type lx_clone, @function
lx_clone:

	/* Check for child_stack == NULL || fn == NULL.  */
	/* cmpwi   cr0,r4,0 */
	/* cmpwi   cr1,r3,0 */
	/* cror    cr0*4+eq,cr1*4+eq,cr0*4+eq */
	/* beq-    cr0, .badargs */

	/* Set up stack frame for parent.  */
	stwu    r1,-32(r1)
	/* cfi_adjust_cfa_offset (32) */

#ifdef RESET_PID
	stmw    r28,16(r1)
#else
# ifndef __ASSUME_FIXED_CLONE_SYSCALL
	stmw    r29,16(r1)
# else
	stmw    r30,16(r1)
# endif
#endif

	/* Set up stack frame for child.  */
	clrrwi  r4,r4,4
	li      r0,0
	stwu    r0,-16(r4)

	/* Save fn, args, stack across syscall.  */
	mr      r30,r3                  /* Function in r30.  */
#ifndef __ASSUME_FIXED_CLONE_SYSCALL
	mr      r29,r4                  /* Stack pointer in r29.  */
#endif
#ifdef RESET_PID
	mr      r28,r5
#endif
	mr      r31,r6                  /* Argument in r31.  */
	/* 'flags' argument is first parameter to clone syscall. (The other
	   argument is the stack pointer, already in r4.)  */
	mr      r3,r5

	/* Move the parent_tid, child_tid and tls arguments. */
	mr      r5,r7
	mr      r6,r8
	mr      r7,r9

	/* End FDE now, because in the child the unwind info will be
	   wrong.  */
	/* cfi_endproc */

	/* Do the call.  */
	li      r0, SYS_clone
	sc

	/* Check for child process.  */
	cmpwi   cr1,r3,0
	crandc  cr1*4+eq,cr1*4+eq,cr0*4+so
	bne-    cr1, .parent           /* The '-' is to minimise the race.  */

#ifndef __ASSUME_FIXED_CLONE_SYSCALL
	/* On at least mklinux DR3a5, clone() doesn't actually change
	   the stack pointer.  I'm pretty sure this is a bug, because
	   it adds a race condition if a signal is sent to a thread
	   just after it is created (in the previous three instructions).  */
	mr      r1,r29
#endif

#ifdef RESET_PID
	andis.  r0,r28,CLONE_THREAD>>16
	bne+    r0, .oldpid
	andi.   r0,r28,CLONE_VM
	li      r3,-1
	bne-    r0, .nomoregetpid
	li      r0, SYS_getpid
	sc

.nomoregetpid:
	stw     r3,TID(r2)
	stw     r3,PID(r2)

.oldpid:
#endif
	/* Call procedure.  */
	mtctr   r30
	mr      r3,r31
	bctrl
	/* Call _exit with result from procedure.  */
	blr

.parent:
	/* Parent.  Restore registers & return.  */
#ifdef RESET_PID
	lmw     r28,16(r1)
#else
# ifndef __ASSUME_FIXED_CLONE_SYSCALL
	lmw     r29,16(r1)
# else
	lmw     r30,16(r1)
# endif
#endif
	addi    r1,r1,32
	bnslr+
	#b       __syscall_error@local
	blr

/* .badargs: */
/* 	li      r3,EINVAL */
/* 	b       __syscall_error@local */

/* tell the linker that this code does not need an executable stack */
.section .note.GNU-stack, "", %progbits

/*
 * \brief  Linux syscall() binding
 * \author Josef Soentgen
 * \date   2013-12-27
 *
 * based on glibc-2.11.3/sysdeps/unix/sysv/linux/powerpc/syscall.S
 *
 * error case:
 *   glibc's syscall() function returns -1 and sets errno
 *   lx_syscall() returns -errno
 */

	.text
	.globl lx_syscall
	.type lx_syscall, @function
	.align 2
lx_syscall:
	mr   r0,r3
	mr   r3,r4
	mr   r4,r5
	mr   r5,r6
	mr   r6,r7
	mr   r7,r8
	mr   r8,r9
	sc
	bns+ 1f
	neg r3, r3
	add r0, r3, 0
	blr
1:
	bnslr+

/* tell the linker that this code does not need an executable stack */
.section .note.GNU-stack, "", %progbits

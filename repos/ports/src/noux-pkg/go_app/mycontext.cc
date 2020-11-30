/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#include <ucontext.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mcontext.h"

#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/backtrace.h>

using namespace Genode;

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	uintptr_t *sp;
	va_list va;


	sp = (uintptr_t*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/sizeof(long);
	sp -= argc;
	sp = (uintptr_t*)((uintptr_t)sp - (uintptr_t)sp%16);	/* 16-align for OS X */

	sp = (uintptr_t *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size/sizeof(uintptr_t);
	sp = (uintptr_t*)((uintptr_t)sp - (uintptr_t)sp%16); /* 16-align for OS X */

	va_start(va, argc);
	switch (argc) {
	case 0:
		break;
	case 1:
		ucp->uc_mcontext.mc_rdi = va_arg(va, int);
		break;
	case 2:
		ucp->uc_mcontext.mc_rdi = va_arg(va, int);
		ucp->uc_mcontext.mc_rsi = va_arg(va, int);
		break;
	default:
		abort();	// oops
		break;
	}
	va_end(va);

	*--sp = 0;	/* return address */
	ucp->uc_mcontext.mc_rip = (long)func;
	ucp->uc_mcontext.mc_rsp = (long)sp;
}
__attribute__((noinline))
static void ___getcontext(ucontext_t* ucp)
{
	// take info about genode thread
	Thread &myself = *Thread::myself();
	Thread_capability tcap = myself.cap();

	__int64_t lcap = *(__int64_t *)&tcap;
	Genode::memset(&ucp->uc_mcontext, 0, sizeof(ucp->uc_mcontext));
	*(__int64_t *)(&ucp->uc_mcontext.mc_spare[0]) = lcap;

}
int getcontext(ucontext_t * ucp) __attribute__((__returns_twice__));
__attribute__((noinline))
int getcontext(ucontext_t* ucp)
{
	___getcontext(ucp);
	_getmcontext(&ucp->uc_mcontext);
	return 0;
}

__attribute__((noinline))
static int  ___setcontext(const ucontext_t* ucp)
{
	// take info about genode thread
	Thread &myself = *Thread::myself();
	Thread_capability tcap = myself.cap();

	__int64_t lcap = *(__int64_t *)&tcap;
	if (lcap != *(__int64_t *)(&ucp->uc_mcontext.mc_spare[0]))
	{
		return 0;
	} else
	{
		// same thread, just longjmp
		return 1;
	}
}
__attribute__((noinline))
int  setcontext(const ucontext_t* ucp)
{
	if ( ___setcontext(ucp))
	{	// same
		;
	}
	_setmcontext(&ucp->uc_mcontext);
	return 0; // never return
}
int
swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}

/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#ifndef MCONTEXT_H
#define MCONTEXT_H

#include <ucontext.h>
//#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

extern	int	_getmcontext(mcontext_t*);
extern	int _setmcontext(const mcontext_t*);

#ifdef __cplusplus
};
#endif

#endif

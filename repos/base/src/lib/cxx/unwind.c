/*
 * \brief  Wrapper for symbols required by libgcc_eh's exception handling
 * \author Sebastian Sumpf
 * \date   2015-09-04
 *
 * The wrapper always calls a function with prefix '_cxx'. In 'cxx.mk' we prefix
 * the wrapped functions with '_cxx'.  This whole procedure became necessary
 * since the wrapped symbols are marked 'GLOBAL', 'HIDDEN' in libgcc_eh.a and
 * thus ligcc_eh had to be linked to *all* binaries. In shared libaries this
 * became unfeasible since libgcc_eh uses global data which might not be
 * initialized during cross-library interaction.  The clean way to go would be
 * to link libgcc_s.so to DSOs and dynamic binaries, unfortunally ligcc_s
 * requires libc6 in the current Genode tool chain.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Unwind function found in all binaries */
void _cxx__Unwind_Resume(void *exc)  __attribute__((weak));
void _Unwind_Resume(void *exc) {
	_cxx__Unwind_Resume(exc); }

void _cxx__Unwind_DeleteException(void *exc) __attribute__((weak));
void _Unwind_DeleteException(void *exc) {
	_cxx__Unwind_DeleteException(exc); }


/* Unwind functions used by language runtimes. e.g. libgo */

void *_cxx__Unwind_GetTextRelBase(void *ctx) __attribute__((weak));
void *_Unwind_GetTextRelBase(void *ctx) {
	return _cxx__Unwind_GetTextRelBase(ctx); }


int _cxx__Unwind_Resume_or_Rethrow(void *exc) __attribute__((weak));
int _Unwind_Resume_or_Rethrow(void *exc) {
	return _cxx__Unwind_Resume_or_Rethrow(exc); }


int _cxx__Unwind_RaiseException(void *exc) __attribute__((weak));
int _Unwind_RaiseException(void *exc) {
	return _cxx__Unwind_RaiseException(exc); }


void *_cxx__Unwind_GetLanguageSpecificData(void *ctx) __attribute__((weak));
void *_Unwind_GetLanguageSpecificData(void *ctx) {
	return _cxx__Unwind_GetLanguageSpecificData(ctx); }


typedef int (*trace_fn)(void *, void *);
int _cxx__Unwind_Backtrace(trace_fn fn, void *) __attribute__((weak));
int _Unwind_Backtrace(trace_fn fn, void *arg) {
	return _cxx__Unwind_Backtrace(fn, arg); }


void *_cxx__Unwind_GetIPInfo(void *ctx, int *info) __attribute__((weak));
void *_Unwind_GetIPInfo(void *ctx, int *info) {
	return _cxx__Unwind_GetIPInfo(ctx, info); }


void _cxx__Unwind_SetGR(void *ctx, int index, unsigned val) __attribute__((weak));
void _Unwind_SetGR(void *ctx, int index, unsigned val) {
	return _cxx__Unwind_SetGR(ctx, index, val); }


void _cxx__Unwind_SetIP(void *ctx, void *ptr) __attribute__((weak));
void _Unwind_SetIP(void *ctx, void *ptr) {
	return _cxx__Unwind_SetIP(ctx, ptr); }


void *_cxx__Unwind_GetRegionStart(void *ctx) __attribute__((weak));
void *_Unwind_GetRegionStart(void *ctx) {
	return _cxx__Unwind_GetRegionStart(ctx); }


void *_cxx__Unwind_GetDataRelBase(void *ctx) __attribute__((weak));
void *_Unwind_GetDataRelBase(void *ctx) {
	return _cxx__Unwind_GetDataRelBase(ctx); }


/* Special ARM-EABI personality functions */
#ifdef __ARM_EABI__

int _cxx___aeabi_unwind_cpp_pr0(int state, void *block, void *context) __attribute__((weak));
int __aeabi_unwind_cpp_pr0(int state, void *block, void *context) {
	return _cxx___aeabi_unwind_cpp_pr0(state, block, context); }

int  _cxx___aeabi_unwind_cpp_pr1(int state, void *block, void *context) __attribute__((weak));
int __aeabi_unwind_cpp_pr1(int state, void *block, void *context) {
	return _cxx___aeabi_unwind_cpp_pr1(state, block, context); }

/* Unwind functions found in some binaries */
void  _cxx__Unwind_Complete(void *exc)  __attribute__((weak));
void _Unwind_Complete(void *exc) {
	_cxx__Unwind_Complete(exc); }

#endif /* __ARM_EABI__ */

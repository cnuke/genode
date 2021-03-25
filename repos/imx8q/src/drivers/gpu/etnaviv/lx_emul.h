/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2021-03-16
 */

#include <stdarg.h>

/* Needed to trace and stop */
#include <lx_emul/debug.h>

/* Needed to print stuff */
#include <lx_emul/printf.h>

/* fix for wait_for_completion_timeout where the __sched include is missing */
#include <linux/sched/debug.h>

/* fix for missing include in linux/dynamic_debug.h */
#include <linux/compiler_attributes.h>

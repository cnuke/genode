#include <lx_emul.h>

#include <asm-generic/sections.h>

char __start_rodata[] = {};
char __end_rodata[]   = {};

#include <asm/preempt.h>

int __preempt_count = 0;

#include <linux/prandom.h>

unsigned long net_rand_noise;


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags pageflag_names[] = { {0,NULL}};


#include <linux/kernel_stat.h>

struct kernel_stat kstat;

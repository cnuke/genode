/**
 * \brief  Shadow copy of linux/uaccess.h
 * \author Josef Soentgen
 * \date   2021-04-19
 */

#pragma once

#include_next <asm/uaccess.h>

#undef put_user
#undef get_user

#define get_user(x, ptr) ({  (x)   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  (x);   0; })

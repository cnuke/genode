/**
 * \brief  Shadow copy of asm/uaccess.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#pragma once

#include_next <asm/uaccess.h>

#undef put_user
#undef get_user

#define get_user(x, ptr) ({  (x)   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  (x);   0; })

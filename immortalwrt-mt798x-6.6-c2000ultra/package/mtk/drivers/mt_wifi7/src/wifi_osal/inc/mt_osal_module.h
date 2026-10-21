/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#ifndef __MT_OSAL_MODULE_H_
#define __MT_OSAL_MODULE_H_

#include <linux/typecheck.h>

#define MT_MAX_DEVICE_NUM 2

#define MT_TIME_AFTER(a, b)		\
	(typecheck(unsigned long, (unsigned long)a) && \
	 typecheck(unsigned long, (unsigned long)b) && \
	 ((long)(b) - (long)(a) < 0))

#define MT_TIME_AFTER_EQ(a, b)	\
	(typecheck(unsigned long, (unsigned long)a) && \
	 typecheck(unsigned long, (unsigned long)b) && \
	 ((long)(a) - (long)(b) >= 0))
#define MT_TIME_BEFORE(a, b)	MT_TIME_AFTER_EQ(b, a)

#ifndef __MOCK
#define DECLARE_SUB_MODULE_INIT(__sub_module_init_func__)	\
	static int __init __sub_module_init_func__(void)
#else
#define DECLARE_SUB_MODULE_INIT(__sub_module_init_func__)	\
	int __init __sub_module_init_func__(void)
#endif /* __MOCK */

#ifndef __MOCK
#define DECLARE_SUB_MODULE_EXIT(__sub_module_exit_func__)	\
	static void __exit __sub_module_exit_func__(void)
#else
#define DECLARE_SUB_MODULE_EXIT(__sub_module_exit_func__)	\
	void __exit __sub_module_exit_func__(void)
#endif /* __MOCK */

typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

extern initcall_t __sub_mod_initcall_start[], __sub_mod_initcall_end[];
extern exitcall_t __sub_mod_exitcall_start[], __sub_mod_exitcall_end[];

#define sub_module_init(__sub_module_init_func__) \
	static initcall_t __initcall1 __used __section(".sub_mod_initcall.init") = \
		__sub_module_init_func__

#define sub_module_exit(__sub_module_exit_func__) \
	static exitcall_t __exitcall3 __used __section(".sub_mod_exitcall.exit") = \
		__sub_module_exit_func__

#endif /* __MT_OSAL_MODULE_H_ */

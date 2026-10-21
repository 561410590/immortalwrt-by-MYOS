/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>

#define SUBMODULE "mt_osal_main"
#include <mt_osal.h>
#include <mt_osal_module.h>

static int __init mt_wifi_osal_init(void)
{
	int ret = 0;
	initcall_t *sub_mod_initcall;

	MT_DEBUG_TRACE_IN("");

	for (sub_mod_initcall = __sub_mod_initcall_start;
	     sub_mod_initcall < __sub_mod_initcall_end;
	     sub_mod_initcall++) {
		ret = (*sub_mod_initcall)();
		if (ret)
			return ret;
	}

	MT_DEBUG_TRACE_OUT("");
	return 0;
}

static void __exit mt_wifi_osal_exit(void)
{
	exitcall_t *sub_mod_exitcall;

	MT_DEBUG_TRACE_IN("");

	for (sub_mod_exitcall = __sub_mod_exitcall_end - 1;
	     sub_mod_exitcall >= __sub_mod_exitcall_start;
	     sub_mod_exitcall--)
		(*sub_mod_exitcall)();

	MT_DEBUG_TRACE_OUT("");
}


MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Mediatek Wifi OS Abstraction Layer");

module_init(mt_wifi_osal_init);
module_exit(mt_wifi_osal_exit);

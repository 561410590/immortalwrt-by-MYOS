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

#define SUBMODULE "mt_osal_ipc"
#include <mt_osal_debug.h>
#include <mt_osal_ipc.h>
#include <mt_osal_module.h>


DECLARE_SUB_MODULE_INIT(mt_osal_ipc_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_ipc_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_ipc_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_ipc_exit);

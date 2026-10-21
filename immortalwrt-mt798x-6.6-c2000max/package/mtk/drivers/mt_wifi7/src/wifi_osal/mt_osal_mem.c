/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#define SUBMODULE "mt_osal_mem"
#include <mt_osal_debug.h>
#include <mt_osal_mem.h>
#include <mt_osal_module.h>

DECLARE_SUB_MODULE_INIT(mt_osal_mem_init)
{
	int status = 0;
	MT_DEBUG_TRACE("loaded");

	return status;
}
sub_module_init(mt_osal_mem_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_mem_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_mem_exit);

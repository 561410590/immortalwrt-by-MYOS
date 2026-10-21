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

#define SUBMODULE "mt_debug"
#include <mt_osal_debug.h>
#include <mt_osal_module.h>


int mt_debug_print(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_DEFAULT, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_print);

int mt_debug_err(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_ERR, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_err);


int mt_debug_warn(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_WARNING, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_warn);


int mt_debug_notice(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_NOTICE, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_notice);

int mt_debug_info(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_INFO, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_info);

int mt_debug_dbg(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
#ifdef CUSTOM_FACILITY
	ret = vprintk_emit(CUSTOM_FACILITY, LOGLEVEL_DEBUG, NULL, 0, fmt, args);
#else
	ret = vprintk(fmt, args);
#endif
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mt_debug_dbg);

DECLARE_SUB_MODULE_INIT(mt_osal_debug_init)
{
	int status = 0;

	MT_DEBUG_TRACE("loaded");

	return status;
}
sub_module_init(mt_osal_debug_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_debug_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_debug_exit);

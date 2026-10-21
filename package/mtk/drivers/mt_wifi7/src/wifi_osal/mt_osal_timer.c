/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
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

#define SUBMODULE "mt_osal_timer"
#include <mt_osal_debug.h>
#include <mt_osal_timer.h>
#include <mt_osal_module.h>

/* Unify all delay routine by using udelay */
void mt_os_usec_delay(unsigned long usec)
{
#ifndef __MOCK
	unsigned long i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
#endif /* !__MOCK */
}

/* timeout -- ms */
void mt_os_set_periodic_timer(
	OS_TIMER *pTimer,
	unsigned long timeout)
{
	if (timer_pending(pTimer))
		return;

	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}
EXPORT_SYMBOL(mt_os_set_periodic_timer);

void mt_os_init_timer(
	void *pReserved,
	OS_TIMER *pTimer,
	TIMER_FUNCTION function,
	void *data)
{
	if (!timer_pending(pTimer)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
		timer_setup(pTimer, function, 0);
#else
		init_timer(pTimer);
		pTimer->data = (unsigned long)data;
		pTimer->function = function;
#endif
	}
}
EXPORT_SYMBOL(mt_os_init_timer);

void mt_os_add_timer(
	OS_TIMER *pTimer,
	unsigned long timeout)
{
	if (timer_pending(pTimer))
		return;

	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}
EXPORT_SYMBOL(mt_os_add_timer);

void mt_os_mod_timer(
	OS_TIMER *pTimer,
	unsigned long timeout)
{
	timeout = ((timeout * OS_HZ) / 1000);
	mod_timer(pTimer, jiffies + timeout);
}
EXPORT_SYMBOL(mt_os_mod_timer);

void mt_os_del_timer(
	OS_TIMER *pTimer,
	unsigned char *pCancelled)
{
	if (timer_pending(pTimer))
		*pCancelled = del_timer_sync(pTimer);
	else
		*pCancelled = true;
}
EXPORT_SYMBOL(mt_os_del_timer);

ktime_t mt_ktime_get(void)
{
	return ktime_get();
}
EXPORT_SYMBOL(mt_ktime_get); /* to-do: remove after logan change merged.@2024/12/25 */

u64 mt_ktime_get_boottime_ns(void)
{
#if (KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE)
	return ktime_get_boottime_ns();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mt_ktime_get_boottime_ns);

unsigned long mt_msecs_to_jiffies(unsigned int m)
{
	return msecs_to_jiffies(m);
}
EXPORT_SYMBOL(mt_msecs_to_jiffies);

mt_os_time_t mt_os_get_time(void)
{
	mt_os_time_t ret;

	MT_DEBUG_TRACE_IN("");
	ret = ktime_get_real();
	MT_DEBUG_TRACE_OUT("");

	return ret;
}
EXPORT_SYMBOL(mt_os_get_time);

u64 mt_os_get_boottime_ns(void)
{
	u64 ret;

	MT_DEBUG_TRACE_IN("");
#if (KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE)
	ret = ktime_get_boottime_ns();
#else
	ret = 0;
#endif
	MT_DEBUG_TRACE_OUT("");

	return ret;
}
EXPORT_SYMBOL(mt_os_get_boottime_ns);

void mt_os_get_time64(mt_os_time64_t *ts)
{
	MT_DEBUG_TRACE_IN("");
	MT_ASSERT(ts == NULL, , "ts should not be NULL");

	ktime_get_real_ts64(ts);

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_get_time64);

u64 mt_os_get_boottime64_ns(void)
{
	mt_os_time64_t ts;
	u64 ns;

        MT_DEBUG_TRACE_IN("");

	getboottime64(&ts);
	ns = timespec64_to_ns(&ts);

        MT_DEBUG_TRACE_OUT("");

	return (u64)ns;
}
EXPORT_SYMBOL(mt_os_get_boottime64_ns);

struct rtc_time mt_rtc_ktime_to_tm(ktime_t ktime)
{
	return rtc_ktime_to_tm(ktime);
}
EXPORT_SYMBOL(mt_rtc_ktime_to_tm); /* to-do: remove after logan change merged.@2024/12/13 */

void mt_get_time_info(
	struct mt_time_info *tm_info)
{
	struct timespec64 txc;
	struct rtc_time tm;

	if (!tm_info) {
		MT_DEBUG_TRACE("tm_info is null.");
		return;
	}
	ktime_get_real_ts64(&txc);
	tm = rtc_ktime_to_tm(txc.tv_sec);
	tm_info->year = tm.tm_year + 1900;
	tm_info->month = tm.tm_mon + 1;
	tm_info->day = tm.tm_hour;
	tm_info->min = tm.tm_min;
	tm_info->sec = tm.tm_sec;
}
EXPORT_SYMBOL(mt_get_time_info); /* TODO:Remove after diag move to wifi_osal. @20250218 */

void mt_get_boottime_ns(u64 *ns_val)
{
	if (!ns_val)
		return;
#if (KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE)
	*ns_val = ktime_get_boottime_ns();
#else
	*ns_val = 0;
#endif
}
EXPORT_SYMBOL(mt_get_boottime_ns);

void mt_get_time(s64 *tm_val)
{
	struct timespec64 txc;

	if (!tm_val)
		return;
	ktime_get_real_ts64(&txc);
	*tm_val = timespec64_to_ktime(txc);
}
EXPORT_SYMBOL(mt_get_time);

void mt_get_time_diff(s64 tm_val_start, s64 tm_val_end, s64 *tm_diff)
{
	if (!tm_diff)
		return;
	*tm_diff = ktime_to_ms(ktime_sub(tm_val_end, tm_val_start));
}
EXPORT_SYMBOL(mt_get_time_diff);

DECLARE_SUB_MODULE_INIT(mt_osal_timer_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_timer_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_timer_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_timer_exit);

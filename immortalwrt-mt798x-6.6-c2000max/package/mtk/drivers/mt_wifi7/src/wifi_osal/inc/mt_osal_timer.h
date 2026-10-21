/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_TIMER_H_
#define __MT_OSAL_TIMER_H_
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/rtc.h>

#define OS_HZ			HZ

typedef struct timer_list	OS_TIMER;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
typedef void (*TIMER_FUNCTION)(OS_TIMER *);
#else
typedef void (*TIMER_FUNCTION)(unsigned long);
#endif

struct mt_time_info {
	int year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
};

void mt_os_usec_delay(unsigned long usec);

void mt_os_set_periodic_timer(
	OS_TIMER *pTimer,
	unsigned long timeout);
void mt_os_init_timer(
	void *pReserved,
	OS_TIMER *pTimer,
	TIMER_FUNCTION function,
	void *data);
void mt_os_add_timer(
	OS_TIMER *pTimer,
	unsigned long timeout);
void mt_os_mod_timer(
	OS_TIMER *pTimer,
	unsigned long timeout);
void mt_os_del_timer(
	OS_TIMER *pTimer,
	unsigned char *pCancelled);

/* to-do: remove after logan change merged.@2024/12/25 */
ktime_t mt_ktime_get(void);

unsigned long mt_msecs_to_jiffies(unsigned int m);

typedef struct timespec64 mt_os_time64_t;
typedef ktime_t mt_os_time_t;

#define mt_os_msecs_to_jiffies(_msec) msecs_to_jiffies(_msec)

#define mt_os_msec_delay(_msec) mdelay(_msec)


mt_os_time_t mt_os_get_time(void);

u64 mt_os_get_boottime_ns(void);

#define mt_os_time_sub(_lhs, _rhs) ktime_sub(_lhs, _rhs)

#define mt_os_time_add(_lhs, _rhs) ktime_add(_lhs, _rhs)

#define mt_os_time_to_us(_ts) ktime_to_us(_ts)

#define mt_os_time_to_ms(_ts) ktime_to_ms(_ts)


void mt_os_get_time64(mt_os_time64_t *ts);

u64 mt_os_get_boottime64_ns(void);

#define ms_os_time64_to_ms(_ts64) (timespec64_to_ns(_ts64) / NSEC_PER_MSEC)

#define ms_os_time64_to_us(_ts64) (timespec64_to_ns(_ts64) / NSEC_PER_USEC)



#define mt_os_time_to_time64(_ts) ktime_to_timespec64(_ts)

#define ms_os_time64_to_time(_ts64) timespec64_to_ktime(_ts64)


/* to-do: remove after logan change merged.@2024/12/13 */
struct rtc_time mt_rtc_ktime_to_tm(ktime_t ktime);

void mt_get_time_info(
	struct mt_time_info *tm_info);
void mt_get_boottime_ns(u64 *ns_val);
void mt_get_time(s64 *tm_val);
void mt_get_time_diff(s64 tm_val_start, s64 tm_val_end, s64 *tm_diff);

#endif /* __MT_OSAL_TIMER_H_ */

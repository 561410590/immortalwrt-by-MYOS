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
#include <linux/err.h>
#include <linux/kthread.h>

#define SUBMODULE "mt_osal_task"
#include <mt_osal_debug.h>
#include <mt_osal_task.h>
#include <mt_osal_module.h>

typedef int (*cast_fn)(void *);

struct mt_dev_handle_info {
	void *ph_dev;
	void (*mt_work_handle)(void *ph_dev);
};

struct os_task_handle_info {
	struct workqueue_struct *dev_wq;
	struct delayed_work dev_work;
	struct mt_dev_handle_info mt_handle;
} g_os_task_handle[MT_MAX_DEVICE_NUM];

static void mt_device_workq_handle_dev0(
	struct work_struct *work)
{
	static u8 device_idx = 0;
/*
	struct os_task_handle_info *handle = container_of(
			work, struct os_task_handle_info, dev_work.work);
*/
	struct mt_dev_handle_info *mt_handle = &g_os_task_handle[device_idx].mt_handle;

	if (mt_handle->mt_work_handle)
		mt_handle->mt_work_handle(mt_handle->ph_dev);
}

static void mt_device_workq_handle_dev1(
	struct work_struct *work)
{
	static u8 device_idx = 1;
/*
	struct os_task_handle_info *handle = container_of(
			work, struct os_task_handle_info, dev_work.work);
*/
	struct mt_dev_handle_info *mt_handle = &g_os_task_handle[device_idx].mt_handle;

	if (mt_handle->mt_work_handle)
		mt_handle->mt_work_handle(mt_handle->ph_dev);
}

struct workqueue_struct *mt_alloc_workqueue(const char *fmt, unsigned int flags, int max_active)
{
	return alloc_workqueue(fmt, flags, max_active);
}
EXPORT_SYMBOL(mt_alloc_workqueue);

void mt_destroy_workqueue(struct workqueue_struct *wq)
{
	return destroy_workqueue(wq);
}
EXPORT_SYMBOL(mt_destroy_workqueue);

void mt_device_init_work(
	void *ph_dev,
	u8 device_idx,
	MT_DEV_HANDLE_FN func)
{
	struct workqueue_struct *dev_wq;
	struct os_task_handle_info *handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("not support(device_id=%d).", device_idx);
		return;
	}
	handle = &g_os_task_handle[device_idx];
	switch (device_idx) {
	default:
	case 0:
		dev_wq = create_workqueue("ph_dev_wq_0");
		if (dev_wq == NULL) {
			MT_DEBUG_TRACE("No memory.");
			handle->dev_wq = NULL;
			return;
		}
		INIT_DELAYED_WORK(&handle->dev_work, mt_device_workq_handle_dev0);
		break;
	case 1:
		dev_wq = create_workqueue("ph_dev_wq_1");
		if (dev_wq == NULL) {
			MT_DEBUG_TRACE("No memory.");
			handle->dev_wq = NULL;
			return;
		}
		INIT_DELAYED_WORK(&handle->dev_work, mt_device_workq_handle_dev1);
		break;
	}
	handle->dev_wq = dev_wq;
	handle->mt_handle.ph_dev = ph_dev;
	handle->mt_handle.mt_work_handle = func;
}
EXPORT_SYMBOL(mt_device_init_work);

void mt_device_clean_work(u8 device_idx)
{
	struct os_task_handle_info *handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("not support(device_id=%d).", device_idx);
		return;
	}
	handle = &g_os_task_handle[device_idx];
	if (handle->dev_wq) {
		flush_workqueue(handle->dev_wq);
		destroy_workqueue(handle->dev_wq);
		handle->dev_wq = NULL;
	}
}
EXPORT_SYMBOL(mt_device_clean_work);

void mt_add_delayed_work(
	u8 device_idx,
	unsigned int delay)
{
	unsigned long timeout = msecs_to_jiffies(delay);
	struct os_task_handle_info *handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("not support(device_id=%d).", device_idx);
		return;
	}
	handle = &g_os_task_handle[device_idx];
	if (handle->dev_wq)
		queue_delayed_work(handle->dev_wq, &handle->dev_work, timeout);
}
EXPORT_SYMBOL(mt_add_delayed_work);

bool mt_os_task_kill(MT_OS_TASK *pTask)
{
	bool ret = false;

	if (pTask->kthread_task) {
		if (kthread_stop(pTask->kthread_task) == 0) {
			pTask->kthread_task = NULL;
			ret = true;
		} else {
			MT_DEBUG_TRACE("kthread_task %s stop failed\n",
				pTask->taskName);
		}
	} else
		MT_DEBUG_TRACE("null kthread_task %s\n",
				pTask->taskName);

	return ret;
}
EXPORT_SYMBOL(mt_os_task_kill);

bool mt_os_task_attach(
	MT_OS_TASK *pTask,
	MT_OS_TASK_CALLBACK __fn,
	unsigned long __arg)
{
	cast_fn fn = (cast_fn)(void *)__fn;
	void *arg = (void *)__arg;
	bool status = true;
	unsigned char retry = 0;

	if (pTask->kthread_task) {
		MT_DEBUG_TRACE("non-null kthread_task %s\n", pTask->taskName);
		status = false;
		goto done;
	}

RETRY:
	pTask->kthread_task = kthread_run(fn, (void *)arg, pTask->taskName);

	if (IS_ERR(pTask->kthread_task)) {
		MT_DEBUG_TRACE("kthread_run %s err %ld\n",
			pTask->taskName, PTR_ERR(pTask->kthread_task));

		/* just an interrupted system call */
		if ((PTR_ERR(pTask->kthread_task) == EINTR) && (retry < 5)) {
			retry++;
			mdelay(1);
			goto RETRY;
		}

		pTask->kthread_task = NULL;
		status = false;
		goto done;
	}

done:
	MT_DEBUG_TRACE("%s %s end %d\n", __func__, pTask->taskName, status);
	return status;
}
EXPORT_SYMBOL(mt_os_task_attach);

bool mt_os_task_init(
	MT_OS_TASK *pTask, char *pTaskName, void *pPriv)
{
	int len;

	if (!pTask)
		return false;
	len = strlen(pTaskName);
	len = len > (MT_OS_TASK_NAME_LEN - 1) ? (MT_OS_TASK_NAME_LEN - 1) : len;
	memmove(&pTask->taskName[0], pTaskName, len);
	pTask->priv = pPriv;
	init_waitqueue_head(&(pTask->kthread_q));
	return true;
}
EXPORT_SYMBOL(mt_os_task_init);

bool mt_os_task_wait(
	void *pReserved, MT_OS_TASK *pTask, int *pStatus)
{
	int status;

	do {
		wait_event_interruptible(
			pTask->kthread_q, pTask->kthread_running || kthread_should_stop());
		pTask->kthread_running = false;
		if (kthread_should_stop()) {
			status = -1;
			break;
		} else
			status = 0;
	} while (0);

	if (pStatus)
		*pStatus = status;
	if (status != 0)
		return false;
	return true;
}
EXPORT_SYMBOL(mt_os_task_wait);

bool mt_os_task_wait_cond(
	void *pReserved, MT_OS_TASK *pTask, unsigned int u4Cond, int *pStatus)
{
	int status;

	do {
		wait_event_interruptible(
			pTask->kthread_q,
			pTask->kthread_running || kthread_should_stop() || (u4Cond));
		pTask->kthread_running = false;
		if (kthread_should_stop()) {
			status = -1;
			break;
		} else
			status = 0;
	} while (0);

	if (pStatus)
		*pStatus = status;
	if (status != 0)
		return false;
	return true;
}
EXPORT_SYMBOL(mt_os_task_wait_cond);

bool mt_os_task_wait_event(
	void *pReserved, MT_OS_TASK *pTask, unsigned int u4Cond, int *pStatus)
{
	do {
		wait_event_interruptible(
			pTask->kthread_q,
			pTask->kthread_running || (u4Cond));
		*pStatus = 0;
	} while (0);

	return true;
}
EXPORT_SYMBOL(mt_os_task_wait_event);

void mt_os_task_wake_up(MT_OS_TASK *pTask, unsigned int u4Cond)
{
	if (u4Cond) {
		pTask->kthread_running = true;
		wake_up_interruptible(&pTask->kthread_q);
	}
}
EXPORT_SYMBOL(mt_os_task_wake_up);

DECLARE_SUB_MODULE_INIT(mt_osal_task_init)
{
	int idx;

	for (idx = 0; idx < MT_MAX_DEVICE_NUM; idx++) {
		g_os_task_handle[idx].dev_wq = NULL;
		g_os_task_handle[idx].mt_handle.ph_dev = NULL;
	}
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_task_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_task_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_task_exit);

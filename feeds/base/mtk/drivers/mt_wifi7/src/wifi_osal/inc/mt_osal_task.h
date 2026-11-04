/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_TASK_H_
#define __MT_OSAL_TASK_H_


typedef int (*MT_OS_TASK_CALLBACK)(unsigned long);

typedef enum _MT_TASK_STATUS_ {
	MT_TASK_STAT_UNKNOWN = 0,
	MT_TASK_STAT_INITED = 1,
	MT_TASK_STAT_RUNNING = 2,
	MT_TASK_STAT_STOPED = 4,
} MT_TASK_STATUS;

#define MT_OS_TASK_NAME_LEN	16

typedef struct _MT_OS_TASK_ {
	char taskName[MT_OS_TASK_NAME_LEN];
	MT_TASK_STATUS taskStatus;
	struct task_struct *kthread_task;
	wait_queue_head_t kthread_q;
	bool kthread_running;
	void *priv;
	unsigned char task_killed;
} MT_OS_TASK;

struct workqueue_struct *mt_alloc_workqueue(const char *fmt, unsigned int flags, int max_active);

void mt_destroy_workqueue(struct workqueue_struct *wq);

typedef void (*MT_DEV_HANDLE_FN)(void *ph_dev);
void mt_device_init_work(
	void *ph_dev,
	u8 device_id,
	MT_DEV_HANDLE_FN func);
void mt_device_clean_work(u8 device_idx);
void mt_add_delayed_work(
	u8 device_idx,
	unsigned int delay);

bool mt_os_task_kill(MT_OS_TASK *pTask);
bool mt_os_task_attach(
	MT_OS_TASK *pTask,
	MT_OS_TASK_CALLBACK fn,
	unsigned long arg);
bool mt_os_task_init(
	MT_OS_TASK *pTask, char *pTaskName, void *pPriv);
bool mt_os_task_wait(
	void *pReserved, MT_OS_TASK *pTask, int *pStatus);
bool mt_os_task_wait_cond(
	void *pReserved, MT_OS_TASK *pTask, unsigned int u4Cond, int *pStatus);
bool mt_os_task_wait_event(
	void *pReserved, MT_OS_TASK *pTask, unsigned int u4Cond, int *pStatus);
void mt_os_task_wake_up(MT_OS_TASK *pTask, unsigned int u4Cond);

#endif /* __MT_OSAL_TASK_H_ */

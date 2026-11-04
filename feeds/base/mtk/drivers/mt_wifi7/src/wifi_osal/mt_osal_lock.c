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

#define SUBMODULE "mt_osal_lock"
#include <mt_osal_debug.h>
#include <mt_osal_lock.h>
#include <mt_osal_module.h>

void mt_rcu_read_lock(void)
{
	rcu_read_lock();
}
EXPORT_SYMBOL(mt_rcu_read_lock);

void mt_rcu_read_unlock(void)
{
	rcu_read_unlock();
}
EXPORT_SYMBOL(mt_rcu_read_unlock);

void mt_call_rcu(struct rcu_head *head, rcu_callback_t func)
{
	call_rcu(head, func);
}
EXPORT_SYMBOL(mt_call_rcu);

void mt_synchronize_rcu(void)
{
	synchronize_rcu();
}
EXPORT_SYMBOL(mt_synchronize_rcu);

struct mt_os_lockless_ctx {
	char *name;
};

int mt_os_lockless_ctx_init(const char *name, void *_ctx)
{
	int ret = 0;
	struct mt_os_lockless_ctx *ctx = (struct mt_os_lockless_ctx *)_ctx;

	MT_DEBUG_TRACE_IN("name:%s ctx:%p", name, ctx);

	if (!name || !ctx) {
		ret = -EINVAL;
		goto out;
	}

	ctx->name = kstrdup(name, GFP_KERNEL);
	if (ctx->name == NULL)
		ret = -ENOMEM;
out:
	MT_DEBUG_TRACE_OUT("");
	return ret;
}
EXPORT_SYMBOL(mt_os_lockless_ctx_init);

void mt_os_lockless_ctx_cleanup(void *_ctx)
{
	struct mt_os_lockless_ctx *ctx = (struct mt_os_lockless_ctx *)_ctx;

	MT_DEBUG_TRACE_IN("ctx:%p", ctx);

	if (!ctx)
		return;

	kfree(ctx->name);
	ctx->name = NULL;

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_ctx_cleanup);

void mt_os_lockless_enter(void * __maybe_unused ctx)
{
	MT_DEBUG_TRACE_IN("ctx:%p", ctx);

	rcu_read_lock();

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_enter);

void mt_os_lockless_exit(void * __maybe_unused ctx)
{
	MT_DEBUG_TRACE_IN("ctx:%p", ctx);

	rcu_read_unlock();

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_exit);

void mt_os_lockless_barrier_update(void * __maybe_unused ctx,
			void (*update_func)(void *),
			void *arg)
{
	MT_DEBUG_TRACE_IN("ctx:%p update_func:%p arg:%p",
			ctx, update_func, arg);

	synchronize_rcu();
	update_func(arg);

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_barrier_update);

void mt_os_lockless_call(void * __maybe_unused ctx,
			void (*callback)(void *),
			void *arg)
{
	struct mt_os_lockless_data *data = (struct mt_os_lockless_data *)arg;

	MT_DEBUG_TRACE_IN("ctx:%p update_func:%p arg:%p",
		ctx, callback, arg);

	call_rcu(&data->rcu, (rcu_callback_t)callback);

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_call);

void mt_os_lockless_barrier(void * __maybe_unused ctx)
{
	MT_DEBUG_TRACE_IN("ctx:%p", ctx);

	synchronize_rcu();

	MT_DEBUG_TRACE_OUT("");
}
EXPORT_SYMBOL(mt_os_lockless_barrier);

DECLARE_SUB_MODULE_INIT(mt_osal_lock_init)
{
	MT_DEBUG_TRACE_IN("loaded");

	MT_DEBUG_TRACE_OUT("");

	return 0;
}
sub_module_init(mt_osal_lock_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_lock_exit)
{
	MT_DEBUG_TRACE_IN("unloaded");

	MT_DEBUG_TRACE_OUT("");
}
sub_module_exit(mt_osal_lock_exit);

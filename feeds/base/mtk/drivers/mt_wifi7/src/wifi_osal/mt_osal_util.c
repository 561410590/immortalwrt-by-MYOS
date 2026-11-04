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

#define SUBMODULE "mt_osal_util"
#include <mt_osal_debug.h>
#include <mt_osal_util.h>
#include <mt_osal_module.h>

void *mt_idr_find(const struct idr *idr, unsigned long id)
{
	return idr_find(idr, id);
}
EXPORT_SYMBOL(mt_idr_find);

void *mt_idr_remove(struct idr *idr, unsigned long id)
{
	return idr_remove(idr, id);
}
EXPORT_SYMBOL(mt_idr_remove);

int mt_idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp)
{
	return idr_alloc(idr, ptr, start, end, gfp);
}
EXPORT_SYMBOL(mt_idr_alloc);

long mt_os_str_tol(const char *str, signed char **endptr, int32_t base)
{
        long value = 0;
        int ret;

        ret = kstrtol(str, base, &value);
        if (ret == 0)
                return value;
        else
                return 0;
}
EXPORT_SYMBOL(mt_os_str_tol);

unsigned long mt_os_str_toul(const char *str, signed char **endptr, int32_t base)
{
        unsigned long value = 0;
        int ret;

        ret = kstrtoul(str, base, &value);
        if (ret == 0)
                return value;
        else
                return 0;
}
EXPORT_SYMBOL(mt_os_str_toul);

int mt_os_dynamic_idr_init(void *ctx,
				int low,
				int high,
				void* data,
				mt_os_dynamic_idr_data_free_t free_fn)
{
	mt_os_dynamic_idr_t *id_ctx = ctx;

	spin_lock_init(&id_ctx->lock);
	spin_lock_bh(&id_ctx->lock);
	id_ctx->cnt = 0;
	id_ctx->low = low;
	id_ctx->high = high;
	id_ctx->data = data;
	id_ctx->free_fn = free_fn;
	idr_init(&id_ctx->id);
	spin_unlock_bh(&id_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(mt_os_dynamic_idr_init);

void mt_os_dynamic_idr_exit(void* ctx)
{
	mt_os_dynamic_idr_t *id_ctx = (mt_os_dynamic_idr_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	if (id_ctx->free_fn)
		idr_for_each(&id_ctx->id, id_ctx->free_fn, id_ctx->data);
	idr_destroy(&id_ctx->id);
	spin_unlock_bh(&id_ctx->lock);
}
EXPORT_SYMBOL(mt_os_dynamic_idr_exit);

int mt_os_dynamic_idr_alloc(void *ctx,
			uint32_t *id,
			void *data,
			bool allow_suspend)
{
	int ret;
	mt_os_dynamic_idr_t *id_ctx = (mt_os_dynamic_idr_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	ret = idr_alloc(&id_ctx->id, data, id_ctx->low, id_ctx->high+1,
		allow_suspend? GFP_KERNEL: GFP_ATOMIC);
	if (ret < 0) {
		goto err;
	}
	*id = (uint32_t)ret;
	id_ctx->cnt++;
	ret = 0;
err:
	spin_unlock_bh(&id_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(mt_os_dynamic_idr_alloc);

void mt_os_dynamic_idr_free(void *ctx, uint32_t id)
{
	mt_os_dynamic_idr_t *id_ctx = (mt_os_dynamic_idr_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	idr_remove(&id_ctx->id, id);
	id_ctx->cnt--;
	spin_unlock_bh(&id_ctx->lock);
}
EXPORT_SYMBOL(mt_os_dynamic_idr_free);

void *mt_os_dynamic_idr_find(void *ctx, uint32_t id)
{
	mt_os_dynamic_idr_t *id_ctx = (mt_os_dynamic_idr_t *)ctx;
	void *data;


	spin_lock_bh(&id_ctx->lock);
	data = idr_remove(&id_ctx->id, id);
	spin_unlock_bh(&id_ctx->lock);

	return data;
}
EXPORT_SYMBOL(mt_os_dynamic_idr_find);

int mt_os_dynamic_id_init(void *ctx,
				int low,
				int high)
{
	mt_os_dynamic_id_t *id_ctx = (mt_os_dynamic_id_t *)ctx;

	spin_lock_init(&id_ctx->lock);
	spin_lock_bh(&id_ctx->lock);
	id_ctx->cnt = 0;
	id_ctx->low = low;
	id_ctx->high = high;
	ida_init(&id_ctx->id);
	spin_unlock_bh(&id_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(mt_os_dynamic_id_init);

void mt_os_dynamic_id_exit(void* ctx)
{
	mt_os_dynamic_id_t *id_ctx = (mt_os_dynamic_id_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	ida_destroy(&id_ctx->id);
	spin_unlock_bh(&id_ctx->lock);
}
EXPORT_SYMBOL(mt_os_dynamic_id_exit);

int mt_os_dynamic_id_alloc(void *ctx,
			uint32_t *id,
			bool allow_suspend)
{
	int ret;
	mt_os_dynamic_id_t *id_ctx = (mt_os_dynamic_id_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	ret = ida_alloc_range(&id_ctx->id, id_ctx->low, id_ctx->high+1,
		allow_suspend? GFP_KERNEL: GFP_ATOMIC);
	if (ret < 0) {
		goto err;
	}
	*id = (uint32_t)ret;
	id_ctx->cnt++;
	ret = 0;
err:
	spin_unlock_bh(&id_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(mt_os_dynamic_id_alloc);

void mt_os_dynamic_id_free(void *ctx, uint32_t id)
{
	mt_os_dynamic_id_t *id_ctx = (mt_os_dynamic_id_t *)ctx;

	spin_lock_bh(&id_ctx->lock);
	ida_free(&id_ctx->id, id);
	id_ctx->cnt--;
	spin_unlock_bh(&id_ctx->lock);
}
EXPORT_SYMBOL(mt_os_dynamic_id_free);


static struct kmem_cache *mt_os_static_id_cache;

void *mt_os_static_id_alloc(void)
{
	struct xarray *xa;

	if (!mt_os_static_id_cache)
		return NULL;

	xa = kmem_cache_alloc(mt_os_static_id_cache, GFP_ATOMIC);
	if (!xa)
		return NULL;

	xa_init(xa);

	return xa;
}
EXPORT_SYMBOL(mt_os_static_id_alloc);

void mt_os_static_id_free(void *static_id)
{
	struct xarray *xa = (struct xarray *)static_id;

	if (!xa)
		return

	xa_destroy(xa);
	kmem_cache_free(mt_os_static_id_cache, xa);
}
EXPORT_SYMBOL(mt_os_static_id_free);

int mt_os_static_id_store(void *static_id,
				unsigned long id,
				void *data,
				void **old_data,
				bool allow_suspend)
{
	struct xarray *xa = (struct xarray *)static_id;
	void *ret_data;

	if (!xa)
		return -EINVAL;

	ret_data = xa_store(xa, id, data, allow_suspend? GFP_KERNEL: GFP_ATOMIC);
	if (xa_err(ret_data))
		return -EINVAL;
	else
		return 0;
}
EXPORT_SYMBOL(mt_os_static_id_store);

void *mt_os_static_id_erase(void *static_id, unsigned long id)
{
	struct xarray *xa = (struct xarray *)static_id;

	if (!xa)
		return NULL;

	return xa_erase(xa, id);
}
EXPORT_SYMBOL(mt_os_static_id_erase);

void *mt_os_static_id_load(void *static_id, unsigned long id)
{
	struct xarray *xa = (struct xarray *)static_id;

	if (!xa)
		return NULL;

	return xa_load(xa, id);
}
EXPORT_SYMBOL(mt_os_static_id_load);

DECLARE_SUB_MODULE_INIT(mt_osal_util_init)
{
	MT_DEBUG_TRACE("loaded");

	mt_os_static_id_cache = kmem_cache_create("mt_os_static_id",
					sizeof(struct xarray),
					0,
					SLAB_HWCACHE_ALIGN,
					NULL);

	return 0;
}
sub_module_init(mt_osal_util_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_util_exit)
{
	if (mt_os_static_id_cache) {
		kmem_cache_destroy(mt_os_static_id_cache);
		mt_os_static_id_cache = NULL;
	}
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_util_exit);

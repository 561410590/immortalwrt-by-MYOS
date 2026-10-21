/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/kallsyms.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>

#define SUBMODULE "mt_osal_system"
#include <mt_osal_debug.h>
#include <mt_osal_system.h>
#include <mt_osal_module.h>

unsigned long mt_kallsyms_lookup_name(const char *name)
{
#if (KERNEL_VERSION(5, 7, 0) > LINUX_VERSION_CODE)
	return kallsyms_lookup_name(name);
#else
	return 0; /* kernel version > 5.7, don't support this API*/
#endif
}
EXPORT_SYMBOL(mt_kallsyms_lookup_name);


struct kobject *mt_create_kobj(void)
{
	return kobject_create_and_add("mtk_wifi", kernel_kobj);
}
EXPORT_SYMBOL(mt_create_kobj);


typedef struct mt_os_sysinfo_dir {
	struct kobject *kobj;
} mt_os_sysinfo_dir_t;

void *mt_os_sysinfo_dir_create(void *dir_ctx, const char *dir_name, bool suspend)
{
	struct kobject *parent;
	struct kobject *kobj;
	mt_os_sysinfo_dir_t *sysinfo_dir = dir_ctx;

	if (!sysinfo_dir)
		parent = kernel_kobj;
	else
		parent = sysinfo_dir->kobj;

	sysinfo_dir = kzalloc(sizeof(*sysinfo_dir), suspend? GFP_KERNEL: GFP_ATOMIC);
	if (!sysinfo_dir)
		return NULL;

	kobj = kobject_create_and_add(dir_name, parent);
	if (!kobj) {
		kfree(sysinfo_dir);
		return NULL;
	}
	sysinfo_dir->kobj = kobj;
	return sysinfo_dir;
}
EXPORT_SYMBOL(mt_os_sysinfo_dir_create);

void mt_os_sysinfo_dir_destroy(void *dir_ctx)
{
	mt_os_sysinfo_dir_t *sysinfo_dir = dir_ctx;

	if (!sysinfo_dir) {
		return;
	}

	kobject_put(sysinfo_dir->kobj);
	kfree(sysinfo_dir);
}
EXPORT_SYMBOL(mt_os_sysinfo_dir_destroy);


typedef struct mt_os_sysinfo_file {
	mt_os_sysinfo_dir_t *sysinfo_dir;
	struct attribute_group attr_grp;
	struct attribute *attrs[2];
	struct kobj_attribute attr;
	void *priv;
	ssize_t (*read_fn)(void *, char*);
	ssize_t (*write_fn)(void *, const char*, size_t);
} mt_os_sysinfo_file_t;

static ssize_t
mt_os_sysinfo_file_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	mt_os_sysinfo_file_t *sysinfo_file;
	ssize_t ret = -EIO;

	if (!attr)
		return -EIO;

	sysinfo_file = container_of(attr, mt_os_sysinfo_file_t, attr);
	if (sysinfo_file->write_fn)
		ret = sysinfo_file->write_fn(sysinfo_file->priv, buf, count);

	return ret;
}

static ssize_t
mt_os_sysinfo_file_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	mt_os_sysinfo_file_t *sysinfo_file;
	ssize_t ret = -EIO;

	if (!attr)
		return -EIO;

	sysinfo_file = container_of(attr, mt_os_sysinfo_file_t, attr);
	if (sysinfo_file->read_fn)
		ret = sysinfo_file->read_fn(sysinfo_file->priv, buf);

	return ret;
}

void *mt_os_sysinfo_file_create(void *dir_ctx,
			const char *file_name,
			mt_os_sysinfo_mode_t mode,
			void *priv,
			ssize_t (*read_fn)(void *, char*),
			ssize_t (*write_fn)(void *, const char*, size_t),
			bool suspend)
{
        mt_os_sysinfo_file_t *sysinfo_file;
	int ret;

        sysinfo_file = kzalloc(sizeof(*sysinfo_file), suspend? GFP_KERNEL: GFP_ATOMIC);
        if (!sysinfo_file)
		goto err;

	sysinfo_file->attr.attr.name = kstrdup(file_name, suspend? GFP_KERNEL: GFP_ATOMIC);
	if (!sysinfo_file->attr.attr.name)
		goto err2;

	sysinfo_file->sysinfo_dir = dir_ctx;
	sysinfo_file->attr.attr.mode = mode;
        sysinfo_file->attr.show = read_fn? mt_os_sysinfo_file_show: NULL;
        sysinfo_file->attr.store = write_fn? mt_os_sysinfo_file_store: NULL;
	sysinfo_file->priv = priv;
	sysinfo_file->read_fn = read_fn;
	sysinfo_file->write_fn = write_fn;
	sysinfo_file->attrs[0] = &sysinfo_file->attr.attr;
	sysinfo_file->attrs[1] = NULL;
	sysinfo_file->attr_grp.attrs = sysinfo_file->attrs;
	sysfs_attr_init(&sysinfo_file->attr.attr);

	ret = sysfs_create_group(sysinfo_file->sysinfo_dir?
			sysinfo_file->sysinfo_dir->kobj: kernel_kobj,
			&sysinfo_file->attr_grp);
	if (ret) {
		goto err3;
	}

	return sysinfo_file;
err3:
	kfree(sysinfo_file->attr.attr.name);
err2:
	kfree(sysinfo_file);
err:
	return NULL;
}
EXPORT_SYMBOL(mt_os_sysinfo_file_create);

void mt_os_sysinfo_file_destroy(void *file_ctx)
{
	mt_os_sysinfo_file_t *sysinfo_file = file_ctx;

	if (!sysinfo_file)
		return;

	if (sysinfo_file->attr.attr.name)
		kfree(sysinfo_file->attr.attr.name);

	sysfs_remove_group(sysinfo_file->sysinfo_dir?
		sysinfo_file->sysinfo_dir->kobj: kernel_kobj,
		&sysinfo_file->attr_grp);

	kfree(sysinfo_file);
}
EXPORT_SYMBOL(mt_os_sysinfo_file_destroy);

void mt_kobject_put(struct kobject *kobj)
{
	kobject_put(kobj);
}
EXPORT_SYMBOL(mt_kobject_put);

void mt_get_online_cpus(void)
{
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
	get_online_cpus();
#else
	cpus_read_lock();
#endif
}
EXPORT_SYMBOL(mt_get_online_cpus);

void mt_put_online_cpus(void)
{
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
	put_online_cpus();
#else
	cpus_read_unlock();
#endif
}
EXPORT_SYMBOL(mt_put_online_cpus);

u8 is_support_smp_async_call(void)
{
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	return 1;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(is_support_smp_async_call);

void *mt_os_csd_alloc(void)
{
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	call_single_data_t *csd;
	csd = (void *)kzalloc(sizeof(call_single_data_t), GFP_KERNEL);
	if (!csd) {
		MT_DEBUG_TRACE("Allocate csd fail\n");
		return NULL;
	}
	return csd;
#else
	return NULL;
#endif
}
EXPORT_SYMBOL(mt_os_csd_alloc);

void mt_os_csd_free(void *csd)
{
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	kfree(csd);
	return;
#else
	return;
#endif
}
EXPORT_SYMBOL(mt_os_csd_free);


void mt_os_smp_call_function_single(int cpuid, MT_OS_SMP_CALLBACK func, void *info, int wait)
{
	smp_call_function_single(cpuid, (smp_call_func_t)func, info, wait);
	return;
}
EXPORT_SYMBOL(mt_os_smp_call_function_single);

void mt_os_smp_call_function_single_async(int cpuid, void *csd, MT_OS_SMP_CALLBACK func, void *info)
{
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	call_single_data_t *_csd = (call_single_data_t *)csd;
	int cpu_num, cpu_idx;

	if (!_csd) {
		MT_DEBUG_TRACE("csd is null\n");
		return;
	}

	cpuid = smp_processor_id();
	cpu_num = num_online_cpus();
	cpu_idx = (cpuid + 1) % cpu_num;
	_csd->info = info;
	_csd->func = (smp_call_func_t)func;

	smp_call_function_single_async(cpu_idx, _csd);
	return;
#else
	return;
#endif
}
EXPORT_SYMBOL(mt_os_smp_call_function_single_async);

void mt_os_random_get(int type, void *val, size_t len)
{
	if (!val)
		return;

	switch (type) {
	case MT_OS_RANDOM_TYPE_U8:
		if (len != sizeof(u8))
			return;
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		*(u8 *)val = get_random_u8();
#else
		get_random_bytes(val, sizeof(u8));
#endif
		break;
	case MT_OS_RANDOM_TYPE_U16:
		if (len != sizeof(u16))
			return;
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		*(u16 *)val = get_random_u16();
#else
		get_random_bytes(val, sizeof(u16));
#endif
		break;
	case MT_OS_RANDOM_TYPE_U32:
		if (len != sizeof(u32))
			return;
		*(u32 *)val = get_random_u32();
		break;
	case MT_OS_RANDOM_TYPE_U64:
		if (len != sizeof(u64))
			return;
		*(u64 *)val = get_random_u64();
		break;
	case MT_OS_RANDOM_TYPE_BYTES:
		if (len <= 0)
			return;
		get_random_bytes(val, len);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(mt_os_random_get);

DECLARE_SUB_MODULE_INIT(mt_osal_system_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_system_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_system_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_system_exit);

/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_SYSTEM_H_
#define __MT_OSAL_SYSTEM_H_

unsigned long mt_kallsyms_lookup_name(const char *name);

struct kobject *mt_create_kobj(void);

void mt_kobject_put(struct kobject *kobj);

typedef umode_t mt_os_sysinfo_mode_t;

void *mt_os_sysinfo_dir_create(void *dir_ctx, const char *dir_name, bool suspend);

void mt_os_sysinfo_dir_destroy(void *dir_ctx);

void *mt_os_sysinfo_file_create(void *dir_ctx,
			const char *file_name,
			mt_os_sysinfo_mode_t mode,
			void *priv,
			ssize_t (*read_fn)(void *, char*),
			ssize_t (*write_fn)(void *, const char*, size_t),
			bool suspend);

void mt_os_sysinfo_file_destroy(void *file_ctx);

void mt_get_online_cpus(void);

void mt_put_online_cpus(void);


u8 is_support_smp_async_call(void);

typedef void (*MT_OS_SMP_CALLBACK)(void *);
void mt_os_smp_call_function_single_async(int cpuid, void *csd, MT_OS_SMP_CALLBACK func, void *info);

void mt_os_smp_call_function_single(int cpuid, MT_OS_SMP_CALLBACK func, void *info, int wait);

void *mt_os_csd_alloc(void);

void mt_os_csd_free(void *csd);

/*
 * Enumeration for different types of random values.
 */
enum {
	MT_OS_RANDOM_TYPE_U8 = 0,		/* 8-bit unsigned integer */
	MT_OS_RANDOM_TYPE_U16,			/* 16-bit unsigned integer */
	MT_OS_RANDOM_TYPE_U32,			/* 32-bit unsigned integer */
	MT_OS_RANDOM_TYPE_U64,			/* 64-bit unsigned integer */
	MT_OS_RANDOM_TYPE_BYTES,		/* Random bytes */
};

/*
 * mt_os_random_get - Generate a random value of the specified type.
 * @type: Type of random value to generate (one of MT_OS_RANDOM_TYPE_*).
 * @val: Pointer to the buffer where the random value will be stored.
 * @len: Length of the buffer (in bytes).
 *
 */
void mt_os_random_get(int type, void *val, size_t len);

#endif /* __MT_OSAL_SYSTEM_H_ */

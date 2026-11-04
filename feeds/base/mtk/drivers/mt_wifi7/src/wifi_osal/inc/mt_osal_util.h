/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_UTIL_H_
#define __MT_OSAL_UTIL_H_

void *mt_idr_find(const struct idr *idr, unsigned long id);

void *mt_idr_remove(struct idr *idr, unsigned long id);

int mt_idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp);

long mt_os_str_tol(const char *str, signed char **endptr, int32_t base);

unsigned long mt_os_str_toul(const char *str, signed char **endptr, int32_t base);

#include <linux/string.h>

static inline char *mt_os_str_chr(char *str, int character)
{
        return strchr(str, character);
}

static inline char *mt_os_str_pbrk(char *str1, char *str2)
{
        return strpbrk(str1, str2);
}

static inline uint32_t mt_os_str_spn(char *str1, char *str2)
{
        return strspn(str1, str2);
}

static inline char *mt_os_str_str(const char *str1, const char *str2)
{
        return strstr(str1, str2);
}

typedef int (*mt_os_dynamic_idr_data_free_t)(int id, void *p, void *data);

typedef struct mt_os_dynamic_idr {
	spinlock_t lock;
	struct idr id;
	uint32_t cnt;
	int high;
	int low;
	mt_os_dynamic_idr_data_free_t free_fn;
	void *data;
} mt_os_dynamic_idr_t;

int mt_os_dynamic_idr_init_range(void *ctx,
			int low,
			int high,
			void* data,
			mt_os_dynamic_idr_data_free_t free_fn);

void mt_os_dynamic_idr_exit(void* ctx);


int mt_os_dynamic_idr_alloc(void *ctx,
			uint32_t *id,
			void *data,
			bool allow_suspend);

void mt_os_dynamic_idr_free(void *ctx, uint32_t id);

void *mt_os_dynamic_idr_find(void *ctx, uint32_t id);

#define mt_os_dynamic_idr_for_each_entry(_id_ctx, _entry, _id)		\
	idr_for_each_entry(&(_id_ctx)->id, _entry, _id)

#define mt_os_dynamic_idr_for_each_entry_continue(_id_ctx, _entry, _id)	\
	idr_for_each_entry_continue(&(_id_ctx)->id, _entry, _id)



typedef struct mt_os_dynamic_id {
	spinlock_t lock;
	struct ida id;
	uint32_t cnt;
	int high;
	int low;
} mt_os_dynamic_id_t;

int mt_os_dynamic_id_init_range(void *ctx,
			int low,
			int high);

void mt_os_dynamic_id_exit(void* ctx);

int mt_os_dynamic_id_alloc(void *ctx,
			uint32_t *id,
			bool allow_suspend);

void mt_os_dynamic_id_free(void *ctx, uint32_t id);

void *mt_os_static_id_alloc(void);

void mt_os_static_id_free(void *static_id);

int mt_os_static_id_store(void *static_id, unsigned long id, void *data, void **old_data, bool allow_suspend);

void *mt_os_static_id_erase(void *static_id, unsigned long id);

void *mt_os_static_id_load(void *static_id, unsigned long id);

static inline int mt_os_snprintf_error(size_t size, int res)
{
	return res < 0 || (unsigned int) res >= size;
}


#endif /* __MT_OSAL_UTIL_H_ */

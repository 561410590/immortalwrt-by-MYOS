/*
 * Copyright (c) [2020], MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws.
 * The information contained herein is confidential and proprietary to
 * MediaTek Inc. and/or its licensors.
 * Except as otherwise provided in the applicable licensing terms with
 * MediaTek Inc. and/or its licensors, any reproduction, modification, use or
 * disclosure of MediaTek Software, and information contained herein, in whole
 * or in part, shall be strictly prohibited.
*/
/*
 ***************************************************************************
 ***************************************************************************

*/
#include "common/static_id.h"

#define STATIC_ID_HASH(__table, __id)  (__id % __table->size)

struct static_id_entry {
	RTMP_HLIST_NODE node;
	unsigned long id;
	void *data;
	void (*data_free_fn)(void *_data);
};

struct static_id {
	int size;
	OS_NDIS_SPIN_LOCK lock;
	RTMP_HLIST_HEAD *hash;
};

void *static_id_alloc(int size)
{
	struct static_id *id_table = NULL;
	int real_size;
	int ret;
	int i;

	if (size <= 0)
		size = 127;

	real_size = sizeof(struct static_id) + (size * sizeof(RTMP_HLIST_HEAD));

	ret = os_alloc_mem(NULL, (UCHAR **)&id_table, real_size);
	if ((ret != NDIS_STATUS_SUCCESS) || (id_table == NULL))
		return NULL;

	id_table->size = size;
	id_table->hash = (RTMP_HLIST_HEAD *)(id_table + 1);
	for (i = 0; i < size; i++)
		RtmpHashHeadInit(&id_table->hash[i]);

	OS_SPIN_LOCK_INIT(&id_table->lock);

	return id_table;
}

void static_id_free(void *ctx)
{
	struct static_id *id_table = (struct static_id *)ctx;
	struct static_id_entry *entry;
	RTMP_HLIST_NODE *tmp;
	int i;

	if (!id_table)
		return;

	OS_SPIN_LOCK_BH(&id_table->lock);
	for (i = 0; i < id_table->size; i++) {
		RtmpHashListForEachSafe(entry, tmp, &id_table->hash[i], node) {
			if (entry->data_free_fn && entry->data)
				(*entry->data_free_fn)(entry->data);
			os_free_mem(entry);
		}
	}
	OS_SPIN_UNLOCK_BH(&id_table->lock);

	os_free_mem(id_table);
}

struct static_id_entry *
_static_id_load(struct static_id *id_table, unsigned long id)
{
	unsigned long id_hash;
	struct static_id_entry *entry;
	RTMP_HLIST_NODE *tmp;

	id_hash = STATIC_ID_HASH(id_table, id);
	RtmpHashListForEachSafe(entry, tmp, &id_table->hash[id_hash], node) {
		if (entry->id == id)
			return entry;
	}
	return NULL;
}

int static_id_store(void *ctx,
		unsigned long id,
		void *data,
		void (*data_free_fn)(void *))

{
	struct static_id *id_table = (struct static_id *)ctx;
	struct static_id_entry *entry;
	unsigned long id_hash;
	int ret;

	if (!id_table)
		return -RTMP_IO_EINVAL;

	OS_SPIN_LOCK_BH(&id_table->lock);
	entry = _static_id_load(id_table, id);
	if (entry) {
		OS_SPIN_UNLOCK_BH(&id_table->lock);
		return -RTMP_IO_EINVAL;
	}

	id_hash = STATIC_ID_HASH(id_table, id);

	ret = os_alloc_mem(NULL, (UCHAR **)&entry, sizeof(*entry));
	if (ret != 0 || !entry) {
		OS_SPIN_UNLOCK_BH(&id_table->lock);
		return -RTMP_IO_ENOMEM;
	}

	entry->id = id;
	entry->data = data;
	entry->data_free_fn = data_free_fn;

	RtmpHashListAddHead(&entry->node, &id_table->hash[id_hash]);
	OS_SPIN_UNLOCK_BH(&id_table->lock);

	return 0;
}

int static_id_erase(void *ctx, unsigned long id)
{
	struct static_id *id_table = (struct static_id *)ctx;
	struct static_id_entry *entry;

	if (!id_table)
		return -RTMP_IO_EINVAL;

	OS_SPIN_LOCK_BH(&id_table->lock);
	entry = _static_id_load(id_table, id);
	if (!entry) {
		OS_SPIN_UNLOCK_BH(&id_table->lock);
		return -RTMP_IO_EINVAL;
	}

	RtmpHashListDel(&entry->node);
	if (entry->data_free_fn && entry->data)
		(*entry->data_free_fn)(entry->data);
	os_free_mem(entry);
	OS_SPIN_UNLOCK_BH(&id_table->lock);

	return 0;
}

void *static_id_load(void *ctx, unsigned long id)
{
	struct static_id *id_table = (struct static_id *)ctx;
	struct static_id_entry *entry;

	OS_SPIN_LOCK_BH(&id_table->lock);
	entry = _static_id_load(id_table, id);
	OS_SPIN_UNLOCK_BH(&id_table->lock);

	if (entry)
		return entry->data;
	else
		return NULL;
}

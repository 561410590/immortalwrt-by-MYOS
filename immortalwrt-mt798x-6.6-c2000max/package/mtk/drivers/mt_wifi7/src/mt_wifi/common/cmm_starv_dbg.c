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

	Module Name:
	cmm_starv_dbg.c
*/


#include "rt_config.h"

/*test only*/
#ifdef DBG_STARVATION
static inline void block_entry_free(struct starv_log_entry *entry)
{
	struct starv_dbg_block *block = entry->block;
	struct starv_log *ctrl = block->ctrl;

	DlListDel(&entry->list);
	if (entry->log)
		os_free_mem(entry->log);
	os_zero_mem(entry, sizeof(struct starv_log_entry));
	DlListAdd(&ctrl->free, &entry->list);
}

static inline void block_init(struct starv_dbg_block *block)
{
	struct starv_log_entry *entry, *tmp;

	DlListForEachSafe(entry, tmp, &block->log_head, struct starv_log_entry, list) {
		block_entry_free(entry);
	}

	block->avg_dur = 0;
	block->max_dur = 0;
	block->min_dur = 1;
	block->count = 0;
	block->timeout_cnt = 0;
}

static struct starv_log_entry *starv_log_insert(struct starv_dbg *starv)
{
	struct starv_dbg_block *block = starv->block;
	struct starv_log *ctrl = block->ctrl;
	struct starv_log_entry *entry = NULL;

	NdisAcquireSpinLock(&ctrl->lock);
	entry = DlListFirst(&ctrl->free, struct starv_log_entry, list);
	if (!entry)
		goto end;

	DlListDel(&entry->list);
	entry->duration = starv->msec;
	entry->block = starv->block;
	DlListAddTail(&block->log_head, &entry->list);
end:
	NdisReleaseSpinLock(&ctrl->lock);
	return entry;
}

static void strav_timeout_handle(struct starv_dbg *starv)
{
	struct starv_log_entry *entry;
	struct starv_dbg_block *block = starv->block;

	entry = starv_log_insert(starv);

	if (block->timeout_fn && entry)
		block->timeout_fn(starv, entry);

	block->timeout_cnt++;
}

void starv_dbg_init(struct starv_dbg_block *block, struct starv_dbg *starv)
{
	/*initial value*/
	starv->msec = 0;
	starv->start_jiffies = 0;
	starv->end_jiffies = 0;
	starv->block = block;
}

void starv_dbg_get(struct starv_dbg *starv)
{
	struct starv_dbg_block *block = starv->block;

	starv->start_jiffies = jiffies;

	if (block->entry_fn)
		block->entry_fn(starv);
}

void starv_dbg_put(struct starv_dbg *starv)
{
	struct starv_dbg_block *block = starv->block;

	starv->end_jiffies = jiffies;
	starv->msec = jiffies_to_msecs(abs(starv->end_jiffies - starv->start_jiffies));
	/*update block*/
	block->max_dur = (starv->msec > block->max_dur) ? starv->msec : block->max_dur;
	block->min_dur = (starv->msec < block->min_dur) ? starv->msec : block->min_dur;
	block->avg_dur = (starv->msec + block->avg_dur * block->count) / (block->count + 1);
	block->count++;
	/*check timeout*/
	if (starv->msec > block->timeout)
		strav_timeout_handle(starv);

	if (block->leave_fn)
		block->leave_fn(starv);
}

static inline starv_log_dump_block(struct starv_dbg_block *block)
{
	MTWF_PRINT("[block %s]\t min: %d, max: %d, avg: %d, timeout cnt: %d, total cnt: %d\n",
		block->name,
		block->min_dur,
		block->max_dur,
		block->avg_dur,
		block->timeout_cnt,
		block->count);
	MTWF_PRINT("===========================================================================\n");

	DlListForEachSafe(entry, tmp, &block->log_head, struct starv_log_entry, list) {
		if (block->log_fn)
			block->log_fn(entry);
	}
	MTWF_PRINT("===========================================================================\n");
}

void starv_log_dump(struct starv_log *ctrl)
{
	struct starv_log_entry *entry, *tmp;
	struct starv_dbg_block *block, *btmp;

	MTWF_PRINT("===========================================================================\n");

	NdisAcquireSpinLock(&ctrl->lock);
	DlListForEachSafe(block, btmp, &ctrl->block_head, struct starv_dbg_block, list) {
		starv_log_dump_block(block);
		block_init(block);
	}

	NdisReleaseSpinLock(&ctrl->lock);
}

void starv_timeout_log_basic(struct starv_log_entry *entry)
{
	struct starv_log_basic *log = entry->log;

	MTWF_PRINT("[%s] id: %d, qsize: %d, duration: %d ms\n",
				entry->block->name, log->id, log->qsize, entry->duration);
}

int register_starv_block(struct starv_dbg_block *block)
{
	struct starv_log *ctrl = block->ctrl;

	NdisAcquireSpinLock(&ctrl->lock);
	DlListInit(&block->log_head);
	block_init(block);
	DlListAddTail(&ctrl->block_head, &block->list);
	NdisReleaseSpinLock(&ctrl->lock);
	return 0;
}

void unregister_starv_block(struct starv_dbg_block *block)
{
	struct starv_log *ctrl = block->ctrl;

	NdisAcquireSpinLock(&ctrl->lock);
	DlListDel(&block->list);
	block_init(block);
	NdisReleaseSpinLock(&ctrl->lock);
}

int starv_log_init(struct starv_log *ctrl)
{
	int i;
	struct starv_log_entry *entry = NULL;

	os_zero_mem(ctrl, sizeof(struct starv_log));
	NdisAllocateSpinLock(NULL, &ctrl->lock);
	DlListInit(&ctrl->free);
	DlListInit(&ctrl->block_head);
	/*add to free pool*/
	for (i = 0; i < STARV_ENTRY_SIZE; i++) {
		entry = &ctrl->pool[i];
		DlListAddTail(&ctrl->free, &entry->list);
	}
	return 0;
}

void starv_log_exit(struct starv_log *ctrl)
{
	starv_log_dump(ctrl);
	DlListInit(&ctrl->free);
	DlListInit(&ctrl->block_head);
	NdisFreeSpinLock(&ctrl->lock);
	os_zero_mem(&ctrl, sizeof(struct starv_log));
}

#endif /*DBG_STARVATION*/

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
	cmm_starv_dbg.h
*/


#ifndef __CMM_STARV_DBG_H__
#define __CMM_STARV_DBG_H__

#include "rt_config.h"

#ifdef DBG_STARVATION
#define STARV_ENTRY_SIZE 2000

struct starv_log_entry;
struct starv_dbg;
struct starv_dbg_block;

struct starv_dbg {
	unsigned int msec;
	unsigned long start_jiffies;
	unsigned long end_jiffies;
	struct starv_dbg_block *block;
};

struct starv_dbg_block {
	char name[32];
	unsigned int timeout;
	struct starv_log *ctrl;
	void (*timeout_fn)(struct starv_dbg *starv, struct starv_log_entry *entry);
	void (*leave_fn)(struct starv_dbg *starv);
	void (*entry_fn)(struct starv_dbg *starv);
	void (*log_fn)(struct starv_log_entry *entry);
	void *priv;
	/*statistic*/
	unsigned int avg_dur;
	unsigned int max_dur;
	unsigned int min_dur;
	unsigned int timeout_cnt;
	unsigned int count;
	DL_LIST log_head;
	DL_LIST list;
};

struct starv_log_basic {
	unsigned int id;
	unsigned int qsize;
};

struct starv_log_entry {
	void *log;
	unsigned int duration;
	struct starv_dbg_block *block;
	DL_LIST list;
};

struct starv_log {
	struct starv_log_entry pool[STARV_ENTRY_SIZE];
	DL_LIST free;
	DL_LIST block_head;
	NDIS_SPIN_LOCK lock;
};

VOID starv_dbg_init(struct starv_dbg_block *block, struct starv_dbg *starv);
VOID starv_dbg_get(struct starv_dbg *starv);
VOID starv_dbg_put(struct starv_dbg *starv);
VOID starv_log_dump(struct starv_log *ctrl);
INT starv_log_init(struct starv_log *ctrl);
VOID starv_log_exit(struct starv_log *ctrl);
INT register_starv_block(struct starv_dbg_block *bk);
VOID unregister_starv_block(struct starv_dbg_block *bk);
VOID starv_timeout_log_basic(struct starv_log_entry *entry);

#endif /*DBG_STARVATION*/
#endif /* __CMM_STARV_DBG_H__ */

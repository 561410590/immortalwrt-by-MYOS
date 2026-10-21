/*
 * Copyright (c) [2021], MediaTek Inc. All rights reserved.
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

#ifndef __AP_MLO_MGR_H__
#define __AP_MLO_MGR_H__

#include "bss_mngr.h"

/* global management for MLD index in driver */
struct mld_index_mgmt {
	u8 idx_map_table[BMGR_MAX_MLD_GRP_CNT];

	/* protect mapping table and free_idx_bitmap */
	struct mutex idx_mgmt_mutex;
};

int mld_idx_mapping_init(void);
int mld_idx_mapping_deinit(void);
void mld_idx_mapping_dump(void);
u8 mld_idx_mapping_acquire(u8 input_idx);
int mld_idx_mapping_release(u8 mapped_idx);

#endif /* __AP_MLO_MGR_H__ */


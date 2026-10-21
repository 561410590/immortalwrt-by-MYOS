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

#include "rt_config.h"
#include "mlo_mgr.h"

#ifdef DOT11_EHT_BE
struct mld_index_mgmt _mld_idx_mgmt;

struct mld_index_mgmt *mld_idx_mgmt_get(void)
{
	return &_mld_idx_mgmt;
}

static bool mld_idx_mapping_assigned(u8 mapped_idx)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();

	/* mapping table */
	return (idx_mgmt->idx_map_table[mapped_idx] != BMGR_INVALID_MLD_GRP);
}

static int mld_idx_mapping_set(u8 input_idx, u8 mapped_idx)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();

	/* sanity */
	if (!(input_idx < BMGR_MAX_MLD_GRP_CNT) || !BMGR_VALID_MLD_GRP_IDX(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: input %d or mapped %d idx is invalid\n",
			input_idx, mapped_idx);
		return -EINVAL;
	}

	/* mapping table */
	if (mld_idx_mapping_assigned(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: Idx %d already used by %d\n",
			mapped_idx, idx_mgmt->idx_map_table[mapped_idx]);
		return -EBUSY;
	}

	idx_mgmt->idx_map_table[mapped_idx] = input_idx;

	MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_NOTICE,
		"Set mapping %d to %d\n", input_idx, mapped_idx);

	return 0;
}

static int mld_idx_mapping_clear(u8 mapped_idx)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();
	u8 input_idx = BMGR_INVALID_MLD_GRP;

	/* sanity */
	if (!BMGR_VALID_MLD_GRP_IDX(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: mapped %d idx is invalid\n",
			mapped_idx);
		return -EINVAL;
	}

	/* mapping table */
	if (mld_idx_mapping_assigned(mapped_idx))
		input_idx = idx_mgmt->idx_map_table[mapped_idx];

	if (input_idx != BMGR_INVALID_MLD_GRP) {
		idx_mgmt->idx_map_table[mapped_idx] = BMGR_INVALID_MLD_GRP;
	} else {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: Idx %d is not used\n", mapped_idx);
		return -EINVAL;
	}

	MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_NOTICE,
		"Clear mapping %d to %d\n", input_idx, mapped_idx);

	return 0;
}

u8 mld_idx_mapping_acquire(u8 input_idx)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();
	u8 mapped_idx;
	bool need_remap = FALSE;

	mutex_lock(&idx_mgmt->idx_mgmt_mutex);

	/* 0 means acquire idx for single-link-type mld */
	if (input_idx == 0) {
		need_remap = TRUE;
	} else {
		if (!BMGR_VALID_MLD_GRP_IDX(input_idx)) {
			MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
				"Error: input %d idx is invalid\n", input_idx);
			goto err;
		}

		/* check if input_idx is mapped previously, return the same mapped_idx */
		for (mapped_idx = 0; mapped_idx < BMGR_MAX_MLD_GRP_CNT; mapped_idx++) {
			if (idx_mgmt->idx_map_table[mapped_idx] == input_idx)
				break;
		}

		if (mapped_idx != BMGR_MAX_MLD_GRP_CNT) {
			MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_NOTICE,
				"Notice: Idx %d is mapped to %d\n", input_idx, mapped_idx);
			mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
			return mapped_idx;
		}

		if (mld_idx_mapping_assigned(input_idx))
			need_remap = TRUE;
		else {
			if (mld_idx_mapping_set(input_idx, input_idx)) {
				MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
					"Error: acquire input_idx %d fail\n", input_idx);
				goto err;
			}

			mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
			return input_idx;
		}
	}

	if (need_remap) {
		/* valid mld group starts from 1 */
		for (mapped_idx = BMGR_MLD_GRP_START_IDX;
				mapped_idx < BMGR_MAX_MLD_GRP_CNT; mapped_idx++) {
			if (!mld_idx_mapping_assigned(mapped_idx))
				break;
		}

		/* no available index */
		if (!BMGR_VALID_MLD_GRP_IDX(mapped_idx)) {
			MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
				"Error: acquire idx for %d fail, no available index\n", input_idx);
			goto err;
		}

		/* occupy index */
		if (mld_idx_mapping_set(input_idx, mapped_idx)) {
			MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
				"Error: map input_idx %d to mapped_idx %d fail\n",
				input_idx, mapped_idx);
			goto err;
		}

		mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
		return mapped_idx;
	}

err:
	mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
	return BMGR_INVALID_MLD_GRP;
}

int mld_idx_mapping_release(u8 mapped_idx)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();
	int ret;

	mutex_lock(&idx_mgmt->idx_mgmt_mutex);

	if (!BMGR_VALID_MLD_GRP_IDX(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: mapped %d idx is invalid\n", mapped_idx);
		ret = -EINVAL;
		goto err;
	}

	if (!mld_idx_mapping_assigned(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: mapped %d idx is not mapped\n", mapped_idx);
		ret = -EINVAL;
		goto err;
	}

	if (mld_idx_mapping_clear(mapped_idx)) {
		MTWF_DBG(NULL, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_ERROR,
			"Error: clear mapped_idx %d fail\n", mapped_idx);
		ret = -EPERM;
		goto err;
	}

	/* mapping table */

	/* resource management */
	ret = 0;
err:
	mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
	return ret;
}

int mld_idx_mapping_init(void)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();

	/* mapping table */
	os_fill_mem(idx_mgmt->idx_map_table, BMGR_MAX_MLD_GRP_CNT, BMGR_INVALID_MLD_GRP);

	mutex_init(&idx_mgmt->idx_mgmt_mutex);

	return 0;
}

int mld_idx_mapping_deinit(void)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();

	mutex_lock(&idx_mgmt->idx_mgmt_mutex);
	/* mapping table */
	os_fill_mem(idx_mgmt->idx_map_table, BMGR_MAX_MLD_GRP_CNT, BMGR_INVALID_MLD_GRP);
	mutex_unlock(&idx_mgmt->idx_mgmt_mutex);

	mutex_init(&idx_mgmt->idx_mgmt_mutex);

	return 0;
}

void mld_idx_mapping_dump(void)
{
	struct mld_index_mgmt *idx_mgmt = mld_idx_mgmt_get();
	u8 mapped_idx;
	u16 log_cnt = 0;

	mutex_lock(&idx_mgmt->idx_mgmt_mutex);

	MTWF_PRINT("%s: MLO Device, (%d)\n", __func__, BMGR_MAX_MLD_GRP_CNT);

	/* mapping table */
	MTWF_PRINT("Mapping Table (Input Index -> Mapped Index):\n");
	for (mapped_idx = 0; mapped_idx < BMGR_MAX_MLD_GRP_CNT; mapped_idx++) {
		if (mld_idx_mapping_assigned(mapped_idx)) {
			MTWF_PRINT("(%d->%d)", idx_mgmt->idx_map_table[mapped_idx], mapped_idx);
			log_cnt++;

			MTWF_PRINT("%s", (log_cnt % 8) ? ", " : "\n");
		}
	}
	MTWF_PRINT("\n\n");

	mutex_unlock(&idx_mgmt->idx_mgmt_mutex);
}

#endif /* DOT11_EHT_BE */

// SPDX-License-Identifier: ISC
/*
 * Copyright (C) 2024 MediaTek Inc.
 */
#include "hwifi_muldf.h"


struct muldf_sub_category {
	char *name;
	u8 dump;
};

struct muldf_category {
	char *cat_name;
	struct muldf_sub_category **sub_cat;
};

static struct muldf_category module_hwifi[] = {
	{	MULDF_CAT_FW_CMD_TX,
		NULL
	},
	{	MULDF_CAT_FW_EVENT_RX,
		(struct muldf_sub_category *[]) {
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_ALL, MTK_DUMP_NONE},
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_WA, MTK_DUMP_NONE},
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_WM, MTK_DUMP_NONE},
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_TX_FREE, MTK_DUMP_NONE},
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_SDO, MTK_DUMP_NONE},
			&(struct muldf_sub_category) {MULDF_SCAT_FW_EVENT_TXS, MTK_DUMP_NONE},
			NULL
		}
	},
	{	MULDF_CAT_MTWF_HWIFI,
		NULL
	},
	{	NULL,
		NULL
	}
};

void muldf_init(void)
{
	int i = 0, j = 0;

	/*add module*/
	muldf_add_module(MULDF_MODULE_NAME, MTK_MSG_INFO);

	/*add category/sub-category*/
	for (i = 0; module_hwifi[i].cat_name != NULL; i++) {
		muldf_add_cat(MULDF_MODULE_NAME, module_hwifi[i].cat_name);
		if (module_hwifi[i].sub_cat != NULL) {
			for (j = 0; module_hwifi[i].sub_cat[j] != NULL; j++) {
				muldf_add_sub_cat(MULDF_MODULE_NAME,
						     module_hwifi[i].cat_name,
						     module_hwifi[i].sub_cat[j]->name,
						     module_hwifi[i].sub_cat[j]->dump);
			}
		}
	}
}


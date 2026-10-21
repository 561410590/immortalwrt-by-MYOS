#ifdef MTK_LICENSE
/****************************************************************************
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
 ***************************************************************************/
#endif /* MTK_LICENSE */
/****************************************************************************

	Abstract:

	All related CFG80211 Station Vendor Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

/*******************************************************************************
 *                           D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           Functions
 *******************************************************************************
 */

#if defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT)
int cfg80211_drv_vndr_set_mac_addr(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->wdev_type != WDEV_TYPE_STA)
		return -RTMP_IO_EFAULT;
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_STA_MAC) {
		char *data;
		UCHAR ifIndex = wdev->func_idx;

		if (ctrl_data->data_len < MAC_ADDR_LEN)
			return -RTMP_IO_EINVAL;
		if (wifi_sys_close(wdev) != TRUE) {
			MTWF_DBG(mac_ad, DBG_CAT_HW, CATHW_MAC, DBG_LVL_ERROR,
				"PE: wifi_sys_close fail!!!\n");
			return -RTMP_IO_EFAULT;
		}
		if (ifIndex >= MAX_MULTI_STA) {
			MTWF_DBG(mac_ad, DBG_CAT_HW, CATHW_MAC, DBG_LVL_ERROR,
				"apcli interface out of range!!!\n");
			return -RTMP_IO_EFAULT;
		}
		data = (char *)ctrl_data->data;
		NdisMoveMemory(mac_ad->ApcliAddr[ifIndex], data, MAC_ADDR_LEN);

		MTWF_DBG_NP(DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
				"%s: "MACSTR".\n", __func__, MAC2STR(data));

		if (wifi_sys_open(wdev) != TRUE) {
			MTWF_DBG(mac_ad, DBG_CAT_HW, CATHW_MAC, DBG_LVL_ERROR,
				"PE: wifi_sys_open fail!!!\n");
			return -RTMP_IO_EFAULT;
		}
	}
	return 0;
}

int cfg80211_drv_vndr_set_seq_num(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->wdev_type != WDEV_TYPE_STA)
		return -RTMP_IO_EFAULT;
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_SEQ_NUM) {
		NdisMoveMemory(&wdev->SecConfig.wpa_supp_seq_num, &ctrl_data->u8_val, sizeof(u8));
		MTWF_DBG(mac_ad, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"wpa_supp_seq_num[%d]\n", wdev->SecConfig.wpa_supp_seq_num);
	}
	return 0;
}
#endif

#endif /* RT_CFG80211_SUPPORT */


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
/****************************************************************************
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All CFG80211 Function Prototype.

***************************************************************************/


#ifndef __CFG80211_VNDR_H__
#define __CFG80211_VNDR_H__

#ifdef RT_CFG80211_SUPPORT

#define MTK_MAX_VENDOR_MSG_LEN 4095

#ifdef CONFIG_AP_SUPPORT

int cfg80211_drv_vndr_set_ap_wireless(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_set_ap_rfeature(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_set_bss(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_set_radio(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_set_cosr_bcn_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_get_bss_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
#endif /* CONFIG_AP_SUPPORT */

#if defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT)
/* mac address policy, used by wpa_supplicant */

int cfg80211_drv_vndr_set_mac_addr(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_set_seq_num(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
#endif /* defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT) */

#ifdef FTM_SUPPORT
int cfg80211_drv_vndr_set_ftm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int cfg80211_drv_vndr_get_ftm_stat(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
VOID mtk_cfg80211_dump_ftm_parm(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev);
VOID mtk_cfg80211_set_ftm_FTMR_timeout(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UINT32 FTMR_timeout);
VOID mtk_cfg80211_set_non_asap_delay_time(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UINT16 non_asap_delay_time);
VOID mtk_cfg80211_set_ftm_test_mode(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT enable);
VOID mtk_cfg80211_set_ftm_enable(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT ftm_enable);
VOID mtk_cfg80211_set_ftm_role(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT role);
VOID mtk_cfg80211_set_ftm_burst_exp(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT burst_exp);
VOID mtk_cfg80211_set_ftm_burst_dur(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT burst_dur);
VOID mtk_cfg80211_update_ftm_min_delta(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev);
VOID mtk_cfg80211_set_ftm_min_delta(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT min_delta);
VOID mtk_cfg80211_set_ftm_ptsf(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT ptsf);
VOID mtk_cfg80211_set_ftm_ptsf_no_perfer(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT ptsf_no_perfer);
VOID mtk_cfg80211_set_ftm_asap(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT asap);
VOID mtk_cfg80211_set_ftm_num(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT ftms_per_burst);
INT mtk_cfg80211_set_fmt_and_bw(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT fmt_and_bw);
VOID mtk_cfg80211_set_ftm_burst_period(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT burst_period);
VOID mtk_cfg80211_set_ftm_target_mac(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UINT8 *macAddr);
VOID mtk_cfg80211_set_ftm_debug(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UCHAR *macAddr);

VOID mtk_cfg80211_set_ftm_toae_cfg(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UCHAR *toae_cfg);
VOID mtk_cfg80211_range_req_mc_after_scan(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev);
VOID mtk_cfg80211_range_req_mc(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, INT enable, UINT8 peer_num);

int mtk_cfg80211_vndr_get_ftm_stat_iSTA(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev, RTMP_STRING *msg, int msg_buf_len, int value);
#endif /* FTM_SUPPORT */

int cfg80211_drv_vndr_set_config(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);

#ifdef CONFIG_MAP_SUPPORT
int cfg80211_drv_vndr_easymesh(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_);
#endif /* CONFIG_MAP_SUPPORT */

#ifdef DOT11_EHT_BE
int mtk_cfg80211_reply_ap_mld(
	struct bmgr_mlo_dev *mld,
	struct mt_cfg80211_vndr_ctrl *ctrl_data);
#endif /* DOT11_EHT_BE */

#endif /* RT_CFG80211_SUPPORT */

#endif /* __CFG80211_VNDR_H__ */



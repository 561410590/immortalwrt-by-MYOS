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
 *	Abstract:
 *
 *	All related CFG80211 function body.
 *
 *	History:
 *		1. 2009/09/17	Sample Lin
 *			(1) Init version.
 *		2. 2009/10/27	Sample Lin
 *			(1) Do not use ieee80211_register_hw() to create virtual interface.
 *				Use wiphy_register() to register nl80211 command handlers.
 *			(2) Support iw utility.
 *		3. 2009/11/03	Sample Lin
 *			(1) Change name MAC80211 to CFG80211.
 *			(2) Modify CFG80211_OpsChannelSet().
 *			(3) Move CFG80211_Register()/CFG80211_UnRegister() to open/close.
 *		4. 2009/12/16	Sample Lin
 *			(1) Patch for Linux 2.6.32.
 *			(2) Add more supported functions in CFG80211_Ops.
 *		5. 2010/12/10	Sample Lin
 *			(1) Modify for OS_ABL.
 *		6. 2011/04/19	Sample Lin
 *			(1) Add more supported functions in CFG80211_Ops v33 ~ 38.
 *
 *	Note:
 *		The feature is supported only in "LINUX" 2.6.28 ~ 2.6.38.
 *
 ***************************************************************************/


#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"
#include "rt_config.h"
#include "rt_linux.h"
#include "bss_mngr.h"
#include "cfg80211/cfg80211.h"
#include <net/netlink.h>
#ifdef RT_CFG80211_SUPPORT
#include "mtk_vendor_nl80211.h"
#endif
#include "ap_cfg.h"
#ifdef VOW_SUPPORT
#include "ap_vow.h"
#endif
#include "hdev/hdev_basic.h"
#include "ap_ch_prio.h"

#if (KERNEL_VERSION(2, 6, 28) <= LINUX_VERSION_CODE)
#ifdef RT_CFG80211_SUPPORT

/* 36 ~ 64, 100 ~ 136, 140 ~ 161 */
#ifdef DOT11_EHT_BE
extern struct bss_manager bss_mngr;
#endif

/* get RALINK pAd control block in 80211 Ops */
#define MAC80211_PAD_GET(__pAd, __pWiphy) \
	__pAd = (struct _RTMP_ADAPTER *)mt_cfg80211_get_mac_ad(__pWiphy); \
	if (__pAd == NULL) { \
		MTWF_DBG(__pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, \
				 "80211> pAd = NULL!");	\
		return -EINVAL; \
	}

#define MAC80211_PAD_GET_NO_RV(__pAd, __pWiphy) \
	__pAd = (struct _RTMP_ADAPTER *)mt_cfg80211_get_mac_ad(__pWiphy); \
	if (__pAd == NULL) { \
		MTWF_DBG(__pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, \
				 "80211> pAd = NULL!");	\
		return; \
	}

#define MAC80211_PAD_GET_RETURN_NULL(__pAd, __pWiphy) \
	__pAd = (struct _RTMP_ADAPTER *)mt_cfg80211_get_mac_ad(__pWiphy); \
	if (__pAd == NULL) { \
		MTWF_DBG(__pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, \
				 "80211> pAd = NULL!");	\
		return NULL; \
	}

POS_COOKIE cfg80211_vndr_cmd_cookie_prepare(struct _RTMP_ADAPTER *pAd, ULONG priv_flags,
	struct net_device *net_dev)
{
	POS_COOKIE pObj;
	int index;

	pObj = (POS_COOKIE) pAd->OS_Cookie;
	pObj->pSecConfig = NULL;

	/* determine this ioctl command is coming from which interface. */
	if (priv_flags == INT_MAIN) {
		pObj->ioctl_if_type = INT_MAIN;
		pObj->ioctl_if = MAIN_MBSSID;
		pObj->pSecConfig = &pAd->ApCfg.MBSSID[pObj->ioctl_if].wdev.SecConfig;
	} else if (priv_flags == INT_MBSSID) {
		pObj->ioctl_if_type = INT_MBSSID;

		for (index = 1; index < pAd->ApCfg.BssidNum; index++) {
			if (pAd->ApCfg.MBSSID[index].wdev.if_dev == net_dev) {
				pObj->ioctl_if = index;
				pObj->pSecConfig = &pAd->ApCfg.MBSSID[index].wdev.SecConfig;
				break;
			}
		}

		/* Interface not found! */
		if (index == pAd->ApCfg.BssidNum)
			return NULL;

		MBSS_MR_APIDX_SANITY_CHECK(pAd, pObj->ioctl_if);
	}

#ifdef WDS_SUPPORT
	else if (priv_flags == INT_WDS) {
		pObj->ioctl_if_type = INT_WDS;

		for (index = 0; index < MAX_WDS_ENTRY; index++) {
			if (pAd->WdsTab.WdsEntry[index].wdev.if_dev == net_dev) {
				pObj->ioctl_if = index;
				pObj->pSecConfig = &pAd->WdsTab.WdsEntry[index].wdev.SecConfig;
				break;
			}

			if (index == MAX_WDS_ENTRY) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
					"can not find wds I/F\n");
				return NULL;
			}
		}
	}

#endif /* WDS_SUPPORT */
#ifdef APCLI_SUPPORT
	else if (priv_flags == INT_APCLI) {
		pObj->ioctl_if_type = INT_APCLI;

		for (index = 0; index < MAX_APCLI_NUM; index++) {
			if (pAd->StaCfg[index].wdev.if_dev == net_dev) {
				pObj->ioctl_if = index;
				pObj->pSecConfig = &pAd->StaCfg[index].wdev.SecConfig;
				break;
			}

			if (index == MAX_APCLI_NUM) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
					"can not find Apcli I/F\n");
				return NULL;
			}
		}

		APCLI_MR_APIDX_SANITY_CHECK(pObj->ioctl_if);
	}

#endif /* APCLI_SUPPORT */
	else {
		return NULL;
	}

	return pObj;
}

#if defined(SIOCGIWSCAN) || defined(RT_CFG80211_SUPPORT)
extern int rt_ioctl_siwscan(struct net_device *dev,
							struct iw_request_info *info,
							union iwreq_data *wreq, char *extra);
#endif /* LINUX_VERSION_CODE: 2.6.30 */

int CFG80211_OpsMcastRateSet(
	IN struct wiphy *wiphy,
	IN struct net_device *dev,
	IN int rate[NUM_NL80211_BANDS])
{
	struct _RTMP_ADAPTER *pAd;
	INT apidx, ret;
	UCHAR band, PhyMode, Mcs;
	union _HTTRANSMIT_SETTING transmit = {0};

	MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_NOTICE, "80211> ==>\n");

	MAC80211_PAD_GET(pAd, wiphy);

	if (!dev) {
		MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"net_dev is NULL\n");
		return -EFAULT;
	}

	apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, dev);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"[ERROR]can't find wdev in MBSS.\n");
		return -EFAULT;
	}

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (rate[band] > 0)//find mcast rate
			break;
	}

	switch (band) {
	case NL80211_BAND_2GHZ:
		if (rate[band] <= 4) {
			PhyMode = MODE_CCK;
			Mcs = rate[band] - 1;//Mcs0/1/2/3 -> CCK_1M/2M/5.5M/11M
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED)) {
				if (Mcs == 0)//Short Preamble Not support CCK_1M
					Mcs = Mcs + 1;
				Mcs |= 0x4;
			}
		} else {
			PhyMode = MODE_OFDM;
			Mcs = (rate[band] - 4) - 1;//Mcs0~7 -> OFDM_6M~54M
		}

		break;

	case NL80211_BAND_5GHZ:
	case NL80211_BAND_6GHZ:
		PhyMode = MODE_OFDM;
		Mcs = rate[band] - 1;
		break;

	default:
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"Error Mcast Rate input.\n");
		return -EINVAL;
	}

	transmit.field.MODE = PhyMode;
	transmit.field.MCS = Mcs;
	transmit.field.BW = BW_20;

	ret = RTMP_DRIVER_80211_MCAST_RATE_SET(pAd, (VOID *)&transmit, apidx);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR, "Not Support\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

int CFG80211_OpsColorChange(
	IN struct wiphy *wiphy,
	IN struct net_device *dev,
	IN struct cfg80211_color_change_settings *params)
{
	struct _RTMP_ADAPTER *pAd;
	INT apidx, ret;

	MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_NOTICE, "80211> ==>\n");

	MAC80211_PAD_GET(pAd, wiphy);

	if (!dev) {
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_ERROR,
			"net_dev is NULL\n");
		return -EFAULT;
	}

	apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, dev);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_ERROR,
			"[ERROR]can't find wdev in MBSS.\n");
		return -EFAULT;
	}

	ret = RTMP_DRIVER_80211_BSS_COLOR_CHANGE(pAd, (VOID *)params, apidx);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_ERROR, "Not Support\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

int CFG80211_OpsFtIesUpate(
	IN struct wiphy *wiphy,
	IN struct net_device *dev,
	IN struct cfg80211_update_ft_ies_params *ftie)
{
	struct _RTMP_ADAPTER *pAd;
	INT apidx, ret;

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "80211> ==>\n");

	MAC80211_PAD_GET(pAd, wiphy);

	if (!dev) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"net_dev is NULL\n");
		return -EFAULT;
	}

	apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, dev);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"[ERROR]can't find wdev in MBSS.\n");
		return -EFAULT;
	}

	/*
	* struct cfg80211_update_ft_ies_params {
	*	u16 md;			[MDID]
	*	const u8 *ie;	[RSN IE + MDIE + FTIE(with MIC field set to 0)
	*					+ RIC-Request (if present) + RSNXE (if present)]
	*	size_t ie_len;	[LEN]
	* };
	*/

	if (!ftie || !ftie->ie)
		return -EINVAL;

	ret = RTMP_DRIVER_80211_FT_IES_UPDATE(pAd, (VOID *)ftie, apidx);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "Not Support\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

enum nl80211_band CFG80211_Get_BandId(struct _RTMP_ADAPTER *pAd)
{
	struct wifi_dev *main_bss_wdev = NULL;

	main_bss_wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;

	if (WMODE_CAP_6G(main_bss_wdev->PhyMode))
		return NL80211_BAND_6GHZ;
	else if (WMODE_CAP_2G(main_bss_wdev->PhyMode))
		return NL80211_BAND_2GHZ;
	else if (WMODE_CAP_5G(main_bss_wdev->PhyMode))
		return NL80211_BAND_5GHZ;

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
		"PhyMode(%d) invalid, return NL80211_BAND_5GHZ!\n", main_bss_wdev->PhyMode);
	return NL80211_BAND_5GHZ;
}


VOID *CFG80211_FindChan(struct _RTMP_ADAPTER *pAd, UCHAR chan_id)
{
	CFG80211_CB *pCfg80211_CB = NULL;
	UINT32 IdChan, ChanNum, ChanFreq;
	enum nl80211_band band_idx;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_supported_band *pBand;

	pCfg80211_CB = pAd->pCfg80211_CB;

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	chandef = wdev_chandef(pCfg80211_CB->pCfg80211_Wdev, 0);
#else
	chandef = &pCfg80211_CB->pCfg80211_Wdev->chandef;
#endif /* CFG_CFG80211_VERSION */
	if (!chandef) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"chandef is NULL\n");
		return NULL;
	}

	band_idx = CFG80211_Get_BandId(pAd);
	pBand = &pCfg80211_CB->Cfg80211_bands[band_idx];
	ChanFreq = ieee80211_channel_to_frequency(chan_id, band_idx);
	ChanNum = pCfg80211_CB->Cfg80211_bands[band_idx].n_channels;

	for (IdChan = 0; IdChan < ChanNum; IdChan++) {
		if (pBand->channels[IdChan].center_freq == ChanFreq)
			return &pBand->channels[IdChan];
	}

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
		"Find fail for chan %d.\n", chan_id);
	return NULL;
}

#ifdef SUPP_SAE_SUPPORT
int mtk_cfg80211_event_connect_params(
	struct _RTMP_ADAPTER *pAd, UCHAR *pmk, int pmk_len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !pmk)
		return -1;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO, "80211> ==>\n");

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.rsp_data = pmk;
	ctrl_data.rsp_data_len = pmk_len;
	mtk_cfg80211_send_connect_params_event(&ctrl_data);

	return 0;
}


int mtk_cfg80211_event_pmksa(struct _RTMP_ADAPTER *pAd,
		UCHAR *pmk, int pmk_len, UCHAR *pmkid, UINT32 akmp, UINT8 *aa)
{

	struct mt_cfg80211_vndr_ctrl ctrl_data;
	MTK_PMKSA_EVENT *pmksa_event = NULL;

	if (!pAd || !pmk || !pmkid || !aa)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;

	MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> ==>\n");

	os_alloc_mem(pAd, (UCHAR **)&pmksa_event, sizeof(MTK_PMKSA_EVENT));
	if (!pmksa_event)
		return 0;

	os_zero_mem(pmksa_event, sizeof(MTK_PMKSA_EVENT));
	os_move_mem(pmksa_event->pmk, pmk, pmk_len);
	pmksa_event->pmk_len = pmk_len;
	os_move_mem(pmksa_event->pmkid, pmkid, 16);
	pmksa_event->akmp = akmp;
	os_move_mem(pmksa_event->aa, aa, 6);

	ctrl_data.rsp_data = pmksa_event;
	ctrl_data.rsp_data_len = sizeof(MTK_PMKSA_EVENT);

	mtk_cfg80211_send_pmksa_event(&ctrl_data);
	os_free_mem(pmksa_event);
	return 0;
}

#endif

struct wifi_dev *mtk_cfg80211_vndr_get_wdev_by_wireless_dev(struct _RTMP_ADAPTER *pAd,
									struct wireless_dev *wl_dev)
{
	struct wifi_dev *wdev;
	struct net_device *pNetDev = wl_dev->netdev;
	INT ifIndex;

	if (wl_dev->iftype == NL80211_IFTYPE_AP) {
		ifIndex = CFG80211_FindMbssApIdxByNetDevice(pAd, pNetDev);
		if (!VALID_MBSS(pAd, ifIndex)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				    "error AP index\n");
			return NULL;
		}
		wdev = &pAd->ApCfg.MBSSID[ifIndex].wdev;
	}
#ifdef APCLI_CFG80211_SUPPORT
	else if (wl_dev->iftype == NL80211_IFTYPE_STATION) {
		ifIndex = CFG80211_FindStaIdxByNetDevice(pAd, pNetDev);
		if (!VALID_MBSS(pAd, ifIndex)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				    "error station index\n");
			return NULL;
		}
		wdev = &pAd->StaCfg[ifIndex].wdev;
	}
#endif /* APCLI_CFG80211_SUPPORT */
	else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				    "Can't find a valid wdev\n");
		return NULL;
	}

	return wdev;
}

int nl80211_send_event_ACS_per_channel_info(
	struct _RTMP_ADAPTER *mac_ad, void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!mac_ad)
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO, "\n");
	ctrl_data.wiphy = mac_ad->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;
	mtk_nl80211_send_event_ACS_per_channel_info(&ctrl_data);
	return 0;
}

int nl80211_send_event_sta_link_mac(struct _RTMP_ADAPTER *pAd, void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !data)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.wl_dev = pAd->pCfg80211_CB->pCfg80211_Wdev;

	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;
	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "\n");

	mtk_nl80211_send_sta_link_mac_event(&ctrl_data);
	return 0;
}

int nl80211_send_event_offchannel_info(struct _RTMP_ADAPTER *pAd,
						void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !data)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.wl_dev = pAd->pCfg80211_CB->pCfg80211_Wdev;

	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "\n");

	mtk_nl80211_send_offchannel_info_event(&ctrl_data);
	return 0;
}

int nl80211_send_event_stop_disassoc_timer(struct _RTMP_ADAPTER *pAd, void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !data)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.wl_dev = pAd->pCfg80211_CB->pCfg80211_Wdev;

	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;

	mtk_nl80211_send_stop_disassoc_timer_event(&ctrl_data);
	return 0;
}

#ifdef WAPP_SUPPORT
int nl80211_send_wapp_qry_rsp(
	struct _RTMP_ADAPTER *pAd,
	struct wapp_event *event)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;
	UINT buflen = sizeof(struct wapp_event);

	if (!pAd || !event)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.wl_dev = pAd->pCfg80211_CB->pCfg80211_Wdev;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "\n");

	event->len = buflen - sizeof(event->len) - sizeof(event->event_id);
	ctrl_data.rsp_data = event;
	ctrl_data.rsp_data_len = buflen;

	mtk_nl80211_send_wapp_qry_rsp_event(&ctrl_data);
	return 0;
}
#ifdef RT_CFG80211_SUPPORT
#endif
#endif /* WAPP_SUPPORT */

int mtk_cfg80211_event_bss_ml_info(
	IN PNET_DEV pNetDev,
	IN struct ml_info_event ml_event)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;
	UINT buflen = sizeof(struct ml_info_event);

	if (!pNetDev)
		return -1;

	ctrl_data.wiphy = pNetDev->ieee80211_ptr->wiphy;
	ctrl_data.wl_dev = pNetDev->ieee80211_ptr;

	ctrl_data.rsp_data = &ml_event;
	ctrl_data.rsp_data_len = buflen;

	mtk_cfg80211_send_bss_ml_info_event(&ctrl_data);
	return 0;
}

int mtk_cfg80211_event_reconf_sm(
	IN PNET_DEV pNetDev,
	IN UINT32 reconfig_state)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;
	UINT buflen = sizeof(UINT32);
	PRTMP_ADAPTER pAd;

	if (!pNetDev)
		return -1;

	ctrl_data.wiphy = pNetDev->ieee80211_ptr->wiphy;
	ctrl_data.wl_dev = pNetDev->ieee80211_ptr;
	MAC80211_PAD_GET(pAd, ctrl_data.wiphy);

	ctrl_data.rsp_data = &reconfig_state;
	ctrl_data.rsp_data_len = buflen;

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_RECONF_FLOW, DBG_LVL_INFO, "\n");

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_RECONF_FLOW, DBG_LVL_INFO,
				"buflen=%d, reconfig_state=%d\n", buflen, reconfig_state);

	mtk_cfg80211_send_reconf_sm_event(&ctrl_data);

	return 0;
}

BOOLEAN mtk_cfg80211_auto_ch_sel(PRTMP_ADAPTER pAd,
										struct wifi_dev *wifi_wdev)
{
	AUTO_CH_CTRL *pAutoChCtrl = HcGetAutoChCtrl(pAd);
	struct _AUTOCH_SEL_CTRL *pAutoChSelCtrl = NULL;

	if (pAutoChCtrl == NULL) {
		MTWF_PRINT("pAutoChCtrl is null!\n");
		return FALSE;
	}
	pAutoChSelCtrl = &pAutoChCtrl->AutoChSelCtrl;
	if (pAutoChSelCtrl == NULL) {
		MTWF_PRINT("pAutoChSelCtrl is null!\n");
		return FALSE;
	}

	if (!wifi_wdev) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR, "NULL wdev!\n");
		return FALSE;
	}

	/*if scan is running, ACS will not be triggered*/
#ifdef SCAN_SUPPORT
	if (scan_in_run_state(pAd, wifi_wdev)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"Failed!!!Scan is running, please try again after scan done!\n");
		return FALSE;
	}
#endif

#ifdef TR181_SUPPORT
	{
		struct hdev_ctrl *ctrl = pAd->hdev_ctrl;

		/*set ACS trigger flag to Manual trigger*/
		if (ctrl)
			ctrl->rdev.pRadioCtrl->ACSTriggerFlag = 2;
	}
#endif /*TR181_SUPPORT*/
	/*To do ACS, need TakeChannelOpCharge first*/
	if (!TakeChannelOpCharge(pAd, wifi_wdev, CH_OP_OWNER_ACS, TRUE)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"TakeChannelOpCharge fail for ACS!!\n");
		return FALSE;
	}
	/*record ACS execute type and time*/
	pAutoChCtrl->AutoChSelCtrl.LatestACSTimeNode = jiffies;
	pAutoChCtrl->AutoChSelCtrl.LatestACSType = ACS_EXECUTE_TYPE_COMMAND;
	pAutoChSelCtrl->CmdTriggerCount++;
	AutoChSelScanStart(pAd, wifi_wdev);

	return TRUE;
}

#ifdef DFS_SLAVE_SUPPORT
int mtk_nl80211_bhstatus_event(
	struct _RTMP_ADAPTER *mac_ad,
	void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!mac_ad)
		return -RTMP_IO_EINVAL;

	ctrl_data.wiphy = mac_ad->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.net_dev = mac_ad->net_dev;
	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;
	mtk_nl80211_send_event_bhstatus(&ctrl_data);
	return 0;
}
#endif /* DFS_SLAVE_SUPPORT */

#ifdef ANDLINK_V4_0
int mtk_nl80211_andlink_wifi_change_event(struct _RTMP_ADAPTER *pAd,
						void *data, int len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !data)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;

	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;

	mtk_nl80211_send_andlink_wifi_change_event(&ctrl_data);
	return 0;
}

#endif /*ANDLINK_V4_0*/
#ifdef ZERO_HANDOFF_ROAMING
int mtk_nl80211_zr_hdo_rm_event(PNET_DEV pNetDev,
					void *data, int len, u32 event_attr)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;
	PRTMP_ADAPTER pAd;

	if (!pNetDev)
		return -1;

	ctrl_data.wiphy = pNetDev->ieee80211_ptr->wiphy;
	ctrl_data.wl_dev = pNetDev->ieee80211_ptr;
	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;
	ctrl_data.net_dev = pNetDev;
	ctrl_data.sub_id = event_attr;

	MAC80211_PAD_GET(pAd, ctrl_data.wiphy);
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "\n");

	mtk_nl80211_send_zr_hdo_rm_event(&ctrl_data);

	return 0;
}

#endif
void cfg80211_drv_init(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_os_net_dev_op_hook *pDevOpHook)
{
	unsigned int mtk_dev_features = 0;
	unsigned int mtk_cfg80211_features = 0;

	pDevOpHook->reg_cfg80211 = 1;
#ifdef RFKILL_HW_SUPPORT
	mtk_dev_features |= MT_FEATURE_RFKILL_HW_SUPPORT;
#endif /* RFKILL_HW_SUPPORT */
#ifdef SEL_DUAL_BAND
	mtk_dev_features |= MT_FEATURE_SEL_DUAL_BAND;
#endif /* SEL_DUAL_BAND */
#ifdef CONFIG_VLAN_GTK_SUPPORT
	mtk_dev_features |= MT_FEATURE_VLAN_GTK_INTF;
#endif /* CONFIG_VLAN_GTK_SUPPORT */
#ifdef DOT11_EHT_BE
	mtk_dev_features |= MT_FEATURE_MLO;
#endif /* DOT11_EHT_BE */
#ifdef DOT11V_MBSSID_SUPPORT
	mtk_dev_features |= MT_FEATURE_11V_MBSS;
#endif /* DOT11V_MBSSID_SUPPORT */
#if defined(IWCOMMAND_CFG80211_SUPPORT) || defined(HOSTAPD_MBSS_SUPPORT)
	mtk_dev_features |= MT_FEATURE_4ADDR_STATION;
#endif /* IWCOMMAND_CFG80211_SUPPORT */

#ifdef WIFI_IAP_IW_SET_CHANNEL_FEATURE
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;
#endif /* WIFI_IAP_IW_SET_CHANNEL_FEATURE */
#ifdef SUPP_SAE_SUPPORT
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_SAE;
#endif /* SUPP_SAE_SUPPORT */
#if defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT)
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_MGMT_TX_RANDOM_TA;
#endif
#ifdef BCN_PROTECTION_SUPPORT
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_BEACON_PROTECTION;
#endif
#ifdef CONFIG_6G_SUPPORT
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_UNSOL_BCAST_PROBE_RESP;
#endif /* CONFIG_6G_SUPPORT */
#if defined(MT_DFS_SUPPORT) && defined(BACKGROUND_SCAN_SUPPORT)
	mtk_cfg80211_features |= MT_CFG80211_FEATURE_RADAR_BACKGROUND;
#endif
	pDevOpHook->mtk_dev_features = mtk_dev_features;
	pDevOpHook->mtk_cfg80211_features = mtk_cfg80211_features;
}

#endif /* RT_CFG80211_SUPPORT */
#endif /* LINUX_VERSION_CODE */

/* End of crda.c */

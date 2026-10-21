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
 *
 ***************************************************************************/
#ifdef RT_CFG80211_SUPPORT

#include "rtmp_comm.h"
#include "rt_os_util.h"

#include "rt_config.h"
#include "chlist.h"
/* all available channels */

static const UCHAR Cfg80211_Chan[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,

	/* 802.11 UNI / HyperLan 2 */
	36, 40, 44, 48, 52, 56, 60, 64,

	/* 802.11 HyperLan 2 */
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136,

	/* 802.11 UNII */
	140, 144, 149, 153, 157, 161, 165, 169, 173, 177,

	1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41,

	45, 49, 53, 57, 61, 65,	69, 73, 77, 81,

	85, 89, 93, 97, 101, 105, 109, 113, 117,

	121, 125, 129, 133, 137, 141, 145, 149,

	153, 157, 161, 165, 169, 173, 177, 181,

	185, 189, 193, 197, 201, 205, 209, 213,

	217, 221, 225, 229, 233,
};

UCHAR Cfg80211_RadarChan[] = {
	52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
};

/*
 *	Array of bitrates the hardware can operate with
 *	in this band. Must be sorted to give a valid "supported
 *	rates" IE, i.e. CCK rates first, then OFDM.
 *
 *	For HT, assign MCS in another structure, ieee80211_sta_ht_cap.
 */
const struct ieee80211_rate Cfg80211_SupRate[12] = {
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 10,
		.hw_value = 0,
		.hw_value_short = 0,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 20,
		.hw_value = 1,
		.hw_value_short = 1,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 55,
		.hw_value = 2,
		.hw_value_short = 2,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 110,
		.hw_value = 3,
		.hw_value_short = 3,
	},
	{
		.flags = 0,
		.bitrate = 60,
		.hw_value = 4,
		.hw_value_short = 4,
	},
	{
		.flags = 0,
		.bitrate = 90,
		.hw_value = 5,
		.hw_value_short = 5,
	},
	{
		.flags = 0,
		.bitrate = 120,
		.hw_value = 6,
		.hw_value_short = 6,
	},
	{
		.flags = 0,
		.bitrate = 180,
		.hw_value = 7,
		.hw_value_short = 7,
	},
	{
		.flags = 0,
		.bitrate = 240,
		.hw_value = 8,
		.hw_value_short = 8,
	},
	{
		.flags = 0,
		.bitrate = 360,
		.hw_value = 9,
		.hw_value_short = 9,
	},
	{
		.flags = 0,
		.bitrate = 480,
		.hw_value = 10,
		.hw_value_short = 10,
	},
	{
		.flags = 0,
		.bitrate = 540,
		.hw_value = 11,
		.hw_value_short = 11,
	},
};

VOID CFG80211OS_PutBss(
	IN VOID *pWiphyOrg,
	IN VOID *pCfg80211Bss)
{
	struct cfg80211_bss *bss = (struct cfg80211_bss *)pCfg80211Bss;
	struct wiphy *pWiphy = (struct wiphy *) pWiphyOrg;

	cfg80211_put_bss(pWiphy, bss);
}

/*
 * ========================================================================
 * Routine Description:
 *	Hint to the wireless core a regulatory domain from driver.
 *
 * Arguments:
 *	pAd				- WLAN control block pointer
 *	pCountryIe		- pointer to the country IE
 *	CountryIeLen	- length of the country IE
 *
 * Return Value:
 *	NONE
 *
 * Note:
 *	Must call the function in kernel thread.
 * ========================================================================
 */
VOID CFG80211OS_RegHint(
	IN VOID *pCB,
	IN UCHAR *pCountryIe,
	IN ULONG CountryIeLen)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
				"crda> regulatory domain hint: %c%c\n",
				 pCountryIe[0], pCountryIe[1]);

	if ((pCfg80211_CB->pCfg80211_Wdev == NULL) || (pCountryIe == NULL)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"crda> regulatory domain hint not support!\n");
		return;
	}

	/* hints a country IE as a regulatory domain "without" channel/power info. */
	regulatory_hint(pCfg80211_CB->pCfg80211_Wdev->wiphy, (const char *)pCountryIe);
}


/*
 * ========================================================================
 * Routine Description:
 *	Hint to the wireless core a regulatory domain from country element.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *	pCountryIe		- pointer to the country IE
 *	CountryIeLen	- length of the country IE
 *
 * Return Value:
 *	NONE
 *
 * Note:
 *	Must call the function in kernel thread.
 * ========================================================================
 */
VOID CFG80211OS_RegHint11D(
	IN VOID *pCB,
	IN UCHAR *pCountryIe,
	IN ULONG CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */
}


BOOLEAN CFG80211OS_BandInfoGet(
	IN VOID *pCB,
	IN VOID *pWiphyOrg,
	OUT VOID **ppBand24,
	OUT VOID **ppBand5,
	OUT VOID **ppBand6)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return FALSE;

	*ppBand24 = pWiphy->bands[IEEE80211_BAND_2GHZ];
	*ppBand5 = pWiphy->bands[IEEE80211_BAND_5GHZ];
	*ppBand6 = pWiphy->bands[IEEE80211_BAND_6GHZ];
	return TRUE;
}


UINT32 CFG80211OS_ChanNumGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN UINT32					IdBand)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL)
		return 0;

	if (pWiphy->bands[IdBand] != NULL)
		return pWiphy->bands[IdBand]->n_channels;

	return 0;
}

BOOLEAN CFG80211OS_ChanInfoGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN UINT32					IdBand,
	IN UINT32					IdChan,
	OUT UINT32					*pChanId,
	OUT UINT32					*pPower,
	OUT BOOLEAN					*pFlgIsRadar)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct wiphy *pWiphy = (struct wiphy *)pWiphyOrg;
	struct ieee80211_supported_band *pSband;
	struct ieee80211_channel *pChan;

	if (pWiphy == NULL) {
		if ((pCfg80211_CB != NULL) && (pCfg80211_CB->pCfg80211_Wdev != NULL))
			pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	}

	if (pWiphy == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR, "pWiphy NULL!\n");
		return FALSE;
	}

	pSband = pWiphy->bands[IdBand];
	if (pSband == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR, "pSband NULL!\n");
		return FALSE;
	}

	pChan = &pSband->channels[IdChan];
	if (pChan == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR, "pChan NULL!\n");
		return FALSE;
	}

	*pChanId = ieee80211_frequency_to_channel(pChan->center_freq);

	if (pChan->flags & IEEE80211_CHAN_DISABLED) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"Chan %03d (frq %d):\tnot allowed!\n",
			(*pChanId), pChan->center_freq);
		return FALSE;
	}

	*pPower = pChan->max_power;

	if (pChan->flags & IEEE80211_CHAN_RADAR)
		*pFlgIsRadar = TRUE;
	else
		*pFlgIsRadar = FALSE;

	return TRUE;
}


/*
 * ========================================================================
 * Routine Description:
 *	Initialize a channel information used in scan inform.
 *
 * Arguments:
 *
 * Return Value:
 *	TRUE		- Successful
 *	FALSE		- Fail
 *
 * ========================================================================
 */
BOOLEAN CFG80211OS_ChanInfoInit(
	IN VOID						*pCB,
	IN UINT32					InfoIndex,
	IN UCHAR					ChanId,
	IN UCHAR					MaxTxPwr,
	IN BOOLEAN					FlgIsNMode,
	IN BOOLEAN					FlgIsBW20M)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct ieee80211_channel *pChan;

	if (InfoIndex >= MAX_NUM_OF_CHANNELS)
		return FALSE;

	pChan = (struct ieee80211_channel *) &(pCfg80211_CB->ChanInfo[InfoIndex]);
	memset(pChan, 0, sizeof(*pChan));

	if (ChanId > 14)
		pChan->band = IEEE80211_BAND_5GHZ;
	else
		pChan->band = IEEE80211_BAND_2GHZ;

	pChan->center_freq = ieee80211_channel_to_frequency(ChanId, pChan->band);
	/* no use currently in 2.6.30 */
	/*	if (ieee80211_is_beacon(((struct ieee80211_mgmt *)pFrame)->frame_control)) */
	/*		pChan->beacon_found = 1; */
	return TRUE;
}


/*
 * ========================================================================
 * Routine Description:
 *	Inform us that a scan is got.
 *
 * Arguments:
 *	pAdCB				- WLAN control block pointer
 *
 * Return Value:
 *	NONE
 *
 * Note:
 *	Call RT_CFG80211_SCANNING_INFORM, not CFG80211_Scaning
 * ========================================================================
 */
VOID CFG80211OS_Scaning(
	IN VOID						*pCB,
	IN UINT32					ChanId,
	IN UCHAR					*pFrame,
	IN UINT32					FrameLen,
	IN INT32					RSSI,
	IN BOOLEAN					FlgIsNMode,
	IN UINT16					RawChannel,
	IN UINT8					BW)
{
#ifdef CONFIG_STA_SUPPORT
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *)pCB;
	struct ieee80211_supported_band *pBand;
	UINT32 IdChan;
	UINT32 CenFreq;
	UINT CurBand;
	struct wiphy *pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	struct cfg80211_bss *bss = NULL;
	struct ieee80211_mgmt *mgmt;

	mgmt = (struct ieee80211_mgmt *) pFrame;

	if (ChanId == 0)
		ChanId = 1;

	/* get channel information */

	if (ChanId > 14)
		CenFreq = ieee80211_channel_to_frequency(ChanId, IEEE80211_BAND_5GHZ);
	else
		CenFreq = ieee80211_channel_to_frequency(ChanId, IEEE80211_BAND_2GHZ);

	if (RawChannel >= CHANNEL_6G_BASE)
		CenFreq = ieee80211_channel_to_frequency(ChanId, IEEE80211_BAND_6GHZ);

	if (ChanId > 14)
		CurBand = IEEE80211_BAND_5GHZ;
	else
		CurBand = IEEE80211_BAND_2GHZ;

	if (RawChannel >= CHANNEL_6G_BASE)
		CurBand = IEEE80211_BAND_6GHZ;

	pBand = &pCfg80211_CB->Cfg80211_bands[CurBand];

	for (IdChan = 0; IdChan < pBand->n_channels; IdChan++) {
		if (pBand->channels[IdChan].center_freq == CenFreq)
			break;
	}

	if (IdChan >= pBand->n_channels) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"80211> Can not find any chan info! ==> %d[%d],[%d]\n",
				 ChanId, CenFreq, pBand->n_channels);
		return;
	}

	if (pWiphy->signal_type == CFG80211_SIGNAL_TYPE_MBM) {
		/* CFG80211_SIGNAL_TYPE_MBM: signal strength in mBm (100*dBm) */
		RSSI = RSSI * 100;
	}

	if (!mgmt->u.probe_resp.timestamp) {
#if (KERNEL_VERSION(5, 0, 0) < LINUX_VERSION_CODE)
		mt_os_time64_t tv;

		mt_os_get_time64(&tv);
		mgmt->u.probe_resp.timestamp = cpu_to_le64(ms_os_time64_to_ms(&tv));
#else
		struct timeval tv;

		do_gettimeofday(&tv);
		mgmt->u.probe_resp.timestamp = ((UINT64) tv.tv_sec * 1000000) + tv.tv_usec;
#endif
	}

	/* inform 80211 a scan is got */
	/* we can use cfg80211_inform_bss in 2.6.31, it is easy more than the one */
	/* in cfg80211_inform_bss_frame(), it will memcpy pFrame but pChan */
	bss = cfg80211_inform_bss_frame(pWiphy, &pBand->channels[IdChan],
									mgmt,	FrameLen,
									RSSI,	GFP_ATOMIC);

	if (unlikely(!bss)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"80211> bss inform fail ==> %d\n", IdChan);
		return;
	}

	CFG80211OS_PutBss(pWiphy, bss);
#endif /* CONFIG_STA_SUPPORT */
}


/*
 * ========================================================================
 * Routine Description:
 *	Inform us that scan ends.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *	FlgIsAborted	- 1: scan is aborted
 *
 * Return Value:
 *	NONE
 * ========================================================================
 */
VOID CFG80211OS_ScanEnd(
	IN struct _RTMP_ADAPTER *pAd,
	IN BOOLEAN FlgIsAborted)
{
	CFG80211_CB *pCfg80211_CB = pAd->pCfg80211_CB;

	NdisAcquireSpinLock(&pCfg80211_CB->scan_notify_lock);

	if (pCfg80211_CB->pCfg80211_ScanReq) {
		struct cfg80211_scan_info info = {
			.aborted = FlgIsAborted,
		};
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_NOTICE,
			"80211> cfg80211_scan_done\n");
		cfg80211_scan_done(pCfg80211_CB->pCfg80211_ScanReq, &info);

		pCfg80211_CB->pCfg80211_ScanReq = NULL;
	} else
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"80211> cfg80211_scan_done ==> NULL\n");

	NdisReleaseSpinLock(&pCfg80211_CB->scan_notify_lock);

	pAd->cfg80211_ctrl.FlgCfg80211Scanning = FALSE;
}


/*
 * ========================================================================
 * Routine Description:
 *	Inform CFG80211 about association status.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *	pBSSID			- the BSSID of the AP
 *	pReqIe			- the element list in the association request frame
 *	ReqIeLen		- the request element length
 *	pRspIe			- the element list in the association response frame
 *	RspIeLen		- the response element length
 *	FlgIsSuccess	- 1: success; otherwise: fail
 *
 * Return Value:
 *	None
 * ========================================================================
 */
void CFG80211OS_ConnectResultInform(
	IN PNET_DEV pNetDev, IN UCHAR * pBSSID, IN UCHAR * pReqIe, IN UINT32 ReqIeLen,
	IN UCHAR * pRspIe, IN UINT32 RspIeLen, IN UCHAR FlgIsSuccess)
{
	UCHAR *tmpReqIe = ReqIeLen ? pReqIe : NULL;
	UCHAR *tmpRsqIe = RspIeLen ? pRspIe : NULL;

	if ((pNetDev == NULL) || (pBSSID == NULL))
		return;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"ReqIeLen:%d, RspIeLen:%d, FlgIsSuccess:%d\n", ReqIeLen, RspIeLen, FlgIsSuccess);

	cfg80211_connect_result(pNetDev, pBSSID, tmpReqIe, ReqIeLen,
			tmpRsqIe, RspIeLen, FlgIsSuccess, GFP_KERNEL);
}

BOOLEAN CFG80211OS_RxMgmt(IN PNET_DEV pNetDev, IN INT32 freq, IN PUCHAR frame, IN UINT32 len, IN CHAR rssi)
{
	struct wireless_dev *wdev = NULL;

	/* Sanity Check */
	if (pNetDev == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"pNetDev == NULL\n");
		return FALSE;
	}

	wdev = pNetDev->ieee80211_ptr;
	/* Sanity Check */
	if (wdev == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
		"pwireless dev == NULL\n");
		return FALSE;
	}

	/* we may check if netdev is running.*/
	if (!netif_running(pNetDev)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
		"%s is not running.\n", RtmpOsGetNetDevName(pNetDev));
		return FALSE;
	}


	return cfg80211_rx_mgmt(pNetDev->ieee80211_ptr,
							freq,
							rssi,
							frame,
							len,
							GFP_ATOMIC);
}

VOID CFG80211OS_TxStatus(IN PNET_DEV pNetDev, IN INT32 cookie, IN PUCHAR frame, IN UINT32 len, IN BOOLEAN ack)
{
	return cfg80211_mgmt_tx_status(pNetDev->ieee80211_ptr, cookie, frame, len, ack, GFP_ATOMIC);
}

VOID CFG80211OS_NewSta(IN PNET_DEV pNetDev,
			IN const PUCHAR mac_addr,
			IN const PUCHAR assoc_frame,
			IN UINT32 assoc_len,
			IN BOOLEAN isReassoc,
			IN const PUCHAR assoc_resp_frame,
			IN UINT32 assoc_resp_len,
			IN BOOLEAN mlo_params_valid,
			IN UINT8 mlo_link_id,
			IN const PUCHAR mld_addr)
{
	struct station_info *sinfo;
	struct ieee80211_mgmt *mgmt;

	os_alloc_mem(NULL, (UCHAR **)&sinfo, sizeof(struct station_info));

	if (!sinfo)
		return;

	os_zero_mem(sinfo, sizeof(struct station_info));

/* If get error here, be sure patch the cfg80211_new_sta.patch into kernel. */
	if (pNetDev->ieee80211_ptr->iftype != RT_CMD_80211_IFTYPE_ADHOC) {
		mgmt = (struct ieee80211_mgmt *) assoc_frame;
		if (isReassoc) {
			sinfo->assoc_req_ies_len = assoc_len - 24 - 10;
			sinfo->assoc_req_ies = mgmt->u.reassoc_req.variable;
		} else {
			sinfo->assoc_req_ies_len = assoc_len - 24 - 4;
			sinfo->assoc_req_ies = mgmt->u.assoc_req.variable;
		}

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
		if (assoc_resp_frame && (assoc_resp_len > 0)) {
			mgmt = (struct ieee80211_mgmt *) assoc_resp_frame;
			sinfo->assoc_resp_ies = mgmt->u.assoc_resp.variable;
			sinfo->assoc_resp_ies_len = assoc_resp_len - 24 - 6;
		}

		if (mlo_params_valid && mld_addr) {
			sinfo->assoc_link_id = mlo_link_id;
			sinfo->mlo_params_valid = TRUE;
			memcpy(sinfo->mld_addr, mld_addr, ETH_ALEN);
#if defined(HOSTAPD_STA_MLD_MAC)
			memcpy(mac_addr, mld_addr, ETH_ALEN);
#endif
		}
#endif
	}

	cfg80211_new_sta(pNetDev, mac_addr, sinfo, GFP_ATOMIC);
	os_free_mem(sinfo);
}

#ifdef CUSTOMISED_MGMT_TRANS_SUPPORT
VOID CFG80211OS_Conn_Failed(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr, IN UCHAR fail_reason)
{
	return cfg80211_conn_failed(pNetDev, mac_addr, fail_reason, GFP_ATOMIC);
}
#endif

VOID CFG80211OS_DelSta(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr)
{
#if defined(HOSTAPD_STA_MLD_MAC) && defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	/*hostapd-2.11, hostapd will use the mld mac for search sta list*/
	if (!pNetDev) {
		RTMP_ADAPTER *pAd = NULL;
		MAC_TABLE_ENTRY *pEntry = NULL;

		GET_PAD_FROM_NET_DEV(pAd, pNetDev);
		if (pAd) {
			pEntry = MacTableLookup(pAd, mac_addr);
			if (pEntry && pEntry->mlo.mlo_en) {
				MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
						"pAd: setuplink("MACSTR") to mld mac"MACSTR".\n",
						MAC2STR(mac_addr), MAC2STR(pEntry->mlo.mld_addr));
				return cfg80211_del_sta(pNetDev, pEntry->mlo.mld_addr, GFP_ATOMIC);
			}
		}
	}
#endif
	return cfg80211_del_sta(pNetDev, mac_addr, GFP_ATOMIC);
}


VOID CFG80211OS_MICFailReport(PNET_DEV pNetDev, const PUCHAR src_addr, BOOLEAN unicast, INT key_id, const PUCHAR tsc)
{
	cfg80211_michael_mic_failure(pNetDev, src_addr,
								 (unicast ? NL80211_KEYTYPE_PAIRWISE : NL80211_KEYTYPE_GROUP),
								 key_id, tsc, GFP_ATOMIC);
}

VOID CFG80211OS_Roamed(
	PNET_DEV pNetDev, IN UCHAR *pBSSID,
	IN UCHAR *pReqIe, IN UINT32 ReqIeLen,
	IN UCHAR *pRspIe, IN UINT32 RspIeLen)
{
	struct cfg80211_roam_info roam_info = {
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
		.links[0].channel = NULL,
		.links[0].bssid = pBSSID,
#else
		.channel = NULL,
		.bssid = pBSSID,
#endif /* CFG_CFG80211_VERSION */
		.req_ie = pReqIe,
		.req_ie_len = ReqIeLen,
		.resp_ie = pRspIe,
		.resp_ie_len = RspIeLen,
	};
	cfg80211_roamed(pNetDev,
			&roam_info, GFP_KERNEL
	);
}

VOID CFG80211OS_EnableChanFlagsByBand(IN struct ieee80211_channel *pChannels,
				      IN UINT32 n_channels,
				      IN UINT32 freq_start_mhz,
				      IN UINT32 freq_end_mhz, IN UINT32 flags)
{
	INT32 idx = 0;

	if (!pChannels)
		return;

	for (idx = 0; idx < n_channels; idx++) {
		if ((pChannels[idx].center_freq >= (UINT16) freq_start_mhz) &&
		    (pChannels[idx].center_freq <= (UINT16) freq_end_mhz)) {
			/* If this is not disabled channel, we clear the flag of IEEE80211_CHAN_DISABLED */
				pChannels[idx].flags &= ~IEEE80211_CHAN_DISABLED;
		}
	}

}

VOID CFG80211OS_ForceUpdateChanFlagsByBand(IN struct ieee80211_supported_band *pBand,
					   IN struct ieee80211_channel *pChannelUpdate)
{
	struct ieee80211_channel *pChannels;
	INT32 idx = 0;

	if (!pBand || !pChannelUpdate)
		return;

	pChannels = pBand->channels;

	for (idx = 0; idx < pBand->n_channels; idx++) {
		if (pChannelUpdate[idx].flags & IEEE80211_CHAN_DISABLED)
			pChannels[idx].flags |= IEEE80211_CHAN_DISABLED;
		else
			pChannels[idx].flags &= ~IEEE80211_CHAN_DISABLED;
	}
}

static VOID cfg80211_clear_band_info(IN VOID *pCB, UCHAR band_idx)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct ieee80211_supported_band tmpBand;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_NOTICE,
		"band_idx=%d\n", band_idx);
	memcpy(&tmpBand, &pCfg80211_CB->Cfg80211_bands[band_idx], sizeof(tmpBand));
	memset(&pCfg80211_CB->Cfg80211_bands[band_idx], 0, sizeof(pCfg80211_CB->Cfg80211_bands[band_idx]));
	pCfg80211_CB->pCfg80211_Wdev->wiphy->bands[band_idx] = NULL;

	tmpBand.channels = NULL;
	tmpBand.bitrates = NULL;
}

INT32 CFG80211OS_UpdateRegRuleByRegionIdx(
IN VOID * pCB, IN VOID * pChDesc2G, IN VOID * pChDesc5G, IN VOID * pChDesc6G)
{
	CFG80211_CB *pCfg80211_CB = (CFG80211_CB *) pCB;
	struct wiphy *pWiphy = NULL;
	UINT32 freq_start_mhz = 0, freq_end_mhz = 0;
#ifdef EXT_BUILD_CHANNEL_LIST
	PCH_DESP pChDesc = NULL;
#else
	PCH_DESC pChDesc = NULL;
#endif
	INT32 n_channels = 0;
	INT32 ii = 0;
	struct ieee80211_supported_band *pSband;
	UCHAR band_idx = IEEE80211_BAND_2GHZ;
	UCHAR chn_idx_step = 1;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_NOTICE,
		"pChDesc2G=%p, pChDesc5G=%p, pChDesc6G=%p\n",
		pChDesc2G, pChDesc5G, pChDesc6G);

	if (!pCB || (!pChDesc2G && !pChDesc5G && !pChDesc6G)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"80211> invalid input!!\n");
		return -EINVAL;
	}

	pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	if (!pWiphy) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
			"80211> invalid pWiphy!!\n");
		return -EFAULT;
	}

	if (pChDesc2G) {
#ifdef EXT_BUILD_CHANNEL_LIST
		pChDesc = (PCH_DESP) pChDesc2G;
#else
		pChDesc = (PCH_DESC) pChDesc2G;
#endif

		cfg80211_clear_band_info(pCB, IEEE80211_BAND_5GHZ);
		cfg80211_clear_band_info(pCB, IEEE80211_BAND_6GHZ);

	} else if (pChDesc5G) {
		band_idx = IEEE80211_BAND_5GHZ;
		chn_idx_step = 4;

#ifdef EXT_BUILD_CHANNEL_LIST
		pChDesc = (PCH_DESP) pChDesc5G;
#else
		pChDesc = (PCH_DESC) pChDesc5G;
#endif

		cfg80211_clear_band_info(pCB, IEEE80211_BAND_2GHZ);
		cfg80211_clear_band_info(pCB, IEEE80211_BAND_6GHZ);
	} else if (pChDesc6G) {
		band_idx = IEEE80211_BAND_6GHZ;
		chn_idx_step = 4;

#ifdef EXT_BUILD_CHANNEL_LIST
		pChDesc = (PCH_DESP) pChDesc6G;
#else
		pChDesc = (PCH_DESC) pChDesc6G;
#endif

		cfg80211_clear_band_info(pCB, IEEE80211_BAND_2GHZ);
		cfg80211_clear_band_info(pCB, IEEE80211_BAND_5GHZ);
	}

	n_channels = pCfg80211_CB->Cfg80211_bands[band_idx].n_channels;
	if (pChDesc && n_channels > 0) {
		struct ieee80211_channel *pTmpCh = NULL;

		os_alloc_mem(NULL, (UCHAR **)&pTmpCh,
			sizeof(struct ieee80211_channel) * n_channels);
		if(pTmpCh == NULL) {
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
				"alloc memory for pTmpCh fail\n");
			return -ENOMEM;
		}

		memset(pTmpCh, 0, sizeof(struct ieee80211_channel) * n_channels);
		/* init all channels to be disabled */
		for (ii = 0; ii < n_channels; ii++) {
			pTmpCh[ii].flags |= IEEE80211_CHAN_DISABLED;
			pTmpCh[ii].center_freq =
			    pCfg80211_CB->Cfg80211_bands[band_idx].channels[ii].center_freq;
		}
		while (pChDesc && pChDesc->FirstChannel) {
			freq_start_mhz = pChDesc->StartFreq;
			freq_end_mhz = pChDesc->EndFreq;
#ifdef EXT_BUILD_CHANNEL_LIST
			CFG80211OS_EnableChanFlagsByBand(pTmpCh, n_channels, freq_start_mhz,
							 freq_end_mhz,
							 0);
#else
			CFG80211OS_EnableChanFlagsByBand(pTmpCh, n_channels, freq_start_mhz,
							 freq_end_mhz,
							 (UINT32) pChDesc->ChannelProp);
#endif
			pChDesc++;
		}
		pSband = pWiphy->bands[band_idx];
		CFG80211OS_ForceUpdateChanFlagsByBand(pSband, pTmpCh);
		os_free_mem(pTmpCh);
	}
	return 0;
}

/* the pChCtrl should clear before call this function */
UCHAR CFG80211OS_SyncChCtrlFromIEEE80211Channels(RTMP_ADAPTER *pAd, CHANNEL_CTRL *pChCtrl, UCHAR BandIdx)
{
	int i = 0;
	int band_idx = 0;
	int ch_idx = 0;
	struct ieee80211_supported_band *pBand = &(pAd->pCfg80211_CB->Cfg80211_bands[BandIdx]);

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
			"pCfg80211_CB=%p BandIdx=%d pBand->n_channels=%d\n",
			pAd->pCfg80211_CB, BandIdx, pBand->n_channels);


	for (i = 0; i < pBand->n_channels; i++) {
		if (pBand->channels[i].flags & IEEE80211_CHAN_DISABLED)
			continue;

		pChCtrl->ChList[ch_idx].Channel =
			(UCHAR)ieee80211_frequency_to_channel(pBand->channels[i].center_freq);
		pChCtrl->ChList[ch_idx].MaxTxPwr = pBand->channels[i].max_power;
		if (pBand->channels[i].flags & IEEE80211_CHAN_RADAR)
			pChCtrl->ChList[ch_idx].DfsReq = TRUE;
		// the Flags will process later
		pChCtrl->ChList[ch_idx].Flags = CHANNEL_DEFAULT_PROP;
#ifdef CONFIG_6G_SUPPORT
		if (BandIdx == IEEE80211_BAND_6GHZ && PSC_Ch_Check(pChCtrl->ChList[ch_idx].Channel))
			pChCtrl->ChList[ch_idx].PSC_Ch = TRUE;
#endif
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
			"freq=%d channel=%d\n",
			pBand->channels[i].center_freq, pChCtrl->ChList[ch_idx].Channel);
		ch_idx += 1;
	}

	pChCtrl->ChListNum = ch_idx;
	// current code not concern the DBDC case now
	for (band_idx = 0; band_idx < ARRAY_SIZE(pAd->pCfg80211_CB->Cfg80211_bands); band_idx++) {
		if (band_idx != IEEE80211_BAND_2GHZ &&
			band_idx != IEEE80211_BAND_5GHZ &&
			band_idx != IEEE80211_BAND_6GHZ)
			continue;
		if (band_idx != BandIdx)
			cfg80211_clear_band_info(pAd->pCfg80211_CB, band_idx);
	}
	return pChCtrl->ChListNum;
}

#ifdef APCLI_CFG80211_SUPPORT
#define AKM_SUITE_SAE 0x08ac0f00
int CFG80211OS_ExternalAuthRequest(PSTA_ADMIN_CONFIG pStaCfg)
{
	int ret, len;
	struct cfg80211_external_auth_params auth = {0};

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_INFO, "80211> ==>\n");

	if (!pStaCfg)
		return -1;

	if (pStaCfg->CfgSsidLen) {
		len = clamp_val(pStaCfg->CfgSsidLen, 0, MAX_LEN_OF_SSID);
		memcpy(auth.ssid.ssid, pStaCfg->CfgSsid, len);
		auth.ssid.ssid_len = len;
	}

	auth.key_mgmt_suite = AKM_SUITE_SAE;
	auth.action = NL80211_EXTERNAL_AUTH_START;
	NdisCopyMemory(auth.bssid, pStaCfg->CfgApCliBssid, MAC_ADDR_LEN);

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_DEBUG,
		"%s, external SAE processing: bssid=%pM action=%u akm=0x%x\n",
		pStaCfg->wdev.if_dev->name, auth.bssid, auth.action, auth.key_mgmt_suite);

	ret = cfg80211_external_auth_request(pStaCfg->wdev.if_dev, &auth, GFP_KERNEL);
	if (ret)
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_ERROR,
				"failed to offload external auth request\n");
	return ret;
}
#endif

#ifdef DOT11_EHT_BE
int CFG80211OS_VendorEventMloResponse(
	struct _RTMP_ADAPTER *pAd, uint8_t *data, uint32_t len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!pAd || !data)
		return -1;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;

	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;

	mtk_cfg80211os_VendorEventMloResponse(&ctrl_data);

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"send vendor response success.\n");
	return 0;
}

int CFG80211OS_MloExternalAuthRequest(struct _RTMP_ADAPTER *pAd, PSTA_ADMIN_CONFIG pStaCfg)
{
	struct wiphy *wiphy = NULL;
	struct wireless_dev *wdev = NULL;
	struct PARAM_EXTERNAL_AUTH_INFO *info = NULL;
	MAC_TABLE_ENTRY *entry = NULL;
	ULONG size = 0, len = 0;
	CFG80211_CB *pCfg80211_CB = NULL;

	pCfg80211_CB = pAd->pCfg80211_CB;
	wdev = pCfg80211_CB->pCfg80211_Wdev;
	wiphy = wdev->wiphy;

	size = sizeof(struct PARAM_EXTERNAL_AUTH_INFO) + MAX_LEN_OF_MLIE;
	if (os_alloc_mem(pAd, (UCHAR **)&info, size) != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
				"alloc vendor external auth event fail\n");
		return -1;
	}
	NdisZeroMemory(info, size);
	info->id = MTK_GRID_MLO_EXTERNAL_AUTH;
	info->len = sizeof(*info) - 2;
	info->action = NL80211_EXTERNAL_AUTH_START;

	entry = MacTableLookup(pAd, pStaCfg->CfgApCliBssid);
	if (!entry) {
		MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
				"can't find the CfgApCliBssid("MACSTR") ENTRY. FAIL\n",
				MAC2STR(pStaCfg->CfgApCliBssid));
		os_free_mem(info);
		return -1;
	}

	NdisCopyMemory(info->bssid, pStaCfg->CfgApCliBssid, MAC_ADDR_LEN);
	if (entry->mlo.mlo_en) {
		MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_WARN,
				"CfgApCliBssid("MACSTR") ENTRY. MLD PEER MAC("MACSTR").\n",
				MAC2STR(pStaCfg->CfgApCliBssid), MAC2STR(entry->mlo.mld_addr));
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_ERROR,
				"CfgApCliBssid("MACSTR") ENTRY.\n",
				MAC2STR(pStaCfg->CfgApCliBssid));
	}
	len = clamp_val(pStaCfg->CfgSsidLen, 0, MAX_LEN_OF_SSID);
	info->ssid_len = len;
	NdisCopyMemory(info->ssid, pStaCfg->CfgSsid, len);
	info->key_mgmt_suite = AKM_SUITE_SAE;
	COPY_MAC_ADDR(info->da, entry->Addr);
	/*add eht ies*/
	len = eht_add_auth_req_ies(&pStaCfg->wdev, info->ext_ie, 0);
	info->len += len;
	hex_dump("\n sae external eht_add_auth_req_ies:", info->ext_ie, len);

	CFG80211OS_VendorEventMloResponse(pAd, (uint8_t *)info, (uint32_t)(sizeof(*info) + len));

	os_free_mem(info);

	return 0;
}
#endif /* DOT11_EHT_BE */

#ifdef CFG_RSNO_SUPPORT
BOOLEAN CFG80211_handle_rsne_override(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	unsigned char *ie,
	unsigned int ie_len)
{
	const UCHAR *rsne_override_ie = NULL;

	rsne_override_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_RSNE_OVERRIDE,
			ie, ie_len);
	wdev->rsneo_len = 0;
	NdisZeroMemory(wdev->rsneo_content, MAX_LEN_OF_RSNIE);
	if (rsne_override_ie != NULL) {
		EID_STRUCT *eid;

		MTWF_DBG(mac_ad, DBG_CAT_SEC, CATSEC_RSNO, DBG_LVL_INFO,
				"RSNE Override IE(=%d)\n", ie_len);
		eid = (EID_STRUCT *)rsne_override_ie;
		if (eid->Len + 2 <= MAX_LEN_OF_RSNIE) {
			NdisCopyMemory(wdev->rsneo_content, rsne_override_ie, eid->Len+2);
			wdev->rsneo_len = eid->Len + 2;
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN CFG80211_handle_rsne_override_2(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	unsigned char *ie,
	unsigned int ie_len)
{
	const UCHAR *rsne_override_2_ie = NULL;

	rsne_override_2_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_RSNE_OVERRIDE_2,
			ie, ie_len);
	wdev->rsneo2_len = 0;
	NdisZeroMemory(wdev->rsneo2_content, MAX_LEN_OF_RSNIE);
	if (rsne_override_2_ie != NULL) {
		EID_STRUCT *eid;

		MTWF_DBG(mac_ad, DBG_CAT_SEC, CATSEC_RSNO, DBG_LVL_INFO,
				"RSNE Override IE(=%d)\n", ie_len);
		eid = (EID_STRUCT *)rsne_override_2_ie;
		if (eid->Len + 2 <= MAX_LEN_OF_RSNIE) {
			NdisCopyMemory(wdev->rsneo2_content, rsne_override_2_ie, eid->Len+2);
			wdev->rsneo2_len = eid->Len + 2;
			return TRUE;
		}
	}
	return FALSE;
}

void CFG80211_handle_rsnxe_override(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	UINT8 h2e_only,
	unsigned char *ie,
	unsigned int ie_len)
{
	const UCHAR *rsnxe_override_ie = NULL;
	UINT8 sae_pk = 0;

	rsnxe_override_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_RSNXE_OVERRIDE,
			ie, ie_len);
	NdisZeroMemory(wdev->rsnxeo_content, MAX_LEN_OF_RSNXEIE);
	wdev->rsnxeo_len = 0;
	if (rsnxe_override_ie != NULL) {
		EID_STRUCT *eid;

		eid = (EID_STRUCT *)rsnxe_override_ie;
		MTWF_DBG(mac_ad, DBG_CAT_SEC, CATSEC_RSNO, DBG_LVL_NOTICE,
				"RSNXE Override IE(=%d)\n", eid->Len);
		if ((eid->Len + 2 <= MAX_LEN_OF_RSNXEIE) && (eid->Len > OUI_WFA_LEN+1)) {
			NdisCopyMemory(wdev->rsnxeo_content, rsnxe_override_ie, eid->Len+2);
			wdev->rsnxeo_len = eid->Len + 2;
			/*bit5 means h2e*/
			if (rsnxe_override_ie[OUI_WFA_LEN+3] & 0x20)
				h2e_only |= 0x2;
			/*bit6 means saepk*/
			if (rsnxe_override_ie[OUI_WFA_LEN+1] & (1 << IE_RSNXE_CAPAB_SAE_PK))
				sae_pk = 1;
			sec_update_sae_cap(h2e_only, sae_pk, &wdev->SecConfig_ext.sae_cap);
		} else
			MTWF_DBG(mac_ad, DBG_CAT_SEC, CATSEC_RSNO, DBG_LVL_NOTICE,
				"Igore this RSNXE Override IE(=%d)\n", eid->Len);
	}
}
#endif /* CFG_RSNO_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */

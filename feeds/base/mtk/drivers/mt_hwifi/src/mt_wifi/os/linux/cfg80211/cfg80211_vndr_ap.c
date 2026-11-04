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

	All related CFG80211 AP Vendor Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT
#ifdef CONFIG_AP_SUPPORT

#include "rt_config.h"

/*******************************************************************************
 *                           D A T A
 *******************************************************************************
 */
#define MIN(a, b)					((a) < (b) ? (a) : (b))

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static INT mtk_cfg80211_set_beacon_period(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u16 bcn_int)
{
	/* only do this for AP MBSS, ignore other inf type */
	if (wdev && wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;

	if ((bcn_int >= 20) && (bcn_int < 1024)) {
		pAd->CommonCfg.BeaconPeriod = bcn_int;

#ifdef AP_QLOAD_SUPPORT
		/* re-calculate QloadBusyTimeThreshold */
		QBSS_LoadAlarmReset(pAd);
#endif /* AP_QLOAD_SUPPORT */
		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));

		wdev->bss_info_argument.bcn_period = bcn_int;
		wdev->bss_info_argument.u8BssInfoFeature = BSS_INFO_BASIC_FEATURE;
		if (AsicBssInfoUpdate(pAd, &wdev->bss_info_argument) != NDIS_STATUS_SUCCESS)
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
				"Fail to apply the bssinfo\n");

		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
			"mwctl:set bss(%d) BeaconPeriod=%d\n", wdev->func_idx, bcn_int);
	}

	return 0;
}

static INT mtk_cfg80211_set_dtim_period(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 dtim_int)
{
	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;

	if ((dtim_int >= 1) && (dtim_int <= 255)) {

		pAd->ApCfg.MBSSID[wdev->func_idx].DtimPeriod = dtim_int;

		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));

		wdev->bss_info_argument.dtim_period = dtim_int;
		wdev->bss_info_argument.u8BssInfoFeature = BSS_INFO_BASIC_FEATURE;
		if (AsicBssInfoUpdate(pAd, &wdev->bss_info_argument) != NDIS_STATUS_SUCCESS)
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
				"Fail to apply the bssinfo\n");
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set bss(%d) Dtim Period=%d\n", wdev->func_idx, dtim_int);

	return 0;
}

static INT mtk_cfg80211_set_hide_ssid(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 hide_en)
{
	struct _BSS_STRUCT *mbss;
	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;

	mbss = &pAd->ApCfg.MBSSID[wdev->func_idx];
	if (mbss->bHideSsid != hide_en)
		mbss->bHideSsid = hide_en;


	UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set bss(%d) HideSSID=%d\n", wdev->func_idx,
		mbss->bHideSsid);

	return 0;

}

static INT mtk_cfg80211_set_ht_op_mode(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 ht_op_mode)
{
	if (ht_op_mode == HTMODE_GF)
		pAd->CommonCfg.RegTransmitSetting.field.HTMODE = HTMODE_GF;
	else if (ht_op_mode == HTMODE_MM)
		pAd->CommonCfg.RegTransmitSetting.field.HTMODE = HTMODE_MM;
	else
		return -RTMP_IO_EINVAL; /*Invalid argument */

	wlan_config_set_ht_mode(wdev, ht_op_mode);
	wlan_operate_loader_greenfield(wdev, ht_op_mode);

	SetCommonHtVht(pAd, wdev);
	UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));

	MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_HT, DBG_LVL_NOTICE,
		"mwctl:set bss(%d) HT_OP_MODE=%s\n",
		wdev->func_idx, pAd->CommonCfg.RegTransmitSetting.field.HTMODE ? "GF":"MM");

	return 0;
}

static INT mtk_cfg80211_set_bss_max_idle_period_proc(
	struct _RTMP_ADAPTER *pAd, IN unsigned int idle_period)
{
	UINT8 IfIdx = 0;
	INT ret;

	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	struct wifi_dev *wdev = NULL;

	/* only do this for AP MBSS, ignore other inf type */
	if ((pObj->ioctl_if_type == INT_MBSSID) || (pObj->ioctl_if_type == INT_MAIN)) {
		IfIdx = PF_TO_BSS_IDX(pAd, pObj->ioctl_if);
		if (IfIdx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "invalid ioctl_if %d\n", IfIdx);
			return FALSE;
		}
		wdev = &pAd->ApCfg.MBSSID[IfIdx].wdev;
	} else
		return FALSE;

	if ((idle_period >= 1) && (idle_period <= 0xffff)) {
		pAd->ApCfg.MBSSID[IfIdx].max_idle_period = idle_period;
		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"(i/f %d) (MaxIdlePeriod=%d)\n", IfIdx, idle_period);

		ret = TRUE;
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"(i/f %d) (ERROR! range 1 to 65535)\n", IfIdx);

		ret = FALSE;
	}

	return ret;
}

static INT mtk_cfg80211_set_vht_bw_signaling(
	struct _RTMP_ADAPTER * pAd, struct wifi_dev *wdev, UCHAR value)
{
	if (value > BW_SIGNALING_DYNAMIC)
		value = BW_SIGNALING_DISABLE;
	wlan_config_set_vht_bw_sig(wdev, value);

	if (value > BW_SIGNALING_DISABLE) {

		UniCmdConfigSetRtsSignalEn(pAd, wdev, TRUE);
		UniCmdConfigSetSchDetDis(pAd, wdev, FALSE);
		/*Set RTS Threshold to a lower Value */
		/* Otherwise RTS Packets are not send - TGAC 5.2.67A*/
		wlan_operate_set_rts_len_thld(wdev, 500);
	} else {

		UniCmdConfigSetRtsSignalEn(pAd, wdev, FALSE);
		UniCmdConfigSetSchDetDis(pAd, wdev, TRUE);
		/*Set RTS Threshold to a lower Value */
		/* Otherwise RTS Packets are not send - TGAC 5.2.67A*/
		wlan_operate_set_rts_len_thld(wdev, 500);
	}

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_VHT, DBG_LVL_ERROR,
		"set_vht_bw_signaling=%d\n", wlan_config_get_vht_bw_sig(wdev));
	return 0;
}

static INT mtk_cfg80211_set_mgmt_rx(
	struct _RTMP_ADAPTER *pAd, u8 mgmt_rx_action)
{
	if (mgmt_rx_action >= MGMT_RX_ACTION_MAX) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"mwctl:invalid mgmt_rx = %d\n", mgmt_rx_action);
		return -RTMP_IO_EINVAL;
	}
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
		"mwctl:set mgmt_rx = %d\n", mgmt_rx_action);
	pAd->ApCfg.reject_mgmt_rx = mgmt_rx_action;
	return 0;
}

static INT mtk_cfg80211_set_no_bcn(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 no_bcn)
{
	if (wdev && wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device\n");
		return -RTMP_IO_EINVAL;
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
		"mwctl:set no_bcn = %d\n", no_bcn);

	if (no_bcn)
		BcnStopHandle(pAd, wdev, TRUE);
	else
		BcnStopHandle(pAd, wdev, FALSE);
	return 0;
}

static INT mtk_cfg80211_set_11axonly(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 enable)
{
	USHORT wmode;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -RTMP_IO_EINVAL;
	}

	wmode = wdev->PhyMode;
	wmode = wmode_2_cfgmode(wmode);
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"wirelessmode %d\n", wmode);

	if ((wmode < 16) || (wmode > 21))
		wdev->IsAXOnly = 0;
	else
		wdev->IsAXOnly = enable;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set 11axonly enable = %d\n Note: wirelessmode must 16~21\n",
		wdev->IsAXOnly);

	return 0;
}

static INT mtk_cfg80211_set_11beonly(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 enable)
{
	USHORT wmode;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -RTMP_IO_EINVAL;
	}

	wmode = wdev->PhyMode;
	wmode = wmode_2_cfgmode(wmode);
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"wirelessmode %d\n", wmode);

	if ((wmode < 22) || (wmode > 27))
		wdev->IsBEOnly = 0;
	else
		wdev->IsBEOnly = enable;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set 11beonly enable = %d\n Note: wirelessmode must 22~27\n",
		wdev->IsBEOnly);

	return 0;
}

static INT mtk_cfg80211_set_no_agmode(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 enable)
{
	USHORT wmode;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -EINVAL;
	}

	wmode = wdev->PhyMode;
	wmode = wmode_2_cfgmode(wmode);
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"wirelessmode %d\n", wmode);

	if ((wmode < 12) || (wmode > 27))
		wdev->IsNoAGMode = 0;
	else
		wdev->IsNoAGMode = enable;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set no_agmode  enable = %d\n Note: wirelessmode must 12~27\n",
		wdev->IsNoAGMode);

	return 0;
}

static INT mtk_cfg80211_set_no_nmode(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 enable)
{
	USHORT wmode;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -EINVAL;
	}

	wmode = wdev->PhyMode;
	wmode = wmode_2_cfgmode(wmode);
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"wirelessmode %d\n", wmode);

	if ((wmode < 12) || (wmode > 27))
		wdev->IsNoNMode = 0;
	else
		wdev->IsNoNMode = enable;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"mwctl:set no_nmode enable = %d\n Note: wirelessmode must 12~27\n",
		wdev->IsNoNMode);

	return 0;
}

INT mtk_cfg80211_set_no_bmode(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 enable)
{
	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -EINVAL;
	}

	wdev->IsNoBMode = enable;

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
	"mwctl:set no_bmode enable = %d\n", wdev->IsNoBMode);

	return 0;
}

static int mtk_cfg80211_reply_bss_stats(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int payload_len = 0, k = 0;
	struct wifi_bss_stats *stats;
	struct _BSS_STRUCT *mbss = wdev->func_dev;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM
			|| wdev->func_idx < 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
		if (!mbss)
			return -RTMP_IO_EFAULT;
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -RTMP_IO_EFAULT;
	}
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		"apidx is %d.\n", wdev->func_idx);
	payload_len = sizeof(struct wifi_bss_stats);
	if (ctrl_data->rsp_data_len < payload_len)
		return -RTMP_IO_EINVAL;
	stats = (struct wifi_bss_stats *)ctrl_data->rsp_data;
	if (stats) {
		ctrl_data->rsp_data_len = payload_len;
		stats->PacketsSent = mbss->TxCount;
		stats->PacketsReceived = mbss->RxCount;
		stats->BytesSent = mbss->TransmittedByteCount;
		stats->BytesReceived = mbss->ReceivedByteCount;
		stats->ErrorsReceived = mbss->RxErrorCount;
		stats->DiscardPacketsReceived = mbss->RxDropCount;
		stats->ErrorsSent = mbss->TxErrorCount;
		stats->DiscardPacketsSent = mbss->TxDropCount;
		stats->UnicastPacketsSent = mbss->ucPktsTx;
		stats->UnicastPacketsReceived = mbss->ucPktsRx;
		stats->MulticastPacketsSent = mbss->mcPktsTx;
		stats->MulticastPacketsReceived = mbss->mcPktsRx;
		stats->BroadcastPacketsSent = mbss->bcPktsTx;
		stats->BroadcastPacketsReceived = mbss->bcPktsRx;
		for (k = 0; k < MAX_ACCESS_CAT_NUM; k++) {
			stats->WmmBytesSent[k] = mbss->TxBytesAdm[k];
			stats->WmmBytesReceived[k] = mbss->RxBytesAdm[k];
		}
	}

	return 0;
}

static INT mtk_cfg80211_set_amsdu_en(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	wlan_config_set_amsdu_en(wdev, value);
	SetCommonHtVht(pAd, wdev);

	MTWF_PRINT("(HtAmsdu=%d)\n", wlan_config_get_amsdu_en(wdev));
	return 0;
}

static INT mtk_cfg80211_set_coex_cnt_thr(RTMP_ADAPTER *pAd, u8 value)
{
	pAd->CommonCfg.BssCoexApCntThr = value;
	MTWF_PRINT("Set BssCoexApCntThr=%d!\n", pAd->CommonCfg.BssCoexApCntThr);
	return TRUE;
}

static INT mtk_cfg80211_get_amsdu_status(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	ctrl_data->u8_val = wlan_config_get_amsdu_en(wdev);
	MTWF_PRINT("(HtAmsdu=%d)\n", ctrl_data->u8_val);
	return 0;
}

static INT mtk_cfg80211_get_bss_isolation_status(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	UINT apidx = wdev->func_idx;

	if (apidx >= pAd->ApCfg.BssidNum)
		return -RTMP_IO_EINVAL;

	if (pAd->ApCfg.MBSSID[apidx].IsolateInterStaTraffic)
		ctrl_data->u8_val = 1;
	else
		ctrl_data->u8_val = 0;

	return 0;
}

static INT mtk_cfg80211_set_bss_isolate_en(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	UINT apidx = 0;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		apidx = wdev->func_idx;
		if (apidx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;

	if (value == 1)
		pAd->ApCfg.MBSSID[apidx].IsolateInterStaTraffic = TRUE;
	else if (value == 0)
		pAd->ApCfg.MBSSID[apidx].IsolateInterStaTraffic = FALSE;

	MTWF_PRINT("(bss_isolate=%ld)\n",
		pAd->ApCfg.MBSSID[apidx].IsolateInterStaTraffic);
	return TRUE;
}

static INT mtk_cfg80211_set_mpdu_density(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	if (value <= 7)
		wlan_config_set_min_mpdu_start_space(wdev, value);
	else
		wlan_config_set_min_mpdu_start_space(wdev, INTERVAL_NO_RESTRICTION);

	MTWF_PRINT("(HtMpduDensity=%d)\n",
		wlan_config_get_min_mpdu_start_space(wdev));
	return 0;
}

static INT mtk_cfg80211_Set_HtTxStream_Proc(RTMP_ADAPTER *pAd, ULONG value)
{
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	UINT32       apidx = pObj->ioctl_if;
	BSS_STRUCT *pMbss;
	struct wifi_dev *wdev = get_wdev_by_ioctl_idx_and_iftype(pAd, pObj->ioctl_if, pObj->ioctl_if_type);
	UINT8 ucTxPath = pAd->Antenna.field.TxPath;

	if (!wdev
		|| apidx >= MAX_BEACON_NUM) {
		MTWF_PRINT("wdev is null or apidx is invalid(=%d)\n", apidx);
		return -RTMP_IO_EFAULT;
	}

	pMbss = &pAd->ApCfg.MBSSID[apidx];

	ucTxPath = MIN(MCS_NSS_CAP(pAd)->max_nss,
		MCS_NSS_CAP(pAd)->max_path[MAX_PATH_TX]);

#ifdef ANTENNA_CONTROL_SUPPORT
	{
		if (pAd->bAntennaSetAPEnable)
			ucTxPath = pAd->TxStream;
	}
#endif /* ANTENNA_CONTROL_SUPPORT */

	if ((value >= 1) && (value <= ucTxPath)) {
		wlan_config_set_tx_stream(wdev, value);
		wlan_operate_set_tx_stream(wdev, value);
	} else {
		wlan_config_set_tx_stream(wdev, ucTxPath);
		wlan_operate_set_tx_stream(wdev, ucTxPath);
	}

	SetCommonHtVht(pAd, wdev);
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
		APStop(pAd, pMbss, AP_BSS_OPER_BY_RF);
		APStartUp(pAd, pMbss, AP_BSS_OPER_BY_RF);
	}
#endif
	MTWF_PRINT("(Tx Stream=%d)\n", wlan_operate_get_tx_stream(wdev));
	return 0;
}

static INT mtk_cfg80211_Set_AP_ASSOC_REQ_RSSI_THRESHOLD(
	struct _RTMP_ADAPTER *pAd, CHAR rssi)
{
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	UCHAR           apidx = pObj->ioctl_if;
	UINT j;

	if (rssi == 0)
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"Disable AP_ASSOC_REQ_RSSI_THRESHOLD\n");
	else if (rssi > 0 || rssi < -100) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "Value Error.\n");
		return FALSE;
	}
	if (apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "invalid ioctl_if %d\n", apidx);
		return FALSE;
	}

	pAd->ApCfg.MBSSID[apidx].AssocReqRssiThreshold = rssi;
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"I/F(ra%d) Set_AP_ASSOC_REQ_RSSI_THRESHOLD=%d\n", apidx,
			 pAd->ApCfg.MBSSID[apidx].AssocReqRssiThreshold);

	for (j = BSS0; j < pAd->ApCfg.BssidNum; j++)
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "%d. ==> %d\n",
			j, pAd->ApCfg.MBSSID[j].AssocReqRssiThreshold);

	return 0;
}

static INT mtk_cfg80211_set_ht_extcha(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	UCHAR bandidx;
	UCHAR i;
	struct wifi_dev *tdev;

	if (value != 0 && value != 1)
		return -RTMP_IO_EINVAL;

	bandidx = HcGetBandByWdev(wdev);
	for (i = 0; i < WDEV_NUM_MAX; i++) {
		tdev = pAd->wdev_list[i];
		if (tdev && (bandidx == HcGetBandByWdev(tdev)))
			set_extcha_for_wdev(pAd, tdev, value);
	}
	MTWF_PRINT("(HtExtcha=%d)\n",
		wlan_config_get_ext_cha(wdev));
	return 0;
}

static INT mtk_cfg80211_set_ht_protect(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	wlan_config_set_ht_protect_en(wdev, value);
	MTWF_PRINT("(HtProtect=%d)\n", value);
	return TRUE;
}

static INT mtk_cfg80211_set_disallow_vht(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	if (value == 0)
		pAd->CommonCfg.bNonVhtDisallow = FALSE;
	else
		pAd->CommonCfg.bNonVhtDisallow = TRUE;

	MTWF_PRINT("(bNonVhtDisallow=%d)\n",  pAd->CommonCfg.bNonVhtDisallow);
	return 0;
}

static INT mtk_cfg80211_set_etxbf_en_cond(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, u8 value)
{
	UINT16 i;
	UCHAR ucStatus = FALSE;
	MAC_TABLE_ENTRY		*pEntry;
	TXBF_STATUS_INFO    TxBfInfo;

	if (value > 1)
		return -RTMP_IO_EINVAL;
	wlan_config_set_etxbf(wdev, value);
	pAd->CommonCfg.ETxBfEnCond = 0;
	pAd->CommonCfg.ETxBfEnCond |= value;
	for (i = 0; VALID_UCAST_ENTRY_WCID(pAd, i); i++) {
		pEntry = entry_get(pAd, i);

		if (!IS_ENTRY_NONE(pEntry)) {
			wdev = pEntry->wdev;
			TxBfInfo.ucTxPathNum = pAd->Antenna.field.TxPath;
			TxBfInfo.ucRxPathNum = pAd->Antenna.field.RxPath;

#ifdef ANTENNA_CONTROL_SUPPORT
			{
				if (pAd->bAntennaSetAPEnable) {
					TxBfInfo.ucTxPathNum = pAd->TxStream;
					TxBfInfo.ucRxPathNum = pAd->RxStream;
				}
			}
#endif /* ANTENNA_CONTROL_SUPPORT */

			TxBfInfo.ucPhyMode   = wdev->PhyMode;
			TxBfInfo.u2Channel   = wdev->channel;
			TxBfInfo.pHtTxBFCap  = &pAd->CommonCfg.HtCapability.TxBFCap;
			TxBfInfo.cmmCfgETxBfIncapable = pAd->CommonCfg.ETxBfIncapable;
			TxBfInfo.cmmCfgETxBfNoncompress = pAd->CommonCfg.ETxBfNoncompress;
#ifdef VHT_TXBF_SUPPORT
			TxBfInfo.pVhtTxBFCap = &pAd->CommonCfg.vht_cap_ie.vht_cap;
#endif
			TxBfInfo.u4WTBL1 = pAd->mac_ctrl.wtbl_base_addr[0] +  i * pAd->mac_ctrl.wtbl_entry_size[0];
			TxBfInfo.u4WTBL2 = pAd->mac_ctrl.wtbl_base_addr[1] +  i * pAd->mac_ctrl.wtbl_entry_size[1];
			TxBfInfo.ucETxBfTxEn = value;
			TxBfInfo.ucITxBfTxEn  = FALSE;
			TxBfInfo.u2Wcid = i;
			TxBfInfo.ucBW = pEntry->HTPhyMode.field.BW;
			TxBfInfo.ucNDPARate = 2; /* MCS2 */
			ucStatus = AsicTxBfEnCondProc(pAd, &TxBfInfo);
		}
	}

	MTWF_PRINT("mtk_cfg80211_set_etxbf_en_cond::(etxbf en=%d)\n", value);
	if (ucStatus)
		return 0;
	else
		return -RTMP_IO_EFAULT;
}

static INT mtk_cfg80211_set_ieee80211h(RTMP_ADAPTER *pAd, UCHAR ieee80211h)
{
	if (ieee80211h == 1)
		pAd->CommonCfg.bIEEE80211H = TRUE;
	else if (ieee80211h == 0)
		pAd->CommonCfg.bIEEE80211H = FALSE;
	else
		return -EINVAL;

	MTWF_PRINT("%s::(IEEE80211H=%d)\n", __func__, pAd->CommonCfg.bIEEE80211H);
	return 0;
}

#ifdef ACK_CTS_TIMEOUT_SUPPORT
static INT mtk_cfg80211_set_ackcts_tout_en(RTMP_ADAPTER *pAd, UCHAR ackcts_tout_en)
{
	if (pAd) {
		pAd->CommonCfg.ack_cts_enable = (ackcts_tout_en == 0 ? FALSE : TRUE);
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"ACK_CTS_TOUT: %s\n",
			ackcts_tout_en == 0 ? "DISABLE" : "ENABLE");
	} else {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"invalid null input: pAd=%p;!!\n", pAd);
		return -RTMP_IO_EFAULT;
	}
	return 0;
}

static INT mtk_cfg80211_set_dst2acktimeout(RTMP_ADAPTER *pAd, UINT distance)
{
	UINT32 bTimeout = 0;
	UINT32 bCckCCAtout = 0, bCckMDRDYtout = 0;
	UINT32 bOfdmCCAtout = 0, bOfdmMDRDYtout = 0;
	UINT32 bOfdmaCCAtout = 0, bOfdmaMDRDYtout = 0;
	UINT32 bCCKTimeout = 0, bOFDMTimeout = 0, bOFDMATimeout = 0;
	UCHAR band_idx = hc_get_hw_band_idx(pAd);

	/* Calculate, timeout=((distance/speed of light)*2) */
	bTimeout = distance / LIGHT_SPEED * 2;
	if (bTimeout > MAX_ACK_TIMEOUT) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"Distance is too long, not support!\n");
		return -RTMP_IO_EINVAL;
	}

	/* Calculate timeout = Suggest Value + (distance / speed of light) * 2*/
	bCckCCAtout = (DEFAULT_TMAC_CDTR_VALUE >> 16) + bTimeout;
	bCckMDRDYtout = (DEFAULT_TMAC_CDTR_VALUE & MAX_ACK_TIMEOUT) + bTimeout;
	bOfdmCCAtout = (DEFAULT_TMAC_ODTR_VALUE >> 16) + bTimeout;
	bOfdmMDRDYtout = (DEFAULT_TMAC_ODTR_VALUE & MAX_ACK_TIMEOUT) + bTimeout;
	bOfdmaCCAtout = (DEFAULT_TMAC_OMDTR_VALUE >> 16) + bTimeout;
	bOfdmaMDRDYtout = (DEFAULT_TMAC_OMDTR_VALUE & MAX_ACK_TIMEOUT) + bTimeout;

	if ((bCckCCAtout <= 0 || bCckCCAtout > MAX_ACK_TIMEOUT)
		|| (bCckMDRDYtout <= 0 || bCckMDRDYtout > MAX_ACK_TIMEOUT)
		|| (bOfdmCCAtout <= 0 || bOfdmCCAtout > MAX_ACK_TIMEOUT)
		|| (bOfdmMDRDYtout <= 0 || bOfdmMDRDYtout > MAX_ACK_TIMEOUT)
		|| (bOfdmaCCAtout <= 0 || bOfdmaCCAtout > MAX_ACK_TIMEOUT)
		|| (bOfdmaMDRDYtout <= 0 || bOfdmaMDRDYtout > MAX_ACK_TIMEOUT)) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"CTS/ACK Timeout Range should between [0xFFFF:0)!!\n");
		return -RTMP_IO_EINVAL;
	}

	bCCKTimeout = DEFAULT_TMAC_CDTR_VALUE;
	bCCKTimeout += (bTimeout << 16) | bTimeout;
	if (set_ack_timeout_mode_byband(pAd, bCCKTimeout, band_idx, CCK_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK CCK Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	bOFDMTimeout = DEFAULT_TMAC_ODTR_VALUE;
	bOFDMTimeout += (bTimeout << 16) | bTimeout;
	if (set_ack_timeout_mode_byband(pAd, bOFDMTimeout, band_idx, OFDM_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK OFDM Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	bOFDMATimeout = DEFAULT_TMAC_OMDTR_VALUE;
	bOFDMATimeout += (bTimeout << 16) | bTimeout;
	if (set_ack_timeout_mode_byband(pAd, bOFDMATimeout, band_idx, OFDMA_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK OFDMA Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"SET CTS/ACK Timeout SUCCESS!!\n");
	return 0;
}

static INT mtk_cfg80211_set_cck_ack_tout(RTMP_ADAPTER *pAd, UINT cck_ack_tout)
{
	UINT16 bCCAtout = 0, bMDRDYtout = 0;
	UCHAR band_idx = hc_get_hw_band_idx(pAd);

	bCCAtout = (cck_ack_tout >> 16) & MAX_ACK_TIMEOUT;
	bMDRDYtout = cck_ack_tout & MAX_ACK_TIMEOUT;
	if ((bCCAtout <= 0 || bCCAtout > MAX_ACK_TIMEOUT)
		|| (bMDRDYtout <= 0 || bMDRDYtout > MAX_ACK_TIMEOUT)) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"CTS/ACK Timeout Range should between [0xFFFF:0)!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (set_ack_timeout_mode_byband(pAd, cck_ack_tout, band_idx, CCK_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"SET CCK CTS/ACK Timeout SUCCESS!!\n");
	return 0;
}

static INT mtk_cfg80211_set_ofdm_ack_tout(RTMP_ADAPTER *pAd, UINT ofdm_ack_tout)
{
	UINT16 bCCAtout = 0, bMDRDYtout = 0;
	UCHAR band_idx = hc_get_hw_band_idx(pAd);

	bCCAtout = (ofdm_ack_tout >> 16) & MAX_ACK_TIMEOUT;
	bMDRDYtout = ofdm_ack_tout & MAX_ACK_TIMEOUT;
	if ((bCCAtout <= 0 || bCCAtout > MAX_ACK_TIMEOUT)
		|| (bMDRDYtout <= 0 || bMDRDYtout > MAX_ACK_TIMEOUT)) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"CTS/ACK Timeout Range should between [0xFFFF:0)!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (set_ack_timeout_mode_byband(pAd, ofdm_ack_tout, band_idx, OFDM_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"SET OFDM CTS/ACK Timeout SUCCESS!!\n");
	return 0;
}

static INT mtk_cfg80211_set_ofdma_ack_tout(RTMP_ADAPTER *pAd, UINT ofdma_ack_tout)
{
	UINT16 bCCAtout = 0, bMDRDYtout = 0;
	UCHAR band_idx = hc_get_hw_band_idx(pAd);

	bCCAtout = (ofdma_ack_tout >> 16) & MAX_ACK_TIMEOUT;
	bMDRDYtout = ofdma_ack_tout & MAX_ACK_TIMEOUT;
	if ((bCCAtout <= 0 || bCCAtout > MAX_ACK_TIMEOUT)
		|| (bMDRDYtout <= 0 || bMDRDYtout > MAX_ACK_TIMEOUT)) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"CTS/ACK Timeout Range should between [0xFFFF:0)!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (set_ack_timeout_mode_byband(pAd, ofdma_ack_tout, band_idx, OFDMA_TIME_OUT) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
			"SET CTS/ACK Timeout Fail!!\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_NOTICE,
			"SET OFDMA CTS/ACK Timeout SUCCESS!!\n");
	return 0;
}
#endif /* ACK_CTS_TIMEOUT_SUPPORT */

static INT mtk_cfg80211_set_csa_2g(RTMP_ADAPTER *pAd, UCHAR csa_2g)
{
	if (csa_2g == 1) {
		pAd->CommonCfg.bIEEE80211H = TRUE;
		pAd->CommonCfg.CSAEnable = TRUE;
	} else if (csa_2g == 0) {
		pAd->CommonCfg.bIEEE80211H = FALSE;
		pAd->CommonCfg.CSAEnable = FALSE;
	} else
		return -EINVAL;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
		"set CSASupportFor2G=%d\n", pAd->CommonCfg.bIEEE80211H);
	return 0;
}

static INT mtk_cfg80211_set_csa(RTMP_ADAPTER *pAd, UCHAR CSAEnable)
{
	if (CSAEnable == 1)
		pAd->CommonCfg.CSAEnable = TRUE;
	else if (CSAEnable == 0)
		pAd->CommonCfg.CSAEnable = FALSE;
	else
		return -EINVAL;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
		"set CSASupport=%d\n", pAd->CommonCfg.CSAEnable);
	return 0;
}

#ifdef CFG_SUPPORT_FALCON_PP
static INT mtk_cfg80211_set_pp_bitmap(RTMP_ADAPTER *pAd, USHORT punctured_bitmap)
{
	UCHAR band_idx = hc_get_hw_band_idx(pAd);
	char buf[20] = {0};
	int ret = 0;
	INT32 status = NDIS_STATUS_FAILURE;

	if (punctured_bitmap) {
		ret = snprintf(buf, sizeof(buf), "%d-0-2-%2x-1-0", band_idx, punctured_bitmap);
		if (os_snprintf_error(sizeof(buf), ret)) {
			MTWF_PRINT("%s: snprintf error\n", __func__);
			return -EINVAL;
		}
	} else {
		ret = snprintf(buf, sizeof(buf), "%d-0-0-0-1-0", band_idx);
		if (os_snprintf_error(sizeof(buf), ret)) {
			MTWF_PRINT("%s: snprintf error\n", __func__);
			return -EINVAL;
		}
	}

	status = set_pp_cap_ctrl(pAd, buf);
	if (status == TRUE) {
		pAd->CommonCfg.punctured_bitmap = punctured_bitmap;
	} else {
		MTWF_PRINT("%s failed - [ERROR]set_pp_cap_ctrl fail\n", __func__);
		return -EINVAL;
	}

	return ret;
}
#endif /* CFG_SUPPORT_FALCON_PP */

int cfg80211_drv_vndr_set_ap_wireless(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct _RTMP_CHIP_CAP *chip_cap = NULL;
	UINT16 max_ba_wsize = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	chip_cap = hc_get_chip_cap(mac_ad->hdev_ctrl);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_FIXED_MCS) {
		wlan_config_set_fixed_mcs(wdev, ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"mcs %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_FIXED_OFDMA) {
		if ((ctrl_data->u8_val == CAPI_OFDMA_DL) ||
			(ctrl_data->u8_val == CAPI_OFDMA_DL_20n80)) {
			wlan_config_set_ofdma_direction(wdev, ctrl_data->u8_val);
			wlan_config_set_mu_dl_ofdma(wdev, 1);
			if (wlan_config_get_etxbf(wdev) != SUBF_ALL) {
				wlan_config_set_etxbf(wdev, SUBF_ALL);
				bf_type_ctrl(mac_ad);
			}
		} else if (ctrl_data->u8_val == CAPI_OFDMA_UL) {
			wlan_config_set_ofdma_direction(wdev, ctrl_data->u8_val);
			wlan_config_set_mu_ul_ofdma(wdev, 1);
		}
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"ofdma %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_PPDU_TX_TYPE) {
		wlan_config_set_ppdu_tx_type(wdev, ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"ppdu_type %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_NUSERS_OFDMA) {
		wlan_config_set_ofdma_user_cnt(wdev, ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"nuser_ofdma %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_BA_BUFFER_SIZE) {
		max_ba_wsize = ba_get_default_max_ba_wsize(wdev, mac_ad);

		wlan_config_set_ba_tx_wsize(
			wdev, ctrl_data->ushort_val, max_ba_wsize);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"ba_buff_size %d\n", ctrl_data->ushort_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_MIMO) {
		if (ctrl_data->u8_val == CAPI_MIMO_DL) {
			wlan_config_set_mu_dl_mimo(wdev, 1);
		} else if (ctrl_data->u8_val == CAPI_MIMO_UL) {
			wlan_config_set_mu_ul_mimo(wdev, 1);
		} else if (ctrl_data->u8_val == CAPI_MIMO_DL_UL) {
			wlan_config_set_mu_dl_mimo(wdev, 1);
			wlan_config_set_mu_ul_mimo(wdev, 1);
		} else {
			/* by default */
			wlan_config_set_mu_dl_mimo(wdev, 1);
		}
		/* ignore it, get from ap_set_wireless_bss_configs by profile */
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"mimo %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_AMPDU) {
		wlan_config_set_ba_enable(wdev, ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"ampdu %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_AMSDU) {
		BOOLEAN ie_update = FALSE;

		if (wlan_config_get_amsdu_en(wdev) != ctrl_data->u8_val)
			ie_update = TRUE;

		if (ie_update && (wdev->wdev_type == WDEV_TYPE_AP)) {
			if (bcn_bpcc_op_lock(mac_ad, wdev, TRUE, BCN_BPCC_HEOP) == FALSE) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
						"bcn_bpcc_op_lock fail!\n");
			}
			wlan_config_set_amsdu_en(wdev, ctrl_data->u8_val);
			UpdateBeaconHandler_BPCC(mac_ad, wdev, BCN_REASON(BCN_UPDATE_IE_CHG),
									BCN_BPCC_HEOP, TRUE);
		}
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"amsdu %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_CERT) {
		unsigned int recv_cnt = 0;
		unsigned char drv_cert_en = 0, fw_cert_en = 0;

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"cert %s\n", (char *)ctrl_data->data);

		recv_cnt = sscanf(ctrl_data->data, "%hhu-%hhu", &drv_cert_en, &fw_cert_en);
		if (recv_cnt == 2) {
			mac_ad->CommonCfg.wifi_cert = drv_cert_en;
#ifdef WIFI_UNIFIED_COMMAND
			if (chip_cap->uni_cmd_support)
				UniCmdCfgInfoUpdate(
					mac_ad, wdev, CFGINFO_CERT_CFG_FEATURE, &fw_cert_en);
			else
#endif /* WIFI_UNIFIED_COMMAND */
				CmdExtCmdCfgUpdate(
					mac_ad, wdev, CFGINFO_CERT_CFG_FEATURE, &fw_cert_en);

			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"drv_cert_en = %u, fw_cert_en = %u\n", drv_cert_en, fw_cert_en);
		} else {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"recv_cnt(%d) doesn't match with number of rule!\n", recv_cnt);
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_HE_TXOP_RTS_THLD) {
		if (ctrl_data->ushort_val > MAX_TXOP_DURATION_RTS_THRESHOLD) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG,
				 CATCFG_DBGLOG,
				 DBG_LVL_ERROR,
				 "incorrect value:%d\n", ctrl_data->ushort_val);
			return -RTMP_IO_EINVAL;
		}
		set_txop_dur_rts_thld(wdev, ctrl_data->ushort_val);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
			"he_txop_rts_thld = %d\n", ctrl_data->ushort_val);
	}

	return 0;
}

int cfg80211_drv_vndr_set_ap_rfeature(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_HE_GI) {
		ap_set_rfeature_he_gi(mac_ad, (VOID *)&ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"gi %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_HE_LTF) {
		ap_set_rfeature_he_ltf(mac_ad, (VOID *)&ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"ltf %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_TRIG_TYPE) {
		unsigned int recv_cnt = 0;
		unsigned int enable = 0;
		unsigned int trig_type = 0;
		char trig_type_buf[64] = "\0";

		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"trig_cfg %s\n", (char *)ctrl_data->data);
		recv_cnt = sscanf(ctrl_data->data, "%d,%d", &enable, &trig_type);
		if (recv_cnt == 2) {
			if (enable == 0) {
				ap_set_rfeature_dis_trig_type(mac_ad, (VOID *)&trig_type);
			} else {
				ret = snprintf(
					trig_type_buf,
					sizeof(trig_type_buf), "%d", trig_type);
				if (os_snprintf_error(sizeof(trig_type_buf), ret)) {
					MTWF_DBG(mac_ad,
						DBG_CAT_TEST, CATTEST_RFEATURE,
						DBG_LVL_WARN, "snprintf error!\n");
					return -RTMP_IO_EFAULT;
				}
				ap_set_rfeature_trig_type(mac_ad, (VOID *)trig_type_buf);
			}
		} else {
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
				"recv_cnt(%d) doesn't match with number of rule!\n",
				recv_cnt);
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_ACK_PLCY) {
		ap_set_rfeature_ack_policy(mac_ad, (VOID *)&ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"ack_plcy %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_PPDU_TYPE) {
		ap_set_rfeature_ppdu_tx_type(mac_ad, (VOID *)&ctrl_data->u8_val);
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"ppdu_type %d\n", ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_HE_TXOP_RTS_THLD) {
		if (ctrl_data->ushort_val > MAX_TXOP_DURATION_RTS_THRESHOLD) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				 	"incorrect rts threshold value %d\n", ctrl_data->ushort_val);
			return -RTMP_IO_EINVAL;
		}
		set_txop_dur_rts_thld(wdev, ctrl_data->ushort_val);
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"rts threshold value %d\n", ctrl_data->ushort_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_EXTRA_LTF_DISABLE) {
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"disable %d\n", ctrl_data->u8_val);
		ap_set_rfeature_ap_extra_eht_ltf_disable(mac_ad, &ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_RFEATURE_EXTRA_LTF) {
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_RFEATURE, DBG_LVL_NOTICE,
			"extra_ltf %d\n", ctrl_data->u8_val);
		ap_set_rfeature_extra_ltf(mac_ad, &ctrl_data->u8_val);
	}
	return 0;
}

int cfg80211_drv_vndr_set_bss(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_WIRELESS_MODE) {
		ret = mtk_cfg80211_set_wireless_mode(mac_ad, wdev->func_idx, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_HT_TX_STREAM_INFO) {
		ret = mtk_cfg80211_Set_HtTxStream_Proc(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ASSOCREQ_RSSI_THRES_INFO) {
		ret = mtk_cfg80211_Set_AP_ASSOC_REQ_RSSI_THRESHOLD(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MPDU_DENSITY) {
		ret = mtk_cfg80211_set_mpdu_density(
			mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_AMSDU_EN) {
		if (ctrl_data->u8_val == 0xf)
			return mtk_cfg80211_get_amsdu_status(mac_ad, wdev, ctrl_data);

		ret = mtk_cfg80211_set_amsdu_en(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef DOT11N_DRAFT3
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_CNT_THR) {
		ret = mtk_cfg80211_set_coex_cnt_thr(mac_ad, ctrl_data->u8_val);

		if (ret < 0)
			return ret;
	}
#endif

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_HT_EXT_CHA) {
		ret = mtk_cfg80211_set_ht_extcha(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_HT_PROTECT) {
		ret = mtk_cfg80211_set_ht_protect(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef DOT11_VHT_AC
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_DISALLOW_NON_VHT) {
		ret = mtk_cfg80211_set_disallow_vht(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif

#ifdef MT_MAC
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_ETXBF_EN_COND) {
		ret = mtk_cfg80211_set_etxbf_en_cond(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif

#ifdef DOT11W_PMF_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_PMF_SHA256) {
		ret = mtk_cfg80211_set_pmf_sha256(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BCN_INT) {
		ret = mtk_cfg80211_set_beacon_period(mac_ad, wdev, ctrl_data->ushort_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_DTIM_INT) {
		ret = mtk_cfg80211_set_dtim_period(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_HIDDEN_SSID) {
		ret = mtk_cfg80211_set_hide_ssid(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_HT_OP_MODE) {
		ret = mtk_cfg80211_set_ht_op_mode(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BSS_MAX_IDLE) {
		ret = mtk_cfg80211_set_bss_max_idle_period_proc(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MURU_OFDMA_DL_EN) {
		ret = mtk_cfg80211_Set_MuOfdma_Dl_Enable(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MURU_OFDMA_UL_EN) {
		ret = mtk_cfg80211_Set_MuOfdma_Ul_Enable(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MU_MIMO_DL_EN) {
		ret = mtk_cfg80211_Set_MuMimo_Dl_Enable(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MU_MIMO_UL_EN) {
		ret = mtk_cfg80211_Set_MuMimo_Ul_Enable(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef DOT11_VHT_AC
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_RTS_BW_SIGNALING) {
		ret = mtk_cfg80211_set_vht_bw_signaling(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif

#ifdef WAPP_SUPPORT
#ifdef DOT11_HE_AX
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_AP_BSS_COLOR) {
		struct bss_color *ap_bss_color;
		UINT8 wdev_idx, action, value;

		if (ctrl_data->data_len < sizeof(struct bss_color))
			return -RTMP_IO_EINVAL;

		ap_bss_color = ctrl_data->data;
		wdev_idx = ap_bss_color->wdev_id;
		action = ap_bss_color->action;
		value = ap_bss_color->bss_color_val;

		if (wdev_idx >= WDEV_NUM_MAX) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_WARN,
					"wdev_index is out of range\n");
			return -RTMP_IO_EINVAL;
		}
		if (action > BSS_COLOR_DBG_MAX) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_WARN,
					"action is out of range\n");
			return -RTMP_IO_EINVAL;
		}

		if (value < BSS_COLOR_VALUE_MIN || value > BSS_COLOR_VALUE_MAX) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_COLOR, DBG_LVL_WARN,
					"bss color value is out of range\n");
			return -RTMP_IO_EINVAL;
		}

		set_bss_color_dbg(mac_ad, (UINT8)wdev_idx, (UINT8)action, (UINT8)value);
	}
#endif /* DOT11_HE_AX */
#endif /* WAPP_SUPPORT */

#ifdef DOT11_HE_AX
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_SET_11AXONLY) {
		ret = mtk_cfg80211_set_11axonly(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif /*DOT11_HE_AX*/

#ifdef DOT11_EHT_BE
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_SET_11BEONLY) {
		ret = mtk_cfg80211_set_11beonly(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
#endif /* DOT11_EHT_BE */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_SET_NO_AGMODE) {
		ret = mtk_cfg80211_set_no_agmode(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_SET_NO_NMODE) {
		ret = mtk_cfg80211_set_no_nmode(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_SET_NO_BMODE) {
		ret = mtk_cfg80211_set_no_bmode(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_6G_AFC_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_AFC_PARAM) {
		struct afc_response *afc_resp;

		if (ctrl_data->data_len < sizeof(struct afc_response))
			return -RTMP_IO_EINVAL;
		afc_resp = (struct afc_response *)ctrl_data->data;

		ret = nl80211_afc_daemon_response(mac_ad, afc_resp, ctrl_data->data_len);
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_AFC, DBG_LVL_NOTICE,
				"nl80211_afc_daemon_response----- ret = %d\n", ret);
		if (ret < 0)
			return ret;

	}
#endif /*CONFIG_6G_AFC_SUPPORT*/

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MGMT_RX) {
		ret = mtk_cfg80211_set_mgmt_rx(mac_ad, ctrl_data->u8_val);
		if (ret)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_NO_BCN) {
		ret = mtk_cfg80211_set_no_bcn(mac_ad, wdev, ctrl_data->u8_val);
		if (ret)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_ISOLATION) {
		if (ctrl_data->u8_val == 0xf)
			return mtk_cfg80211_get_bss_isolation_status(mac_ad, wdev, ctrl_data);
		ret = mtk_cfg80211_set_bss_isolate_en(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int cfg80211_drv_vndr_set_radio(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_IEEE80211H_INFO) {
		ret = mtk_cfg80211_set_ieee80211h(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef ACK_CTS_TIMEOUT_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_ACKCTS_TOUT_EN_INFO) {
		ret = mtk_cfg80211_set_ackcts_tout_en(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_DISTANCE_INFO) {
		ret = mtk_cfg80211_set_dst2acktimeout(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_CCK_ACK_TOUT_INFO) {
		ret = mtk_cfg80211_set_cck_ack_tout(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_OFDM_ACK_TOUT_INFO) {
		ret = mtk_cfg80211_set_ofdm_ack_tout(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_OFDMA_ACK_TOUT_INFO) {
		ret = mtk_cfg80211_set_ofdma_ack_tout(mac_ad, ctrl_data->uint_val);
		if (ret < 0)
			return ret;
	}
#endif /* ACK_CTS_TIMEOUT_SUPPORT */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_2G_CSA_SUPPORT) {
		ret = mtk_cfg80211_set_csa_2g(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_CSA_SUPPORT) {
		ret = mtk_cfg80211_set_csa(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

#ifdef CFG_SUPPORT_FALCON_PP
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MRU_INFO) {
		if (ctrl_data->ushort_val == 0xffff) {
			ctrl_data->rsp_data = &mac_ad->CommonCfg.punctured_bitmap;
			ctrl_data->rsp_data_len = sizeof(USHORT);
			ret = 0;
		}
		else
			ret = mtk_cfg80211_set_pp_bitmap(mac_ad, ctrl_data->ushort_val);

		if (ret < 0)
			return ret;
	}
#endif /* CFG_SUPPORT_FALCON_PP */

#ifdef ANDLINK_V4_0
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BSS_MAX_STA_NUM) {
		UINT		apidx = wdev->func_idx;
		UINT max_sta_num = ctrl_data->uint_val;

		if (max_sta_num > 0
			&& apidx < MAX_BEACON_NUM) {
			mac_ad->ApCfg.MBSSID[apidx].MaxStaNum = max_sta_num;
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"IF(rai%d) (MaxStaNum=%d) max=%p\n", apidx,
				mac_ad->ApCfg.MBSSID[apidx].MaxStaNum,
				mac_ad->ApCfg.MBSSID[apidx].MaxStaNum);
			return 0;
		}
	}
#endif/*ANDLINK_V4_0*/
	return 0;
}

int cfg80211_drv_vndr_set_cosr_bcn_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	return -RTMP_IO_EOPNOTSUPP;

}

int cfg80211_drv_vndr_get_bss_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_BSS_STATS) {
		ret = mtk_cfg80211_reply_bss_stats(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_ssid_stats failed. ret=%x.\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}

	return ret;
}

#endif /* CONFIG_AP_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */


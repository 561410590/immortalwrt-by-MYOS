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

#include "rt_config.h"
#include "mgmt/be_internal.h"

#define BSSID_WCID_TO_REMOVE 1 /* Pat:TODO */


/*******************************************************************************
 *                P R I V A T E    F U N C T I O N S
 *******************************************************************************
 */

static void cfg80211_drv_get_band_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_band *band_info)
{
	struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(mac_ad->hdev_ctrl);

	if (band_info) {
		if (WMODE_CAP_6G(wdev->PhyMode))
			band_info->phy_mode = MTK_PHY_MODE_6G;
		else if (WMODE_CAP_2G(wdev->PhyMode))
			band_info->phy_mode = MTK_PHY_MODE_2G;
		else if (WMODE_CAP_5G(wdev->PhyMode))
			band_info->phy_mode = MTK_PHY_MODE_5G;
		CFG80211_BANDINFO_FILL(mac_ad, wdev, band_info);
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_24G))
			band_info->phy_caps |= MTK_PHY_CAP_RFIC_24GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_5G))
			band_info->phy_caps |= MTK_PHY_CAP_RFIC_5GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_6G))
			band_info->phy_caps |= MTK_PHY_CAP_RFIC_6GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_HT))
			band_info->phy_caps |= MTK_PHY_CAP_HT;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_VHT))
			band_info->phy_caps |= MTK_PHY_CAP_VHT;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_HE))
			band_info->phy_caps |= MTK_PHY_CAP_HE;
		if (PD_GET_GEN_DOWN(mac_ad->physical_dev) == false) {
			if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_EHT))
				band_info->phy_caps |= MTK_PHY_CAP_EHT;
		}
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160C_STD))
			band_info->phy_caps |= MTK_PHY_CAP_BW160C_STD;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160NC))
			band_info->phy_caps |= MTK_PHY_CAP_BW160NC;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160C))
			band_info->phy_caps |= MTK_PHY_CAP_BW160C;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW80))
			band_info->phy_caps |= MTK_PHY_CAP_BW80;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW320))
			band_info->phy_caps |= MTK_PHY_CAP_BW320;
#ifdef NON_WIFI_FREQ_5MHZ_SHIFT_SUPPORT
		band_info->phy_caps |= MTK_PHY_CAP_5M_SHIFT;
#endif
	}
}


static void cfg80211_drv_unregister(
	struct _RTMP_ADAPTER *mac_ad)
{
	PCFG80211_CTRL pCfg80211_ctrl;

	if (!mac_ad) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
		"==========> mac_ad is null <==========\n");
		return;
	}
	pCfg80211_ctrl = &mac_ad->cfg80211_ctrl;
	/* Reset CFG80211 Global Setting Here */
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
		"==========> TYPE Reset CFG80211 Global Setting Here <==========\n");
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = FALSE;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = FALSE;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;
	mac_ad->CommonCfg.HT_Disable = 0;

	/* It should be free when ScanEnd, */
	/*  But Hit close the device in Scanning */
	if (pCfg80211_ctrl->pCfg80211ChanList != NULL) {
		os_free_mem(pCfg80211_ctrl->pCfg80211ChanList);
		pCfg80211_ctrl->pCfg80211ChanList = NULL;
	}

	pCfg80211_ctrl->Cfg80211ChanListLen = 0;
	pCfg80211_ctrl->Cfg80211CurChanIndex = 0;

	if (pCfg80211_ctrl->pExtraIe) {
		os_free_mem(pCfg80211_ctrl->pExtraIe);
		pCfg80211_ctrl->pExtraIe = NULL;
	}

	pCfg80211_ctrl->ExtraIeLen = 0;
#ifdef CONFIG_AP_SUPPORT
	if (mac_ad->cfg80211_ctrl.beacon_tail_buf != NULL) {
		os_free_mem(mac_ad->cfg80211_ctrl.beacon_tail_buf);
		mac_ad->cfg80211_ctrl.beacon_tail_buf = NULL;
	}

	mac_ad->cfg80211_ctrl.beacon_tail_len = 0;
#endif /* CONFIG_AP_SUPPORT */

	if (mac_ad->cfg80211_ctrl.BeaconExtraIe != NULL) {
		os_free_mem(mac_ad->cfg80211_ctrl.BeaconExtraIe);
		mac_ad->cfg80211_ctrl.BeaconExtraIe = NULL;
	}
	mac_ad->cfg80211_ctrl.BeaconExtraIeLen = 0;
}

static NDIS_802_11_AUTHENTICATION_MODE cfg80211_drv_get_auth(UINT8 auth)
{
	switch (auth) {
	case MTK_AUTH_OPEN:
		return Ndis802_11AuthModeOpen;
	case MTK_AUTH_SHARED:
		return Ndis802_11AuthModeShared;
	case MTK_AUTH_AUTO_SWITCH:
		return Ndis802_11AuthModeAutoSwitch;
	case MTK_AUTH_WPA:
		return Ndis802_11AuthModeWPA;
	case MTK_AUTH_WPAPSK:
		return Ndis802_11AuthModeWPAPSK;
	case MTK_AUTH_WPANone:
		return Ndis802_11AuthModeWPANone;
	case MTK_AUTH_WPA2:
		return Ndis802_11AuthModeWPA2;
	case MTK_AUTH_WPA2PSK:
		return Ndis802_11AuthModeWPA2PSK;
	case MTK_AUTH_WPA1WPA2:
		return Ndis802_11AuthModeWPA1WPA2;
	case MTK_AUTH_WPA1PSKWPA2PSK:
		return Ndis802_11AuthModeWPA1PSKWPA2PSK;
#if defined(DOT11_SAE_SUPPORT) || defined(SUPP_SAE_SUPPORT)
	case MTK_AUTH_SAE:
		return Ndis802_11AuthModeSAE;
#endif /* DOT11_SAE_SUPPORT && SUPP_SAE_SUPPORT */
	default:
		return Ndis802_11AuthModeMax;
	}
}

/********************************************************************************/

extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      RTMP_ADAPTER * pAd,
	IN      VOID * pData,
	IN      ULONG                            Data);

extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      RTMP_ADAPTER * pAd,
	IN      VOID * pData,
	IN      ULONG                            Data);


INT CFG80211DRV_IoctlHandle(
	IN	VOID * pAdSrc,
	IN	RTMP_IOCTL_INPUT_STRUCT * wrq,
	IN	INT						cmd,
	IN	USHORT					subcmd,
	IN	VOID * pData,
	IN	ULONG					Data)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdSrc;
	POS_COOKIE pObj = (POS_COOKIE)pAd->OS_Cookie;
	struct wifi_dev *wdev = get_wdev_by_ioctl_idx_and_iftype(pAd, pObj->ioctl_if, pObj->ioctl_if_type);


	switch (cmd) {
	case CMD_RTPRIV_IOCTL_80211_START:
	case CMD_RTPRIV_IOCTL_80211_END:
		/* nothing to do */
		break;

	case CMD_RTPRIV_IOCTL_80211_SCAN:
		if (CFG80211DRV_OpsScanCheckStatus(pAd, Data) != TRUE)
			return NDIS_STATUS_FAILURE;

		break;

	case CMD_RTPRIV_IOCTL_80211_SCAN_STATUS_LOCK_INIT:
		CFG80211_ScanStatusLockInit(pAd, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_IBSS_JOIN:
		CFG80211DRV_OpsJoinIbss(pAd, pData);
		break;

	case CMD_RTPRIV_IOCTL_80211_STA_KEY_ADD:
#ifdef APCLI_CFG80211_SUPPORT
		CFG80211DRV_ApClientKeyAdd(pAd, pData);
#else
		CFG80211DRV_StaKeyAdd(pAd, pData);
#endif /* APCLI_CFG80211_SUPPORT */
		break;
#ifdef CONFIG_STA_SUPPORT

	case CMD_RTPRIV_IOCTL_80211_STA_KEY_DEFAULT_SET:
		CFG80211_setStaDefaultKey(pAd, Data);
		break;
#endif /*CONFIG_STA_SUPPORT*/

#ifdef SUPP_SAE_SUPPORT
	case CMD_RTPRIV_IOCTL_80211_EXT_CONNECT: {
		UINT8 staIndex = Data;
		SECURITY_CONFIG *pSecConfig;
		struct wifi_dev *pWdev;
		STA_ADMIN_CONFIG *pstacfg = &pAd->StaCfg[staIndex];
		CMD_RTPRIV_IOCTL_80211_CONNECT_PARAM *pConnParam;
		pConnParam = (CMD_RTPRIV_IOCTL_80211_CONNECT_PARAM *)pData;
		pWdev = &pAd->StaCfg[staIndex].wdev;
		pSecConfig = &pWdev->SecConfig;

		if (pConnParam->psk_len < 65) {
			os_move_mem(pSecConfig->PSK, pConnParam->psk, pConnParam->psk_len);
			pstacfg->sae_cfg_group = (UCHAR)pConnParam->sae_group;
			pSecConfig->PSK[pConnParam->psk_len] = '\0';
		}
	}
		break;
#endif
	case CMD_RTPRIV_IOCTL_80211_REG_NOTIFY_TO:
		CFG80211DRV_RegNotify(pAd, pData);
		break;


	case CMD_RTPRIV_IOCTL_80211_BANDINFO_GET: {
		struct mt_cfg80211_band *pBandInfo = (struct mt_cfg80211_band *)pData;
		struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(pAd->hdev_ctrl);

		CFG80211_BANDINFO_FILL(pAd, wdev, pBandInfo);

		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_24G))
			pBandInfo->RFICType |= RFIC_24GHZ;

		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_5G))
			pBandInfo->RFICType |= RFIC_5GHZ;

		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_6G))
			pBandInfo->RFICType |= RFIC_6GHZ;

		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_24G))
			pBandInfo->phy_caps |= MTK_PHY_CAP_RFIC_24GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_5G))
			pBandInfo->phy_caps |= MTK_PHY_CAP_RFIC_5GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_6G))
			pBandInfo->phy_caps |= MTK_PHY_CAP_RFIC_6GHZ;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_HT))
			pBandInfo->phy_caps |= MTK_PHY_CAP_HT;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_VHT))
			pBandInfo->phy_caps |= MTK_PHY_CAP_VHT;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_HE))
			pBandInfo->phy_caps |= MTK_PHY_CAP_HE;
		if (PD_GET_GEN_DOWN(pAd->physical_dev) == false) {
			if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_EHT))
				pBandInfo->phy_caps |= MTK_PHY_CAP_EHT;
		}
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160C_STD))
			pBandInfo->phy_caps |= MTK_PHY_CAP_BW160C_STD;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160NC))
			pBandInfo->phy_caps |= MTK_PHY_CAP_BW160NC;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW160C))
			pBandInfo->phy_caps |= MTK_PHY_CAP_BW160C;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW80))
			pBandInfo->phy_caps |= MTK_PHY_CAP_BW80;
		if (IS_PHY_CAPS(cap->phy_caps, fPHY_CAP_BW320))
			pBandInfo->phy_caps |= MTK_PHY_CAP_BW320;
	}
	break;

	case CMD_RTPRIV_IOCTL_80211_SURVEY_GET:
		CFG80211DRV_SurveyGet(pAd, pData);
		break;

#ifdef APCLI_CFG80211_SUPPORT
	case CMD_RTPRIV_IOCTL_APCLI_SITE_SURVEY:
		CFG80211DRV_ApcliSiteSurvey(pAd, pData);
		break;
#endif

	case CMD_RTPRIV_IOCTL_AP_SITE_SURVEY:
		CFG80211DRV_ApSiteSurvey(pAd, pData);
		break;

	case CMD_RTPRIV_IOCTL_80211_EXTRA_IES_SET:
		CFG80211DRV_OpsScanExtraIesSet(pAd);
		break;

	case CMD_RTPRIV_IOCTL_80211_CHANNEL_LOCK:
		CFG80211_SwitchTxChannel(pAd, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_CHANNEL_RESTORE:
		break;

	case CMD_RTPRIV_IOCTL_80211_MGMT_FRAME_SEND:
		CFG80211_SendMgmtFrame(pAd, pData, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_CHANNEL_LIST_SET:
		return CFG80211DRV_OpsScanSetSpecifyChannel(pAd, pData, Data);
#ifdef CONFIG_AP_SUPPORT
	case CMD_RTPRIV_IOCTL_80211_BEACON_DEL: {
#ifndef DISABLE_HOSTAPD_BEACON
			if (pAd->cfg80211_ctrl.beacon_tail_buf != NULL) {
				os_free_mem(pAd->cfg80211_ctrl.beacon_tail_buf);
				pAd->cfg80211_ctrl.beacon_tail_buf = NULL;
			}
			pAd->cfg80211_ctrl.beacon_tail_len = 0;
#endif
	}
	break;

	case CMD_RTPRIV_IOCTL_80211_AP_KEY_DEL:
		CFG80211DRV_ApKeyDel(pAd, pData);
		break;

	case CMD_RTPRIV_IOCTL_80211_AP_KEY_DEFAULT_SET:
		CFG80211_setApDefaultKey(pAd, pData, Data);
		break;
#ifdef BCN_PROTECTION_SUPPORT
	case CMD_RTPRIV_IOCTL_80211_AP_BEACON_KEY_DEFAULT_SET:
		CFG80211_setApBeaconDefaultKey(pAd, pData, Data);
		break;
#endif /*BCN_PROTECTION_SUPPORT*/

#ifdef DOT11W_PMF_SUPPORT
	case CMD_RTPRIV_IOCTL_80211_AP_KEY_DEFAULT_MGMT_SET:
		CFG80211_setApDefaultMgmtKey(pAd, pData, Data);
		break;
#endif /*DOT11W_PMF_SUPPORT*/

#endif /* CONFIG_AP_SUPPORT */

	case CMD_RTPRIV_IOCTL_80211_AP_PROBE_RSP_EXTRA_IE:
		break;

	case CMD_RTPRIV_IOCTL_80211_MCAST_RATE_SET:
		CFG80211DRV_OpsMcastRateParm(pAd, pData, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_BSS_COLOR_CHANGE:
		CFG80211DRV_OpsBssColorParm(pAd, pData, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_FT_IES_UPDATE:
		CFG80211DRV_OpsFtIesParm(pAd, pData, Data);
		break;

	case CMD_RTPRIV_IOCTL_80211_RESET:
		CFG80211_reSetToDefault(pAd);
		break;

	case CMD_RTPRIV_IOCTL_80211_NETDEV_EVENT: {
		/*
		 * CFG_TODO: For Scan_req per netdevice
		 * PNET_DEV pNetDev = (PNET_DEV) pData;
		 * struct wireless_dev *pWdev = pAd->pCfg80211_CB->pCfg80211_Wdev;
		 * if (RTMPEqualMemory(pNetDev->dev_addr, pNewNetDev->dev_addr, MAC_ADDR_LEN))
		 */
		if (CFG80211DRV_OpsScanRunning(pAd)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
				"CFG_SCAN: close the scan cmd in device close phase\n");
			CFG80211OS_ScanEnd(pAd, TRUE);
		}
	}
	break;
#if defined(CONFIG_STA_SUPPORT) || defined(APCLI_CFG80211_SUPPORT)

	case CMD_RTPRIV_IOCTL_80211_P2PCLI_ASSSOC_IE_SET: {
		CMD_RTPRIV_IOCTL_80211_ASSOC_IE *pAssocIe;

		pAssocIe = (CMD_RTPRIV_IOCTL_80211_ASSOC_IE *)pData;
#ifdef APCLI_CFG80211_SUPPORT
		CFG80211DRV_SetApCliAssocIe(pAd, pAssocIe->pNetDev, pAssocIe->ie, pAssocIe->ie_len);
#else
		RTMP_DRIVER_80211_GEN_IE_SET(pAd, pAssocIe->ie, pAssocIe->ie_len);
#endif
		break;
	}
#endif /*CONFIG_STA_SUPPORT || APCLI_CFG80211_SUPPORT*/

	case CMD_RTPRIV_IOCTL_80211_ANTENNA_CTRL:
	{
		BOOLEAN is_get = (Data == 1) ? TRUE : FALSE;
		struct mt_priv_ops_antenna_info *ant_cnt = (struct mt_priv_ops_antenna_info *)pData;
		struct mcs_nss_caps *nss_cap = MCS_NSS_CAP(pAd);

		if (is_get) {
			if (OPSTATUS_TEST_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED) &&
				(wdev != NULL) && (wdev->if_up_down_state == TRUE)) {
#ifdef DOT11_EHT_BE
				if (WMODE_CAP_BE(wdev->PhyMode)) {
					ant_cnt->rx_ant = wlan_config_get_eht_rx_nss(wdev);
					ant_cnt->tx_ant = wlan_config_get_eht_tx_nss(wdev);
				} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
				if (WMODE_CAP_AX(wdev->PhyMode)) {
					ant_cnt->rx_ant = wlan_config_get_he_rx_nss(wdev);
					ant_cnt->tx_ant = wlan_config_get_he_tx_nss(wdev);
				} else
#endif /* DOT11_HE_AX */
				{
					ant_cnt->tx_ant = wlan_config_get_tx_stream(wdev);
					ant_cnt->rx_ant = wlan_config_get_rx_stream(wdev);
				}
			} else {
				UINT tx_max_nss;
				UINT rx_max_nss;

				tx_max_nss = nss_cap->max_nss;
				rx_max_nss = nss_cap->max_nss;
				ant_cnt->tx_ant = tx_max_nss;
				ant_cnt->rx_ant = rx_max_nss;
			}
		} else {
			UINT8 Txstream = ant_cnt->tx_ant;
			UINT8 Rxstream = ant_cnt->rx_ant;

			if (wdev == NULL) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
						"wdev is null, return!!!\n");
				return NDIS_STATUS_FAILURE;
			}

			if (Txstream != Rxstream) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
						"Wrong Input: Tx & Rx Antenna number different!\n");
				return NDIS_STATUS_FAILURE;
			}

			if (Txstream > nss_cap->max_nss) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
						"Wrong Input: More than max ant cap (%d)!!\n", nss_cap->max_nss);
				return NDIS_STATUS_FAILURE;
			}
#ifdef ANTENNA_CONTROL_SUPPORT
			pAd->TxStream  = Txstream;
			pAd->RxStream  = Rxstream;
			pAd->bAntennaSetAPEnable = 1;
#endif /* ANTENNA_CONTROL_SUPPORT */

			wlan_config_set_tx_stream(wdev, min(Txstream, nss_cap->max_nss));
			wlan_config_set_rx_stream(wdev, min(Rxstream, nss_cap->max_nss));
			wlan_operate_set_tx_stream(wdev, min(Txstream, nss_cap->max_nss));
			wlan_operate_set_rx_stream(wdev, min(Rxstream, nss_cap->max_nss));
#ifdef DOT11_HE_AX
			wlan_config_set_he_tx_nss(wdev, min(Txstream, nss_cap->max_nss));
			wlan_config_set_he_rx_nss(wdev, min(Rxstream, nss_cap->max_nss));
#endif /* DOT11_HE_AX */
#ifdef DOT11_EHT_BE
			wlan_config_set_eht_tx_nss(wdev, min(Txstream, nss_cap->max_nss));
			wlan_config_set_eht_rx_nss(wdev, min(Rxstream, nss_cap->max_nss));
#endif /* DOT11_EHT_BE */

			SetCommonHtVht(pAd, wdev);
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
				APStop(pAd, NULL, AP_BSS_OPER_ALL);
				APStartUp(pAd, NULL, AP_BSS_OPER_ALL);
			}
#endif /* CONFIG_AP_SUPPORT */
		}
	}
	break;

	default:
		return NDIS_STATUS_FAILURE;
	}

	return NDIS_STATUS_SUCCESS;
}

static VOID CFG80211DRV_OpsMgmtFrameProbeRegister(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	BOOLEAN isReg)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

#if defined(APCLI_CFG80211_SUPPORT)
	if (wdev->wdev_type == WDEV_TYPE_STA) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_DEBUG,
			"Do not register or unreg probe Req for station intf\n");
		return;
	}
#endif /* APCLI_CFG80211_SUPPORT */

	/* IF Not Exist on VIF List, the device must be MAIN_DEV */
	if (isReg) {
		if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount < 255)
			pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount++;
	} else {
		if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount > 0)
			pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount--;
	}

	if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount > 0)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = TRUE;
	else {
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = FALSE;
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;
	}

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_DEBUG,
		"[%d] pAd->Cfg80211RegisterProbeReqFrame=%d[%d]\n",
			 isReg, pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame,
			 pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount);
}

static VOID CFG80211DRV_OpsMgmtFrameActionRegister(
	struct _RTMP_ADAPTER *pAd,
	BOOLEAN isReg)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

	/* IF Not Exist on VIF List, the device must be MAIN_DEV */
	if (isReg) {
		if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount < 255)
			pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount++;
	} else {
		if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount > 0)
			pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount--;
	}

	if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount > 0)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = TRUE;
	else {
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = FALSE;
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;
	}

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_DEBUG,
		"[%d] TYPE pAd->Cfg80211RegisterActionFrame=%d[%d]\n",
			 isReg, pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame,
			 pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount);
}


static int CFG80211DRV_OpsBitRateParm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int apidx = MAX_BEACON_NUM;
	UCHAR band = 0;
#ifdef CONFIG_RA_PHY_RATE_SUPPORT
	INT i = 0;
	UCHAR rate[] = { 0x82, 0x84, 0x8b, 0x96, 0x8C, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c};
	struct legacy_rate *eap_legacy_rate = NULL;
	UCHAR tx_stream = 0;
	UCHAR ch_band;
#endif /* CONFIG_RA_PHY_RATE_SUPPORT */
	struct mt_cfg80211_bit_rate_info *bit_rate_info;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev)
		apidx = wdev->func_idx;
	if (apidx < 0 || apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"invalid apidx:%d\n", apidx);
		return 0;
	}
	bit_rate_info = (struct mt_cfg80211_bit_rate_info *)ctrl_data->data;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
			"[apidx:%d] he_gi=%d, he_ltf=%d, legacy=%d\n",
			apidx, bit_rate_info->he_gi, bit_rate_info->he_ltf, bit_rate_info->legacy);
	if (mac_ad->CommonCfg.wifi_cert) {
		u8 he_gi = bit_rate_info->he_gi;
		u8 he_ltf = bit_rate_info->he_ltf;

		/* 0-2G; 1-5G; 2-6G */
		band = wlan_operate_get_ch_band(wdev);
		if (band > CMD_CH_BAND_6G) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"invalid value: band = %d\n", band);
			return 0;
		}

		if (he_gi < CAPI_HE_GI_NUM) {
			ap_set_rfeature_he_gi(mac_ad, (VOID *)&he_gi);
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
				"band = %d, update to he_gi = %d\n",
				band, he_gi);
		}

		if (he_ltf < CAPI_HE_LTF_NUM) {
			ap_set_rfeature_he_ltf(mac_ad, (VOID *)&he_ltf);
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
				"band = %d, update to he_ltf = %d\n",
				band, he_ltf);
		}
	}
#ifdef CONFIG_RA_PHY_RATE_SUPPORT
	else {
		eap_legacy_rate = &wdev->eap.eap_legacy_rate;
		ch_band = wlan_config_get_ch_band(wdev);

		if (ch_band == CMD_CH_BAND_5G || ch_band == CMD_CH_BAND_6G) {
			/* legacy mode*/
			if ((bit_rate_info->legacy == 0) || (bit_rate_info->legacy == 0xff)) {
				wdev->eap.eap_suprate_en = FALSE;
				RTMPUpdateRateInfo(wdev->PhyMode, &wdev->rate);
			} else {
				wdev->eap.eap_suprate_en = TRUE;
				eap_legacy_rate->sup_rate_len = 0;
				eap_legacy_rate->ext_rate_len = 0;

				for (i = 0; i < 8; i++) {
					if (bit_rate_info->legacy & (1 << i)) {
						eap_legacy_rate->sup_rate[eap_legacy_rate->sup_rate_len] = rate[i+4];
						eap_legacy_rate->sup_rate_len++;
						wdev->eap.eapsupportofdmmcs |= (1 << i);
						wdev->eap.eapsupportratemode |= SUPPORT_OFDM_MODE;
					}
				}
				rtmpeapupdaterateinfo(wdev->PhyMode, &wdev->rate, &wdev->eap);
			}
		} else {
			if ((bit_rate_info->legacy == 0) || (bit_rate_info->legacy == 0xfff)) {
				wdev->eap.eap_suprate_en = FALSE;
				RTMPUpdateRateInfo(wdev->PhyMode, &wdev->rate);
			} else {
				wdev->eap.eap_suprate_en = TRUE;
				eap_legacy_rate->sup_rate_len = 0;
				eap_legacy_rate->ext_rate_len = 0;
				for (i = 0; i < MAX_LEN_OF_SUPPORTED_RATES; i++) {
					if (bit_rate_info->legacy & (1 << i)) {
						if (WMODE_EQUAL(wdev->PhyMode, WMODE_B) && (wdev->channel <= 14)) {
							eap_legacy_rate->sup_rate[eap_legacy_rate->sup_rate_len] = rate[i];
							eap_legacy_rate->sup_rate_len++;
							wdev->eap.eapsupportcckmcs |= (1 << i);
							wdev->eap.eapsupportratemode |= SUPPORT_CCK_MODE;
						} else {
							if ((i < 4) || (i == 5) || (i == 7) || (i == 9) || (i == 11)) {
								eap_legacy_rate->sup_rate[eap_legacy_rate->sup_rate_len]
														= rate[i];
								eap_legacy_rate->sup_rate_len++;
								if (i < 4) {
									wdev->eap.eapsupportcckmcs |= (1 << i);
									wdev->eap.eapsupportratemode |= SUPPORT_CCK_MODE;
								} else {
									wdev->eap.eapsupportofdmmcs |= (1 << (i - 4));
									wdev->eap.eapsupportratemode |= SUPPORT_OFDM_MODE;
								}
							} else {
								eap_legacy_rate->ext_rate[eap_legacy_rate->ext_rate_len]
													= rate[i] & 0x7f;
								eap_legacy_rate->ext_rate_len++;
								wdev->eap.eapsupportofdmmcs |= (1 << (i - 4));
								wdev->eap.eapsupportratemode |= SUPPORT_OFDM_MODE;
							}
						}
				    }
				}
				rtmpeapupdaterateinfo(wdev->PhyMode, &wdev->rate, &wdev->eap);
			}
		}

		tx_stream = wlan_config_get_tx_stream(wdev);
		if (tx_stream == 1) {
			/* HT mode*/
			if ((bit_rate_info->ht_mcs[0] == 0) || (bit_rate_info->ht_mcs[0] == 0xff)) {
				wdev->eap.eap_htsuprate_en = FALSE;
			} else {
				wdev->eap.eap_htsuprate_en = TRUE;
				wdev->eap.eapmcsset[0] = bit_rate_info->ht_mcs[0];
				wdev->eap.eapmcsset[1] = 0;
				wdev->eap.eapmcsset[2] = 0;
				wdev->eap.eapmcsset[3] = 0;
			}
			/* VHT mode*/
			if ((bit_rate_info->vht_mcs[0] == 0) || (bit_rate_info->vht_mcs[0] == 0x3ff)) {
				wdev->eap.eap_vhtsuprate_en = FALSE;
			} else {
				wdev->eap.eap_vhtsuprate_en = TRUE;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3)
					wdev->eap.rx_mcs_map.mcs_ss1 = 2;
				else
					wdev->eap.rx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
				wdev->eap.rx_mcs_map.mcs_ss2 = 0;
				wdev->eap.rx_mcs_map.mcs_ss3 = 0;
				wdev->eap.rx_mcs_map.mcs_ss4 = 0;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3)
					wdev->eap.tx_mcs_map.mcs_ss1 = 2;
				else
					wdev->eap.tx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
				wdev->eap.tx_mcs_map.mcs_ss2 = 0;
				wdev->eap.tx_mcs_map.mcs_ss3 = 0;
				wdev->eap.tx_mcs_map.mcs_ss4 = 0;
			}
		} else if (tx_stream == 2) {
			/* HT mode*/
			if (((bit_rate_info->ht_mcs[0] == 0) && (bit_rate_info->ht_mcs[1] == 0))
				|| ((bit_rate_info->ht_mcs[0] == 0xff) && (bit_rate_info->ht_mcs[1] == 0xff))) {
				wdev->eap.eap_htsuprate_en = FALSE;
			} else {
				wdev->eap.eap_htsuprate_en = TRUE;
				wdev->eap.eapmcsset[0] = bit_rate_info->ht_mcs[0];
				wdev->eap.eapmcsset[1] = bit_rate_info->ht_mcs[1];
				wdev->eap.eapmcsset[2] = 0;
				wdev->eap.eapmcsset[3] = 0;
			}
			/* VHT mode*/
			if (((bit_rate_info->vht_mcs[0] == 0) && (bit_rate_info->vht_mcs[1] == 0))
					|| ((bit_rate_info->vht_mcs[0] == 0x3ff)
						&& (bit_rate_info->vht_mcs[1] == 0x3ff))) {
				wdev->eap.eap_vhtsuprate_en = FALSE;
			} else {
				wdev->eap.eap_vhtsuprate_en = TRUE;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3)
					wdev->eap.rx_mcs_map.mcs_ss1 = 2;
				else
					wdev->eap.rx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;

				if (((bit_rate_info->vht_mcs[1] & 0x00000300) >> 8) == 3)
					wdev->eap.rx_mcs_map.mcs_ss2 = 2;
				else
					wdev->eap.rx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
				wdev->eap.rx_mcs_map.mcs_ss3 = 0;
				wdev->eap.rx_mcs_map.mcs_ss4 = 0;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3)
					wdev->eap.tx_mcs_map.mcs_ss1 = 2;
				else
					wdev->eap.tx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;

				if (((bit_rate_info->vht_mcs[1] & 0x00000300) >> 8) == 3)
					wdev->eap.tx_mcs_map.mcs_ss2 = 2;
				else
					wdev->eap.tx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
				wdev->eap.tx_mcs_map.mcs_ss3 = 0;
				wdev->eap.tx_mcs_map.mcs_ss4 = 0;
			}
		} else if (tx_stream == 3) {
			/* HT mode*/
			if (((bit_rate_info->ht_mcs[0] == 0) && (bit_rate_info->ht_mcs[1] == 0)
				&& (bit_rate_info->ht_mcs[2] == 0))
				|| ((bit_rate_info->ht_mcs[0] == 0xff) && (bit_rate_info->ht_mcs[1] == 0xff)
				&& (bit_rate_info->ht_mcs[2] == 0xff))) {
				wdev->eap.eap_htsuprate_en = FALSE;
			} else {
				wdev->eap.eap_htsuprate_en = TRUE;
				wdev->eap.eapmcsset[0] = bit_rate_info->ht_mcs[0];
				wdev->eap.eapmcsset[1] = bit_rate_info->ht_mcs[1];
				wdev->eap.eapmcsset[2] = bit_rate_info->ht_mcs[2];
				wdev->eap.eapmcsset[3] = 0;
			}
			/* VHT mode*/
			if (((bit_rate_info->vht_mcs[0] == 0) && (bit_rate_info->vht_mcs[1] == 0)
				 && (bit_rate_info->vht_mcs[2] == 0))
				|| ((bit_rate_info->vht_mcs[0] == 0x3ff) && (bit_rate_info->vht_mcs[1] == 0x3ff)
				 && (bit_rate_info->vht_mcs[2] == 0x3ff))) {
				wdev->eap.eap_vhtsuprate_en = FALSE;
			} else {
				wdev->eap.eap_vhtsuprate_en = TRUE;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss1 = 2;
					wdev->eap.tx_mcs_map.mcs_ss1 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
				}

				if (((bit_rate_info->vht_mcs[1] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss2 = 2;
					wdev->eap.tx_mcs_map.mcs_ss2 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
				}

				if (((bit_rate_info->vht_mcs[2] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss3 = 2;
					wdev->eap.tx_mcs_map.mcs_ss3 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss3 = (bit_rate_info->vht_mcs[2] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss3 = (bit_rate_info->vht_mcs[2] & 0x00000300) >> 8;
				}

				wdev->eap.rx_mcs_map.mcs_ss4 = 0;
				wdev->eap.tx_mcs_map.mcs_ss4 = 0;
			}
		} else if (tx_stream == 4) {
			/* HT mode*/
			if (((bit_rate_info->ht_mcs[0] == 0) && (bit_rate_info->ht_mcs[1] == 0)
				  && (bit_rate_info->ht_mcs[2] == 0) && (bit_rate_info->ht_mcs[3] == 0))
				|| ((bit_rate_info->ht_mcs[0] == 0xff) && (bit_rate_info->ht_mcs[1] == 0xff)
				  && (bit_rate_info->ht_mcs[2] == 0xff) && (bit_rate_info->ht_mcs[3] == 0xff))) {
				wdev->eap.eap_htsuprate_en = FALSE;
			} else {
				wdev->eap.eap_htsuprate_en = TRUE;
				wdev->eap.eapmcsset[0] = bit_rate_info->ht_mcs[0];
				wdev->eap.eapmcsset[1] = bit_rate_info->ht_mcs[1];
				wdev->eap.eapmcsset[2] = bit_rate_info->ht_mcs[2];
				wdev->eap.eapmcsset[3] = bit_rate_info->ht_mcs[3];
			}
			/* VHT mode*/
			if (((bit_rate_info->vht_mcs[0] == 0) && (bit_rate_info->vht_mcs[1] == 0)
				  && (bit_rate_info->vht_mcs[2] == 0) && (bit_rate_info->vht_mcs[3] == 0))
				|| ((bit_rate_info->vht_mcs[0] == 0x3ff) && (bit_rate_info->vht_mcs[1] == 0x3ff)
				  && (bit_rate_info->vht_mcs[2] == 0x3ff) && (bit_rate_info->vht_mcs[3] == 0x3ff))) {
				wdev->eap.eap_vhtsuprate_en = FALSE;
			} else {
				wdev->eap.eap_vhtsuprate_en = TRUE;
				if (((bit_rate_info->vht_mcs[0] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss1 = 2;
					wdev->eap.tx_mcs_map.mcs_ss1 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss1 = (bit_rate_info->vht_mcs[0] & 0x00000300) >> 8;
				}

				if (((bit_rate_info->vht_mcs[1] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss2 = 2;
					wdev->eap.tx_mcs_map.mcs_ss2 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss2 = (bit_rate_info->vht_mcs[1] & 0x00000300) >> 8;
				}

				if (((bit_rate_info->vht_mcs[2] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss3 = 2;
					wdev->eap.tx_mcs_map.mcs_ss3 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss3 = (bit_rate_info->vht_mcs[2] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss3 = (bit_rate_info->vht_mcs[2] & 0x00000300) >> 8;
				}

				if (((bit_rate_info->vht_mcs[3] & 0x00000300) >> 8) == 3) {
					wdev->eap.rx_mcs_map.mcs_ss4 = 2;
					wdev->eap.tx_mcs_map.mcs_ss4 = 2;
				} else {
					wdev->eap.rx_mcs_map.mcs_ss4 = (bit_rate_info->vht_mcs[3] & 0x00000300) >> 8;
					wdev->eap.tx_mcs_map.mcs_ss4 = (bit_rate_info->vht_mcs[3] & 0x00000300) >> 8;
				}
			}
		}
#ifdef DOT11_N_SUPPORT
		SetCommonHtVht(mac_ad, wdev);
#endif /* DOT11_N_SUPPORT */

		UpdateBeaconHandler(mac_ad, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
	}
#endif /* CONFIG_RA_PHY_RATE_SUPPORT */
	return 0;
}

VOID CFG80211DRV_OpsMcastRateParm(
	VOID *pAdOrg,
	VOID *pData,
	INT apidx)
{
#ifdef MCAST_RATE_SPECIFIC
	PRTMP_ADAPTER pAd = NULL;
	struct wifi_dev *wdev = NULL;
	union _HTTRANSMIT_SETTING *transmit, *pMcastTransmit;
	BSS_INFO_ARGUMENT_T bss_info_argument;

	if (apidx < 0 || apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"invalid apidx:%d\n", apidx);
		return;
	}

	pAd = (PRTMP_ADAPTER)pAdOrg;
	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	transmit = (union _HTTRANSMIT_SETTING *)pData;

	pMcastTransmit = &wdev->rate.mcastphymode;
	pMcastTransmit->field.MODE = transmit->field.MODE;
	pMcastTransmit->field.MCS = transmit->field.MCS;
	pMcastTransmit->field.BW = transmit->field.BW;

	NdisZeroMemory(&bss_info_argument, sizeof(BSS_INFO_ARGUMENT_T));
	bss_info_argument.bss_state = BSS_ACTIVE;
	bss_info_argument.ucBssIndex = wdev->bss_info_argument.ucBssIndex;
	bss_info_argument.u8BssInfoFeature = BSS_INFO_BROADCAST_INFO_FEATURE;
	NdisMoveMemory(&bss_info_argument.BcTransmit, pMcastTransmit, sizeof(union _HTTRANSMIT_SETTING));
	NdisMoveMemory(&bss_info_argument.McTransmit, pMcastTransmit, sizeof(union _HTTRANSMIT_SETTING));
	bss_info_argument.mloFixedRateIdx = wdev->bss_info_argument.mloFixedRateIdx;
	bss_info_argument.priv = wdev;

	if (AsicBssInfoUpdate(pAd, &bss_info_argument) != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"Fail to apply the bssinfo, BSSID=%d!\n", apidx);
		return;
	}
#endif /* MCAST_RATE_SPECIFIC */
}

VOID CFG80211DRV_OpsBssColorParm(
	VOID *pAdOrg,
	VOID *pData,
	INT apidx)
{
#ifdef DOT11_HE_AX
	PRTMP_ADAPTER pAd = NULL;
	struct wifi_dev *wdev = NULL;
	struct cfg80211_color_change_settings *params;
	UCHAR Color, Count;
	struct _BSS_INFO_ARGUMENT_T *bssinfo;
	struct bss_color_ctrl *bss_color;

	if (apidx < 0 || apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"invalid apidx:%d\n", apidx);
		return;
	}

	pAd = (PRTMP_ADAPTER)pAdOrg;
	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	params = (struct cfg80211_color_change_settings *)pData;

	if (WMODE_CAP_AX(wdev->PhyMode)) {
		bssinfo = &wdev->bss_info_argument;
		bss_color = &bssinfo->bss_color;

		/* Don't Care other params? beacon data? */
		Color = params->color;
		Count = params->count;

		/* check range */
		if (Color < BSS_COLOR_VALUE_MIN || Color > BSS_COLOR_VALUE_MAX)
			return;

		/* ignore it if desired color is same as current setting */
		if (bss_color->color == Color)
			return;

		/*acquire BPCC lock*/
		if (bcn_bpcc_op_lock(pAd, wdev, TRUE, BCN_BPCC_BSSCCA) == FALSE) {
			if (in_interrupt()) {
				struct _BCN_BSSCCA_UPDATE_PARAM update_param = {0};

				update_param.BSSCAUpdateReason = RSN_CFG80211_PATH;
				update_param.next_color = Color;
				update_param.count = Count;
				/* defer it to workqueue */
				bcn_bpcc_ct_switch(pAd, wdev, BCN_BPCC_BSSCCA, BCN_REASON(BCN_UPDATE_IE_CHG), update_param);
				return;
			} else
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BPCC, DBG_LVL_INFO,
					"%s: BSSCCA bcn_bpcc_op_lock fail!\n", __func__);
		}

		bss_color->disabled = TRUE;

		/* update state on firmware side */
		deliver_bss_color(pAd, bssinfo, bss_color);

		/* update wlan_operation state */
		wlan_operate_set_he_bss_color(wdev, bss_color->color, TRUE);
		bss_color->next_color = Color;
		wlan_operate_set_he_bss_next_color(wdev, Color, Count);

		/* update the Beacon content */
		UpdateBeaconHandler_BPCC(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG),
								BCN_BPCC_BSSCCA, TRUE);
	}
#endif
}

VOID CFG80211DRV_OpsFtIesParm(
	VOID *pAdOrg,
	VOID *pData,
	INT apidx)
{
#ifdef DOT11R_FT_SUPPORT
	PRTMP_ADAPTER pAd = NULL;
	struct wifi_dev *wdev = NULL;
	struct cfg80211_update_ft_ies_params *ftie;
	const u8 *end, *pos;
	struct _SECURITY_CONFIG *pSecConfig;
	UCHAR rsne_idx, ftcapflag;
	PFT_CFG pFtCfg;

	if (apidx < 0 || apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"invalid apidx:%d\n", apidx);
		return;
	}

	pAd = (PRTMP_ADAPTER)pAdOrg;
	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	ftie = (struct cfg80211_update_ft_ies_params *)pData;

	pos = ftie->ie;
	end = ftie->ie + ftie->ie_len;

	while (end - pos >= 2) {
		u8 id, len;

		id = *pos++;
		len = *pos++;
		if (len > end - pos)
			break;

		switch (id) {
		case IE_RSN:
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE, "FT: RSN\n");
			pSecConfig = &wdev->SecConfig;
			for (rsne_idx = 0; rsne_idx < SEC_RSNIE_NUM; rsne_idx++) {
				if (pSecConfig->RSNE_Type[rsne_idx] == SEC_RSNIE_NONE)
					continue;

				pSecConfig->RSNE_EID[rsne_idx][0] = IE_RSN;
				pSecConfig->RSNE_Len[rsne_idx] = len;
				if (len)
					NdisMoveMemory(
						&pSecConfig->RSNE_Content[rsne_idx][0], pos, len);
			}
			break;

		case IE_FT_MDIE:
			if (len <= FT_MDID_LEN)
				break;
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE, "FT: MDE\n");
			pFtCfg = &wdev->FtCfg;
			NdisMoveMemory(pFtCfg->FtMdId, pos, FT_MDID_LEN);
			ftcapflag = *(pos + FT_MDID_LEN);
			pFtCfg->FtCapFlag.FtOverDs = ftcapflag & BIT0;
			pFtCfg->FtCapFlag.RsrReqCap = (ftcapflag >> 1) & BIT0;
			pFtCfg->FtCapFlag.Dot11rFtEnable = (ftcapflag >> 2) & BIT0;
			break;

		case IE_FT_FTIE:
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE, "FT: FTE\n");
			//refer to wpa_ft_parse_ftie ?

			break;

		case IE_FT_RIC_DATA:
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE, "FT: RDE\n");

			break;

		case IE_RSNXE:
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE, "FT: RSNXE\n");

			break;
		}
		pos += len;
	}
#endif /* DOT11R_FT_SUPPORT */
}

void CFG8021DRV_AP_PHYINIT(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UINT8 channel)
{
	CHANNEL_CTRL *pchctrl = NULL;

	/*phy mode setting */
	if (channel > 14) {
		wdev->PhyMode = (WMODE_A | WMODE_AN | WMODE_AC | WMODE_AX_5G);
	} else {
		wdev->PhyMode = (WMODE_B | WMODE_G | WMODE_GN | WMODE_AX_24G);
	}
	/*change channel state to NONE*/
	pchctrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
	hc_set_ChCtrlChListStat(pchctrl, CH_LIST_STATE_NONE);
#ifdef EXT_BUILD_CHANNEL_LIST
	BuildChannelListEx(pAd, wdev);
#else
	BuildChannelList(pAd, wdev);
#endif
	RTMPSetPhyMode(pAd, wdev, wdev->PhyMode);

	return;
}

BOOLEAN CFG80211DRV_AP_SetChanBw_byWdev(
	RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_priv_ops_chan_info *pchan_info)
{
	UCHAR bw = 0;
	UCHAR ext_cha = EXTCHA_BELOW;
	INT32 ret = FALSE, success = FALSE;

	if (!pAd || !wdev || !pchan_info) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"ERROR! invalid parameter. pAd=%p,wdev=%p, pchan_info=%p\n",
				pAd, wdev, pchan_info);
		return FALSE;
	}

	/*kernel4.4 set bw*/
	if (MTK_NL80211_CHAN_WIDTH_20_NOHT == pchan_info->ChanType) {
		/*bw and ht check*/
		bw = wlan_operate_get_ht_bw(wdev);
		if (HT_BW_20 == (enum ht_bw_def)bw && 1 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 20 is already set.\n");
			goto set_channel;
		}

		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		wlan_config_set_ht_bw(wdev, BW_20);
		if (WLAN_OPER_OK == wlan_operate_set_ht_bw(wdev, HT_BW_20, EXTCHA_NONE)) {
			pAd->CommonCfg.HT_Disable = 1;/*HT disable*/
			SetCommonHtVht(pAd, wdev);
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 20 FAIL\n");
			ret = FALSE;
		}
	} else if (MTK_NL80211_CHAN_WIDTH_20 == pchan_info->ChanType) {
		/*bw and ht check*/
		bw = wlan_operate_get_ht_bw(wdev);
		if (HT_BW_20 == (enum ht_bw_def)bw && 0 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 20 is already set.\n");
			goto set_channel;
		}
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		wlan_config_set_ht_bw(wdev, BW_20);
		if (WLAN_OPER_OK == wlan_operate_set_ht_bw(wdev, HT_BW_20, EXTCHA_NONE)) {
			pAd->CommonCfg.HT_Disable = 0;/*HT enable*/
			SetCommonHtVht(pAd, wdev);
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 20 FAIL\n");
			ret = FALSE;
		}
	} else if (MTK_NL80211_CHAN_WIDTH_40 == pchan_info->ChanType) {
		/*extra_bw*/
		if (pchan_info->CenterChanId > pchan_info->ChanId)
			ext_cha = EXTCHA_ABOVE;
		else
			ext_cha = EXTCHA_BELOW;

		/*bw and ht check*/
		bw = wlan_operate_get_ht_bw(wdev);

		if (HT_BW_40 == (enum ht_bw_def)bw && ext_cha == wlan_operate_get_ext_cha(wdev)
			&& 0 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 40 ext_bw(%s) is already set.\n",
				ext_cha == EXTCHA_ABOVE ? "above":"below");
			goto set_channel;
		}
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);

		/*set bw*/
		wlan_config_set_ht_bw(wdev, BW_40);
		if (WLAN_OPER_OK == wlan_operate_set_ht_bw(wdev, HT_BW_40, ext_cha)) {
			pAd->CommonCfg.HT_Disable = 0;/*HT enable*/
			SetCommonHtVht(pAd, wdev);
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 40 FAIL\n");
			ret = FALSE;
		}
	} else if (MTK_NL80211_CHAN_WIDTH_80 == pchan_info->ChanType) {
		/*bw and ht check*/
		bw = wlan_operate_get_vht_bw(wdev);
		if (VHT_BW_80 == (enum vht_config_bw)bw && 0 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 80 is already set.\n");
			goto set_channel;
		}
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		/*set bw*/
		if (WLAN_OPER_OK == wlan_operate_set_vht_bw(wdev, VHT_BW_80)) {
			pAd->CommonCfg.HT_Disable = 0;/*HT disable*/
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 80 FAIL\n");
			ret = FALSE;
		}

	} else if (MTK_NL80211_CHAN_WIDTH_80P80 == pchan_info->ChanType) {
		/*bw and vht check*/
		bw = wlan_operate_get_vht_bw(wdev);
		if (VHT_BW_8080 == (enum vht_config_bw)bw && 0 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 8080 is already set.\n");
			goto set_channel;
		}
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		/*set bw*/
		if (WLAN_OPER_OK == wlan_operate_set_vht_bw(wdev, VHT_BW_8080)) {
			pAd->CommonCfg.HT_Disable = 0;/*HT disable*/
			wlan_config_set_he_bw(wdev, HE_BW_8080);
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 8080 FAIL\n");
			ret = FALSE;
		}
	} else if (MTK_NL80211_CHAN_WIDTH_160 == pchan_info->ChanType) {
		/*bw and ht check*/
		bw = wlan_operate_get_vht_bw(wdev);
		if (VHT_BW_160 == (enum vht_config_bw)bw && 0 == pAd->CommonCfg.HT_Disable) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"bw 160 is already set.\n");
			goto set_channel;
		}
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		/*set vbw*/
		if (WLAN_OPER_OK == wlan_operate_set_vht_bw(wdev, VHT_BW_160)) {
			pAd->CommonCfg.HT_Disable = 0;/*HT disable*/
			wlan_config_set_he_bw(wdev, HE_BW_160);
			ret = TRUE;
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"ERROR! wlan_operate_set_ht_bw 160 FAIL\n");
			ret = FALSE;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"WARN, bw=%d not support\n", pchan_info->ChanType);
		CFG8021DRV_AP_PHYINIT(pAd, wdev, pchan_info->ChanId);
		ret = FALSE;
	}

set_channel:
	/*bw and channel is not same as input check*/
	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
		"ret:%d (FALSE:%d), wdev->channel:%d, pchan_info->ChanId:%d\n",
		ret, FALSE, wdev->channel, pchan_info->ChanId);

	/*set channel*/
	if (wdev->channel != pchan_info->ChanId) {
		/*To do set channel, need TakeChannelOpCharge first*/
		if (!TakeChannelOpCharge(pAd, wdev, CH_OP_OWNER_SET_CHN, TRUE)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"TakeChannelOpCharge fail for SET channel!!\n");
			return FALSE;
		}
		pAd->ApCfg.iwpriv_event_flag = TRUE;

		RTMP_OS_REINIT_COMPLETION(&pAd->ApCfg.set_ch_aync_done);
		success = rtmp_set_channel(pAd, wdev, pchan_info->ChanId);

		/*non-csa flow should release ChannelOpCharge here,
		CSA flow will release ChannelOpCharge when CSA count down to zero*/
		if (pAd->ApCfg.set_ch_async_flag == FALSE) {
			pAd->ApCfg.iwpriv_event_flag = FALSE;
			ReleaseChannelOpCharge(pAd, wdev, CH_OP_OWNER_SET_CHN);
		}

		if (success == FALSE) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"ERROR! channel(%d) set fail\n", pchan_info->ChanId);
			return FALSE;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"channel(%d) is already set!\n", pchan_info->ChanId);
	}
	return TRUE;

}


int CFG80211DRV_AP_OpsSetChannel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_chan_info *pchan_info;

	pchan_info = (struct mt_priv_ops_chan_info *)ctrl_data->data;

	if (CFG80211DRV_AP_SetChanBw_byWdev(pAd, wdev, pchan_info) != TRUE) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"ERROR! CFG80211DRV_AP_SetChanBw_byWdev FAIL!\n");
		return -RTMP_IO_EINVAL;
	}
	return 0;
}


BOOLEAN CFG80211DRV_OpsSetChannel(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_chan_info *pChan)
{
	UINT8 ChanId, IfType, ChannelType;

	UCHAR ht_bw = 0, vht_bw = 0, eht_bw = 0;
	UCHAR ext_cha = EXTCHA_NONE;
	struct wifi_dev *wdev = NULL, *tdev = NULL;
	BOOLEAN b_ch_change = FALSE, b_bw_change = FALSE;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
	UCHAR i = 0;
	struct wlan_config *cfg = NULL;

	wdev = &pMbss->wdev;

	/*
	 *  enum nl80211_channel_type {
	 *	NL80211_CHAN_NO_HT,
	 *	NL80211_CHAN_HT20,
	 *	NL80211_CHAN_HT40MINUS,
	 *	NL80211_CHAN_HT40PLUS
	 *  };
	 */
	/* init */
	ChanId = pChan->ChanId;
	IfType = pChan->IfType;
	ChannelType = pChan->ChanType;

#ifdef HOSTAPD_AUTO_CH_SUPPORT
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
		"HOSTAPD AUTO_CH_SUPPORT Ignore Channel %d from hostapd", pChan->ChanId);
	return TRUE;
#endif

	if (IfType != RT_CMD_80211_IFTYPE_MONITOR) {
		if (ChanId != wdev->channel)
			b_ch_change = TRUE;
	}

	cfg = (struct wlan_config *)wdev->wpf_cfg;
	if (!cfg) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"80211> null wdev->wpf_cfg!\n");
		return FALSE;
	}

	ht_bw = cfg->ht_conf.ht_bw;
	vht_bw = cfg->vht_conf.vht_bw;
	eht_bw = cfg->eht_conf.bw;

	switch (ChannelType) {
	case RT_CMD_80211_CHANTYPE_NOHT:
		ht_bw = HT_BW_20;
		vht_bw = VHT_BW_2040;
		eht_bw = EHT_BW_20;
		break;
	case RT_CMD_80211_CHANTYPE_HT20:
		ht_bw = HT_BW_20;
		vht_bw = VHT_BW_2040;
		eht_bw = EHT_BW_20;
		break;
	case RT_CMD_80211_CHANTYPE_HT40MINUS:
		ht_bw = HT_BW_40;
		ext_cha = EXTCHA_BELOW;
		vht_bw = VHT_BW_2040;
		eht_bw = EHT_BW_2040;
		break;
	case RT_CMD_80211_CHANTYPE_HT40PLUS:
		ht_bw = HT_BW_40;
		ext_cha = EXTCHA_ABOVE;
		vht_bw = VHT_BW_2040;
		eht_bw = EHT_BW_2040;
		break;
	case RT_CMD_80211_CHANTYPE_VHT80:
		ht_bw = HT_BW_40;
		vht_bw = VHT_BW_80;
		eht_bw = EHT_BW_80;
		break;
	case RT_CMD_80211_CHANTYPE_VHT80P80:
		ht_bw = HT_BW_40;
		vht_bw = VHT_BW_8080;
		eht_bw = EHT_BW_160;
		break;
	case RT_CMD_80211_CHANTYPE_VHT160:
		ht_bw = HT_BW_40;
		vht_bw = VHT_BW_160;
		eht_bw = EHT_BW_160;
		break;
	default:
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"80211> ChannelType(%d) not match!\n", ChannelType);
		break;
	}

	if (b_ch_change) {
		if (IfType != RT_CMD_80211_IFTYPE_MONITOR) {
			wdev->channel = ChanId;
			wlan_config_set_ht_bw(wdev, ht_bw);
			wlan_config_set_vht_bw(wdev, vht_bw);
			wlan_config_set_eht_bw(wdev, eht_bw);
			wlan_operate_set_prim_ch(wdev, wdev->channel);
		}
	} else {
		b_bw_change = FALSE;

		for (i = 0; i < WDEV_NUM_MAX; i++) {
			tdev = pAd->wdev_list[i];
			if (tdev) {
				struct wlan_config *cfg = (struct wlan_config *)tdev->wpf_cfg;

				if (cfg) {

					if (cfg->ht_conf.ht_bw != ht_bw) {
						wlan_config_set_ht_bw(tdev, ht_bw);
						cfg->ht_conf.ext_cha = ext_cha;
						b_bw_change = TRUE;
					}
					if (cfg->vht_conf.vht_bw != vht_bw) {
						wlan_config_set_vht_bw(tdev, vht_bw);
						b_bw_change = TRUE;
					}
					if (cfg->eht_conf.bw != eht_bw) {
						wlan_config_set_eht_bw(tdev, eht_bw);
						b_bw_change = TRUE;
					}

				}
			}
		}

		if (b_bw_change) {
			struct freq_cfg freq_cfg;

			/* Reset ht coex result */
			pAd->CommonCfg.BssCoexScanLastResult.bNeedFallBack = FALSE;

			os_zero_mem(&freq_cfg, sizeof(freq_cfg));
			phy_freq_get_cfg(wdev, &freq_cfg);
			operate_loader_phy(wdev, &freq_cfg);

			for (i = 0; i < WDEV_NUM_MAX; i++) {
				tdev = pAd->wdev_list[i];
				if (tdev)
					SetCommonHtVht(pAd, tdev);
			}
		}
	}

	if ((b_ch_change || b_bw_change) && pAd->CommonCfg.bBssCoexEnable) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
			"Start 20/40 BSSCoex Channel Scan\n");
		/* Reset ht coex result */
		pAd->CommonCfg.BssCoexScanLastResult.LastScanTime = 0;
		pAd->CommonCfg.BssCoexScanLastResult.bNeedFallBack = FALSE;
		pAd->CommonCfg.bRcvBSSWidthTriggerEvents = FALSE;
		APOverlappingBSSScan(pAd, wdev);
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
		"80211> b_ch_change=%d, b_bw_change=%d, New Chan=%d with Ext[%d], ht_bw=%d, vht_bw=%d, eht_bw=%d\n",
		b_ch_change, b_bw_change, ChanId, ext_cha, ht_bw, vht_bw, eht_bw);

	return TRUE;
}

BOOLEAN CFG80211DRV_OpsJoinIbss(
	VOID						*pAdOrg,
	VOID						*pData)
{
#ifdef CONFIG_STA_SUPPORT
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_IBSS *pIbssInfo;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	struct wifi_dev *wdev = &pAd->StaCfg[0].wdev;

	pIbssInfo = (CMD_RTPRIV_IOCTL_80211_IBSS *)pData;
	pAd->StaCfg[0].bAutoReconnect = TRUE;
	pAd->CommonCfg.BeaconPeriod = pIbssInfo->BeaconInterval;

	if (pIbssInfo->privacy) {
		SET_AKM_OPEN(wdev->SecConfig.AKMMap);
		SET_CIPHER_WEP(wdev->SecConfig.PairwiseCipher);
		SET_CIPHER_WEP(wdev->SecConfig.GroupCipher);
		SET_CIPHER_WEP(pAd->StaCfg[0].GroupCipher);
		SET_CIPHER_WEP(pAd->StaCfg[0].PairwiseCipher);
	}

	if (pIbssInfo->BeaconExtraIeLen > 0) {
		const UCHAR *ie = NULL;

		if (pCfg80211_ctrl->BeaconExtraIe != NULL) {
			os_free_mem(pCfg80211_ctrl->BeaconExtraIe);
			pCfg80211_ctrl->BeaconExtraIe = NULL;
		}

		os_alloc_mem(NULL, (UCHAR **)&pCfg80211_ctrl->BeaconExtraIe, pIbssInfo->BeaconExtraIeLen);

		if (pCfg80211_ctrl->BeaconExtraIe != NULL) {
			NdisCopyMemory(pCfg80211_ctrl->BeaconExtraIe, pIbssInfo->BeaconExtraIe, pIbssInfo->BeaconExtraIeLen);
			pCfg80211_ctrl->BeaconExtraIeLen = pIbssInfo->BeaconExtraIeLen;
		} else {
			pCfg80211_ctrl->BeaconExtraIeLen = 0;
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"CFG80211: MEM ALLOC ERROR\n");
			return FALSE;
		}

		ie = pCfg80211_ctrl->BeaconExtraIe;

		if ((ie[0] == WLAN_EID_VENDOR_SPECIFIC) &&
			(ie[1] >= 4) &&
			(ie[2] == 0x00) && (ie[3] == 0x50) && (ie[4] == 0xf2) && (ie[5] == 0x01)) {
			/* skip wpa_version [6][7] */
			if ((ie[8] == 0x00) && (ie[9] == 0x50) && (ie[10] == 0xf2) && (ie[11] == 0x04)) {
				SET_CIPHER_CCMP128(wdev->SecConfig.PairwiseCipher);
				SET_CIPHER_CCMP128(wdev->SecConfig.GroupCipher);
				SET_CIPHER_CCMP128(pAd->StaCfg[0].GroupCipher);
				SET_CIPHER_CCMP128(pAd->StaCfg[0].PairwiseCipher);
			} else {
				SET_CIPHER_TKIP(wdev->SecConfig.PairwiseCipher);
				SET_CIPHER_TKIP(wdev->SecConfig.GroupCipher);
				SET_CIPHER_TKIP(pAd->StaCfg[0].GroupCipher);
				SET_CIPHER_TKIP(pAd->StaCfg[0].PairwiseCipher);
			}

			SET_AKM_WPANONE(wdev->SecConfig.AKMMap);
			pAd->StaCfg[0].WpaState = SS_NOTUSE;
		}
	}
	Set_SSID_Proc(pAd, (RTMP_STRING *)pIbssInfo->Ssid);
#endif /* CONFIG_STA_SUPPORT */
	return TRUE;
}

static INT CFG80211DRV_BW_Set(CHAR *bw)
{

	if (bw == NULL) {
		MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"ERROR, Invalid data bw = NULL;\n");
		return -RTMP_IO_EINVAL;
	}

	switch (*bw) {
	case BW_5:
		*bw = MT_NL80211_CHAN_WIDTH_5;
		break;
	case BW_10:
		*bw = MT_NL80211_CHAN_WIDTH_10;
		break;
	case BW_20:
		*bw = MT_NL80211_CHAN_WIDTH_20;
		break;
	case BW_40:
		*bw = MT_NL80211_CHAN_WIDTH_40;
		break;
	case BW_80:
		*bw = MT_NL80211_CHAN_WIDTH_80;
		break;
	case BW_160:
	case BW_8080:
		*bw = MT_NL80211_CHAN_WIDTH_160;
		break;
	case BW_320:
		*bw = MT_NL80211_CHAN_WIDTH_320;
		break;
	default:
		MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"default ERROR, Invalid data bw = %d;\n", *bw);
		break;
	}

	return 0;
}

static VOID CFG80211DRV_GetTxRxRate(
	struct _RTMP_ADAPTER *pAd,
	MAC_TABLE_ENTRY *pEntry,
	struct mt_priv_rate_info *txrate,
	struct mt_priv_rate_info *rxrate)
{
	ULONG DataRate = 0, DataRate_r = 0;
	struct mt_priv_rate_info *prate_info = NULL;
#ifdef RACTRL_FW_OFFLOAD_SUPPORT
	struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(pAd->hdev_ctrl);
#endif

	getRate(pEntry->HTPhyMode, &DataRate);

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
	if (cap->fgRateAdaptFWOffload == TRUE) {
		UCHAR phy_mode, rate, bw, sgi, stbc;
		UCHAR phy_mode_r, rate_r, bw_r, sgi_r, stbc_r;
		UCHAR nss;
		UCHAR nss_r;
		UINT32 RawData;
		UINT32 lastTxRate;
		UINT32 lastRxRate = pEntry->LastRxRate;
		UCHAR ucBand = HcGetBandByWdev(pEntry->wdev);

		EXT_EVENT_TX_STATISTIC_RESULT_T rTxStatResult;
		EXT_EVENT_PHY_STATE_RX_RATE rRxStatResult;
		union _HTTRANSMIT_SETTING LastTxRate;
		union _HTTRANSMIT_SETTING LastRxRate;

		NdisZeroMemory(&rRxStatResult, sizeof(rRxStatResult));
		NdisZeroMemory(&rTxStatResult, sizeof(rTxStatResult));

		MtCmdGetTxStatistic(pAd, GET_TX_STAT_ENTRY_TX_RATE, 0/*Don't Care*/, pEntry->wcid, &rTxStatResult);
		LastTxRate.field.MODE = rTxStatResult.rEntryTxRate.MODE;
		LastTxRate.field.BW = rTxStatResult.rEntryTxRate.BW;
		LastTxRate.field.ldpc = rTxStatResult.rEntryTxRate.ldpc ? 1 : 0;
		LastTxRate.field.ShortGI = rTxStatResult.rEntryTxRate.ShortGI ? 1 : 0;
		LastTxRate.field.STBC = rTxStatResult.rEntryTxRate.STBC;

		if (LastTxRate.field.MODE >= MODE_VHT)
			LastTxRate.field.MCS = (((rTxStatResult.rEntryTxRate.VhtNss - 1) & 0x3) << 4) + rTxStatResult.rEntryTxRate.MCS;
		else if (LastTxRate.field.MODE == MODE_OFDM)
			LastTxRate.field.MCS = getLegacyOFDMMCSIndex(rTxStatResult.rEntryTxRate.MCS) & 0x0000003F;
		else
			LastTxRate.field.MCS = rTxStatResult.rEntryTxRate.MCS;

		lastTxRate = (UINT32)(LastTxRate.word);
		LastRxRate.word = (USHORT)lastRxRate;
		RawData = lastTxRate;
		phy_mode = rTxStatResult.rEntryTxRate.MODE;
		rate = RawData & 0x3F;
		/* bw from txrx info should be transfer to oid*/
		bw = (rTxStatResult.rEntryTxRate.BW < 4) ? rTxStatResult.rEntryTxRate.BW : BW_320;
		sgi = rTxStatResult.rEntryTxRate.ShortGI;
		stbc = ((RawData >> 10) & 0x1);
		nss = rTxStatResult.rEntryTxRate.VhtNss;

		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Tx: phy_mode=%d, rate=%d, bw=%d, sgi=%d, stbc=%d, nss=%d\n",
			phy_mode, rate, bw, sgi, stbc, nss);

		MtCmdPhyGetRxRate(pAd, CMD_PHY_STATE_CONTENTION_RX_PHYRATE, ucBand, pEntry->wcid, (UINT32 *)&rRxStatResult);
		LastRxRate.field.MODE = rRxStatResult.u1RxMode;
		LastRxRate.field.BW = rRxStatResult.u1BW;
		LastRxRate.field.ldpc = rRxStatResult.u1Coding;
		LastRxRate.field.ShortGI = rRxStatResult.u1Gi ? 1 : 0;
		LastRxRate.field.STBC = rRxStatResult.u1Stbc;

		if (LastRxRate.field.MODE >= MODE_VHT)
			LastRxRate.field.MCS = ((rRxStatResult.u1RxNsts & 0x3) << 4) + rRxStatResult.u1RxRate;
		else if (LastRxRate.field.MODE == MODE_OFDM)
			LastRxRate.field.MCS = getLegacyOFDMMCSIndex(rRxStatResult.u1RxRate) & 0x0000003F;
		else
			LastRxRate.field.MCS = rRxStatResult.u1RxRate;

		phy_mode_r = rRxStatResult.u1RxMode;
		rate_r = rRxStatResult.u1RxRate & 0x3F;
		/* bw from txrx info should be transfer to oid*/
		bw_r = (rRxStatResult.u1BW < 4) ? rRxStatResult.u1BW : BW_320;
		sgi_r = rRxStatResult.u1Gi;
		stbc_r = rRxStatResult.u1Stbc;
		nss_r = rRxStatResult.u1RxNsts + 1;

		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Rx: phy_mode_r=%d, rate_r=%d, bw_r=%d, sgi_r=%d, stbc_r=%d, nss_r=%d\n",
			phy_mode_r, rate_r, bw_r, sgi_r, stbc_r, nss_r);
/*TX MCS*/
#ifdef DOT11_VHT_AC
		if (phy_mode >= MODE_VHT)
			rate = rate & 0xF;
#endif /* DOT11_VHT_AC */


/*RX MCS*/
#ifdef DOT11_VHT_AC
		if (phy_mode_r >= MODE_VHT)
			rate_r = rate_r & 0xF;
		else
#endif /* DOT11_VHT_AC */
#if DOT11_N_SUPPORT
			if (phy_mode_r >= MODE_HTMIX)
				MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
					"MODE_HTMIX(%d >= %d): MCS=%d\n",
					phy_mode_r, MODE_HTMIX, rate_r);
		else
#endif
			if (phy_mode_r == MODE_OFDM) {
				if (rate_r == TMI_TX_RATE_OFDM_6M)
					LastRxRate.field.MCS = 0;
				else if (rate_r == TMI_TX_RATE_OFDM_9M)
					LastRxRate.field.MCS = 1;
				else if (rate_r == TMI_TX_RATE_OFDM_12M)
					LastRxRate.field.MCS = 2;
				else if (rate_r == TMI_TX_RATE_OFDM_18M)
					LastRxRate.field.MCS = 3;
				else if (rate_r == TMI_TX_RATE_OFDM_24M)
					LastRxRate.field.MCS = 4;
				else if (rate_r == TMI_TX_RATE_OFDM_36M)
					LastRxRate.field.MCS = 5;
				else if (rate_r == TMI_TX_RATE_OFDM_48M)
					LastRxRate.field.MCS = 6;
				else if (rate_r == TMI_TX_RATE_OFDM_54M)
					LastRxRate.field.MCS = 7;
				else
					LastRxRate.field.MCS = 0;

				rate_r = LastRxRate.field.MCS;
				rxrate->mcs = rate_r;/*rx mcs: ofdm*/
		} else if (phy_mode_r == MODE_CCK) {
			if (rate_r == TMI_TX_RATE_CCK_1M_LP)
				LastRxRate.field.MCS = 0;
			else if (rate_r == TMI_TX_RATE_CCK_2M_LP)
				LastRxRate.field.MCS = 1;
			else if (rate_r == TMI_TX_RATE_CCK_5M_LP)
				LastRxRate.field.MCS = 2;
			else if (rate_r == TMI_TX_RATE_CCK_11M_LP)
				LastRxRate.field.MCS = 3;
			else if (rate_r == TMI_TX_RATE_CCK_2M_SP)
				LastRxRate.field.MCS = 1;
			else if (rate_r == TMI_TX_RATE_CCK_5M_SP)
				LastRxRate.field.MCS = 2;
			else if (rate_r == TMI_TX_RATE_CCK_11M_SP)
				LastRxRate.field.MCS = 3;
			else
				LastRxRate.field.MCS = 0;

			rate_r = LastRxRate.field.MCS;
			rxrate->mcs = LastRxRate.field.MCS;/*rx_mcs:cck*/
		}

		/*tx_gi*/
		if (sgi)
			txrate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		/*rx_gi*/
		if (sgi_r)
			rxrate->flags |= RATE_INFO_FLAGS_SHORT_GI;

		/* tx rate calculation */
		if (phy_mode >= MODE_HE) {
#ifdef DOT11_EHT_BE
			/* be tx */
			if ((phy_mode == MODE_EHT) || (phy_mode == MODE_EHT_ER_SU) || (phy_mode == MODE_EHT_TB) || (phy_mode == MODE_EHT_MU))
				get_rate_eht((rate & 0xf), bw, nss, 0, &DataRate);
			else
#endif
			/* ax tx */
				get_rate_he((rate & 0xf), bw, nss, 0, &DataRate);
			if (sgi == 1)
				DataRate = (DataRate * 967) >> 10;
			if (sgi == 2)
				DataRate = (DataRate * 870) >> 10;
		} else {
			getRate(LastTxRate, &DataRate);
		}

		/* rx rate calculation */
		if (phy_mode_r >= MODE_HE) {
#ifdef DOT11_EHT_BE
			/* be rx */
			if ((phy_mode_r == MODE_EHT) || (phy_mode_r == MODE_EHT_ER_SU) || (phy_mode_r == MODE_EHT_TB) || (phy_mode_r == MODE_EHT_MU))
				get_rate_eht((rate_r & 0xf), bw_r, nss_r, 0, &DataRate_r);
			else
#endif
			/* ax rx */
				get_rate_he((rate_r & 0xf), bw_r, nss_r, 0, &DataRate_r);
			if (sgi_r == 1)
				DataRate_r = (DataRate_r * 967) >> 10;
			if (sgi_r == 2)
				DataRate_r = (DataRate_r * 870) >> 10;
		} else {
			getRate(LastRxRate, &DataRate_r);
		}

		/*tx rate infos*/
		prate_info = txrate;
		get_rate_flag(phy_mode, &prate_info->flags);
		prate_info->mcs = rate;
		prate_info->legacy = (UINT16)DataRate;
		prate_info->nss = nss;
		prate_info->bw = bw;
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Tx: flag=%u, mcs=%u, legacy=%u, nss=%u, bw=%u\n",
			prate_info->flags, prate_info->mcs, prate_info->legacy, prate_info->nss, prate_info->bw);

		/*rx rate infos*/
		prate_info = rxrate;
		get_rate_flag(phy_mode_r, &prate_info->flags);
		prate_info->mcs = rate_r;
		prate_info->legacy = (UINT16)DataRate_r;
		prate_info->nss = nss_r;
		prate_info->bw = bw_r;
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Rx: flag=%u, mcs=%u, legacy=%u, nss=%u, bw=%u\n",
			prate_info->flags, prate_info->mcs, prate_info->legacy, prate_info->nss, prate_info->bw);
	}
	if (txrate)
		CFG80211DRV_BW_Set(&txrate->bw);
	if (rxrate)
		CFG80211DRV_BW_Set(&rxrate->bw);
#endif /* RACTRL_FW_OFFLOAD_SUPPORT */

}

BOOLEAN CFG80211DRV_StaGet(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_sta_info *pIbssInfo)
{
	STA_ADMIN_CONFIG *pstacfg = NULL;
	MAC_TABLE_ENTRY *pEntry = NULL;
	STA_TR_ENTRY *tr_entry;
	UINT8 i;
	RSSI_SAMPLE rx_rssi_sample;
	UINT8 RxPath;
#ifdef VOW_SUPPORT
	struct _RTMP_CHIP_DBG *chip_dbg = hc_get_chip_dbg(pAd->hdev_ctrl);
#endif

	pEntry = MacTableLookup(pAd, pIbssInfo->MAC);

	if (pEntry == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"sta not found\n");
		return FALSE;
	}
	if (!pEntry->wdev) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"ERROR! invalid parameter! <wdev>\n");
		return FALSE;
	}
	if (pEntry->Sst != SST_ASSOC) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"idx: %d, mac: "MACSTR" is disassociated\n",
			pEntry->wcid, MAC2STR(pEntry->Addr));
		return FALSE;
	}

#ifdef CONFIG_STA_SUPPORT
	pstacfg = (STA_ADMIN_CONFIG *)GetStaCfgByWdev(pAd, pEntry->wdev);
#endif

	/* fill tx_bytes count */
	pIbssInfo->tx_bytes = pEntry->TxBytes;

	/* fill tx_packets count */
	pIbssInfo->tx_packets = pEntry->TxPackets.u.LowPart;

	/* fill tx_retries */
	pIbssInfo->tx_retries = (UINT32)pEntry->mpdu_retries.QuadPart;

	/* fill tx_failed  */
	pIbssInfo->tx_failed = pEntry->TxFailCount;

	/* fill rx_bytes */
	pIbssInfo->rx_bytes = pEntry->RxBytes;

	/* fill rx_packets count */
	pIbssInfo->rx_packets = pEntry->RxPackets.u.LowPart;

#ifdef VOW_SUPPORT
	if (chip_dbg->get_sta_airtime) {
		UCHAR ac;

		for (ac = 0; ac < WMM_NUM; ac++) {
			/* fill tx_duration  */
			pIbssInfo->tx_duration += chip_dbg->get_sta_airtime(pAd, pEntry->wcid, ac, TRUE);
			/* fill rx_duration  */
			pIbssInfo->rx_duration += chip_dbg->get_sta_airtime(pAd, pEntry->wcid, ac, FALSE);
		}
	}
#endif /* VOW_SUPPORT */

	/* fill inactive time */
	pIbssInfo->InactiveTime = pEntry->NoDataIdleCount * 1000; /* unit: ms */

	/* fill tx/rx rate */
	CFG80211DRV_GetTxRxRate(pAd, pEntry, &pIbssInfo->txrate, &pIbssInfo->rxrate);

	/* fill signal */
	pIbssInfo->Signal = RTMPAvgRssi(pAd, &pEntry->RssiSample);

	/* fill chains */
	RxPath = pAd->Antenna.field.RxPath;
	RxPath = min(RxPath, (UINT8)(sizeof(pEntry->RssiSample.AvgRssi) / sizeof(CHAR)));
	for (i = 0; i < RxPath; i++) {
		if ((pEntry->RssiSample.AvgRssi[i] > -127) && (pEntry->RssiSample.AvgRssi[i] < 0)) {
			/* fill chains signal*/
			pIbssInfo->chain_signal[i] = pEntry->RssiSample.AvgRssi[i];
			pIbssInfo->chains |= BIT(i);
		}
	}
	/*fill ack signal*/
	if (chip_dbg->get_sta_rx_rcpi) {
		UINT32 rx_rcpi;

		rx_rcpi = chip_dbg->get_sta_rx_rcpi(pAd, pEntry->wcid);

		for (i = 0; i < RxPath; i++) {
			rx_rssi_sample.AvgRssi[i] = (rx_rcpi & (0xff << (i * 8))) >> (i * 8);
			rx_rssi_sample.AvgRssi[i] = RCPI_TO_RSSI(rx_rssi_sample.AvgRssi[i]);
		}
		pIbssInfo->ack_signal = RTMPAvgRssi(pAd, &rx_rssi_sample);
	}

	/* fill associated at */
	pIbssInfo->assoc_at = pEntry->assoc_at;

	/* fill authorized */
	tr_entry = tr_entry_get(pAd, pEntry->wcid);
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_AUTHORIZED);
	if (tr_entry && tr_entry->PortSecured == WPA_802_1X_PORT_SECURED)
		pIbssInfo->sta_flags.set |= BIT(NL80211_STA_FLAG_AUTHORIZED);

	/* fill authenticated */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_AUTHENTICATED);
	if (pEntry->Sst >= SST_AUTH)
		pIbssInfo->sta_flags.set |= BIT(NL80211_STA_FLAG_AUTHENTICATED);

	/* fill associated */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_ASSOCIATED);
	if (pEntry->Sst == SST_ASSOC)
		pIbssInfo->sta_flags.set |= BIT(NL80211_STA_FLAG_ASSOCIATED);

	/* fill WME */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_WME);
	if (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_WMM_CAPABLE))
		pIbssInfo->sta_flags.set |= BIT(NL80211_STA_FLAG_WME);

	/* fill MFP */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_MFP);
	if (pEntry->SecConfig.PmfCfg.UsePMFConnect)
		pIbssInfo->sta_flags.set |= BIT(NL80211_STA_FLAG_MFP);

	/* fill preamble */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
		if (CAP_IS_SHORT_PREAMBLE_ON(pEntry->CapabilityInfo))
			pIbssInfo->sta_flags.set  |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);

	/* fill TDLS peer */
	pIbssInfo->sta_flags.mask |= BIT(NL80211_STA_FLAG_TDLS_PEER);
	/*not support*/
	pIbssInfo->sta_flags.set  &= ~(BIT(NL80211_STA_FLAG_TDLS_PEER));



	if (IS_ENTRY_PEER_AP(pEntry) && pstacfg && pstacfg->ApcliInfStat.Valid) {
		/*fill connected time*/
		ULONG time_now = 0;

		/* fill connected Time */
		NdisGetSystemUpTime(&time_now);
		time_now -= pstacfg->ApcliInfStat.ApCliLinkUpTime;
		pIbssInfo->connected_time = (UINT32)(time_now/OS_HZ);


#ifdef WIFI_IAP_BCN_STAT_FEATURE
		/*add beacon infos here*/
		pIbssInfo->beacon_mask |= BIT(NL80211_STA_INFO_BEACON_LOSS);
		pIbssInfo->beacon_loss_count = pstacfg->beacon_loss_count;
		pIbssInfo->beacon_mask |= BIT(NL80211_STA_INFO_BEACON_RX);
		pIbssInfo->rx_beacon = pstacfg->rx_beacon;
#endif/*WIFI_IAP_BCN_STAT_FEATURE*/

		for (i = 0; i < MAX_RSSI_LEN; i++)
			rx_rssi_sample.AvgRssi[i] = pstacfg->BcnRssiAvg[i];
		pIbssInfo->rx_beacon_signal_avg = RTMPAvgRssi(pAd, &rx_rssi_sample);

		/* fill bss param */
		if (CAP_IS_SHORT_PREAMBLE_ON(pstacfg->StaActive.CapabilityInfo))
			pIbssInfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_PREAMBLE;
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED))
			pIbssInfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
		pIbssInfo->bss_param.dtim_period  = pstacfg->DtimPeriod;
		pIbssInfo->bss_param.beacon_interval = pstacfg->BeaconPeriod;

	} else {
		BSS_STRUCT *pmbss = &pAd->ApCfg.MBSSID[pEntry->wdev->func_idx];

		/* fill connected time */
		pIbssInfo->connected_time = pEntry->StaConnectTime;

		/* fill bss param */
		if (CAP_IS_SHORT_PREAMBLE_ON(pmbss->CapabilityInfo))
			pIbssInfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_PREAMBLE;
		if (pAd->CommonCfg.bUseShortSlotTime)
			pIbssInfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
		pIbssInfo->bss_param.dtim_period = pmbss->DtimPeriod;
		pIbssInfo->bss_param.beacon_interval = pAd->CommonCfg.BeaconPeriod;
	}
	return TRUE;
}

BOOLEAN CFG80211DRV_StaKeyAdd(
	VOID						*pAdOrg,
	VOID						*pData)
{
#ifdef CONFIG_STA_SUPPORT
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;
	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;

	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40 || pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104) {
		RT_CMD_STA_IOCTL_SECURITY IoctlSec;
		MAC_TABLE_ENTRY *pEntry = NULL;
		INT groupWcid = 0;
		os_zero_mem(&IoctlSec, sizeof(RT_CMD_STA_IOCTL_SECURITY));

		if (ADHOC_ON(pAd))
			groupWcid = pAd->StaCfg[0].wdev.tr_tb_idx;

		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "RT_CMD_80211_KEY_WEP\n");
		pEntry = entry_get(pAd, BSSID_WCID_TO_REMOVE);
		IoctlSec.KeyIdx = pKeyInfo->KeyId;
		IoctlSec.pData = pKeyInfo->KeyBuf;
		IoctlSec.length = pKeyInfo->KeyLen;
		IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_WEP;
		IoctlSec.flags = RT_CMD_STA_IOCTL_SECURITY_ENABLED;
		RTMP_STA_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_STA_SIOCSIWENCODEEXT, 0,
							 &IoctlSec, 0, INT_MAIN);

		if (pKeyInfo->bPairwise == FALSE)
		{
			struct _ASIC_SEC_INFO *info = NULL;

			if (ADHOC_ON(pAd)) {
				UINT i = 0;

				for (i = BSSID_WCID_TO_REMOVE; i < groupWcid; i++) {
					pEntry = entry_get(pAd, i);

					if (pEntry->wcid == 0)
						continue;

					if (IS_ENTRY_ADHOC(pEntry)) {
						/* Set key material to Asic */
						os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
						if (info) {
							os_zero_mem(info, sizeof(ASIC_SEC_INFO));
							info->Operation = SEC_ASIC_ADD_PAIRWISE_KEY;
							info->Direction = SEC_ASIC_KEY_BOTH;
							info->Wcid = pEntry->wcid;
							info->BssIndex = BSS0;
							info->Cipher = pEntry->SecConfig.PairwiseCipher;
							info->KeyIdx = pKeyInfo->KeyId;
							os_move_mem(&info->PeerAddr[0], pEntry->Addr, MAC_ADDR_LEN);
							os_move_mem(&info->Key, &pEntry->SecConfig.WepKey[info->KeyIdx], sizeof(SEC_KEY_INFO));
							HW_ADDREMOVE_KEYTABLE(pAd, info);
							os_free_mem(info);
						}
					} else
						MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
							 "=====> can't add to [%d]Wcid %d, type=%d\n", i,
							  pEntry->wcid, pEntry->EntryType);
				}
			}

			/* Set key material to Asic */
			os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
			if (info) {
				os_zero_mem(info, sizeof(ASIC_SEC_INFO));
				info->Operation = SEC_ASIC_ADD_GROUP_KEY;
				info->Direction = SEC_ASIC_KEY_RX;
				info->Wcid = groupWcid;
				info->BssIndex = BSS0;
				info->Cipher = pEntry->SecConfig.GroupCipher;
				info->KeyIdx = pKeyInfo->KeyId;
				os_move_mem(&info->PeerAddr[0], BROADCAST_ADDR, MAC_ADDR_LEN);
				os_move_mem(&info->Key, &pEntry->SecConfig.WepKey[info->KeyIdx], sizeof(SEC_KEY_INFO));
				HW_ADDREMOVE_KEYTABLE(pAd, info);
				os_free_mem(info);
			}
		} else {
			struct _ASIC_SEC_INFO *info = NULL;
			/* Set key material to Asic */
			os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
			if (info) {
				os_zero_mem(info, sizeof(ASIC_SEC_INFO));
				info->Operation = SEC_ASIC_ADD_PAIRWISE_KEY;
				info->Direction = SEC_ASIC_KEY_BOTH;
				info->Wcid = pEntry->wcid;
				info->BssIndex = BSS0;
				info->Cipher = pEntry->SecConfig.PairwiseCipher;
				info->KeyIdx = pKeyInfo->KeyId;
				os_move_mem(&info->PeerAddr[0], pEntry->Addr, MAC_ADDR_LEN);
				os_move_mem(&info->Key, &pEntry->SecConfig.WepKey[info->KeyIdx], sizeof(SEC_KEY_INFO));
				HW_ADDREMOVE_KEYTABLE(pAd, info);
				os_free_mem(info);
			}
		}
	} else {
		RT_CMD_STA_IOCTL_SECURITY IoctlSec;
		os_zero_mem(&IoctlSec, sizeof(RT_CMD_STA_IOCTL_SECURITY));

		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Set_WPAPSK_Proc ==> id:%d, type:%d, len:%d\n",
				 pKeyInfo->KeyId, pKeyInfo->KeyType, (INT32)strlen(pKeyInfo->KeyBuf));
		IoctlSec.KeyIdx = pKeyInfo->KeyId;
		IoctlSec.pData = pKeyInfo->KeyBuf;
		IoctlSec.length = pKeyInfo->KeyLen;

		/* YF@20120327: Due to WepStatus will be set in the cfg connect function.*/
		if (IS_CIPHER_TKIP_Entry(&pAd->StaCfg[0].wdev))
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
		else if (IS_CIPHER_AES_Entry(&pAd->StaCfg[0].wdev))
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

		IoctlSec.flags = RT_CMD_STA_IOCTL_SECURITY_ENABLED;

		if (pKeyInfo->bPairwise == FALSE)
		{
			if (IS_CIPHER_TKIP(pAd->StaCfg[0].GroupCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (IS_CIPHER_CCMP128(pAd->StaCfg[0].GroupCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "Install GTK: %d\n", IoctlSec.Alg);
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_GROUP_KEY;
		} else {
			if (IS_CIPHER_TKIP(pAd->StaCfg[0].PairwiseCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (IS_CIPHER_CCMP128(pAd->StaCfg[0].PairwiseCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "Install PTK: %d\n", IoctlSec.Alg);
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_SET_TX_KEY;
		}

		/*Set_GroupKey_Proc(pAd, &IoctlSec) */
		RTMP_STA_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_STA_SIOCSIWENCODEEXT, 0,
							 &IoctlSec, 0, INT_MAIN);

		if (IS_AKM_WPANONE(pAd->StaCfg[0].wdev.SecConfig.AKMMap)) {
			if (IS_CIPHER_TKIP(pAd->StaCfg[0].PairwiseCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (IS_CIPHER_CCMP128(pAd->StaCfg[0].PairwiseCipher))
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "Install ADHOC PTK: %d\n", IoctlSec.Alg);
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_SET_TX_KEY;
			RTMP_STA_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_STA_SIOCSIWENCODEEXT, 0,
								 &IoctlSec, 0, INT_MAIN);
		}
	}

#endif /* CONFIG_STA_SUPPORT */
	return TRUE;
}

static BOOLEAN CFG80211DRV_Connect(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_priv_connect_info *pConnInfo)
{
#ifdef APCLI_CFG80211_SUPPORT
	UCHAR SSID[NDIS_802_11_LENGTH_SSID + 1]; /* Add One for SSID_Len == 32 */
	UINT32 SSIDLen;
	PSTA_ADMIN_CONFIG pStaCfg = NULL;
	INT staidx = 0;
#ifdef DOT11W_PMF_SUPPORT
	struct _PMF_CFG *pPmfCfg = NULL;
#endif
	struct _SECURITY_CONFIG *sec_cfg;

	SSIDLen = pConnInfo->SsidLen;
	if (SSIDLen > NDIS_802_11_LENGTH_SSID)
		SSIDLen = NDIS_802_11_LENGTH_SSID;

	memset(&SSID, 0, sizeof(SSID));
	memcpy(SSID, pConnInfo->pSsid, SSIDLen);

	sec_cfg = &wdev->SecConfig;
	pStaCfg = GetStaCfgByWdev(pAd, wdev);
#ifdef DOT11W_PMF_SUPPORT
	pPmfCfg = &sec_cfg->PmfCfg;
#endif
	pStaCfg->wpa_supplicant_info.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE;

	/* Check the connection is WPS or not */
	if (pConnInfo->bWpsConnection) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"AP_CLI WPS Connection onGoing.....\n");
		pStaCfg->wpa_supplicant_info.WpaSupplicantUP |= WPA_SUPPLICANT_ENABLE_WPS;
	}

	sec_cfg->AuthType = cfg80211_drv_get_auth(pConnInfo->AuthType);

	switch (pConnInfo->AkmSuite) {
	case WLAN_AKM_SUITE_8021X:
		if (pConnInfo->WpaVer == 2)
			Set_ApCli_AuthMode(pAd, staidx, "WPA2");
		else
			Set_ApCli_AuthMode(pAd, staidx, "WPA");
		break;
	case WLAN_AKM_SUITE_PSK:
		if (pConnInfo->WpaVer == 2)
			Set_ApCli_AuthMode(pAd, staidx, "WPA2PSK");
		else
			Set_ApCli_AuthMode(pAd, staidx, "WPAPSK");
		break;
	case WLAN_AKM_SUITE_8021X_SHA256:
		Set_ApCli_AuthMode(pAd, staidx, "WPA3");
		break;
	case WLAN_AKM_SUITE_PSK_SHA256:
		Set_ApCli_AuthMode(pAd, staidx, "WPA2PSK");
		break;
#ifdef SUPP_SAE_SUPPORT
	case WLAN_AKM_SUITE_SAE:
		Set_ApCli_AuthMode(pAd, staidx, "WPA3PSK");
		break;
#endif /* SUPP_SAE_SUPPORT */
#ifdef HOSTAPD_SUITEB_SUPPORT
	case WLAN_AKM_SUITE_8021X_SUITE_B_192:
		Set_ApCli_AuthMode(pAd, staidx, "WPA3-192");
		break;
#endif /* HOSTAPD_SUITEB_SUPPORT */
#ifdef SUPP_OWE_SUPPORT
	case WLAN_AKM_SUITE_OWE:
		Set_ApCli_AuthMode(pAd, staidx, "OWE");
		break;
#endif /* SUPP_OWE_SUPPORT */
	case WLAN_AKM_SUITE_WFA_DPP:
		Set_ApCli_AuthMode(pAd, staidx, "DPP");
		break;
#ifdef SUPP_SAE_SUPPORT
	case WLAN_AKM_SUITE_SAE_EXT:
		Set_ApCli_AuthMode(pAd, staidx, "WPA3PSK_EXT");
		break;
#endif /* SUPP_SAE_SUPPORT */
	default:
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Unknown AkmSuite: %08x\n", pConnInfo->AkmSuite);
		if (pConnInfo->AuthType == MTK_AUTH_SHARED)
			Set_ApCli_AuthMode(pAd, staidx, "SHARED");
		else if (pConnInfo->AuthType == MTK_AUTH_OPEN)
			Set_ApCli_AuthMode(pAd, staidx, "OPEN");
		else
			Set_ApCli_AuthMode(pAd, staidx, "WEPAUTO");
	}

	switch (pConnInfo->Pairwise) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		Set_ApCli_EncrypType(pAd, staidx, "WEP");

		if (pConnInfo->pKey) {
			UCHAR KeyBuf[50] = {0};

			sec_cfg->PairwiseKeyId = pConnInfo->KeyIdx;/* base 0 */

			if (pConnInfo->KeyLen >= sizeof(KeyBuf))
				return FALSE;

			memset(KeyBuf, 0, sizeof(KeyBuf));
			memcpy(KeyBuf, pConnInfo->pKey, pConnInfo->KeyLen);
			KeyBuf[pConnInfo->KeyLen] = 0x00;

			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
				"sec_cfg->DefaultKeyId = %d\n",
				 sec_cfg->PairwiseKeyId);

			Set_Wep_Key_Proc(pAd, (RTMP_STRING *) KeyBuf, (INT) pConnInfo->KeyLen,
					(INT) pConnInfo->KeyIdx);
		}

		break;
	case WLAN_CIPHER_SUITE_TKIP:
		Set_ApCli_EncrypType(pAd, staidx, "TKIP");
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		Set_ApCli_EncrypType(pAd, staidx, "AES");
		break;
	case WLAN_CIPHER_SUITE_GCMP:
		Set_ApCli_EncrypType(pAd, staidx, "GCMP128");
		break;
#ifdef HOSTAPD_SUITEB_SUPPORT
	case WLAN_CIPHER_SUITE_GCMP_256:
		Set_ApCli_EncrypType(pAd, staidx, "GCMP256");
		break;
#endif /* HOSTAPD_SUITEB_SUPPORT */
	case WLAN_CIPHER_SUITE_CCMP_256:
		Set_ApCli_EncrypType(pAd, staidx, "CCMP256");
		break;
	default:
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Unknown Pairwise cipher: %08x\n", pConnInfo->Pairwise);
		Set_ApCli_EncrypType(pAd, staidx, "NONE");
	}

	if (pConnInfo->pBssid != NULL) {
		os_zero_mem(pStaCfg->CfgApCliBssid, MAC_ADDR_LEN);
		NdisCopyMemory(pStaCfg->CfgApCliBssid, pConnInfo->pBssid, MAC_ADDR_LEN);
	}

#ifdef DOT11W_PMF_SUPPORT
	pPmfCfg->Desired_MFPC = pConnInfo->mfpc;
	pPmfCfg->Desired_MFPR = pConnInfo->mfpr;

	pPmfCfg->MFPC = pPmfCfg->Desired_MFPC;
	pPmfCfg->MFPR = pPmfCfg->Desired_MFPR;
	pPmfCfg->PMFSHA256 = pPmfCfg->MFPR;

	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_PMF, DBG_LVL_INFO,
		"[PMF]: MFPC=%d, MFPR=%d, SHA256=%d\n",
		pPmfCfg->MFPC, pPmfCfg->MFPR, pPmfCfg->PMFSHA256);
#endif /* DOT11W_PMF_SUPPORT */

	OPSTATUS_SET_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = TRUE;
	Set_ApCli_Ssid(pAd, staidx, (RTMP_STRING *) SSID);

	Set_ApCli_Enable(pAd, staidx, "1");

	upSpecificApCliIf(pAd, staidx);

	MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_NOTICE,
		"80211> APCLI CONNECTING SSID = %s\n", SSID);
#endif /* APCLI_CFG80211_SUPPORT */

	return TRUE;
}


VOID CFG80211DRV_RegNotify(
	VOID						*pAdOrg,
	VOID						*pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	struct mt_cfg80211_io_reg_notify *pRegInfo;

	pRegInfo = (struct mt_cfg80211_io_reg_notify *)pData;
	/* keep Alpha2 and we can re-call the function when interface is up */
	pAd->cfg80211_ctrl.Cfg80211_Alpha2[0] = pRegInfo->Alpha2[0];
	pAd->cfg80211_ctrl.Cfg80211_Alpha2[1] = pRegInfo->Alpha2[1];

	/* apply the new regulatory rule only when cert mode */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)) {
		/* interface is up */
		if (pAd->CommonCfg.wifi_cert)
			CFG80211_RegRuleApply(pAd, pRegInfo->pWiphy, (UCHAR *)pRegInfo->Alpha2);
	} else
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"crda> interface is down!\n");
}


VOID CFG80211DRV_SurveyGet(
	VOID						*pAdOrg,
	VOID						*pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_SURVEY *pSurveyInfo;
#ifdef AP_QLOAD_SUPPORT
	QLOAD_CTRL *pQloadCtrl = HcGetQloadCtrl(pAd);
#endif
	pSurveyInfo = (CMD_RTPRIV_IOCTL_80211_SURVEY *)pData;
	pSurveyInfo->pCfg80211 = pAd->pCfg80211_CB;
#ifdef AP_QLOAD_SUPPORT
	pSurveyInfo->ChannelTimeBusy = pQloadCtrl->QloadLatestChannelBusyTimePri;
	pSurveyInfo->ChannelTimeExtBusy = pQloadCtrl->QloadLatestChannelBusyTimeSec;
#endif /* AP_QLOAD_SUPPORT */
}


/*
 * ========================================================================
 * Routine Description:
 *	Parse and handle country region in beacon from associated AP.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *	pVIE			- Beacon elements
 *	LenVIE			- Total length of Beacon elements
 *
 * Return Value:
 *	NONE
 * ========================================================================
 */
VOID CFG80211_BeaconCountryRegionParse(
	IN VOID						*pAdCB,
	IN NDIS_802_11_VARIABLE_IEs * pVIE,
	IN UINT16					LenVIE)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	UCHAR *pElement = (UCHAR *)pVIE;
	UINT32 LenEmt;

	while (LenVIE > 1) {
		pVIE = (NDIS_802_11_VARIABLE_IEs *)pElement;

		LenEmt = pVIE->Length + 2;
		if (pVIE->ElementID == IE_COUNTRY
			&& (LenEmt <= LenVIE)) {
			/* send command to do regulation hint only when associated */
			/* RT_CFG80211_CRDA_REG_HINT11D(pAd, pVIE->data, pVIE->Length); */
			RTEnqueueInternalCmd(pAd, CMDTHREAD_REG_HINT_11D,
								 pVIE->data, pVIE->Length);
			break;
		}

		if (LenVIE <= LenEmt)
			break; /* length is not enough */

		pElement += LenEmt;
		LenVIE -= LenEmt;
	}
} /* End of CFG80211_BeaconCountryRegionParse */

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
VOID CFG80211_RegHint(
	IN VOID						*pAdCB,
	IN UCHAR					*pCountryIe,
	IN ULONG					CountryIeLen)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	CFG80211OS_RegHint(CFG80211CB, pCountryIe, CountryIeLen);
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
VOID CFG80211_RegHint11D(
	IN VOID						*pAdCB,
	IN UCHAR					*pCountryIe,
	IN ULONG					CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	CFG80211OS_RegHint11D(CFG80211CB, pCountryIe, CountryIeLen);
}


/*
 * ========================================================================
 * Routine Description:
 *	Apply new regulatory rule.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *	pWiphy			- Wireless hardware description
 *	pAlpha2			- Regulation domain (2B)
 *
 * Return Value:
 *	NONE
 *
 * Note:
 *	Can only be called when interface is up.
 *
 *	For general mac80211 device, it will be set to new power by Ops->config()
 *	In rt2x00/, the settings is done in rt2x00lib_config().
 * ========================================================================
 */
VOID CFG80211_RegRuleApply(
	IN VOID						*pAdCB,
	IN VOID						*pWiphy,
	IN UCHAR					*pAlpha2)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	VOID *pBand24G, *pBand5G, *pBand6G;
	UINT32 IdBand, IdChan, IdPwr;
	UINT32 ChanNum, ChanId, Power, RecId, DfsType;
	BOOLEAN FlgIsRadar;
	ULONG IrqFlags;
	CHANNEL_CTRL *pChCtrl = NULL;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"crda> ==>\n");
	/* init */
	pBand24G = NULL;
	pBand5G = NULL;
	pBand6G = NULL;

	if (pAd == NULL)
		return;

	RTMP_IRQ_LOCK(&PD_GET_DEVICE_IRQ(pAd->physical_dev), IrqFlags);
	/* zero first */
	/* 2.4GHZ & 5GHz */
	RecId = 0;
	/* find the DfsType */
	DfsType = CE;
	pBand24G = NULL;
	pBand5G = NULL;
	pBand6G = NULL;

	if (CFG80211OS_BandInfoGet(CFG80211CB, pWiphy, &pBand24G, &pBand5G, &pBand6G) == FALSE) {
		RTMP_IRQ_UNLOCK(&PD_GET_DEVICE_IRQ(pAd->physical_dev), IrqFlags);
		return;
	}

#ifdef AUTO_CH_SELECT_ENHANCE
#ifdef EXT_BUILD_CHANNEL_LIST

	if ((pAlpha2[0] != '0') && (pAlpha2[1] != '0')) {
		UINT32 IdReg;

		if (pBand5G != NULL) {
			for (IdReg = 0;; IdReg++) {
				if (ChRegion[IdReg].CountReg[0] == 0x00)
					break;

				if ((pAlpha2[0] == ChRegion[IdReg].CountReg[0]) &&
					(pAlpha2[1] == ChRegion[IdReg].CountReg[1])) {
					if (pAd->CommonCfg.DfsType != MAX_RD_REGION)
						DfsType = pAd->CommonCfg.DfsType;
					else
						DfsType = ChRegion[IdReg].op_class_region;

					MTWF_DBG(NULL,
						DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
						"crda> find region %c%c, DFS Type %d\n",
						pAlpha2[0], pAlpha2[1], DfsType);
					break;
				}
			}
		}
	}

#endif /* EXT_BUILD_CHANNEL_LIST */
#endif /* AUTO_CH_SELECT_ENHANCE */

	pAd->CommonCfg.CountryCode[0] = pAlpha2[0];
	pAd->CommonCfg.CountryCode[1] = pAlpha2[1];
	pAd->CommonCfg.bCountryFlag = TRUE;

	for (IdBand = 0; IdBand < IEEE80211_NUM_BANDS; IdBand++) {
		if (((IdBand == NL80211_BAND_2GHZ) && (pBand24G == NULL)) ||
			((IdBand == NL80211_BAND_5GHZ) && (pBand5G == NULL)))
		continue;

		if (IdBand == NL80211_BAND_2GHZ)
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
				"crda> reset chan/power for 2.4GHz\n");
		else
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
				"crda> reset chan/power for 5GHz\n");

		ChanNum = CFG80211OS_ChanNumGet(CFG80211CB, pWiphy, IdBand);

		for (IdChan = 0; IdChan < ChanNum; IdChan++) {
			if (CFG80211OS_ChanInfoGet(CFG80211CB, pWiphy, IdBand, IdChan,
									   &ChanId, &Power, &FlgIsRadar) == FALSE) {
				/* the channel is not allowed in the regulatory domain */
				/* get next channel information */
				continue;
			}

			if (!WMODE_CAP_2G(pAd->CommonCfg.cfg_wmode) && !WMODE_CAP_6G(pAd->CommonCfg.cfg_wmode)) {
				/* 5G-only mode */
				if (ChanId <= MTK_CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}

			if (!WMODE_CAP_5G(pAd->CommonCfg.cfg_wmode) && !WMODE_CAP_6G(pAd->CommonCfg.cfg_wmode)) {
				/* 2.4G-only mode */
				if (ChanId > MTK_CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}
			pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);

			/* zero first */
			os_zero_mem(pChCtrl->ChList, MAX_NUM_OF_CHANNELS * sizeof(CHANNEL_TX_POWER));
			for (IdPwr = 0; IdPwr < MAX_NUM_OF_CHANNELS; IdPwr++) {
				/* sachin - TODO */
				/* if (ChanId == pAd->TxPower[IdPwr].Channel) */
				{
					/* sachin - TODO */
					/* init the channel info. */
					/* os_move_mem(&pAd->ChannelList[RecId],&pAd->TxPower[IdPwr],sizeof(CHANNEL_TX_POWER)); */
					/* keep channel number */
					pChCtrl->ChList[RecId].Channel = ChanId;
					/* keep maximum tranmission power */
					pChCtrl->ChList[RecId].MaxTxPwr = Power;

					/* keep DFS flag */
					if (FlgIsRadar == TRUE)
						pChCtrl->ChList[RecId].DfsReq = TRUE;
					else
						pChCtrl->ChList[RecId].DfsReq = FALSE;

					/* keep DFS type */
					pChCtrl->ChList[RecId].RegulatoryDomain = DfsType;
					/* re-set DFS info. */
					pAd->CommonCfg.RDDurRegion = DfsType;
					MTWF_DBG(NULL,
						DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
						"Chan %03d:\tpower %d dBm, DFS %d, DFS Type %d\n",
						ChanId, Power,
						((FlgIsRadar == TRUE) ? 1 : 0),
						DfsType);
					/* change to record next channel info. */
					RecId++;
					/*break;*/
				}
			}
		}
	}
	if (pChCtrl) {
		pChCtrl->ChListNum = RecId;
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
			"pChCtrl->ChListNum = %d\n", pChCtrl->ChListNum);
	}
	RTMP_IRQ_UNLOCK(&PD_GET_DEVICE_IRQ(pAd->physical_dev), IrqFlags);
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"crda> Number of channels = %d\n", RecId);
} /* End of CFG80211_RegRuleApply */

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
VOID CFG80211_ConnectResultInform(
	IN VOID *pAdCB, IN UCHAR *pBSSID, IN UCHAR ifIndex, IN UCHAR *pReqIe, IN UINT32 ReqIeLen,
	IN UCHAR *pRspIe, IN UINT32 RspIeLen, IN UCHAR FlgIsSuccess)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	struct wifi_dev *wdev = &pAd->StaCfg[ifIndex].wdev;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"80211> ==>\n");

	if (CFG80211DRV_OpsScanRunning(pAd) ||
		RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"Abort running scan\n");
		RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
		CFG80211OS_ScanEnd(pAd, TRUE);
	}

	if (wdev && (pAd->cfg80211_ctrl.FlgCfg80211Connecting))
		CFG80211OS_ConnectResultInform(wdev->if_dev, pBSSID,
			pReqIe, ReqIeLen, pRspIe, RspIeLen, FlgIsSuccess);

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = FALSE;
} /* End of CFG80211_ConnectResultInform */

/*
 * ========================================================================
 * Routine Description:
 *	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.
 *
 * Arguments:
 *	pAdCB			- WLAN control block pointer
 *
 * Return Value:
 *	TRUE			- re-init successfully
 *	FALSE			- re-init fail
 *
 * Note:
 *	CFG80211_SupBandInit() is called in xx_probe().
 *	But we do not have complete chip information in xx_probe() so we
 *	need to re-init bands in xx_open().
 * ========================================================================
 */
BOOLEAN CFG80211_SupBandReInit(
	struct _RTMP_ADAPTER *mac_ad)
{
	struct mt_cfg80211_band BandInfo;
	struct wifi_dev *wdev = &mac_ad->ApCfg.MBSSID[MAIN_MBSSID].wdev;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"80211> re-init bands...\n");
	/* re-init bands */
	os_zero_mem(&BandInfo, sizeof(BandInfo));
	cfg80211_drv_get_band_info(mac_ad, wdev, &BandInfo);
	return mt_cfg80211_sup_band_reinit(wdev->if_dev, &BandInfo);
}

#ifdef CONFIG_STA_SUPPORT
INT CFG80211_setStaDefaultKey(
	IN VOID                     *pAdCB,
	IN UINT					Data
)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "Set Sta Default Key: %d\n", Data);
	pAd->StaCfg[0].wdev.SecConfig.PairwiseKeyId = Data; /* base 0 */
	return 0;
}
#endif /*CONFIG_STA_SUPPORT*/

INT CFG80211_reSetToDefault(
	IN VOID                                         *pAdCB)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "\n");
#ifdef CONFIG_STA_SUPPORT
	/* Driver Internal Parm */
	pAd->StaCfg[0].bAutoConnectByBssid = FALSE;
#endif /*CONFIG_STA_SUPPORT*/
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = FALSE;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = FALSE;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;
	pCfg80211_ctrl->Cfg80211RocTimerInit = FALSE;
	pCfg80211_ctrl->Cfg80211RocTimerRunning = FALSE;
	pCfg80211_ctrl->FlgCfg80211Scanning = FALSE;
	/* pCfg80211_ctrl->isMccOn = FALSE; */
	return TRUE;
}

/* initList(&pAd->Cfg80211VifDevSet.vifDevList); */
/* initList(&pAd->cfg80211_ctrl.cfg80211TxPacketList); */
#if defined(APCLI_CFG80211_SUPPORT)
BOOLEAN CFG80211_checkScanResInKernelCache(
	IN VOID *pAdCB,
	IN UCHAR *pBSSID,
	IN UCHAR *pSsid,
	IN INT ssidLen)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	CFG80211_CB *pCfg80211_CB  = (CFG80211_CB *)pAd->pCfg80211_CB;
	struct wiphy *pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	struct cfg80211_bss *bss;

	bss = cfg80211_get_bss(pWiphy, NULL, pBSSID,
						   pSsid, ssidLen,
						   WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);

	if (bss) {
		CFG80211OS_PutBss(pWiphy, bss);
		return TRUE;
	}

	return FALSE;
}

BOOLEAN CFG80211_checkScanTable(
	IN VOID *pAdCB)
{
#ifndef APCLI_CFG80211_SUPPORT
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	CFG80211_CB *pCfg80211_CB  = (CFG80211_CB *)pAd->pCfg80211_CB;
	struct wiphy *pWiphy = pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ULONG bss_idx = BSS_NOT_FOUND;
	struct cfg80211_bss *bss;
	struct ieee80211_channel *chan;
	UINT32 CenFreq;
	UINT64 timestamp;
	struct timeval tv;
	UCHAR *ie, ieLen = 0;
	BOOLEAN isOk = FALSE;
	BSS_ENTRY *pBssEntry;
	USHORT ifIndex = 0;
	PSTA_ADMIN_CONFIG pApCliEntry = NULL;
	struct wifi_dev *wdev = NULL;
	BSS_TABLE *ScanTab = NULL;

	pApCliEntry = &pAd->StaCfg[ifIndex];
	wdev = &pApCliEntry->wdev;
	ScanTab = get_scan_tab_by_wdev(pAd, wdev);

	if (MAC_ADDR_EQUAL(pApCliEntry->MlmeAux.Bssid, ZERO_MAC_ADDR)) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"pAd->ApCliMlmeAux.Bssid ==> ZERO_MAC_ADDR\n");
		/* ToDo: pAd->StaCfg[0].CfgApCliBssid */
		return FALSE;
	}

	/* Fake TSF */
	do_gettimeofday(&tv);
	timestamp = ((UINT64)tv.tv_sec * 1000000) + tv.tv_usec;
	bss = cfg80211_get_bss(pWiphy, NULL, pApCliEntry->MlmeAux.Bssid,
						   pApCliEntry->MlmeAux.Ssid, pApCliEntry->MlmeAux.SsidLen,
						IEEE80211_BSS_TYPE_ESS, IEEE80211_PRIVACY_ANY);

	if (bss) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "Found %s in Kernel_ScanTable with CH[%d]\n", pApCliEntry->MlmeAux.Ssid, bss->channel->center_freq);
		CFG80211OS_PutBss(pWiphy, bss);
		return TRUE;
	}
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
		"Can't Found %s in Kernel_ScanTable & Try Fake it\n", pApCliEntry->MlmeAux.Ssid);

	bss_idx = BssSsidTableSearchBySSID(ScanTab, pApCliEntry->MlmeAux.Ssid, pApCliEntry->MlmeAux.SsidLen);

	if (bss_idx != BSS_NOT_FOUND) {
		/* Since the cfg80211 kernel scanTable not exist this Entry,
		 * Build an Entry for this connect inform event.
			 */
		pBssEntry = &ScanTab->BssEntry[bss_idx];

		if (ScanTab->BssEntry[bss_idx].Channel > 14)
			CenFreq = ieee80211_channel_to_frequency(pBssEntry->Channel, IEEE80211_BAND_5GHZ);
		else
			CenFreq = ieee80211_channel_to_frequency(pBssEntry->Channel, IEEE80211_BAND_2GHZ);

		chan = ieee80211_get_channel(pWiphy, CenFreq);
		ieLen = 2 + pApCliEntry->MlmeAux.SsidLen + pBssEntry->VarIeFromProbeRspLen;
		os_alloc_mem(NULL, (UCHAR **)&ie, ieLen);

		if (!ie) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"Memory Allocate Fail\n");
			return FALSE;
		}

		ie[0] = WLAN_EID_SSID;
		ie[1] = pApCliEntry->MlmeAux.SsidLen;
		NdisCopyMemory(ie + 2, pApCliEntry->MlmeAux.Ssid, pApCliEntry->MlmeAux.SsidLen);
		NdisCopyMemory(ie + 2 + pApCliEntry->MlmeAux.SsidLen, pBssEntry->pVarIeFromProbRsp,
					   pBssEntry->VarIeFromProbeRspLen);
		bss = cfg80211_inform_bss(pWiphy, chan,
								  pApCliEntry->MlmeAux.Bssid, timestamp, WLAN_CAPABILITY_ESS, pApCliEntry->MlmeAux.BeaconPeriod,
								  ie, ieLen,
#ifdef CFG80211_SCAN_SIGNAL_AVG
								  (pBssEntry->AvgRssi * 100),
#else
								  (pBssEntry->Rssi * 100),
#endif
								  GFP_KERNEL);

		if (bss) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
					 "Fake New %s("MACSTR") in Kernel_ScanTable with CH[%d][%d] BI:%d len:%d\n",
					  pApCliEntry->MlmeAux.Ssid,
					  MAC2STR(pApCliEntry->MlmeAux.Bssid), bss->channel->center_freq, pBssEntry->Channel,
					  pApCliEntry->MlmeAux.BeaconPeriod, pBssEntry->VarIeFromProbeRspLen);
			CFG80211OS_PutBss(pWiphy, bss);
			isOk = TRUE;
		}

		if (ie != NULL)
			os_free_mem(ie);

		if (isOk)
			return TRUE;
	} else
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				 "%s Not In Driver Scan Table\n", pApCliEntry->MlmeAux.Ssid);

	return FALSE;
#else
	return TRUE;
#endif /* APCLI_CFG80211_SUPPORT */
}
#endif /* APCLI_CFG80211_SUPPORT */

/* CFG_TODO */
UCHAR CFG80211_getCenCh(RTMP_ADAPTER *pAd, UCHAR prim_ch)
{
	UCHAR ret_channel = 0;
	struct wifi_dev *wdev = NULL;
	UCHAR ht_bw = 0;
	UCHAR ext_cha = 0;

	wdev = get_default_wdev(pAd);
	if (wdev) {
		ht_bw = wlan_operate_get_ht_bw(wdev);
		ext_cha = wlan_operate_get_ext_cha(wdev);
	}

	if (ht_bw == BW_40) {
		if (ext_cha == EXTCHA_ABOVE)
			ret_channel = prim_ch + 2;
		else {
			if (prim_ch == 14)
				ret_channel = prim_ch - 1;
			else
				ret_channel = prim_ch - 2;
		}
	} else
		ret_channel = prim_ch;

	return ret_channel;
}

VOID CFG80211_InitTxSCallBack(RTMP_ADAPTER *pAd)
{
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO, "\n");
}

#ifdef CONFIG_VLAN_GTK_SUPPORT
struct wifi_dev *CFG80211_GetWdevByVlandev(PRTMP_ADAPTER pAd, PNET_DEV vlan_dev)
{
	UINT8 apidx;
	struct wifi_dev *wdev = NULL;

	if (!pAd)
		return NULL;

	for (apidx = MAIN_MBSSID; apidx < pAd->ApCfg.BssidNum; apidx++) {
		PNET_DEV tmp_ndev;
		UINT8 ifname_len;
		char *pch;

		tmp_ndev = pAd->ApCfg.MBSSID[apidx].wdev.if_dev;
		if (!tmp_ndev)
			continue;

		pch = strchr(vlan_dev->name, '.');
		ifname_len = pch - tmp_ndev->name;
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
				"ifname=%s, vlan_ifname=%s\n",
				tmp_ndev->name, vlan_dev->name);
		if (strncmp(tmp_ndev->name, vlan_dev->name, ifname_len) == 0) {
			wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
			break;
		}
	}

	return wdev;
}

BOOLEAN CFG80211_MatchVlandev(struct wifi_dev *wdev, PNET_DEV vlan_dev)
{
	struct list_head *listptr;
	struct vlan_gtk_info *vg_info;

	if (!wdev)
		return FALSE;

	if (wdev->vlan_cnt == 0)
		return FALSE;

	list_for_each(listptr, &wdev->vlan_gtk_list) {
		vg_info = list_entry(listptr, struct vlan_gtk_info, list);
		if (vg_info && vg_info->vlan_dev == vlan_dev)
			return TRUE;
	}

	return FALSE;
}

struct vlan_gtk_info *CFG80211_GetVlanInfoByVlandev(struct wifi_dev *wdev, PNET_DEV vlan_dev)
{
	struct list_head *listptr;
	struct vlan_gtk_info *vg_info;

	if (!wdev)
		return NULL;

	list_for_each(listptr, &wdev->vlan_gtk_list) {
		vg_info = list_entry(listptr, struct vlan_gtk_info, list);
		if (vg_info && vg_info->vlan_dev == vlan_dev)
			return vg_info;
	}

	return NULL;
}

struct vlan_gtk_info *CFG80211_GetVlanInfoByVlanid(struct wifi_dev *wdev, UINT16 vlan_id)
{
	struct list_head *listptr;
	struct vlan_gtk_info *vg_info;

	if (!wdev)
		return NULL;

	list_for_each(listptr, &wdev->vlan_gtk_list) {
		vg_info = list_entry(listptr, struct vlan_gtk_info, list);
		if (vg_info && vg_info->vlan_id == vlan_id)
			return vg_info;
	}

	return NULL;
}

/* return VLAN ID on success */
INT16 CFG80211_IsVlanPkt(PNDIS_PACKET pkt)
{
	UINT16 vlan_id = 0;
	UINT16 TypeLen = 0;
	struct sk_buff *skb = RTPKT_TO_OSPKT(pkt);
	UCHAR *data;

	if (!skb || skb->len < 16)
		return vlan_id;

	data = skb->data;

	TypeLen = (data[12] << 8) | data[13];
	if (TypeLen == ETH_TYPE_VLAN) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO, "%s() get vlan pkt\n", __func__);
		vlan_id = ((data[14] << 8) + data[15]) & 0xfff;
	}

	return vlan_id;
}
#endif /* CONFIG_VLAN_GTK_SUPPORT */

static int CFG80211DRV_OpsStartAp(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_AP_SUPPORT
	struct mt_priv_ops_ap_setting *ap_setting;
	struct mt_priv_ops_beacon *bcn;
	struct mt_priv_ops_beacon_cfg *cfg;
	struct mt_priv_ops_chan_info *chan_info;
	UINT apidx = 0;
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE, "80211> ==>\n");

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		apidx = (UINT)wdev->func_idx;
		if (apidx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid wdev idx %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;

	if (ctrl_data == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
			"failed - [ERROR]chandef.chan NULL.\n");
		ret = -RTMP_IO_EINVAL;
		goto end;
	}

	/* update info into bcn structure */
	ap_setting = (struct mt_priv_ops_ap_setting *)ctrl_data->data;
	bcn = &ap_setting->bcn;
	cfg = &ap_setting->bcn_cfg;
	chan_info = &ap_setting->chan_info;
	bcn->apidx = apidx;
	bcn->pNetDev = ctrl_data->net_dev;

#ifdef DOT11_EHT_BE
	MBSS_Reconfig_Init(pAd, &pAd->ApCfg.MBSSID[apidx]);
	mutex_init(&pAd->ApCfg.MBSSID[apidx].mld_mutex);
#endif

	if (cfg->flags & MT_FEATURE_11V_MBSS) {
		/* Currently should be only for AX Cert. Case */
		pAd->ApCfg.dot11v_mbssid_bitmap |= 1 << apidx;

		if (apidx == 0) {
			pAd->ApCfg.MBSSID[apidx].mbss_11v.mbss_11v_enable = MBSS_11V_T;
			pAd->ApCfg.dot11v_BssidNum[apidx] = pAd->ApCfg.BssidNum;
			pAd->ApCfg.dot11v_max_indicator[apidx] =
					mbss_11v_bssid_num_to_max_indicator(pAd->ApCfg.BssidNum);
		} else {
			pAd->ApCfg.MBSSID[apidx].mbss_11v.mbss_11v_enable = MBSS_11V_NT;
		}
		pAd->ApCfg.MBSSID[apidx].mbss_11v.mbss_11v_t_bss_idx = 0;
		entrytb_aid_bitmap_reserve(pAd, &pAd->aid_info);
	}

	if (cfg->enable_1x) {
		MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> This is a 1X wdev\n");
		wdev->IsCFG1xWdev = TRUE;
	} else
		wdev->IsCFG1xWdev = FALSE;

#ifdef CONFIG_6G_SUPPORT
	if (cfg->unsol_6g_interval
		&& cfg->unsol_6g_interval <= 0xFF)
		in_band_discovery_update_conf(wdev, UNSOLICIT_TX_PROBE_RSP,
			cfg->unsol_6g_interval, UNSOLICIT_TXMODE_NON_HT, TRUE);
#endif /* CONFIG_6G_SUPPORT */

	if (pAd->CommonCfg.wifi_cert
		&& pAd->ApCfg.AutoChannelAlg == 0) {
		/* set channel callback has been replaced by chandef of cfg80211_ap_settings */
		CFG80211_CB *p80211CB;

		p80211CB = pAd->pCfg80211_CB;

		if (p80211CB == NULL) {
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_ERROR,
				"80211> p80211CB == NULL!\n");
			goto end;
		}
		chan_info->MonFilterFlag = p80211CB->MonFilterFlag;
		/* set channel */
		CFG80211DRV_OpsSetChannel(pAd, chan_info);
	}

	/*ht green filed*/
#ifdef DOT11_N_SUPPORT
	if (pAd->CommonCfg.wifi_cert) {
		if (cfg->ht_cap) {
			if (cfg->ht_cap == MTK_HTMODE_GF)
				pAd->CommonCfg.RegTransmitSetting.field.HTMODE = HTMODE_GF;
			else
				pAd->CommonCfg.RegTransmitSetting.field.HTMODE = HTMODE_MM;

			wlan_config_set_ht_mode(
				wdev, pAd->CommonCfg.RegTransmitSetting.field.HTMODE);
			wlan_operate_loader_greenfield(
				wdev, pAd->CommonCfg.RegTransmitSetting.field.HTMODE);

			SetCommonHtVht(pAd, wdev);

			MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
				"CFG80211:Setting HTMODE = %s!\n",
				pAd->CommonCfg.RegTransmitSetting.field.HTMODE ? "HT_GF" : "HT_MM");
		}
	}
#endif

	CFG80211DRV_OpsBeaconAdd(pAd, bcn, cfg);
#ifdef DOT11_EHT_BE
	/* sync ML Probe.rsp Per-STA Profile buffer */
	bss_mngr_mld_sync_ml_probe_rsp(wdev);
#endif/*DOT11_EHT_BE*/
end:
#endif /* CONFIG_AP_SUPPORT */
	return ret;
}

#ifdef CONFIG_AP_SUPPORT
VOID CFG80211_UpdateAssocRespExtraIe(
	struct _RTMP_ADAPTER *pAd,
	UINT32 apidx,
	UCHAR *assocresp_ies,
	UINT32 assocresp_ies_len)
{
	UINT32 len = 0;
	PEID_STRUCT eid_ptr = (PEID_STRUCT)assocresp_ies;
	PUCHAR pAssocRespBuf = (PUCHAR)pAd->ApCfg.MBSSID[apidx].AssocRespExtraIe;

	MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_ASSOC, DBG_LVL_INFO, "80211> ==>\n");
	MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_ASSOC, DBG_LVL_INFO,
		"IE len = %d\n", assocresp_ies_len);
	if (assocresp_ies_len > sizeof(pAd->ApCfg.MBSSID[apidx].AssocRespExtraIe)) {
		MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_ASSOC, DBG_LVL_INFO,
			"AssocResp buf size not enough\n");
		return;
	}

	while (((UCHAR *)eid_ptr + eid_ptr->Len + 1) < (assocresp_ies + assocresp_ies_len)) {
		switch (eid_ptr->Eid) {
		case IE_EXT_CAPABILITY:
			break;
		default:
			NdisCopyMemory(pAssocRespBuf+len, (UCHAR *)eid_ptr, (eid_ptr->Len + 2));
			len += eid_ptr->Len + 2;
			break;
		}
		eid_ptr = (PEID_STRUCT)((UCHAR *)eid_ptr + 2 + eid_ptr->Len);
		if (((UCHAR *)eid_ptr >= (assocresp_ies + assocresp_ies_len))
			|| ((UCHAR *)(eid_ptr+2) > (assocresp_ies + assocresp_ies_len)))
			break;
	}

	pAd->ApCfg.MBSSID[apidx].AssocRespExtraIeLen = len;
}

#endif /* CONFIG_AP_SUPPORT */

static int CFG80211DRV_OpsChangeBeacon(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_priv_ops_beacon *bcn;
	int ret = 0;
	INT apidx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO, "80211> ==>\n");
	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid wdev idx %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
		apidx = wdev->func_idx;
	} else
		return -RTMP_IO_EFAULT;

	bcn = ctrl_data->data;
	if (bcn->beacon_head) {
		bcn->apidx = get_apidx_by_addr(pAd, bcn->beacon_head+10);
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
			"apidx %d\n", bcn->apidx);

		/* Update assoc resp extra ie */
		if (bcn->assocresp_ies_len && bcn->assocresp_ies)
			CFG80211_UpdateAssocRespExtraIe(
				pAd, bcn->apidx, (UCHAR *)bcn->assocresp_ies, bcn->assocresp_ies_len);
	}
	/* update beacon ies */
	CFG80211DRV_OpsBeaconSet(pAd, bcn, NULL);
#ifdef DOT11_EHT_BE
	/* sync ML Probe.rsp Per-STA Profile buffer */
	bss_mngr_mld_sync_ml_probe_rsp(wdev);
#endif/*DOT11_EHT_BE*/

	return ret;

}

static int CFG80211DRV_OpsStopAp(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_AP_SUPPORT
	INT apidx = -1, i;
	struct _NDIS_AP_802_11_PMKID *pmkid_cache;
#ifdef DOT11_EHT_BE
	UCHAR *own_mac_mld = NULL;
#endif /*DOT11_EHT_BE*/
	PMAC_TABLE_ENTRY pEntry;

	if (RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_HALT_IN_PROGRESS)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> ==> fRTMP_ADAPTER_HALT_IN_PROGRESS\n");
		return 0;
	}
	if (!wdev)
		return 0;
	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> ==> not ap, ignore\n");
		return 0;
	}
	apidx = wdev->func_idx;
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
		"80211> ==> apidx = %d\n", apidx);

	for (i = 0; VALID_UCAST_ENTRY_WCID(mac_ad, i); i++) {
		pEntry = entry_get(mac_ad, i);

		if (pEntry && IS_ENTRY_CLIENT(pEntry) && pEntry->wdev == wdev)
			APMlmeKickOutSta(mac_ad, pEntry->Addr, pEntry->wcid, REASON_DISASSOC_INACTIVE);
	}

	if (!VALID_MBSS(mac_ad, apidx)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_ERROR,
			"80211> wdev not found<==\n");
		return -ENODEV;
	}
	pmkid_cache = PD_GET_PMKID_PTR(mac_ad->physical_dev);
#ifdef DOT11_EHT_BE
	MBSS_Reconfig_Deinit(mac_ad, &mac_ad->ApCfg.MBSSID[apidx]);
#endif
	RTMP_DRIVER_80211_BEACON_DEL(mac_ad, apidx);
#ifdef DOT11_EHT_BE
	/* sync ML Probe.rsp Per-STA Profile buffer */
	bss_mngr_mld_sync_ml_probe_rsp(wdev);
	own_mac_mld = wdev->bss_info_argument.mld_info.mld_addr;
#endif /* DOT11_EHT_BE */
	/* flush all pmk cache while ap stop*/
	OS_SEM_LOCK(&pmkid_cache->pmkid_lock);
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
		"80211> flush pmkid cache of bss ("MACSTR")<==\n",
		MAC2STR(wdev->if_addr));
	for (i = 0; i < MAX_PMKID_COUNT; i++) {
		RTMPDeletePMKIDCache(pmkid_cache, wdev->if_addr, i);
#ifdef DOT11_EHT_BE
		if (!MAC_ADDR_EQUAL(own_mac_mld, wdev->if_addr)
			&& !MAC_ADDR_EQUAL(own_mac_mld, ZERO_MAC_ADDR)) {
			MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
				"clear PMKID cache for MLO, own mac mld="MACSTR"\n", own_mac_mld);
			RTMPDeletePMKIDCache(pmkid_cache, own_mac_mld, i);
		}
#endif /* DOT11_EHT_BE */
	}
	OS_SEM_UNLOCK(&pmkid_cache->pmkid_lock);
#endif /* CONFIG_AP_SUPPORT */

	return 0;
}

static int CFG80211DRV_OpsGetChannel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_band BandInfo = {0};
	struct mtk_mac_dev *mac_dev = NULL;
	struct mtk_mac_phy *mac_phy = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_chan_info *chaninfo;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "\n");
	if (!ctrl_data_) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR, "NULL input!\n");
		return -ENODEV;
	}
	chaninfo = ctrl_data->data;

	if (!RTMP_TEST_FLAG(pAd->physical_dev, PH_DEV_SYSTEM_START_UP)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN, "physical_dev not ready!\n");
		return -RTMP_IO_ENODEV;
	}
	cfg80211_drv_get_band_info(pAd, wdev, &BandInfo);
	mac_dev = hc_get_mac_dev(pAd);
	mac_phy = &mac_dev->mac_phy;
	if (!mac_phy)
		return -RTMP_IO_ENETDOWN;

	OS_SEM_LOCK(&mac_phy->lock);
	if (!mac_phy->state) {
		OS_SEM_UNLOCK(&mac_phy->lock);
		return -ENETDOWN;
	}
#ifndef IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ
	if (mac_phy->bw > BW_160) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"320M not support in cfg80211!\n");
		OS_SEM_UNLOCK(&mac_phy->lock);
		return -EOPNOTSUPP;
	}
#endif /* IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ */
	chaninfo->phy_mode = BandInfo.phy_mode;
	chaninfo->ChanId = mac_phy->chan;
	switch (mac_phy->bw) {
	case BW_20:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_20;
		break;
	case BW_40:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_40;
		break;
	case BW_80:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_80;
		break;
	case BW_8080:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_80P80;
		break;
	case BW_160:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_160;
		break;
#ifdef IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ
	case BW_320:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_320;
		break;
#endif /* IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ */
	default:
		chaninfo->bw = MT_NL80211_CHAN_WIDTH_20_NOHT;
		break;
	}
	chaninfo->CenterChanId = mac_phy->cen_chan;
	OS_SEM_UNLOCK(&mac_phy->lock);

	return 0;
}

static int CFG80211DRV_OpsStartRadarDetect(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CFG80211_FULL_OPS_SUPPORT
	struct DOT11_H *pDot11h = NULL;
	PDFS_PARAM pDfsParam = NULL;
	UCHAR rd_region = 0;
	INT ret = FALSE;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	u32 cac_time_ms;
	UCHAR BandIdx;

	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
		"80211> ==>\n");

	pDot11h = &pAd->Dot11_H;
	if (pDot11h == NULL)
		return -RTMP_IO_EINVAL;

	BandIdx = HcGetBandByWdev(wdev);
	pDfsParam = &pAd->CommonCfg.DfsParameter;
	rd_region = pAd->CommonCfg.RDDurRegion;
	cac_time_ms = ctrl_data->uint_val;

	/* DfsCacNormalStart(pAd, wdev, pDot11h->ChannelMode); */
	pDot11h->RDCount = 0;
	pDot11h->cac_time = cac_time_ms;

	if (WMODE_CAP_5G(wdev->PhyMode)) {
		ret = mtRddControl(pAd, RDD_STOP, HW_RDD1, 0, 0);
		ret = mtRddControl(pAd, RDD_START, HW_RDD1, RXSEL_0, rd_region);
		ret = mtRddControl(pAd, RDD_DET_MODE, HW_RDD1, 0, RDD_DETMODE_ON);
		pDfsParam->bNoSwitchCh = TRUE;

		if (ret) {
			MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"StartRadarDetect Fail, (ret = %d)\n", ret);
			return -EINVAL;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_HW, CATHW_PHY, DBG_LVL_ERROR,
			"Error DFS Not Support 2GHZ or 6GHZ.\n");
		return -EINVAL;
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"BandIdx = %d, cac_time = %d (secs), ChannelMode = %d\n",
		BandIdx, cac_time_ms, pDot11h->ChannelMode);


	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE, "80211> end.\n");
#endif /* CFG80211_FULL_OPS_SUPPORT */
	return 0;
}

static int CFG80211DRV_OpsEndCac(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CFG80211_FULL_OPS_SUPPORT
	struct DOT11_H *pDot11h = NULL;
	PDFS_PARAM pDfsParam = NULL;
	UCHAR BandIdx;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "80211> ==>\n");

	BandIdx = HcGetBandByWdev(wdev);
	pDfsParam = &pAd->CommonCfg.DfsParameter;

	pDot11h = &pAd->Dot11_H;
	if (pDot11h == NULL)
		return 0;

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
		"set CAC value to 0, CAC End, origin is %d.\n",	pDot11h->RDCount);

	if ((pDot11h->ChannelMode == CHAN_SILENCE_MODE)
		|| (pDfsParam->BW160ZeroWaitSupport == TRUE))
		pDot11h->RDCount = pDot11h->cac_time;

	pDfsParam->DedicatedOutBandCacCount = pDfsParam->DedicatedOutBandCacTime;
	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE, "80211> end.\n");
#endif /* CFG80211_FULL_OPS_SUPPORT */
	return 0;
}

static int CFG80211DRV_OpsScan(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	CFG80211_CB *pCfg80211_CB;
	RT_CMD_STA_IOCTL_SCAN scan_cmd;
	CHAR staIndex, apIndex;
	UCHAR ssid[32] = {0};
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	UINT iftype = ctrl_data->uint_val;

	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
		"80211> ==> iftype = %d\n", iftype);

	pCfg80211_CB = pAd->pCfg80211_CB;
	apIndex = wdev->func_idx;
	staIndex = wdev->func_idx;
	if (iftype == NL80211_IFTYPE_STATION) {
		if (staIndex != WDEV_NOT_FOUND) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"Scan Request for Apcli i/f proceed for scanning\n");
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
				"Invaliad APCLI index i/f end scan\n");
			pCfg80211_CB->pCfg80211_ScanReq = ctrl_data->data; /* used in scan end */
			CFG80211OS_ScanEnd(pAd, TRUE);
			return 0;
		}
	} else if (iftype == NL80211_IFTYPE_AP) {
		if (apIndex != WDEV_NOT_FOUND) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"Scan Request for AP i/f proceed for scanning\n");
		} else {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
				"Invaliad Ap index, i/f end Scan.\n");
			pCfg80211_CB->pCfg80211_ScanReq = ctrl_data->data; /* used in scan end */
			CFG80211OS_ScanEnd(pAd, TRUE);
			return 0;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"Invaliad interface type, i/f end Scan, interface type=%d.\n",
			iftype);
		pCfg80211_CB->pCfg80211_ScanReq = ctrl_data->data; /* used in scan end */
		CFG80211OS_ScanEnd(pAd, TRUE);
		return 0;
	}

	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
		"========================================================================\n");
	pCfg80211_CB->pCfg80211_ScanReq = ctrl_data->data; /* used in scan end */

	/* Driver ICOTL Sanity Check */
	if (RTMP_DRIVER_IOCTL_SANITY_CHECK(pAd, NULL) != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR, "80211> Network is down!\n");
		CFG80211OS_ScanEnd(pAd, TRUE);
		return -ENETDOWN;
	}

	/*do dfs cac check for 5G band*/
#ifdef MT_DFS_SUPPORT
	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G) {
		struct DOT11_H *pDot11h = &pAd->Dot11_H;

		if (pDot11h && (pDot11h->ChannelMode == CHAN_SILENCE_MODE)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"still in silence mode, can not trigger scan\n");
			CFG80211OS_ScanEnd(pAd, TRUE);
			return -EBUSY;
		}
	}
#endif

	if (!TakeChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_SCAN, FALSE)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
		"TakeChannelOpCharge fail for SCAN!!\n");
		CFG80211OS_ScanEnd(pAd, TRUE);
		return -EBUSY;
	}

	if (RTMP_DRIVER_80211_SCAN(pAd, iftype) != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"\n\n\n\n\n80211> BUSY - SCANNING\n\n\n\n\n");
		CFG80211OS_ScanEnd(pAd, TRUE);
		ReleaseChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_SCAN);
		return -EBUSY;
	}

	if (pCfg80211_CB->pCfg80211_ScanReq->ie_len != 0) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
			"80211> ExtraIEs Not Null in ProbeRequest from upper layer...\n");
		RTMP_DRIVER_80211_SCAN_EXTRA_IE_SET(pAd);
	} else
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
			"80211> ExtraIEs Null in ProbeRequest from upper layer...\n");
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG,
		"80211> Num %d of SSID from upper layer...\n",
		pCfg80211_CB->pCfg80211_ScanReq->n_ssids);

	/* Set Channel List for this Scan Action */
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG,
		"80211> [%d] Channels In ProbeRequest.\n",
		pCfg80211_CB->pCfg80211_ScanReq->n_channels);

	if (pCfg80211_CB->pCfg80211_ScanReq->n_channels > 0) {
		INT32 *pChanList;
		UINT32  idx, ch_cnt = 0;
		UINT8 band;

		os_alloc_mem(NULL, (UCHAR **)&pChanList,
			sizeof(UINT32) * pCfg80211_CB->pCfg80211_ScanReq->n_channels);

		if (pChanList == NULL) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
				"Alloc memory fail\n");
			CFG80211OS_ScanEnd(pAd, TRUE);
			ReleaseChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_SCAN);
			return -ENOMEM;
		}
		os_zero_mem((PVOID) pChanList,
			sizeof(UINT32) * (pCfg80211_CB->pCfg80211_ScanReq->n_channels));
		for (idx = 0; idx < pCfg80211_CB->pCfg80211_ScanReq->n_channels; idx++) {
			band = pCfg80211_CB->pCfg80211_ScanReq->channels[idx]->band;
			if ((WMODE_CAP_2G(wdev->PhyMode) && band == NL80211_BAND_2GHZ) ||
				(WMODE_CAP_5G(wdev->PhyMode) && band == NL80211_BAND_5GHZ) ||
				(WMODE_CAP_6G(wdev->PhyMode) && band == NL80211_BAND_6GHZ)) {
				pChanList[ch_cnt++] =
					ieee80211_frequency_to_channel(
					pCfg80211_CB->pCfg80211_ScanReq->channels[idx]->center_freq);
				MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG,
					"%d,", pChanList[ch_cnt-1]);
			}
		}

		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG, "\n");
		RTMP_DRIVER_80211_SCAN_CHANNEL_LIST_SET(pAd, pChanList, ch_cnt);

		if (pChanList)
			os_free_mem(pChanList);
	} else {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
				"no channel list for SCAN!!\n");
		CFG80211OS_ScanEnd(pAd, TRUE);
		ReleaseChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_SCAN);
		return -EINVAL;
	}

	memset(&scan_cmd, 0, sizeof(RT_CMD_STA_IOCTL_SCAN));
	memset(ssid, 0, sizeof(ssid));

	if (pCfg80211_CB->pCfg80211_ScanReq->n_ssids && pCfg80211_CB->pCfg80211_ScanReq->ssids) {
		scan_cmd.SsidLen = pCfg80211_CB->pCfg80211_ScanReq->ssids->ssid_len;
		if ((scan_cmd.SsidLen > 0) && (scan_cmd.SsidLen <= sizeof(ssid)))
			memcpy(ssid, pCfg80211_CB->pCfg80211_ScanReq->ssids->ssid, scan_cmd.SsidLen);
		scan_cmd.pSsid = ssid;
	}

	scan_cmd.duration = pCfg80211_CB->pCfg80211_ScanReq->duration;
	scan_cmd.ScanType = SCAN_ACTIVE;
	if (iftype == NL80211_IFTYPE_STATION) {
		scan_cmd.StaIndex = staIndex;
		RTMP_DRIVER_80211_APCLI_SCAN(pAd, &scan_cmd);
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG,
			"Into RTMP_DRIVER_80211_APCLI_SCAN\n");
	} else {
		scan_cmd.ApIndex = apIndex;
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_DEBUG,
			"Into RTMP_DRIVER_80211_AP_SCAN\n");
		RTMP_DRIVER_80211_AP_SCAN(pAd, &scan_cmd);
	}
	return 0;
}

static int CFG80211DRV_OpsAbortScan(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	SCAN_CTRL *ScanCtrl = NULL;

	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE, "80211> ==>\n");

	if (wdev && CFG80211DRV_OpsScanRunning(pAd)) {
		ScanCtrl = get_scan_ctrl_by_wdev(pAd, wdev);
		if (ScanCtrl) {
			ScanCtrl->opAbortScan = TRUE;
			if (TakeChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_ABORT_SCAN, TRUE)) {
				MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
					"scan is aborted with success\n");
				ReleaseChannelOpChargeByBand(pAd, 0, CH_OP_OWNER_ABORT_SCAN);
			} else
				MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
					"fail to abort scan in time\n");
		}
	}
	return 0;
}

static int CFG80211DRV_OpsStaGet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_priv_ops_sta_info *StaInfo;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO, "80211> ==>\n");
	StaInfo = ctrl_data->data;
	/* get sta information */
	if (CFG80211DRV_StaGet(pAd, StaInfo) == FALSE)
		return -RTMP_IO_EFAULT;

	return 0;
}

#define CFG_IS_VALID_MAC(addr) \
	((addr[0])|(addr[1])|(addr[2])|(addr[3])|(addr[4])|(addr[5]))

static INT CFG80211DRV_OpsStaDump(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_priv_ops_sta_info *StaInfo;
	INT i = 0;
	UINT assoc_idx = 0;
	UCHAR is_find = FALSE;
	PMAC_TABLE_ENTRY pEntry = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	UINT idx;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO, "80211> ==>\n");
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
		"request infname: %s, mac:"MACSTR";\n",
		RTMP_OS_NETDEV_GET_DEVNAME(wdev->if_dev), MAC2STR(wdev->if_addr));

	/*init data*/
	StaInfo = ctrl_data->data;
	idx = ctrl_data->uint_val;
	/*from idx find the assoc_idx sta_mac*/
	for (i = 0; VALID_UCAST_ENTRY_WCID(pAd, i); i++) {
		pEntry = entry_get(pAd, i);
		if (pEntry && wdev == pEntry->wdev && (pEntry->Sst == SST_ASSOC) &&
			CFG_IS_VALID_MAC(pEntry->Addr) && (IS_ENTRY_CLIENT(pEntry) || IS_ENTRY_PEER_AP(pEntry))) {
			if (assoc_idx == idx) {
				NdisMoveMemory(StaInfo->MAC, pEntry->Addr, MAC_ADDR_LEN);
				MTWF_DBG(NULL, DBG_CAT_FW, CATFW_DEVINFO, DBG_LVL_INFO,
				"idx: %d, mac: "MACSTR";(TYPE:%x)\n",
				idx, MAC2STR(StaInfo->MAC), pEntry->EntryType);
				is_find = TRUE;
				break;
			}

			assoc_idx++;
		}
	}

	/*find check*/
	if (is_find == FALSE) {
		MTWF_DBG(NULL, DBG_CAT_FW, CATFW_DEVINFO, DBG_LVL_INFO,
			"cann't find sta idx(%d) in sta table!\n", idx);
		return -ENOENT;
	}
	/*get ap stainfo*/
	if (CFG80211DRV_StaGet(pAd, StaInfo) == FALSE)
		return -ENOENT;
	return 0;
}

static int CFG80211DRV_OpsWiphyParamsSet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO, "80211> ==>\n");

	if (ctrl_data->sub_id == MTK_OPS_SET_PARAM_RTS_THRESHOLD) {
		UCHAR i = 0;

		for (i = 0; i < WDEV_NUM_MAX; i++) {
			if (pAd->wdev_list[i] && HcIsRadioAcq(pAd->wdev_list[i])) {
				wdev = pAd->wdev_list[i];
				CFG80211DRV_RtsThresholdAdd(pAd, wdev, ctrl_data->uint_val);
			}
		}
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
			"80211> ==> rts_threshold(%d)\n", ctrl_data->uint_val);
		return 0;
	} else if (ctrl_data->sub_id == MTK_OPS_SET_PARAM_FRAG_THRESHOLD) {
		UCHAR i = 0;

		for (i = 0; i < WDEV_NUM_MAX; i++) {
			if (pAd->wdev_list[i]) {
				wdev = pAd->wdev_list[i];
				CFG80211DRV_FragThresholdAdd(pAd, wdev, ctrl_data->uint_val);
			}
		}
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO,
			"80211> ==> frag_threshold(%d)\n", ctrl_data->uint_val);
		return 0;
	}
#ifdef ACK_CTS_TIMEOUT_SUPPORT
	else if (ctrl_data->sub_id == MTK_OPS_SET_PARAM_COVERAGE_CLASS) {
		UINT ack_time = ctrl_data->uint_val;

		if (CFG80211DRV_AckThresholdAdd(pAd, wdev, ack_time) != TRUE)
			return -EOPNOTSUPP;
		return 0;
	}
#endif/*ACK_CTS_TIMEOUT_SUPPORT*/
	return -RTMP_IO_EOPNOTSUPP;
}

static int CFG80211DRV_OpsKeyAdd(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	CMD_RTPRIV_IOCTL_80211_KEY KeyInfo;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_key_info *cfg_key_info;
	UINT iftype = ctrl_data->uint_val;

	/* init */
	os_zero_mem(&KeyInfo, sizeof(KeyInfo));
	cfg_key_info = (struct mt_priv_ops_key_info *)ctrl_data->data;
	memcpy(KeyInfo.KeyBuf, cfg_key_info->KeyBuf, cfg_key_info->KeyLen);
	KeyInfo.KeyBuf[cfg_key_info->KeyLen] = 0x00;
	KeyInfo.KeyId = cfg_key_info->KeyId;
	KeyInfo.bPairwise = cfg_key_info->bPairwise;
	KeyInfo.KeyLen = cfg_key_info->KeyLen;
	NdisCopyMemory(KeyInfo.MAC, cfg_key_info->MAC, MAC_ADDR_LEN);
#ifdef DOT11W_PMF_SUPPORT
#ifndef APCLI_CFG80211_SUPPORT
#ifdef CONFIG_STA_SUPPORT
	if ((iftype == RT_CMD_80211_IFTYPE_STATION) &&
		(cfg_key_info->cipher == WLAN_CIPHER_SUITE_AES_CMAC)) {
		MAC_TABLE_ENTRY *pEntry = NULL;

		pEntry = GetAssociatedAPByWdev(pAd, wdev);
		memcpy(KeyInfo.KeyBuf, pEntry->SecConfig.GTK, LEN_TK);
		memcpy(KeyInfo.KeyBuf + LEN_TK, cfg_key_info->KeyBuf, cfg_key_info->KeyLen);
		KeyInfo.KeyLen = LEN_TK + cfg_key_info->KeyLen;
		KeyInfo.KeyType = RT_CMD_80211_KEY_WPA;
		KeyInfo.cipher = Ndis802_11AESEnable;
		KeyInfo.KeyId = pEntry->SecConfig.GroupKeyId;
	} else
#endif /* CONFIG_STA_SUPPORT */
#endif /*APCLI_CFG80211_SUPPORT */
#endif	/* DOT11W_PMF_SUPPORT */
		if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_WEP40)
			KeyInfo.KeyType = RT_CMD_80211_KEY_WEP40;
		else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_WEP104)
			KeyInfo.KeyType = RT_CMD_80211_KEY_WEP104;
		else if ((cfg_key_info->cipher == WLAN_CIPHER_SUITE_TKIP) ||
			 (cfg_key_info->cipher == WLAN_CIPHER_SUITE_CCMP)) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_WPA;

			if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_TKIP)
				KeyInfo.cipher = Ndis802_11TKIPEnable;
			else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_CCMP)
				KeyInfo.cipher = Ndis802_11AESEnable;
#ifdef HOSTAPD_SUITEB_SUPPORT
		} else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_GCMP_256) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_WPA;
			KeyInfo.cipher = Ndis802_11GCMP256Enable;
#endif
		} else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_GCMP) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_WPA;
			KeyInfo.cipher = Ndis802_11GCMP128Enable;
		} else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_CCMP_256) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_WPA;
			KeyInfo.cipher = Ndis802_11CCMP256Enable;
		}

#ifdef DOT11W_PMF_SUPPORT
		else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_AES_CMAC) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_AES_CMAC;
			KeyInfo.bPairwise = FALSE;
		}
#ifdef HOSTAPD_SUITEB_SUPPORT
		else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_256) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_AES_GMAC256;
			KeyInfo.bPairwise = FALSE;
		}
#endif
		else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_128) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_AES_GMAC128;
			KeyInfo.bPairwise = FALSE;
		} else if (cfg_key_info->cipher == WLAN_CIPHER_SUITE_BIP_CMAC_256) {
			KeyInfo.KeyType = RT_CMD_80211_KEY_AES_CMAC256;
			KeyInfo.bPairwise = FALSE;
		}
#endif /* DOT11W_PMF_SUPPORT */
		else
			return -RTMP_IO_EOPNOTSUPP;

	KeyInfo.pNetDev = cfg_key_info->pNetDev;

	if ((iftype == RT_CMD_80211_IFTYPE_AP) ||
		(iftype == RT_CMD_80211_IFTYPE_P2P_GO) ||
		(iftype == RT_CMD_80211_IFTYPE_AP_VLAN)) {
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> AP Key Add\n");
#ifdef CONFIG_AP_SUPPORT
		CFG80211DRV_ApKeyAdd(pAd, wdev, &KeyInfo);
#endif /* CONFIG_AP_SUPPORT */
	} else {
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> STA Key Add\n");
#if defined(CONFIG_STA_SUPPORT) || defined(APCLI_CFG80211_SUPPORT)
		RTMP_DRIVER_80211_STA_KEY_ADD(pAd, &KeyInfo);
#endif	/* CONFIG_STA_SUPPORT */
	}

	return 0;
}

static int CFG80211DRV_OpsKeyGet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	UCHAR *tx_tsc;
	UINT32 pn_type_mask = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else
		return -RTMP_IO_EFAULT;
	NdisZeroMemory(&tx_tsc, sizeof(tx_tsc));
	if (ctrl_data->uint_val & MTK_TSC_TYPE_IGTK_PN)
		pn_type_mask |= TSC_TYPE_IGTK_PN_MASK;
	else if (ctrl_data->uint_val & MTK_TSC_TYPE_BIGTK_PN)
		pn_type_mask |= TSC_TYPE_BIGTK_PN_MASK;
	tx_tsc = ctrl_data->data;
	AsicGetTxTsc(pAd, wdev, pn_type_mask, tx_tsc);
#ifdef BCN_PROTECTION_SUPPORT
	wdev->SecConfig.Bigtk_Reinstall = 1;
#endif
	return 0;
}

static int CFG80211DRV_OpsKeyDel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	CMD_RTPRIV_IOCTL_80211_KEY KeyInfo;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_key_info *cfg_key_info;
	UINT iftype = ctrl_data->uint_val;

	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> ==>\n");

	cfg_key_info = (struct mt_priv_ops_key_info *)ctrl_data->data;
	memset(&KeyInfo, 0, sizeof(KeyInfo));
	NdisCopyMemory(KeyInfo.MAC, cfg_key_info->MAC, MAC_ADDR_LEN);
	KeyInfo.KeyId = cfg_key_info->KeyId;
	KeyInfo.pNetDev = cfg_key_info->pNetDev;
	KeyInfo.bPairwise = cfg_key_info->bPairwise;
#ifdef CONFIG_AP_SUPPORT
	if ((iftype == RT_CMD_80211_IFTYPE_AP) ||
		(iftype == RT_CMD_80211_IFTYPE_P2P_GO)) {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> AP Key Del\n");
		RTMP_DRIVER_80211_AP_KEY_DEL(pAd, &KeyInfo);
	} else
#endif /* CONFIG_AP_SUPPORT */
	{
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> STA Key Del\n");
		if (wdev && !MAC_ADDR_EQUAL(KeyInfo.MAC, ZERO_MAC_ADDR)) {
			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
				"80211> STA Key Del -- DISCONNECT\n");
			RTMPWPARemoveAllKeys(pAd, wdev);
		}
	}

	return 0;
}

static int CFG80211DRV_OpsKeyDefaultSet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_key_info *cfg_key_info;
	UINT iftype = ctrl_data->uint_val;

	cfg_key_info = (struct mt_priv_ops_key_info *)ctrl_data->data;
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> ==>\n");
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
		"80211> Default KeyIdx = %d\n", cfg_key_info->KeyId);
#ifdef CONFIG_AP_SUPPORT
	if ((iftype == RT_CMD_80211_IFTYPE_AP) ||
		(iftype == RT_CMD_80211_IFTYPE_P2P_GO))
		RTMP_DRIVER_80211_AP_KEY_DEFAULT_SET(
			pAd, cfg_key_info->pNetDev, cfg_key_info->KeyId);
	else
#endif /* CONFIG_AP_SUPPORT */
		RTMP_DRIVER_80211_STA_KEY_DEFAULT_SET(pAd, cfg_key_info->KeyId);

	return 0;
}


static int CFG80211DRV_OpsDefaultBeaconKeySet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef BCN_PROTECTION_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_key_info *cfg_key_info;
	UINT iftype = ctrl_data->uint_val;

	cfg_key_info = (struct mt_priv_ops_key_info *)ctrl_data->data;
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> ==>\n");
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
		"80211> Default KeyIdx = %d\n", cfg_key_info->KeyId);
#ifdef CONFIG_AP_SUPPORT
	if ((iftype == RT_CMD_80211_IFTYPE_AP) ||
		(iftype == RT_CMD_80211_IFTYPE_P2P_GO))
		RTMP_DRIVER_80211_AP_BEACON_KEY_DEFAULT_SET(pAd, cfg_key_info->pNetDev, cfg_key_info->KeyId);
#endif /* CONFIG_AP_SUPPORT */

	return 0;
#else /* BCN_PROTECTION_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !BCN_PROTECTION_SUPPORT */
}

static int CFG80211DRV_OpsMgmtKeyDefaultSet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)

{
#ifdef DOT11W_PMF_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_key_info *cfg_key_info;
	UINT iftype = ctrl_data->uint_val;

	cfg_key_info = (struct mt_priv_ops_key_info *)ctrl_data->data;
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "80211> ==>\n");
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
		"80211> Default Mgmt KeyIdx = %d\n", cfg_key_info->KeyId);

#ifdef CONFIG_AP_SUPPORT
	if ((iftype == RT_CMD_80211_IFTYPE_AP) ||
		(iftype == RT_CMD_80211_IFTYPE_P2P_GO))
		RTMP_DRIVER_80211_AP_KEY_DEFAULT_MGMT_SET(
			pAd, cfg_key_info->pNetDev, cfg_key_info->KeyId);
#endif

	return 0;
#else /* DOT11W_PMF_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11W_PMF_SUPPORT */
}

static int CFG80211DRV_OpsConnect(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_STA_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_connect_info *ops_connect;
	UINT iftype = ctrl_data->uint_val;
	CMD_RTPRIV_IOCTL_80211_ASSOC_IE AssocIe;

	if (wdev->wdev_type != WDEV_TYPE_STA)
		return -RTMP_IO_EFAULT;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_INFO, "80211> ==>\n");

	ops_connect = (struct mt_priv_ops_connect_info *)ctrl_data->data;
	AssocIe.ie = ops_connect->assoc_ie.ie;
	AssocIe.ie_len = ops_connect->assoc_ie.ie_len;
	AssocIe.pNetDev = ops_connect->pNetDev;
	RTMP_DRIVER_80211_STA_ASSSOC_IE_SET(pAd, &AssocIe, iftype);
	CFG80211DRV_Connect(pAd, wdev, &ops_connect->conn_info);
#endif/*CONFIG_STA_SUPPORT*/
	return 0;
}

static int CFG80211DRV_OpsDisconnect(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_STA_SUPPORT
	MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_INFO, "80211> ==>\n");

	if (wdev->wdev_type != WDEV_TYPE_STA)
		return -RTMP_IO_EFAULT;
	pAd->cfg80211_ctrl.FlgCfg80211Connecting = FALSE;
	SetApCliEnableByWdev(pAd, wdev, FALSE);
#endif /*CONFIG_STA_SUPPORT*/
	return 0;
}

static int CFG80211DRV_OpsExternalAuth(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef APCLI_CFG80211_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_STA, DBG_LVL_NOTICE, "80211> ==>\n");

	auth_fsm_state_transition(wdev, AUTH_FSM_IDLE, __func__);
	cntl_auth_assoc_conf(wdev, CNTL_MLME_AUTH_CONF, ctrl_data->ushort_val);
	return 0;
#else /* APCLI_CFG80211_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !APCLI_CFG80211_SUPPORT */
}

static int CFG80211DRV_OpsRFKillPoll(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef RFKILL_HW_SUPPORT
	UINT32 data = 0;
	BOOLEAN active;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_CMM, DBG_LVL_INFO, "80211> ==>\n");

	/* Read GPIO pin2 as Hardware controlled radio state */
	RTMP_IO_READ32(pAd->hdev_ctrl, GPIO_CTRL_CFG, &data);
	active = !!(data & 0x04);

	if (!active) {
		RTMPSetLED(pAd, LED_RADIO_OFF, DBDC_BAND0);
		ctrl_data->u8_val = 0;
	} else
		ctrl_data->u8_val = 1;
#endif /* RFKILL_HW_SUPPORT */
	return 0;
}

static int CFG80211DRV_OpsSurveyGet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CFG80211_FULL_OPS_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_chan_survey_info *mt_priv_chan_survey_info;
	struct mtk_mac_dev *mac_dev = NULL;
	struct mtk_mac_phy *mac_phy = NULL;
	int idx;

	mac_dev = hc_get_mac_dev(pAd);
	mac_phy = &mac_dev->mac_phy;
	if (!mac_phy)
		return -RTMP_IO_ENETDOWN;
	idx = (int)ctrl_data->uint_val;
	OS_SEM_LOCK(&mac_phy->lock);
	if (!mac_phy->state) {
		OS_SEM_UNLOCK(&mac_phy->lock);
		return -RTMP_IO_ENETDOWN;
	}
	if (idx == 0)
		RTMP_UPDATE_CHANNEL_INFO(pAd, Channel2Index(pAd, mac_phy->chan));
	ctrl_data->u8_val = mac_phy->chan;
	OS_SEM_UNLOCK(&mac_phy->lock);
	mt_priv_chan_survey_info = ctrl_data->data;
	mt_priv_chan_survey_info->busytime = pAd->ScanCtrl.busytime[idx];
	mt_priv_chan_survey_info->rx_time = pAd->ScanCtrl.rx_time[idx];
	mt_priv_chan_survey_info->bss_rx_time = pAd->ScanCtrl.bss_rx_time[idx];
	mt_priv_chan_survey_info->tx_time = pAd->ScanCtrl.tx_time[idx];
	mt_priv_chan_survey_info->ch_AvgNF = pAd->ScanCtrl.ch_AvgNF[idx];
	mt_priv_chan_survey_info->active_time = pAd->ScanCtrl.active_time[idx];
	return 0;
#else /* !CFG80211_FULL_OPS_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* CFG80211_FULL_OPS_SUPPORT */
}

static int CFG80211DRV_OpsPmksaSet(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#if defined(CONFIG_STA_SUPPORT) || defined(APCLI_CFG80211_SUPPORT)
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_NOTICE,
		"80211> STA PMK Cache Set!\n");
	RtmpIoctl_rt_ioctl_siwpmksa(
		pAd, (struct mt_priv_pmksa_info *)ctrl_data->data);
#endif
	return 0;
}

static int CFG80211DRV_OpsPmksaFlush(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifndef APCLI_CFG80211_SUPPORT
#ifdef CONFIG_STA_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR, "80211> ==>\n");
	RtmpIoctl_rt_ioctl_siwpmksa(
		pAd, (struct mt_priv_pmksa_info *)ctrl_data->data);
#endif /* CONFIG_STA_SUPPORT */
#endif
	return 0;
} /* End of CFG80211_OpsPmksaFlush */

static int CFG80211DRV_OpsMgmtTx(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
#if defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT)
	UCHAR supp_seq_num;
	UCHAR ifIndex;
#endif
	struct mt_priv_ops_mgmt_tx_info *mt_priv_info;

	MTWF_DBG(NULL, DBG_CAT_TX, CATTX_MGMT, DBG_LVL_DEBUG, "80211> ==>\n");
	mt_priv_info = ctrl_data->data;

#ifndef HOSTAPD_AUTO_CH_SUPPORT
	if (mt_priv_info->ChanId != 0)
		RTMP_DRIVER_80211_CHANNEL_LOCK(pAd, mt_priv_info->ChanId);
#endif

#ifdef APCLI_CFG80211_SUPPORT
	if (wdev->wdev_type == WDEV_TYPE_STA) {
		UINT frmLen = mt_priv_info->Len;
		UCHAR *pFrmBuf = NULL;
		POS_COOKIE pObj = (POS_COOKIE)pAd->OS_Cookie;
		PSTA_ADMIN_CONFIG pStaCfg = &pAd->StaCfg[pObj->ioctl_if];
		PFRAME_802_11 pFrame = (PFRAME_802_11)mt_priv_info->pBuf;

		if (!pFrame)
			return 0;
		ifIndex = wdev->func_idx;
		if (ifIndex >= MAX_MULTI_STA)
			return 0;
		supp_seq_num = wdev->SecConfig.wpa_supp_seq_num;
		if (mt_priv_info->action_frame
			&& mt_priv_info->action_category == CATEGORY_WNM) {
			os_alloc_mem(NULL, (UCHAR **)&pFrmBuf, frmLen);
			if (pFrmBuf == NULL) {
				MTWF_DBG(NULL, DBG_CAT_TX, CATTX_MGMT, DBG_LVL_WARN,
					"allocate memory failed.\n");
				return 0;
			}

			MTWF_DBG_NP(DBG_CAT_TX, CATTX_MGMT, DBG_LVL_DEBUG,
					"[cfg80211] send btm rsp to "MACSTR".\n",
					MAC2STR(pFrame->Hdr.Addr3));
			Set_AutoRoaming_Proc(pAd, "1");
			pStaCfg->isReassoc = TRUE;
			NdisCopyMemory(pFrmBuf, mt_priv_info->pBuf, frmLen);
			MiniportMMRequest(pAd, 0, pFrmBuf, frmLen, NULL);
			os_free_mem(pFrmBuf);
			return 0;
		}

		if ((mt_priv_info->auth_frame && mt_priv_info->auth_alg == AUTH_MODE_SAE)
			|| mt_priv_info->action_frame) {
			/*add it to support DPP R1 action frame*/
			os_alloc_mem(NULL, (UCHAR **)&pFrmBuf, frmLen);
			if (pFrmBuf == NULL) {
				MTWF_DBG(NULL, DBG_CAT_TX, CATTX_MGMT, DBG_LVL_WARN,
					"allocate memory failed.\n");
				return 0;
			}

#if defined(APCLI_CFG80211_SUPPORT) && defined(SUPP_RANDOM_MAC_IN_DRIVER_SUPPORT)
			if (pAd->StaCfg[ifIndex].apcli_random_mac_support)
				UniCmdStaSNSet(pAd, 0, supp_seq_num);
#endif

			NdisCopyMemory(pFrmBuf, mt_priv_info->pBuf, frmLen);
			MiniportMMRequest(pAd, 0, pFrmBuf, frmLen, NULL);
			/*dpp action frame need driver indicate TX status event*/
			if (mt_priv_info->action_frame) {
				int idx = 0;

				for (idx = 0; idx < MAX_MULTI_STA; idx++) {
					struct wifi_dev *wdev = &pAd->StaCfg[idx].wdev;

					if (wdev->if_dev != NULL) {
						if (RTMPEqualMemory(pFrame->Hdr.Addr2,
							wdev->if_dev->dev_addr, MAC_ADDR_LEN)) {
							CFG80211OS_TxStatus(
								wdev->if_dev, MTK_COOKIE_VALUE,
								pFrmBuf, frmLen, 1);
							break;
						}
					}
				}
			}
			os_free_mem(pFrmBuf);
		}
	} else
#endif
		RTMP_DRIVER_80211_MGMT_FRAME_SEND(
			pAd, (VOID *)mt_priv_info->pBuf, mt_priv_info->Len);

	/* Mark it for using Supplicant-Based off-channel wait
		if (Offchan)
			RTMP_DRIVER_80211_CHANNEL_RESTORE(pAd);
	*/

	return 0;
}

static int
CFG80211DRV_OpsUpdate_mgmt_frame_reg(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	BOOLEAN isAp = TRUE;
	UINT mgmt_reg = ctrl_data->uint_val;

	if (wdev->wdev_type == WDEV_TYPE_STA)
		isAp = FALSE;
#ifdef BACKPORT_NOSTDINC
	if (ctrl_data->sub_id == MTK_GET_MASK) {
		ctrl_data->ushort_val = pAd->cfg80211_ctrl.mgmt_bit_mask;
#if defined(APCLI_CFG80211_SUPPORT)
		if (!isAp)
			ctrl_data->ushort_val = (pAd->cfg80211_ctrl.mgmt_bit_mask_sta);
#endif
		return 0;
	}
#endif /* BACKPORT_NOSTDINC */
	if (mgmt_reg & MTK_MGMT_PROBE_REQ_SET)
		CFG80211DRV_OpsMgmtFrameProbeRegister(pAd, wdev, TRUE);
	if (mgmt_reg & MTK_MGMT_PROBE_REQ_CLEAR)
		CFG80211DRV_OpsMgmtFrameProbeRegister(pAd, wdev, FALSE);

	if (mgmt_reg & MTK_MGMT_ACTION_SET)
		CFG80211DRV_OpsMgmtFrameActionRegister(pAd, TRUE);
	if (mgmt_reg & MTK_MGMT_ACTION_CLEAR)
		CFG80211DRV_OpsMgmtFrameActionRegister(pAd, FALSE);

#ifdef BACKPORT_NOSTDINC
	if (ctrl_data->sub_id == MTK_SET_MASK_V2) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"new mask: 0x%x\n", ctrl_data->ushort_val);
#if defined(APCLI_CFG80211_SUPPORT)
		if (isAp)
			pAd->cfg80211_ctrl.mgmt_bit_mask = ctrl_data->ushort_val;
		else
			pAd->cfg80211_ctrl.mgmt_bit_mask_sta = ctrl_data->ushort_val;
#else
		pAd->cfg80211_ctrl.mgmt_bit_mask = ctrl_data->ushort_val;
#endif /* APCLI_CFG80211_SUPPORT */
	}
#endif /* BACKPORT_NOSTDINC */
	return 0;
}

static int CFG80211DRV_OpsSetQosMap(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef HOSTAPD_HS_R2_SUPPORT
	UINT apidx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_HS_R2, DBG_LVL_INFO, "80211> ==>\n");
	/*get mbss from net device and for all the clients associated with that mbss set the given qosmap */
	if (ctrl_data->data == NULL) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_HS_R2, DBG_LVL_INFO, "Qos Map NULL\n");
		return 0;
	}

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		apidx = wdev->func_idx;
		if (apidx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", apidx);
			return 0;
		}
	} else
		return 0;

	CFG80211DRV_SetQosParam(pAd, ctrl_data->data, apidx);
	return 0;
#else /* HOSTAPD_HS_R2_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !HOSTAPD_HS_R2_SUPPORT */
}

static int CFG80211DRV_OpsSetAntenna(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_priv_ops_antenna_info *ant_cnt;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_INFO, "80211> ==>\n");

	ant_cnt = ctrl_data->data;
	MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_INFO,
		"80211> TX ant count = %x, RX ant count = %x\n",
		ant_cnt->tx_ant, ant_cnt->rx_ant);
	ret = RTMP_DRIVER_80211_SET_ANTENNA(pAd, ant_cnt);

	return ret;
}

static int CFG80211DRV_OpsGetAntenna(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_priv_ops_antenna_info *ant_cnt;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	MTWF_DBG(NULL, DBG_CAT_HW, CATHW_PHY, DBG_LVL_INFO, "80211> ==>\n");
	ant_cnt = ctrl_data->data;
	ret = RTMP_DRIVER_80211_GET_ANTENNA(pAd, ant_cnt);
	return ret;
}

static int CFG80211DRV_OpsChangeBss(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->func_idx < 0 || wdev->func_idx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"invalid wdev->func_idx:%d\n", wdev->func_idx);
		return -RTMP_IO_EINVAL;
	}

	/* ap isolate */
	if (ctrl_data->u8_val == 1)
		pAd->ApCfg.MBSSID[wdev->func_idx].IsolateInterStaTraffic = TRUE;
	else if (ctrl_data->u8_val == 0)
		pAd->ApCfg.MBSSID[wdev->func_idx].IsolateInterStaTraffic = FALSE;


	return 0;
}

static int CFG80211DRV_OpsStaDel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_priv_ops_sta_del_info *mt_priv_info;

	mt_priv_info = ctrl_data->data;
	MTWF_DBG(pAd, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
		"80211> ==> for bssid ("MACSTR")\n",
		MAC2STR(wdev->bssid));
	CFG80211_ApStaDel(pAd, wdev, mt_priv_info, mt_priv_info->reason);
	return 0;
}

static int CFG80211DRV_OpsStaChg(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	MAC_TABLE_ENTRY *pEntry;
	struct mt_priv_ops_change_sta_info *mt_priv_info;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	mt_priv_info = ctrl_data->data;
	pEntry = MacTableLookup(pAd, (UCHAR *)mt_priv_info->mac_addr);

	if (pEntry && IS_AKM_WPA2(pEntry->SecConfig.AKMMap)) {
		if ((mt_priv_info->authorized) &&
			(pEntry->SecConfig.Handshake.WpaState == AS_PTKINITDONE)) {
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
				"80211> STA("MACSTR") ==> PortSecured\n",
				MAC2STR(mt_priv_info->mac_addr));
			CFG80211_StaPortSecured(pAd, mt_priv_info->mac_addr, 1);
		}
	} else {
		if (mt_priv_info->authorized) {
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
				"80211> STA("MACSTR") ==> PortSecured\n",
				MAC2STR(mt_priv_info->mac_addr));
			CFG80211_StaPortSecured(pAd, mt_priv_info->mac_addr, 1);
		}
	}
#ifdef CONFIG_VLAN_GTK_SUPPORT
	if ((mt_priv_info->vlan_id) || (mt_priv_info->vlan_dev)) {
		struct vlan_gtk_info *vg_info;
		MAC_TABLE_ENTRY *pEntry;
		UINT32 vlan_id = 0;

		pEntry = MacTableLookup(pAd, (UCHAR *)mt_priv_info->mac_addr);
		if (!pEntry) {
			MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
				"can't find STA %02X:%02X:%02X:%02X:%02X:%02X, set VLAN failed\n",
				PRINT_MAC(mt_priv_info->mac_addr));
			return 0;
		}

		if (mt_priv_info->vlan_id) {
			vg_info = CFG80211_GetVlanInfoByVlanid(wdev, mt_priv_info->vlan_id);
			if (vg_info)
				vlan_id = vg_info->vlan_id;
		} else if (mt_priv_info->vlan_dev) {
			wdev = CFG80211_GetWdevByVlandev(
				(PRTMP_ADAPTER)pAd, mt_priv_info->vlan_dev);
			if (!wdev) {
				MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
					"%s() invalid params->vlan vlan_dev=%p\n", __func__,
					mt_priv_info->vlan_dev);
				return 0;
			}
			vg_info = CFG80211_GetVlanInfoByVlandev(wdev, mt_priv_info->vlan_dev);
			vlan_id = (vg_info) ? vg_info->vlan_id : 0;
		}
		if (vlan_id == 0)
			vlan_id = mt_priv_info->vlan_id;
#ifdef DYNAMIC_VLAN_SUPPORT
		if (vlan_id > 0)
			RTMP_DRIVER_80211_AP_BIND_VLAN(pAd, pEntry, vlan_id);
		MTWF_PRINT("%s STA %02X:%02X:%02X:%02X:%02X:%02X vlan_id %d\n",
			__func__, PRINT_MAC(mt_priv_info->mac_addr), pEntry->vlan_id);
#endif
	}
#endif
	return 0;
}

static int CFG80211DRV_OpsTestModeCmd(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#if defined(CONFIG_NL80211_TESTMODE) && defined(HOSTAPD_PMKID_IN_DRIVER_SUPPORT)
	struct mt_priv_pmkid *pmkid_entry;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	pmkid_entry = ctrl_data->data;
	MTWF_DBG(NULL, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_INFO,
		"80211> datalen:%d==>\n", ctrl_data->data_len);

	hex_dump_with_lvl("testmode_buffer",
		(unsigned char *) ctrl_data->data, ctrl_data->data_len, DBG_LVL_INFO);

	CFG80211_ApUpdateStaPmkid(pAd, pmkid_entry);
	return 0;
#else
	return -RTMP_IO_EOPNOTSUPP;
#endif /*HOSTAPD_PMKID_IN_DRIVER_SUPPORT*/
}

static int CFG80211DRV_OpsGetTxPower(
		struct _RTMP_ADAPTER *pAd,
		struct wifi_dev *wdev,
		void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	*((int *)ctrl_data->data) = rtmp_get_macPower(pAd);
	return 0;
}

void cfg80211_drv_send_acs_complete_event(
	struct wifi_dev *wdev)
{
	struct _RTMP_ADAPTER *mac_ad = (struct _RTMP_ADAPTER *)wdev->sys_handle;
	struct mt_phy_cap phy_cap;
	struct mt_cfg80211_vndr_ctrl ctrl_data;

	if (!mac_ad)
		return;

	ctrl_data.wiphy = mac_ad->pCfg80211_CB->pCfg80211_Wdev->wiphy;
	ctrl_data.rsp_data = &phy_cap;
	phy_cap.channel = wdev->channel;
	if (WMODE_CAP_6G(wdev->PhyMode))
		phy_cap.phy_mode = MTK_PHY_MODE_6G;
	else if (WMODE_CAP_2G(wdev->PhyMode))
		phy_cap.phy_mode = MTK_PHY_MODE_2G;
	else if (WMODE_CAP_5G(wdev->PhyMode))
		phy_cap.phy_mode = MTK_PHY_MODE_5G;
	mtk_nl80211_send_acs_complete_event(&ctrl_data);
}

void cfg80211_drv_start(
	struct _RTMP_ADAPTER *mac_ad)
{
	CFG80211_SupBandReInit(mac_ad);
#ifndef DISABLE_HOSTAPD_BEACON
	RT_CFG80211_CRDA_REG_RULE_APPLY(mac_ad);
#endif
}

int cfg80211_drv_init_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	int ret = -RTMP_IO_EOPNOTSUPP;
	int sub_cmd = ioctl_cfg->subcmd;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_DEBUG,
		"mac_ad: %p, sub_cmd = %d\n", mac_ad, sub_cmd);
	if (sub_cmd == MT_80211_INIT_REG_NOTIFY_TO)
		CFG80211DRV_RegNotify(mac_ad, ioctl_cfg->data);
	else if (sub_cmd == MT_80211_DRIVER_REGISTER) {
		mac_ad->pCfg80211_CB = (struct mt_cfg80211_priv *)ioctl_cfg->data;
		if (mac_ad->pCfg80211_CB)
			mac_ad->cfg80211_ctrl.wiphy = mt_cfg80211_get_wiphy(
					mac_ad->pCfg80211_CB->pCfg80211_Wdev);
		CFG80211_reSetToDefault(mac_ad);
	} else if (sub_cmd == MT_80211_GET_BAND_INFO)
		cfg80211_drv_get_band_info(
			mac_ad, wdev, (struct mt_cfg80211_band *)ioctl_cfg->data);
	else if (sub_cmd == MT_80211_DRIVER_UNREGISTER)
		cfg80211_drv_unregister(mac_ad);
	return ret;
}

struct cfg80211_drv_ops_handler {
	enum mt_cfg80211_ops_cmd ops_cmd_type;
	int (*drv_ops_handler)(
		struct _RTMP_ADAPTER *mac_ad, struct wifi_dev *wdev, void *ctrl_data_);
};

struct cfg80211_drv_ops_handler drv_ops_handler[] = {
	{mtk_ops_set_bit_rate, CFG80211DRV_OpsBitRateParm},
#if defined(IWCOMMAND_CFG80211_SUPPORT) || defined(HOSTAPD_MBSS_SUPPORT)
	{mtk_ops_add_vif_inf, CFG80211DRV_OpsVifAdd},
	{mtk_ops_del_vif_inf, CFG80211DRV_OpsVifDel},
#endif /* IWCOMMAND_CFG80211_SUPPORT || HOSTAPD_MBSS_SUPPORT */
	{mtk_ops_start_ap, CFG80211DRV_OpsStartAp},
	{mtk_ops_change_beacon, CFG80211DRV_OpsChangeBeacon},
	{mtk_ops_stop_ap, CFG80211DRV_OpsStopAp},
	{mtk_ops_set_chanwidth, CFG80211DRV_AP_OpsSetChannel},
	{mtk_ops_chan_switch, CFG80211DRV_AP_OpsSetChannel},
	{mtk_ops_get_channel, CFG80211DRV_OpsGetChannel},
	{mtk_ops_start_radar_detect, CFG80211DRV_OpsStartRadarDetect},
	{mtk_ops_end_cac, CFG80211DRV_OpsEndCac},
	{mtk_ops_scan, CFG80211DRV_OpsScan},
	{mtk_ops_abort_scan, CFG80211DRV_OpsAbortScan},
	{mtk_ops_sta_get, CFG80211DRV_OpsStaGet},
	{mtk_ops_sta_dump, CFG80211DRV_OpsStaDump},
	{mtk_ops_wiphy_params_set, CFG80211DRV_OpsWiphyParamsSet},
	{mtk_ops_add_key, CFG80211DRV_OpsKeyAdd},
	{mtk_ops_get_key, CFG80211DRV_OpsKeyGet},
	{mtk_ops_del_key, CFG80211DRV_OpsKeyDel},
	{mtk_ops_set_default_key, CFG80211DRV_OpsKeyDefaultSet},
	{mtk_ops_set_default_beacon_key, CFG80211DRV_OpsDefaultBeaconKeySet},
	{mtk_ops_set_default_mgmt_key, CFG80211DRV_OpsMgmtKeyDefaultSet},
	{mtk_ops_connect, CFG80211DRV_OpsConnect},
	{mtk_ops_disconnect, CFG80211DRV_OpsDisconnect},
	{mtk_ops_external_auth, CFG80211DRV_OpsExternalAuth},
	{mtk_ops_rfkill_poll, CFG80211DRV_OpsRFKillPoll},
	{mtk_ops_survey_get, CFG80211DRV_OpsSurveyGet},
	{mtk_ops_pmksa_set, CFG80211DRV_OpsPmksaSet},
	{mtk_ops_pmksa_flush, CFG80211DRV_OpsPmksaFlush},
	{mtk_ops_mgmt_tx, CFG80211DRV_OpsMgmtTx},
	{mtk_ops_update_mgmt_frame_reg, CFG80211DRV_OpsUpdate_mgmt_frame_reg},
	{mtk_ops_set_qos_map, CFG80211DRV_OpsSetQosMap},
	{mtk_ops_set_antenna, CFG80211DRV_OpsSetAntenna},
	{mtk_ops_get_antenna, CFG80211DRV_OpsGetAntenna},
	{mtk_ops_change_bss, CFG80211DRV_OpsChangeBss},
	{mtk_ops_sta_del, CFG80211DRV_OpsStaDel},
	{mtk_ops_sta_chg, CFG80211DRV_OpsStaChg},
	{mtk_ops_test_mode_cmd, CFG80211DRV_OpsTestModeCmd},
	{mtk_ops_get_tx_power, CFG80211DRV_OpsGetTxPower},
	{0, }
};

int cfg80211_drv_ops_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	int ret = -RTMP_IO_EOPNOTSUPP;
	int cmd_attr_id = ioctl_cfg->subcmd;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ioctl_cfg->data;
	struct cfg80211_drv_ops_handler *ops_handler;

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_DEBUG,
		"mac_ad: %p, cmd_attr_id = %d\n", mac_ad, cmd_attr_id);
	for (ops_handler = drv_ops_handler; (ops_handler->ops_cmd_type != 0); ops_handler++) {
		if (ops_handler->ops_cmd_type == cmd_attr_id)
			ret = ops_handler->drv_ops_handler(mac_ad, wdev, ctrl_data);
	}
	return ret;
}

#endif /* RT_CFG80211_SUPPORT */


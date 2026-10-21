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
 *
 *	Abstract:
 *
 *	All related CFG80211 function body.
 *
 *	History:
 *
 ***************************************************************************/

#ifdef RT_CFG80211_SUPPORT
#ifdef CONFIG_AP_SUPPORT
#include <linux/inetdevice.h>
#include <net/arp.h>
#include "rt_config.h"


INT CFG80211_FindMbssApIdxByNetDevice(RTMP_ADAPTER *pAd, PNET_DEV pNetDev)
{
	USHORT index = 0;
	BOOLEAN found = FALSE;

	if (pNetDev == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_DISCON, DBG_LVL_WARN, "pNetDev is NULL\n");
		return WDEV_NOT_FOUND;
	}

	for (index = 0; index < MAX_MBSSID_NUM(pAd); index++) {
		if (pAd->ApCfg.MBSSID[index].wdev.if_dev == pNetDev) {
			found = TRUE;
			break;
		}
#ifdef CONFIG_VLAN_GTK_SUPPORT
		else if (CFG80211_MatchVlandev(&pAd->ApCfg.MBSSID[index].wdev, pNetDev)) {
			found = TRUE;
			break;
		}
#endif
	}

	return (found) ? index : WDEV_NOT_FOUND;
}

INT CFG80211_FindApcliIdxByNetDevice(RTMP_ADAPTER *pAd, PNET_DEV pNetDev)
{
	USHORT index = 0;
	BOOLEAN found = FALSE;

	for (index = 0; index < MAX_MULTI_STA; index++) {
		if (pAd->StaCfg[index].wdev.if_dev == pNetDev) {
			found = TRUE;
			break;
		}
	}

	return (found) ? index : WDEV_NOT_FOUND;
}

static INT CFG80211DRV_UpdateTimIE(
	PUCHAR pBeaconFrame,
	UINT32 tim_ie_pos,
	UCHAR dtim_count,
	UCHAR dtim_period)
{
	UCHAR  TimFirst, TimLast, *ptr, New_Tim_Len;

	ptr = pBeaconFrame + tim_ie_pos; /* TIM LOCATION */
	*ptr = IE_TIM;
	*(ptr + 2) = dtim_count;
	*(ptr + 3) = dtim_period;
	TimFirst = 0; /* record first TIM byte != 0x00 */
	TimLast = 0;  /* record last  TIM byte != 0x00 */

	*(ptr + 1) = 3 + (TimLast - TimFirst + 1); /* TIM IE length */
	*(ptr + 4) = TimFirst;

	/* adjust BEACON length according to the new TIM */
	New_Tim_Len = (2 + *(ptr + 1));
	return New_Tim_Len;
}

VOID CFG80211_SyncSsid(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pCfg)
{
	const UCHAR *ssid_ie = NULL;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;

	ssid_ie = cfg80211_find_ie(WLAN_EID_SSID, pBeacon->beacon_head + 36, pBeacon->beacon_head_len - 36);

	/* SSID */
	if (ssid_ie == NULL) {
		os_move_mem(pMbss->Ssid, "CFG_Linux_GO", 12);
		pMbss->SsidLen = 12;
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
			"CFG: SSID Not Found In Packet\n");
	} else if (pCfg	&& pCfg->ssid_len && (pCfg->ssid_len <= MAX_LEN_OF_SSID)) {
		pMbss->SsidLen = pCfg->ssid_len;
		os_zero_mem(pMbss->Ssid, MAX_LEN_OF_SSID + 1);
		os_move_mem(pMbss->Ssid, pCfg->ssid, pMbss->SsidLen);
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_DEBUG,
			"\nCFG : SSID: %s, %d\n", pMbss->Ssid, pMbss->SsidLen);
		if (IS_MAP_BS_ENABLE(pAd))
			wapp_send_ssid_change_rsp(pAd, wdev, pMbss->Ssid, pMbss->SsidLen);
		if (pCfg->hidden_ssid > 0 && pCfg->hidden_ssid < 3) {
			pMbss->bHideSsid = TRUE;
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_DEBUG,
				"80211> [Hidden] SSID: %s, %d, (hidden_ssid = %d, ssid_len = %d)\n",
				pMbss->Ssid, pMbss->SsidLen, pCfg->hidden_ssid, pCfg->ssid_len);
		} else
			pMbss->bHideSsid = FALSE;
	}
#ifdef DISABLE_HOSTAPD_BEACON
	else {
		UCHAR ssid_len = *(ssid_ie+1);
		PNET_DEV net_dev_p;
		struct wireless_dev *cfg80211_wdev;

		if (ssid_len > MAX_LEN_OF_SSID) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"cfg80211 ops get too long SSID!\n");
			return;
		}
		if (ssid_len != 0) {
			if (MlmeValidateSSID((unsigned char *)(ssid_ie+2), ssid_len) == FALSE)
				pMbss->bHideSsid = TRUE;
			else {
				os_zero_mem(pMbss->Ssid, MAX_LEN_OF_SSID + 1);
				os_move_mem(pMbss->Ssid, (void *)ssid_ie+2, ssid_len);
				pMbss->SsidLen = ssid_len;
				if (IS_MAP_BS_ENABLE(pAd))
					wapp_send_ssid_change_rsp(pAd, wdev, pMbss->Ssid, pMbss->SsidLen);
				/* update cfg80211 wdev ssid */
				net_dev_p = wdev->if_dev;
				cfg80211_wdev = net_dev_p->ieee80211_ptr;
#if defined(CFG_CFG80211_VERSION)
#if (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
				os_zero_mem(cfg80211_wdev->u.ap.ssid, MAX_LEN_OF_SSID);
				os_move_mem(cfg80211_wdev->u.ap.ssid, (void *)ssid_ie+2, ssid_len);
				cfg80211_wdev->u.ap.ssid_len = ssid_len;
#else /* (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION) */
				os_zero_mem(cfg80211_wdev->ssid, MAX_LEN_OF_SSID);
				os_move_mem(cfg80211_wdev->ssid, (void *)ssid_ie+2, ssid_len);
				cfg80211_wdev->ssid_len = ssid_len;
#endif /* (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION) */
#else /* defined (CFG_CFG80211_VERSION) */
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
				os_zero_mem(cfg80211_wdev->u.ap.ssid, MAX_LEN_OF_SSID);
				os_move_mem(cfg80211_wdev->u.ap.ssid, (void *)ssid_ie+2, ssid_len);
				cfg80211_wdev->u.ap.ssid_len = ssid_len;
#else /* (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) */
				os_zero_mem(cfg80211_wdev->ssid, MAX_LEN_OF_SSID);
				os_move_mem(cfg80211_wdev->ssid, (void *)ssid_ie+2, ssid_len);
				cfg80211_wdev->ssid_len = ssid_len;
#endif /* (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) */
#endif /* defined (CFG_CFG80211_VERSION) */
				pMbss->bHideSsid = FALSE;
			}
		} else
			pMbss->bHideSsid = TRUE;

		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_DEBUG,
			"\nCFG : SSID: %s, %d\n", pMbss->Ssid, pMbss->SsidLen);
	}
#endif /* DISABLE_HOSTAPD_BEACON */
}

VOID CFG80211_SyncBssHECap(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *he_ie = NULL;
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;

	if (!pAd->CommonCfg.wifi_cert)
		return;

	if (!(WMODE_CAP_AX(wdev->PhyMode) || WMODE_CAP_BE(wdev->PhyMode)))
		return;

	he_ie = cfg80211_find_ext_ie(WLAN_EID_EXT_HE_CAPABILITY,
		pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (he_ie) {
		if (he_ie[1] >= (1 + sizeof(struct ieee80211_he_cap_elem))) {
			struct ieee80211_he_cap_elem *he_cap = NULL;

			he_cap = (struct ieee80211_he_cap_elem *)(he_ie + 3);
			if (he_cap->phy_cap_info[1] & IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD)
				wlan_config_set_he_ldpc(wdev, 1);
			else
				wlan_config_set_he_ldpc(wdev, 0);

			if ((he_cap->phy_cap_info[3] & IEEE80211_HE_PHY_CAP3_SU_BEAMFORMER) ||
				(he_cap->phy_cap_info[4] & IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE) ||
				(he_cap->phy_cap_info[4] & IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER)) {
				if (wlan_config_get_etxbf(wdev) != SUBF_ALL) {
					wlan_config_set_etxbf(wdev, SUBF_ALL);
					pAd->CommonCfg.ETxBfEnCond = SUBF_ALL;
					bf_type_ctrl(pAd);
				}
			} else {
				if (wlan_config_get_etxbf(wdev) != SUBF_OFF) {
					wlan_config_set_etxbf(wdev, SUBF_OFF);
					pAd->CommonCfg.ETxBfEnCond = SUBF_OFF;
					bf_type_ctrl(pAd);
				}
			}
		}
	} else {
		wlan_config_set_he_ldpc(wdev, 0);
	}
}

VOID CFG80211_SyncBssHTCap(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *ht_cap = NULL;

	ht_cap = cfg80211_find_ie(WLAN_EID_HT_CAPABILITY, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
}

VOID CFG80211_SyncBssHTOp(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *ht_info = NULL;

	ht_info = cfg80211_find_ie(WLAN_EID_HT_OPERATION, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
}

VOID CFG80211_SyncBssVHTCap(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *vht_ie = NULL;
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;

	if (!pAd->CommonCfg.wifi_cert)
		return;

	if (!(WMODE_CAP_AC(wdev->PhyMode)))
		return;

	vht_ie = cfg80211_find_ie(WLAN_EID_VHT_CAPABILITY,
		pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (vht_ie) {
		if (vht_ie[1] >= sizeof(struct ieee80211_vht_cap)) {
			struct ieee80211_vht_cap *vht_cap = NULL;

			vht_cap = (struct ieee80211_vht_cap *)(vht_ie + 2);

			if ((vht_cap->vht_cap_info & IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE) ||
				(vht_cap->vht_cap_info & IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE) ||
				(vht_cap->vht_cap_info & IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE)) {
				if (wlan_config_get_etxbf(wdev) != SUBF_ALL) {
					wlan_config_set_etxbf(wdev, SUBF_ALL);
					pAd->CommonCfg.ETxBfEnCond = SUBF_ALL;
					bf_type_ctrl(pAd);
				}
			} else {
				if (wlan_config_get_etxbf(wdev) != SUBF_OFF) {
					wlan_config_set_etxbf(wdev, SUBF_OFF);
					pAd->CommonCfg.ETxBfEnCond = SUBF_OFF;
					bf_type_ctrl(pAd);
				}
			}
		}
	}
}

#ifdef CONFIG_DOT11U_INTERWORKING
VOID CFG80211_SyncInterworking(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	INT32 Ret;
	PUCHAR tmp_buf_ptr = NULL;
	const UCHAR *interworking_ie = NULL;
	PGAS_CTRL pGasCtrl = &pAd->ApCfg.MBSSID[mbss_idx].GASCtrl;
#ifdef HOSTAPD_HS_R2_SUPPORT
	PHOTSPOT_CTRL pHSCtrl = &pAd->ApCfg.MBSSID[mbss_idx].HotSpotCtrl;
#endif

	interworking_ie = cfg80211_find_ie(IE_INTERWORKING, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (interworking_ie != NULL) {
		EID_STRUCT *eid = (EID_STRUCT *)interworking_ie;

		Ret = os_alloc_mem(NULL, &tmp_buf_ptr, eid->Len + 2);

		if (Ret != NDIS_STATUS_SUCCESS) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"Not enough memory\n");
			return;
		}
		NdisMoveMemory(tmp_buf_ptr, interworking_ie, eid->Len + 2);

		OS_SEM_LOCK(&pGasCtrl->IeLock);
		if (pGasCtrl->InterWorkingIE)
			os_free_mem(pGasCtrl->InterWorkingIE);
		pGasCtrl->InterWorkingIE = tmp_buf_ptr;
		pGasCtrl->InterWorkingIELen = eid->Len + 2;
#ifdef HOSTAPD_HS_R2_SUPPORT
		OS_SEM_LOCK(&pHSCtrl->IeLock);
		pHSCtrl->AccessNetWorkType  = (*(pGasCtrl->InterWorkingIE + 2)) & 0x0F;
		OS_SEM_UNLOCK(&pHSCtrl->IeLock);
		if (pGasCtrl->InterWorkingIELen > 3) {
			pHSCtrl->IsHessid = TRUE;

			if (pGasCtrl->InterWorkingIELen == 7)
				NdisMoveMemory(pHSCtrl->Hessid, pGasCtrl->InterWorkingIE + 3, MAC_ADDR_LEN);
			else
				NdisMoveMemory(pHSCtrl->Hessid, pGasCtrl->InterWorkingIE + 5, MAC_ADDR_LEN);
		}
#endif /* HOSTAPD_HS_R2_SUPPORT */
		OS_SEM_UNLOCK(&pGasCtrl->IeLock);
	}
}

VOID CFG80211_SyncAdvProto(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	INT32 Ret;
	PUCHAR tmp_buf_ptr = NULL;
	const UCHAR *adv_proto_ie = NULL;
	PGAS_CTRL pGasCtrl = &pAd->ApCfg.MBSSID[mbss_idx].GASCtrl;

	adv_proto_ie = cfg80211_find_ie(IE_ADVERTISEMENT_PROTO, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (adv_proto_ie != NULL) {
		EID_STRUCT *eid = (EID_STRUCT *)adv_proto_ie;

		Ret = os_alloc_mem(NULL, &tmp_buf_ptr, eid->Len + 2);
		if (Ret != NDIS_STATUS_SUCCESS) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"Not enough memory\n");
			return;
		}
		NdisMoveMemory(tmp_buf_ptr, adv_proto_ie, eid->Len + 2);

		OS_SEM_LOCK(&pGasCtrl->IeLock);
		if (pGasCtrl->AdvertisementProtoIE)
			os_free_mem(pGasCtrl->AdvertisementProtoIE);
		pGasCtrl->AdvertisementProtoIE = tmp_buf_ptr;
		pGasCtrl->AdvertisementProtoIELen = eid->Len + 2;
		OS_SEM_UNLOCK(&pGasCtrl->IeLock);
	}
}

VOID CFG80211_SyncRoamingConsortium(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	INT32 Ret;
	PUCHAR tmp_buf_ptr = NULL;
	const UCHAR *roam_consort_ie = NULL;
	PGAS_CTRL pGasCtrl = &pAd->ApCfg.MBSSID[mbss_idx].GASCtrl;

	roam_consort_ie = cfg80211_find_ie(IE_ROAMING_CONSORTIUM, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (roam_consort_ie != NULL) {
		EID_STRUCT *eid = (EID_STRUCT *)roam_consort_ie;

		Ret = os_alloc_mem(NULL, &tmp_buf_ptr, eid->Len + 2);
		if (Ret != NDIS_STATUS_SUCCESS) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"Not enough memory\n");
			return;
		}
		NdisMoveMemory(tmp_buf_ptr, roam_consort_ie, eid->Len + 2);

		OS_SEM_LOCK(&pGasCtrl->IeLock);
		if (pGasCtrl->RoamingConsortiumIE)
			os_free_mem(pGasCtrl->RoamingConsortiumIE);
		pGasCtrl->RoamingConsortiumIE = tmp_buf_ptr;
		pGasCtrl->RoamingConsortiumIELen = eid->Len + 2;
		OS_SEM_UNLOCK(&pGasCtrl->IeLock);
	}
}
#endif /* CONFIG_DOT11U_INTERWORKING */

VOID CFG80211_SyncCapability(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *ext_cap_ies = NULL;
	struct _SECURITY_CONFIG *sec_cfg = &pAd->ApCfg.MBSSID[mbss_idx].wdev.SecConfig;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;

	ext_cap_ies = cfg80211_find_ie(IE_EXT_CAPABILITY, pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (ext_cap_ies != NULL) {
		EXT_CAP_INFO_ELEMENT ext_cap;
		EID_STRUCT *eid = (EID_STRUCT *)ext_cap_ies;
#if defined(HOSTAPD_HS_R2_SUPPORT) || defined(CONFIG_PROXY_ARP)
		PHOTSPOT_CTRL pHSCtrl = &pAd->ApCfg.MBSSID[mbss_idx].HotSpotCtrl;
#endif

		NdisZeroMemory(&ext_cap, sizeof(EXT_CAP_INFO_ELEMENT));
		if (eid->Len < sizeof(EXT_CAP_INFO_ELEMENT))
			NdisMoveMemory(&ext_cap, &eid->Octet[0], eid->Len);
		else
			NdisMoveMemory(&ext_cap, &eid->Octet[0], sizeof(EXT_CAP_INFO_ELEMENT));

		if (ext_cap.bcn_prot_en) {
			sec_cfg->bcn_prot_cfg.desired_bcn_prot_en = 1;
			ext_cap.bcn_prot_en = 1;
		} else {
			sec_cfg->bcn_prot_cfg.desired_bcn_prot_en = 0;
		}

#if defined(HOSTAPD_HS_R2_SUPPORT) || defined(CONFIG_PROXY_ARP)
#ifdef HOSTAPD_HS_R2_SUPPORT
		if (ext_cap.wnm_notification)
			pMbss->WNMCtrl.WNMNotifyEnable = 1;
		else
			pMbss->WNMCtrl.WNMNotifyEnable = 0;
#endif
		if (ext_cap.proxy_arp) {
			pMbss->WNMCtrl.ProxyARPEnable = 1;
			hotspot_update_bssflag(pAd, fgProxyArpEnable, 1, pHSCtrl);
			hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
		} else {
			pMbss->WNMCtrl.ProxyARPEnable = 0;
			hotspot_update_bssflag(pAd, fgProxyArpEnable, 0, pHSCtrl);
			hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
		}
#ifdef HOSTAPD_HS_R2_SUPPORT
		if (ext_cap.BssTransitionManmt)
			pMbss->WNMCtrl.WNMBTMEnable = 1;
		else
			pMbss->WNMCtrl.WNMBTMEnable = 0;

		if (ext_cap.qosmap) {
			hotspot_update_bssflag(pAd, fgQosMapEnable, 1, pHSCtrl);
			hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
			pMbss->HotSpotCtrl.QosMapEnable = 1;
		} else {
			pMbss->HotSpotCtrl.QosMapEnable = 0;
			hotspot_update_bssflag(pAd, fgQosMapEnable, 0, pHSCtrl);
			hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
		}
#endif
#endif /* defined(HOSTAPD_HS_R2_SUPPORT) || defined(CONFIG_PROXY_ARP) */
	}
	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_SAE, DBG_LVL_INFO,
			"hostapd config Beacon protection:%d\n", sec_cfg->bcn_prot_cfg.desired_bcn_prot_en);
}

#ifdef HOSTAPD_HS_R2_SUPPORT
VOID CFG80211_SyncWfaHs2(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	PHOTSPOT_CTRL pHSCtrl = &pAd->ApCfg.MBSSID[mbss_idx].HotSpotCtrl;
	const UCHAR *hs2_indication_ie = NULL;
	INT32 Ret;
	PUCHAR tmp_buf_ptr = NULL;

	hs2_indication_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_HS2,
			pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (hs2_indication_ie != NULL) {
		EID_STRUCT *eid = (EID_STRUCT *)hs2_indication_ie;

		Ret = os_alloc_mem(NULL, &tmp_buf_ptr, eid->Len + 2);
		if (Ret != NDIS_STATUS_SUCCESS) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"Not enough memory\n");
			return;
		}
		NdisMoveMemory(tmp_buf_ptr, hs2_indication_ie, eid->Len + 2);

		OS_SEM_LOCK(&pHSCtrl->IeLock);
		if (pHSCtrl->HSIndicationIE)
			os_free_mem(pHSCtrl->HSIndicationIE);
		pHSCtrl->HSIndicationIE = tmp_buf_ptr;
		pHSCtrl->HSIndicationIELen = eid->Len + 2;
		pHSCtrl->HotSpotEnable = 1;
		hotspot_update_bssflag(pAd, fgHotspotEnable, 1, pHSCtrl);
		OS_SEM_UNLOCK(&pHSCtrl->IeLock);
	} else {
		pHSCtrl->HotSpotEnable = 0;
		hotspot_update_bssflag(pAd, fgHotspotEnable, 0, pHSCtrl);
	}
}

VOID CFG80211_SyncWfaP2p(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	PHOTSPOT_CTRL pHSCtrl = &pAd->ApCfg.MBSSID[mbss_idx].HotSpotCtrl;
	const UCHAR *p2p_ie = NULL;
	INT32 Ret;
	PUCHAR tmp_buf_ptr = NULL;

	p2p_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_P2P,
			pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (p2p_ie != NULL) {
		EID_STRUCT *eid = (EID_STRUCT *)p2p_ie;

		Ret = os_alloc_mem(NULL, &tmp_buf_ptr, eid->Len + 2);
		if (Ret != NDIS_STATUS_SUCCESS) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"Not enough memory\n");
			return;
		}
		NdisMoveMemory(tmp_buf_ptr, p2p_ie, eid->Len + 2);

		OS_SEM_LOCK(&pHSCtrl->IeLock);
		if (pHSCtrl->P2PIE)
			os_free_mem(pHSCtrl->P2PIE);
		pHSCtrl->P2PIE = tmp_buf_ptr;
		pHSCtrl->P2PIELen = eid->Len + 2;
		OS_SEM_UNLOCK(&pHSCtrl->IeLock);
	}
}
#endif /* HOSTAPD_HS_R2_SUPPORT */

#ifdef DISABLE_HOSTAPD_BEACON
VOID CFG80211_SyncWpaWmeWps(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *wsc_ie = NULL;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;

	wdev->WscIEBeacon.ValueLen = 0;
	wdev->WscIEProbeResp.ValueLen = 0;
#if defined(WSC_INCLUDED)
	wdev->wps_triggered = 0;
#endif

	wsc_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WPAWME, OUI_WPAWME_WPS,
			pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (wsc_ie != NULL) {
#if defined(HOSTAPD_MAP_SUPPORT) || defined(MTK_HOSTAPD_SUPPORT)
		if ((IS_MAP_ENABLE(pAd)) && (wdev) &&
		(wdev->MAPCfg.DevOwnRole & BIT(MAP_ROLE_BACKHAUL_BSS)) &&
		(!(wdev->MAPCfg.DevOwnRole & BIT(MAP_ROLE_FRONTHAUL_BSS)))) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_DEBUG,
				"Strictly BH BSS: %s, donot BC WPS cap\n", pMbss->Ssid);
		} else
#endif
		{
			EID_STRUCT *eid;

			eid = (EID_STRUCT *)wsc_ie;

			if (eid->Len + 2 <= 500) {
#if defined(WSC_INCLUDED)
				USHORT i;
				PWSC_IE wsc_tlv = NULL;
#endif
				NdisCopyMemory(wdev->WscIEBeacon.Value, wsc_ie, eid->Len+2);
				wdev->WscIEBeacon.ValueLen = eid->Len + 2;
				NdisCopyMemory(wdev->WscIEProbeResp.Value, wsc_ie, eid->Len + 2);
				wdev->WscIEProbeResp.ValueLen = eid->Len + 2;
#if defined(WSC_INCLUDED)
				for (i = 6; i < eid->Len + 2; i += be2cpu16(wsc_tlv->Length) + 4) {
					wsc_tlv = (PWSC_IE) &wsc_ie[i];
					switch (be2cpu16(wsc_tlv->Type)) {
					case WSC_ID_SEL_REGISTRAR:
						wdev->wps_triggered = wsc_tlv->Data[0];
						break;
					}
				}
#endif
			}
		}
	}
}

#ifdef HOSTAPD_11R_SUPPORT
VOID CFG80211_SyncMobilityDomain(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *md_ie = NULL;

	/* WLAN_EID_MOBILITY_DOMAIN=54 */
	md_ie = cfg80211_find_ie(WLAN_EID_MOBILITY_DOMAIN, pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (md_ie != NULL) {
		PFT_CFG pFtCfg = &pAd->ApCfg.MBSSID[mbss_idx].wdev.FtCfg;

		pFtCfg->FtCapFlag.Dot11rFtEnable = TRUE;
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO, "FT is enable in driver\n");
		NdisCopyMemory(pFtCfg->FtMdId, md_ie+2, FT_MDID_LEN);
		pFtCfg->FtCapFlag.FtOverDs = (0x01)&(*(md_ie + 4));
		pFtCfg->FtCapFlag.RsrReqCap = (0x02)&(*(md_ie + 4));
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
				"MDID::%x %x FToverDS:%d RsrCap:%d\n",
				pFtCfg->FtMdId[0], pFtCfg->FtMdId[1],
				pFtCfg->FtCapFlag.FtOverDs, pFtCfg->FtCapFlag.RsrReqCap);
	} else
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO, "MDIE is NULL\n");
}
#endif /* HOSTAPD_11R_SUPPORT */
#endif /* DISABLE_HOSTAPD_BEACON */

#ifdef HOSTAPD_OWE_SUPPORT
VOID CFG80211_SyncWFAOwe(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	const UCHAR *trans_ie = NULL;

	/*owe trans oui */
	trans_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_OWE,
			pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (trans_ie != NULL) {
		EID_STRUCT *eid;

		eid = (EID_STRUCT *)trans_ie;
		if (eid->Len + 2 <= MAX_LEN_OF_TRANS_IE) {
			NdisCopyMemory(pMbss->TRANS_IE, trans_ie, eid->Len+2);
			pMbss->TRANSIE_Len = eid->Len + 2;
		}
	}
}
#endif /* HOSTAPD_OWE_SUPPORT */

VOID CFG80211_SyncPrivacy(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;
	UINT16 capa_info;

	capa_info = *(UINT16 *)(pBeacon->beacon_head + 34);
	/*check if it is open mode*/
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN,
		DBG_LVL_NOTICE, "beacon capability info field: %04x\n", capa_info);
	if (!CAP_IS_PRIVACY_ON(capa_info)) {
		CLEAR_SEC_AKM(wdev->SecConfig.AKMMap);
		CLEAR_CIPHER(wdev->SecConfig.PairwiseCipher);
		CLEAR_CIPHER(wdev->SecConfig.GroupCipher);
		SET_AKM_OPEN(wdev->SecConfig.AKMMap);
		SET_CIPHER_NONE(wdev->SecConfig.PairwiseCipher);
		SET_CIPHER_NONE(wdev->SecConfig.GroupCipher);
	}
}

#ifdef MBO_SUPPORT
VOID CFG80211_SyncWFAMbo(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	P_MBO_CTRL pMboCtrl = &pAd->ApCfg.MBSSID[mbss_idx].wdev.MboCtrl;
	const UCHAR *mbo_caps = NULL;

	mbo_caps = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_MBO, pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (mbo_caps) {
		EID_STRUCT *eid;

		eid = (EID_STRUCT *)mbo_caps;

		if (pMboCtrl && (eid->Len + 2 <= MBO_IE_MAX_LEN)) {
			pMboCtrl->bMboEnable = TRUE;
			NdisMoveMemory(pMboCtrl->MBOIE, mbo_caps, eid->Len + 2);
			pMboCtrl->MboIELen = eid->Len + 2;
			if (pMboCtrl->MboIELen == MBO_IE_MAX_LEN) {
				pMboCtrl->CellularPreference = eid->Octet[6];
				pMboCtrl->AssocDisallowReason = eid->Octet[9];
			} else {
				pMboCtrl->CellularPreference = eid->Octet[6];
				pMboCtrl->AssocDisallowReason = 0;
			}
		}
	}
}
#endif /* MBO_SUPPORT */

#ifdef HOSTAPD_11K_SUPPORT
VOID CFG80211_SyncWFARrm(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	const UCHAR *rrm_caps = NULL;

	rrm_caps = cfg80211_find_ie(WLAN_EID_RRM_ENABLED_CAPABILITIES, pBeacon->beacon_tail, pBeacon->beacon_tail_len);

	if (rrm_caps != NULL) {

		EID_STRUCT *eid;
		INT loop;
		UINT8 bit_nr, bit_lci;
		PRRM_CONFIG pRrmCfg;

		eid = (EID_STRUCT *)rrm_caps;

		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
			"RRM : len %d eid %d octet %d\n", eid->Len, eid->Eid, eid->Octet[0]);

		pMbss->CapabilityInfo |= RRM_CAP_BIT;

		bit_nr = (eid->Octet[0] >> 1) & 1; /*checking bit position 1:neighbor report */
		bit_lci = (eid->Octet[1] >> 4) & 1; /*checking LCI */

		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"bit_nr bit_lci: %d %d\n", bit_nr, bit_lci);

		for (loop = 0; loop < MAX_MBSSID_NUM(pAd); loop++) {
			pRrmCfg = &pAd->ApCfg.MBSSID[loop].wdev.RrmCfg;

			if (bit_lci)
				pRrmCfg->bPeerReqLCI = TRUE;
			pRrmCfg->bDot11kRRMEnable = 1;
			pRrmCfg->bDot11kRRMEnableSet = 1;
			pRrmCfg->max_rrm_capabilities.word = 0;
			pRrmCfg->max_rrm_capabilities.field.NeighborRepCap = 1;
			pRrmCfg->rrm_capabilities.word = pRrmCfg->max_rrm_capabilities.word;
		}
	}
}
#endif /* HOSTAPD_11K_SUPPORT */

#ifndef DISABLE_HOSTAPD_BEACON
#ifdef HOSTAPD_AUTO_CH_SUPPORT
VOID CFG80211_SyncDsParam(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *dsparam_ie = NULL;
	PEID_STRUCT pEid;
	UCHAR channel = 0;

	dsparam_ie = cfg80211_find_ie(WLAN_EID_DS_PARAMS, pBeacon->beacon_head+36, pBeacon->beacon_head_len-36);
	channel = HcGetChannel(pAd);

	MTWF_DBG(NULL, DBG_CAT_CHN, DBG_CAT_CHN, DBG_LVL_INFO,
			 "Channel from Auto selection is :%d\n", channel);

	if (dsparam_ie != NULL) {
		pEid = (PEID_STRUCT)dsparam_ie;
		*pEid->Octet = channel;
	}
}

VOID CFG80211_SyncHtOp(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *ht_operation = NULL;
	PEID_STRUCT pEid;
	PADD_HT_INFO_IE phtinfo;
	UCHAR channel = 0;
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;

	ht_operation = cfg80211_find_ie(WLAN_EID_HT_OPERATION, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	channel = HcGetChannel(pAd);

	MTWF_DBG(NULL, DBG_CAT_CHN, DBG_CAT_CHN, DBG_LVL_INFO,
			 "Channel from Auto selection is :%d\n", channel);

	if (ht_operation != NULL) {
		pEid = (PEID_STRUCT)ht_operation;
		phtinfo = (PADD_HT_INFO_IE)pEid->Octet;
		phtinfo->ControlChan = channel;
		phtinfo->AddHtInfo.RecomWidth = wlan_operate_get_ht_bw(wdev);
		/* phtinfo->AddHtInfo.ExtChanOffset = 3; */
		phtinfo->AddHtInfo.ExtChanOffset = HcGetExtCha(pAd, channel);
	}
}

VOID CFG80211_SyncVhtOp(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	const UCHAR *vht_operation = NULL;
	VHT_OP_IE	*vhtinfo;
	PEID_STRUCT pEid;
	UCHAR channel = 0;
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;

	vht_operation = cfg80211_find_ie(WLAN_EID_VHT_OPERATION, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	channel = HcGetChannel(pAd);

	MTWF_DBG(NULL, DBG_CAT_CHN, DBG_CAT_CHN, DBG_LVL_INFO,
		 "Channel from Auto selection is :%d\n", channel);

	if (vht_operation != NULL) {
		UCHAR bw = pAd->CommonCfg.vht_bw;
		UCHAR ch_band = wlan_config_get_ch_band(wdev);
		UCHAR cent_ch = vht_cent_ch_freq(channel, bw, ch_band);

		pEid = (PEID_STRUCT)vht_operation;
		vhtinfo = (VHT_OP_IE *)pEid->Octet;

		switch (bw) {
		case  VHT_BW_2040:
			vhtinfo->vht_op_info.ch_width = 0;
			vhtinfo->vht_op_info.center_freq_1 = 0;
			vhtinfo->vht_op_info.center_freq_2 = 0;
			break;

		case VHT_BW_80:
			vhtinfo->vht_op_info.ch_width = 1;
			vhtinfo->vht_op_info.center_freq_1 = cent_ch;
			vhtinfo->vht_op_info.center_freq_2 = 0;
			break;

		case VHT_BW_160:
			vhtinfo->vht_op_info.ch_width = 2;
			vhtinfo->vht_op_info.center_freq_1 = cent_ch;
			vhtinfo->vht_op_info.center_freq_2 = pAd->CommonCfg.vht_cent_ch2;
			break;

		case VHT_BW_8080:

			vhtinfo->vht_op_info.ch_width = 3;
			vhtinfo->vht_op_info.center_freq_1 = cent_ch;
			vhtinfo->vht_op_info.center_freq_2 = pAd->CommonCfg.vht_cent_ch2;
			break;
		}
	}
}
#endif /* HOSTAPD_AUTO_CH_SUPPORT */
#endif /* #ifndef DISABLE_HOSTAPD_BEACON */

#define CFG80211_CCK_1M 10
#define CFG80211_CCK_2M 20
#define CFG80211_CCK_5_5M 55
#define CFG80211_CCK_11M 110

#define CFG80211_OFDM_6M 60
#define CFG80211_OFDM_9M 90
#define CFG80211_OFDM_12M 120
#define CFG80211_OFDM_18M 180
#define CFG80211_OFDM_24M 240
#define CFG80211_OFDM_36M 360
#define CFG80211_OFDM_48M 480
#define CFG80211_OFDM_54M 540

enum CCK_MCS_IDX {
	MCS_CCK_1M,
	MCS_CCK_2M,
	MCS_CCK_5_5M,
	MCS_CCK_11M,
};

enum OFDM_MCS_IDX {
	MCS_OFDM_6M,
	MCS_OFDM_9M,
	MCS_OFDM_12M,
	MCS_OFDM_18M,
	MCS_OFDM_24M,
	MCS_OFDM_36M,
	MCS_OFDM_48M,
	MCS_OFDM_54M,
};

void cfg80211_update_beacon_rate(
	struct wifi_dev *wdev,
	struct mt_priv_ops_beacon_cfg *pCfg)
{
	union _HTTRANSMIT_SETTING *transmit;

	transmit = &wdev->bcn_rate;

	wdev->cfg80211_bcn_rate = TRUE;

	if (pCfg->bitrate == 0)
		return;

	if (pCfg->band != NL80211_BAND_2GHZ) {
		if (pCfg->bitrate == CFG80211_CCK_1M || pCfg->bitrate == CFG80211_CCK_2M ||
			pCfg->bitrate == CFG80211_CCK_5_5M || pCfg->bitrate == CFG80211_CCK_11M) {
			MTWF_PRINT("Cann't set CCK mode for Bcn in 5/6G band!\n");
			wdev->cfg80211_bcn_rate = FALSE;
			return;
		}
	}

	transmit->field.BW =  BW_20;
	switch (pCfg->bitrate) {
	case CFG80211_CCK_1M:
		transmit->field.MCS = MCS_CCK_1M;
		transmit->field.MODE = MODE_CCK;
		break;
	case CFG80211_CCK_2M:
		transmit->field.MCS = MCS_CCK_2M;
		transmit->field.MODE = MODE_CCK;
		break;
	case CFG80211_CCK_5_5M:
		transmit->field.MCS = MCS_CCK_5_5M;
		transmit->field.MODE = MODE_CCK;
		break;
	case CFG80211_CCK_11M:
		transmit->field.MCS = MCS_CCK_11M;
		transmit->field.MODE = MODE_CCK;
		break;
	case CFG80211_OFDM_6M:
		transmit->field.MCS = MCS_OFDM_6M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_9M:
		transmit->field.MCS = MCS_OFDM_9M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_12M:
		transmit->field.MCS = MCS_OFDM_12M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_18M:
		transmit->field.MCS = MCS_OFDM_18M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_24M:
		transmit->field.MCS = MCS_OFDM_24M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_36M:
		transmit->field.MCS = MCS_OFDM_36M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_48M:
		transmit->field.MCS = MCS_OFDM_48M;
		transmit->field.MODE = MODE_OFDM;
		break;
	case CFG80211_OFDM_54M:
		transmit->field.MCS = MCS_OFDM_54M;
		transmit->field.MODE = MODE_OFDM;
		break;
	default:
		wdev->cfg80211_bcn_rate = FALSE;
		MTWF_PRINT("unknown rate(%d)\n", pCfg->bitrate);
		break;
	}
}

VOID CFG80211_SyncMbssCapInfo(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;
	BOOLEAN SpectrumMgmt = FALSE;

#ifdef A_BAND_SUPPORT
	/* Decide the Capability information field */
	/* In IEEE Std 802.1h-2003, the spectrum management
	bit is enabled in the 5 GHz band */
	if (((wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G) ||
		(wlan_config_get_ch_band(wdev) == CMD_CH_BAND_6G))
		&& pAd->CommonCfg.bIEEE80211H == TRUE)
		SpectrumMgmt = TRUE;
#endif /* A_BAND_SUPPORT */

	pMbss->CapabilityInfo = CAP_GENERATE(1, 0, (!IS_CIPHER_NONE(wdev->SecConfig.PairwiseCipher)),
			(pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong ? 0 : 1),
			pAd->CommonCfg.bUseShortSlotTime, SpectrumMgmt);
#ifdef DOT11K_RRM_SUPPORT
	if (IS_RRM_ENABLE(wdev))
		pMbss->CapabilityInfo |= RRM_CAP_BIT;
#endif /* DOT11K_RRM_SUPPORT */
#ifdef UAPSD_SUPPORT
		if (pMbss->wdev.UapsdInfo.bAPSDCapable == TRUE) {
			/*
			QAPs set the APSD subfield to 1 within the Capability
			Information field when the MIB attribute
			dot11APSDOptionImplemented is true and set it to 0 otherwise.
			STAs always set this subfield to 0.
			*/
			pMbss->CapabilityInfo |= 0x0800;
		}
#endif /* UAPSD_SUPPORT */
}

VOID CFG80211_UpdateBcnInterval(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pCfg,
	OUT unsigned char *bss_info_change)
{
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[mbss_idx].wdev;
	UINT16 bcn_interval;

	bcn_interval = *(UINT16 *)(pBeacon->beacon_head + 32);
	if (bcn_interval != pAd->CommonCfg.BeaconPeriod) {
		pAd->CommonCfg.BeaconPeriod = bcn_interval;
		wdev->bss_info_argument.bcn_period = bcn_interval;
		*bss_info_change = TRUE;
	}
}

VOID CFG80211_UpdateSecurity(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pCfg,
	OUT unsigned char *bss_info_change)
{
	const UCHAR *ext_supp_rates_ie = NULL, *supp_rates_ie = NULL;
	const UCHAR *wpa_ie = NULL, *rsn_ie = NULL, *rsnx_ie = NULL;
	UINT8 h2e_only = 0, sae_pk = 0;
	UINT32 akm_map_org, pairwise_cipher_org, group_cipher_org;
#ifdef DOT11W_PMF_SUPPORT
	UINT32 igtk_cipher_org;
#endif
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;
#ifdef HOSTAPD_HS_R2_SUPPORT
	const UCHAR *hs2_osen_ie = NULL;
	PHOTSPOT_CTRL pHSCtrl = &pMbss->HotSpotCtrl;
#endif
	struct _SECURITY_CONFIG *sec_cfg = &wdev->SecConfig;

	supp_rates_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, pBeacon->beacon_head + 36, pBeacon->beacon_head_len - 36);
	if (supp_rates_ie != NULL) {
		int supp_rates_len;
		int i;

		supp_rates_len = supp_rates_ie[1];
		for (i = 2; i < supp_rates_len + 2; i++) {
			if (supp_rates_ie[i] == 0xfb) { /*bss member selector*/
				h2e_only |= 0x1;
				break;
			}
		}
	}
	/* if it doesn't find WPA_IE in tail first 30 bytes. treat it as is not found */
	wpa_ie = cfg80211_find_ie(WLAN_EID_VENDOR_SPECIFIC, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	/* wpa2 case. */
	rsn_ie = cfg80211_find_ie(WLAN_EID_RSN, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	/* wpa3 r3*/
	rsnx_ie = cfg80211_find_ie(WLAN_EID_RSNX, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (rsnx_ie != NULL) {
		if (rsnx_ie[2] & 0x20) {  /*bit5 means h2e*/
			h2e_only |= 0x2;
		}
		if (rsnx_ie[2] & (1 << IE_RSNXE_CAPAB_SAE_PK)) {  /*bit6 means saepk*/
			sae_pk = 1;
		}

	}

	ext_supp_rates_ie = cfg80211_find_ie(WLAN_EID_EXT_SUPP_RATES, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (ext_supp_rates_ie != NULL) {
		int supp_rates_len;
		int i;

		supp_rates_len = ext_supp_rates_ie[1];
		for (i = 2; i < supp_rates_len + 2; i++) {
			if (ext_supp_rates_ie[i] == 0xfb) {
				h2e_only |= 0x1;
				break;
			}
		}
	}

	if (sec_cfg == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR, "sec_cfg is NULL\n");
		return;
	}

	akm_map_org = sec_cfg->AKMMap;
	pairwise_cipher_org = sec_cfg->PairwiseCipher;
	group_cipher_org = sec_cfg->GroupCipher;
#ifdef DOT11W_PMF_SUPPORT
	igtk_cipher_org = sec_cfg->PmfCfg.igtk_cipher;
#endif
	sec_update_sae_cap(h2e_only, sae_pk, &sec_cfg->sae_cap);

	/* Privacy */
	CFG80211_SyncPrivacy(pAd, mbss_idx, pBeacon);

#ifdef HOSTAPD_HS_R2_SUPPORT
	hs2_osen_ie = (UCHAR *)cfg80211_find_vendor_ie(OUI_WFA, OUI_WFA_HS20_OSEN,
			pBeacon->beacon_tail, pBeacon->beacon_tail_len);
	if (hs2_osen_ie != NULL) {
		hotspot_update_bssflag(pAd, fgASANEnable, 1, pHSCtrl);
		hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
	} else {
		hotspot_update_bssflag(pAd, fgASANEnable, 0, pHSCtrl);
		hotspot_update_bss_info_to_cr4(pAd, mbss_idx, wdev->bss_info_argument.ucBssIndex);
	}
#endif

	if (pCfg) {
		if (pCfg->privacy) {
			/* Security */
			if (pCfg->auth_type == NL80211_AUTHTYPE_SHARED_KEY) {
				/* Shared WEP */
				/* wdev->WepStatus = Ndis802_11WEPEnabled; */
				/* wdev->AuthMode = Ndis802_11AuthModeShared; */
				CLEAR_SEC_AKM(wdev->SecConfig.AKMMap);
				CLEAR_CIPHER(wdev->SecConfig.PairwiseCipher);
				CLEAR_CIPHER(wdev->SecConfig.GroupCipher);
				SET_AKM_SHARED(wdev->SecConfig.AKMMap);
				SET_CIPHER_WEP(wdev->SecConfig.PairwiseCipher);
				SET_CIPHER_WEP(wdev->SecConfig.GroupCipher);
			}
#ifdef HOSTAPD_HS_R2_SUPPORT
			else if (hs2_osen_ie != NULL) {
				CLEAR_SEC_AKM(wdev->SecConfig.AKMMap);
				CLEAR_PAIRWISE_CIPHER(&wdev->SecConfig);
				CLEAR_GROUP_CIPHER(&wdev->SecConfig);
#ifdef DOT11W_PMF_SUPPORT
				wdev->SecConfig.PmfCfg.MFPC = 0;
				wdev->SecConfig.PmfCfg.MFPR = 0;
				wdev->SecConfig.PmfCfg.igtk_cipher = 0;
				wdev->SecConfig.PmfCfg.Desired_MFPC = 0;
				wdev->SecConfig.PmfCfg.Desired_MFPR = 0;
				wdev->SecConfig.PmfCfg.Desired_PMFSHA256 = 0;
#endif
				pMbss->HotSpotCtrl.bASANEnable = 1;
				SET_AKM_WPA2(wdev->SecConfig.AKMMap);
				SET_CIPHER_CCMP128(wdev->SecConfig.GroupCipher);
				SET_CIPHER_CCMP128(wdev->SecConfig.PairwiseCipher);
			}
#endif
			else
				CFG80211_ParseBeaconIE(pAd, pMbss, wdev, pCfg, (UCHAR *)wpa_ie, (UCHAR *)rsn_ie);
			if ((IS_CIPHER_NONE(wdev->SecConfig.PairwiseCipher)) &&
				(IS_AKM_OPEN(wdev->SecConfig.AKMMap))) {
				/* WEP Auto */
				/* wdev->WepStatus = Ndis802_11WEPEnabled; */
				/* wdev->AuthMode = Ndis802_11AuthModeAutoSwitch; */
				CLEAR_SEC_AKM(wdev->SecConfig.AKMMap);
				CLEAR_CIPHER(wdev->SecConfig.PairwiseCipher);
				CLEAR_CIPHER(wdev->SecConfig.GroupCipher);
				SET_AKM_OPEN(wdev->SecConfig.AKMMap);
				SET_AKM_AUTOSWITCH(wdev->SecConfig.AKMMap);
				SET_CIPHER_WEP(wdev->SecConfig.PairwiseCipher);
				SET_CIPHER_WEP(wdev->SecConfig.GroupCipher);
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
				    "change to AuthMode=%s,Cipher(%s %s)\n",
				    GetAuthModeStr(wdev->SecConfig.AKMMap),
				    GetEncryModeStr(wdev->SecConfig.PairwiseCipher),
				    GetEncryModeStr(wdev->SecConfig.GroupCipher));
			}
		} else {
			SET_AKM_OPEN(wdev->SecConfig.AKMMap);
			SET_CIPHER_NONE(wdev->SecConfig.PairwiseCipher);
			CFG80211_ParseBeaconIE(pAd, pMbss, wdev, pCfg, (UCHAR *)wpa_ie, (UCHAR *)rsn_ie);
		}
		/* Dtim */
		if ((pCfg->dtim_period >= 1) && (pCfg->dtim_period <= 255)) {
			wdev->bss_info_argument.dtim_period = pCfg->dtim_period;
			pMbss->DtimPeriod = pCfg->dtim_period;
		}
		/* beacon rate*/
		if (pCfg->bitrate != 0)
			cfg80211_update_beacon_rate(wdev, pCfg);
	}
#ifdef HOSTAPD_HS_R3_SUPPORT
	else if (hs2_osen_ie != NULL) {
		CLEAR_SEC_AKM(wdev->SecConfig.AKMMap);
		CLEAR_PAIRWISE_CIPHER(&wdev->SecConfig);
		CLEAR_GROUP_CIPHER(&wdev->SecConfig);
#ifdef DOT11W_PMF_SUPPORT
		wdev->SecConfig.PmfCfg.MFPC = 0;
		wdev->SecConfig.PmfCfg.MFPR = 0;
		wdev->SecConfig.PmfCfg.igtk_cipher = 0;
		wdev->SecConfig.PmfCfg.Desired_MFPC = 0;
		wdev->SecConfig.PmfCfg.Desired_MFPR = 0;
		wdev->SecConfig.PmfCfg.Desired_PMFSHA256 = 0;
#endif
		pMbss->HotSpotCtrl.bASANEnable = 1;
		SET_AKM_WPA2(wdev->SecConfig.AKMMap);
		SET_CIPHER_CCMP128(wdev->SecConfig.GroupCipher);
		SET_CIPHER_CCMP128(wdev->SecConfig.PairwiseCipher);
	}
#endif
	else
		CFG80211_ParseBeaconIE(pAd, pMbss, wdev, pCfg, (UCHAR *)wpa_ie, (UCHAR *)rsn_ie);

#ifdef DOT11W_PMF_SUPPORT
	/*decide pmf configuration after parsing beacon from hostapd*/
	APPMFInit(pAd, wdev);
#endif /* DOT11W_PMF_SUPPORT */

	if (akm_map_org != sec_cfg->AKMMap ||
		pairwise_cipher_org != sec_cfg->PairwiseCipher ||
		group_cipher_org != sec_cfg->GroupCipher ||
#ifdef DOT11W_PMF_SUPPORT
		igtk_cipher_org != sec_cfg->PmfCfg.igtk_cipher
#endif
	) {
		*bss_info_change = TRUE;

#if defined(DOT11W_PMF_SUPPORT)
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
			"security changes from %08x,%08x,%08x,%08x to %08x,%08x,%08x,%08x\n",
			akm_map_org, pairwise_cipher_org, group_cipher_org, igtk_cipher_org,
			sec_cfg->AKMMap, sec_cfg->PairwiseCipher, sec_cfg->GroupCipher,
			sec_cfg->PmfCfg.igtk_cipher);
#else
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
			"security changes from %08x,%08x,%08x to %08x,%08x,%08x\n",
			akm_map_org, pairwise_cipher_org, group_cipher_org,
			sec_cfg->AKMMap, sec_cfg->PairwiseCipher, sec_cfg->GroupCipher);
#endif
	} else {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
			"security(akm,pcipher,gcipher) does not change 0x%08x, 0x%08x, 0x%08x\n",
			akm_map_org, pairwise_cipher_org, group_cipher_org);
	}

#ifndef RT_CFG80211_SUPPORT
	/* Disable Driver-Internal Rekey */
	pMbss->WPAREKEY.ReKeyInterval = 0;
	pMbss->WPAREKEY.ReKeyMethod = DISABLE_REKEY;
#endif /*#ifndef RT_CFG80211_SUPPORT*/
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"security (akm, pcipher, gcipher, igtk):  0x%x, 0x%x, 0x%x, 0x%x\n",
		sec_cfg->AKMMap, sec_cfg->PairwiseCipher, sec_cfg->GroupCipher, sec_cfg->PmfCfg.igtk_cipher);

	WPAMakeRSNIE(wdev->wdev_type, &wdev->SecConfig, NULL);
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
		"Make new RSNE after beacon set\n");

#ifdef CFG_RSNO_SUPPORT
	if (CFG80211_handle_rsne_override(
		pAd, wdev, pBeacon->beacon_tail, pBeacon->beacon_tail_len) == TRUE)
		cfg80211_ap_parse_rsn_ie(pAd,
			&wdev->SecConfig_ext,
			wdev->rsneo_content,
			FALSE, TRUE);
	if (CFG80211_handle_rsne_override_2(
		pAd, wdev, pBeacon->beacon_tail, pBeacon->beacon_tail_len) == TRUE)
		cfg80211_ap_parse_rsn_ie(pAd,
			&wdev->SecConfig_ext,
			wdev->rsneo2_content,
			FALSE, TRUE);

	CFG80211_handle_rsnxe_override(
		pAd, wdev, h2e_only, pBeacon->beacon_tail, pBeacon->beacon_tail_len);
#endif /* CFG_RSNO_SUPPORT */
}

#ifdef ANDLINK_V4_0
VOID CFG80211_SyncAndlinkV4(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon)
{
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[mbss_idx];
	struct wifi_dev *wdev = &pMbss->wdev;

	if (pAd->CommonCfg.andlink_enable) {
		struct andlink_wifi_ch_info wifi_ch_info;
		/*security mode*/
		snprintf(wifi_ch_info.sec_mode, ANDLINK_SEC_LEN, "%s", GetAuthModeStr(wdev->SecConfig.AKMMap));
		/*pwd*/
		snprintf(wifi_ch_info.pwd, LEN_PSK, "%s", wdev->SecConfig.PSK);
		/*ssid*/
		snprintf(wifi_ch_info.ssid, SSID_LEN, "%s", pMbss->Ssid);
		wifi_ch_info.is_hidden = pMbss->bHideSsid;
		wifi_ch_info.max_sta_num = pMbss->MaxStaNum;
		/*send wireless event*/
		andlink_send_wifi_change_event(pAd, wdev, &wifi_ch_info);
	}
}
#endif/*ANDLINK_V4_0*/

static INT CFG80211DRV_UpdateApSettingFromBeacon(
	PRTMP_ADAPTER pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pCfg,
	OUT unsigned char *bss_info_change)
{
	CFG80211_UpdateBcnInterval(pAd, mbss_idx, pBeacon, pCfg, bss_info_change);
	CFG80211_SyncBssVHTCap(pAd, mbss_idx, pBeacon);
	CFG80211_SyncBssHECap(pAd, mbss_idx, pBeacon);
	CFG80211_SyncBssHTCap(pAd, mbss_idx, pBeacon);
	CFG80211_SyncBssHTOp(pAd, mbss_idx, pBeacon);
#ifdef MBO_SUPPORT
	CFG80211_SyncWFAMbo(pAd, mbss_idx, pBeacon);
#endif

#ifdef HOSTAPD_OWE_SUPPORT
	CFG80211_SyncWFAOwe(pAd, mbss_idx, pBeacon);
#endif
#ifdef CONFIG_DOT11U_INTERWORKING
	CFG80211_SyncInterworking(pAd, mbss_idx, pBeacon);
	CFG80211_SyncAdvProto(pAd, mbss_idx, pBeacon);
	CFG80211_SyncRoamingConsortium(pAd, mbss_idx, pBeacon);
#endif /* CONFIG_DOT11U_INTERWORKING */
#ifdef HOSTAPD_HS_R2_SUPPORT
	CFG80211_SyncWfaHs2(pAd, mbss_idx, pBeacon);
	CFG80211_SyncWfaP2p(pAd, mbss_idx, pBeacon);
#endif
	/* sec_cfg related / HS R2 related */
	CFG80211_SyncCapability(pAd, mbss_idx, pBeacon);
	CFG80211_SyncSsid(pAd, mbss_idx, pBeacon, pCfg);
#ifdef DISABLE_HOSTAPD_BEACON
	/* WPS related */
	CFG80211_SyncWpaWmeWps(pAd, mbss_idx, pBeacon);
#ifdef HOSTAPD_11R_SUPPORT
	CFG80211_SyncMobilityDomain(pAd, mbss_idx, pBeacon);
#endif /* HOSTAPD_11R_SUPPORT */
#endif /* DISABLE_HOSTAPD_BEACON */

#ifndef DISABLE_HOSTAPD_BEACON
#ifdef HOSTAPD_AUTO_CH_SUPPORT
	CFG80211_SyncDsParam(pAd, mbss_idx, pBeacon);
	CFG80211_SyncHtOp(pAd, mbss_idx, pBeacon);
	CFG80211_SyncVhtOp(pAd, mbss_idx, pBeacon);
#endif
#endif

	if (pAd->CommonCfg.wifi_cert) {
		/* WMM EDCA Parameter */
		CFG80211_SyncBssWmmCap(pAd, mbss_idx, pBeacon);
	}
	CFG80211_SyncBssUAPSDCap(pAd, mbss_idx, pBeacon);
	/* SEC related update */
	CFG80211_UpdateSecurity(pAd, mbss_idx, pBeacon, pCfg, bss_info_change);
	/* Cap Info update after SEC updated */
	CFG80211_SyncMbssCapInfo(pAd, mbss_idx, pBeacon);
#ifdef HOSTAPD_11K_SUPPORT
	/* Cap Info update */
	CFG80211_SyncWFARrm(pAd, mbss_idx, pBeacon);
#endif

#ifdef ANDLINK_V4_0
	CFG80211_SyncAndlinkV4(pAd, mbss_idx, pBeacon);
#endif/*ANDLINK_V4_0*/

	return TRUE;
}


VOID CFG80211_UpdateBeacon(
	VOID                                            *pAdOrg,
	UCHAR										    *beacon_head_buf,
	UINT32											beacon_head_len,
	UCHAR										    *beacon_tail_buf,
	UINT32											beacon_tail_len,
	BOOLEAN											isAllUpdate,
	UINT32											apidx)

{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	PUCHAR pBeaconFrame;
	UCHAR *tmac_info, New_Tim_Len = 0;
	UINT32 beacon_len = 0;
	BSS_STRUCT *pMbss;
	struct wifi_dev *wdev;
	COMMON_CONFIG *pComCfg;
#ifdef RT_CFG80211_SUPPORT
	RTMP_ARCH_OP *arch_ops = hc_get_arch_ops(pAd->hdev_ctrl);
#endif
	struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(pAd->hdev_ctrl);
	UCHAR tx_hw_hdr_len = cap->tx_hw_hdr_len;
#ifdef BCN_V2_SUPPORT	/* add bcn v2 support , 1.5k beacon support */
	UINT8 max_v2_bcn_num = cap->max_v2_bcn_num;
#endif
	BCN_BUF_STRUCT *pbcn_buf = NULL;

	pComCfg = &pAd->CommonCfg;
	pMbss = &pAd->ApCfg.MBSSID[apidx];
	wdev = &pMbss->wdev;
	pbcn_buf = &wdev->bcn_buf;

	if (!pMbss || !pMbss->wdev.bcn_buf.BeaconPkt)
		return;

	OS_SEM_LOCK(&pbcn_buf->BcnContentLock);
	tmac_info = (UCHAR *)GET_OS_PKT_DATAPTR(pMbss->wdev.bcn_buf.BeaconPkt);

	pBeaconFrame = (UCHAR *)(tmac_info + tx_hw_hdr_len);

	if (isAllUpdate) { /* Invoke From CFG80211 OPS For setting Beacon buffer */
		/* 1. Update the Buf before TIM IE */
		NdisCopyMemory(pBeaconFrame, beacon_head_buf, beacon_head_len);
		/* 2. Update the Location of TIM IE */
		pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.tim_ie_offset = beacon_head_len;

		/* 3. Store the Tail Part For appending later */
		if (pCfg80211_ctrl->beacon_tail_buf != NULL)
			os_free_mem(pCfg80211_ctrl->beacon_tail_buf);

		os_alloc_mem(NULL, (UCHAR **)&pCfg80211_ctrl->beacon_tail_buf, beacon_tail_len);

		if (pCfg80211_ctrl->beacon_tail_buf != NULL) {
			NdisCopyMemory(pCfg80211_ctrl->beacon_tail_buf, beacon_tail_buf, beacon_tail_len);
			pCfg80211_ctrl->beacon_tail_len = beacon_tail_len;
		} else {
			pCfg80211_ctrl->beacon_tail_len = 0;
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
				"CFG80211 Beacon: MEM ALLOC ERROR\n");
		}

		/* return; */
	} else { /* Invoke From Beacon Timer */
		if (pAd->ApCfg.DtimCount == 0)
			pAd->ApCfg.DtimCount = pAd->ApCfg.DtimPeriod - 1;
		else
			pAd->ApCfg.DtimCount -= 1;
	}

	/* 4. Update the TIM IE */
	New_Tim_Len = CFG80211DRV_UpdateTimIE(pBeaconFrame,
					  pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.tim_ie_offset,
					  pAd->ApCfg.DtimCount,
					  pAd->ApCfg.DtimPeriod);

	/* 5. Update the Buffer AFTER TIM IE */
	if (pCfg80211_ctrl->beacon_tail_buf != NULL) {
		NdisCopyMemory(pBeaconFrame + pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.tim_ie_offset + New_Tim_Len,
					   pCfg80211_ctrl->beacon_tail_buf, pCfg80211_ctrl->beacon_tail_len);
		beacon_len = pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.tim_ie_offset + pCfg80211_ctrl->beacon_tail_len
					 + New_Tim_Len;
		pAd->ApCfg.MBSSID[apidx].wdev.bcn_buf.FrameLen = beacon_len;
	} else {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR, "BEACON ====> OOPS\n");
		OS_SEM_UNLOCK(&pbcn_buf->BcnContentLock);
		return;
	}

	OS_SEM_UNLOCK(&pbcn_buf->BcnContentLock);

	arch_ops->archUpdateBeacon(pAd, &pAd->ApCfg.MBSSID[apidx].wdev, TRUE, BCN_UPDATE_MAX);
}

VOID update_bss_info(
	IN RTMP_ADAPTER *pAd,
	IN struct wifi_dev *wdev)
{
	os_move_mem(wdev->bss_info_argument.Bssid, wdev->bssid, MAC_ADDR_LEN);
	WDEV_BSS_STATE(wdev) = BSS_ACTIVE;
	wdev->bss_info_argument.u8BssInfoFeature = 0;

	bssinfo_feature_decision(wdev, wdev->bss_info_argument.u4ConnectionType,
		wdev->bss_info_argument.bmc_wlan_idx, &wdev->bss_info_argument.u8BssInfoFeature);
	wdev->bss_info_argument.peer_wlan_idx = wdev->bss_info_argument.bmc_wlan_idx;

	wdev->bss_info_argument.CipherSuit = SecHWCipherSuitMapping(wdev->SecConfig.PairwiseCipher);

	AsicBssInfoUpdate(pAd, &wdev->bss_info_argument);
	MTWF_DBG(NULL, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO, "New AP BSSID "MACSTR" (%d)\n",
		MAC2STR(wdev->bssid), wdev->PhyMode);
}

#ifdef DOT11V_MBSSID_SUPPORT
BOOLEAN asic_sync_update_11v_mbssid_info(struct _RTMP_ADAPTER *pAd)
{
	BSS_STRUCT *bss = NULL, *t_bss = NULL;
	struct wifi_dev *wdev = NULL;
	INT32 IdBss;
	struct _BSS_INFO_ARGUMENT_T bss_info;
	struct mbss_11v_ctrl *mbss_11v_ctrl = NULL;

	if (!IS_BSSID_11V_ENABLED(pAd))
		return FALSE;

	for (IdBss = MAIN_MBSSID; IdBss < pAd->ApCfg.BssidNum; IdBss++) {
		bss = &pAd->ApCfg.MBSSID[IdBss];
		wdev = &bss->wdev;

		if (!wdev->DevInfo.WdevActive)
			continue;

		mbss_11v_ctrl = &bss->mbss_11v;
		t_bss = mbss_11v_ctrl->mbss_11v_t_bss;
		if ((pAd->ApCfg.dot11v_mbssid_bitmap & (1 << bss->mbss_idx)) && t_bss) {
			memcpy(&bss_info, &wdev->bss_info_argument, sizeof(bss_info));
			bss_info.u8BssInfoFeature = BSS_INFO_11V_MBSSID_FEATURE;

			OS_SEM_LOCK(&t_bss->mbss_11v.mbss_11v_ctrl_lock);
			bss_info.max_bssid_indicator = t_bss->mbss_11v.mbss_11v_max_bssid_indicator;
			OS_SEM_UNLOCK(&t_bss->mbss_11v.mbss_11v_ctrl_lock);

			if (bss->mbss_idx >= mbss_11v_ctrl->mbss_11v_t_bss_idx) {
				bss_info.mbssid_index =
					(bss->mbss_idx - mbss_11v_ctrl->mbss_11v_t_bss_idx);
				bss_info.tx_omac_idx = mbss_11v_ctrl->mbss_11v_t_bss_idx;
			}
			AsicBssInfoUpdate(pAd, &bss_info);
			if (IS_BSSID_11V_NON_TRANS(mbss_11v_ctrl) && wdev->bAllowBeaconing) {
				wdev->bAllowBeaconing = FALSE;
				wdev->bcn_buf.bBcnSntReq = FALSE;
				bss_info.u8BssInfoFeature = BSS_INFO_OFFLOAD_PKT_FEATURE;
				AsicBssInfoUpdate(pAd, &bss_info);
				wdev->bcn_buf.bBcnSntReq = TRUE;
			}
		}
	}

	return TRUE;
}
#endif /* DOT11V_MBSSID_SUPPORT */

BOOLEAN CFG80211DRV_OpsBeaconSet(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pBcnCfg)
{
#ifdef DOT11V_MBSSID_SUPPORT
	BOOLEAN bMakeBeacon = FALSE;
#endif /* DOT11V_MBSSID_SUPPORT */
	UINT apidx = pBeacon->apidx;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apidx];
	struct wifi_dev *wdev = &pMbss->wdev;
	BOOLEAN bss_info_change = FALSE;
	UINT16 tr_tb_idx;

	tr_tb_idx = wdev->tr_tb_idx;
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE, "80211> ==>\n");

	if (IsHcRadioCurStatOffByWdev(wdev)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> radio is off.%s\n",
			wdev->if_dev ? wdev->if_dev->name : "");
		return FALSE;
	}

	CFG80211DRV_UpdateApSettingFromBeacon(
		pAd, pBeacon->apidx, pBeacon, pBcnCfg, &bss_info_change);
	APSecInit(pAd, wdev);


	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
		"80211> ==> apidx=%u, tr_tb_idx=%u, bss_info_change=%d\n", apidx, tr_tb_idx, bss_info_change);

	if (bss_info_change) {
		struct _NDIS_AP_802_11_PMKID *pmkid_cache = PD_GET_PMKID_PTR(pAd->physical_dev);
		INT i;
#ifdef DOT11_EHT_BE
		UCHAR *own_mac_mld = wdev->bss_info_argument.mld_info.mld_addr;
#endif /* DOT11_EHT_BE */

		/* flush all pmk cache while ap stop*/
		OS_SEM_LOCK(&pmkid_cache->pmkid_lock);
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> flush pmkid cache of bss ("MACSTR")<==\n",
			MAC2STR(wdev->if_addr));
		for (i = 0; i < MAX_PMKID_COUNT; i++) {
			RTMPDeletePMKIDCache(pmkid_cache, wdev->if_addr, i);
#ifdef DOT11_EHT_BE
			if (!MAC_ADDR_EQUAL(own_mac_mld, wdev->if_addr) && !MAC_ADDR_EQUAL(own_mac_mld, ZERO_MAC_ADDR)) {
				MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
					"clear PMKID cache for MLO, own mac mld="MACSTR"\n", own_mac_mld);
				RTMPDeletePMKIDCache(pmkid_cache, own_mac_mld, i);
			}
#endif /* DOT11_EHT_BE */
		}
		OS_SEM_UNLOCK(&pmkid_cache->pmkid_lock);


		MSDU_FORBID_CLEAR(wdev, MSDU_FORBID_CONNECTION_NOT_READY);

		os_msec_delay(200);
		update_bss_info(pAd, wdev);
	}
	WDEV_BSS_STATE(wdev) = BSS_READY;
#ifdef DISABLE_HOSTAPD_BEACON
#ifdef DOT11V_MBSSID_SUPPORT
	/* if BSSID is non-transmitted, must do update by transmitted BSSID */
	if (IS_BSSID_11V_NON_TRANS(&pMbss->mbss_11v))
		wdev->bAllowBeaconing = FALSE;
	wdev = mbss_11v_get_tx_wdev(pAd, wdev, pMbss, &bMakeBeacon);
#endif /* DOT11V_MBSSID_SUPPORT */
	/* in case bss'ie may be changed by hostapd
	but bss mngr do not sync, such as ssid
	so do update here */
#ifdef CONFIG_6G_SUPPORT
	pMbss->ShortSSID = Crcbitbybitfast(pMbss->Ssid, pMbss->SsidLen);
#endif
	bss_mngr_dev_reg(&pMbss->wdev);
	MTWF_DBG(NULL, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
		"############MakeBeacon for apidx %d OpsBeaconSet \n",
		pBeacon->apidx);
#ifdef DOT11_N_SUPPORT
	SetCommonHtVht(pAd, wdev);
#endif /* DOT11_N_SUPPORT */
	UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
#else
	CFG80211_UpdateBeacon(pAd, pBeacon->beacon_head, pBeacon->beacon_head_len,
		pBeacon->beacon_tail, pBeacon->beacon_tail_len,
		TRUE, pBeacon->apidx);
#endif

	return TRUE;
}

#ifdef HOSTAPD_HS_R2_SUPPORT

BOOLEAN CFG80211DRV_SetQosParam(VOID *pAdOrg, VOID *pData, INT apindex)
{

	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	struct cfg80211_qos_map *qos_map = (struct cfg80211_qos_map *)pData;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apindex];
	PHOTSPOT_CTRL pHSCtrl = &pMbss->HotSpotCtrl;
	PUCHAR pos;
	int tmp = 0;

	OS_SEM_LOCK(&pHSCtrl->IeLock);
	if (pHSCtrl->QosMapSetIE) {
		os_free_mem(pHSCtrl->QosMapSetIE);
		pHSCtrl->QosMapSetIE = NULL;
	}
	if (qos_map == NULL) {
		OS_SEM_UNLOCK(&pHSCtrl->IeLock);
		return TRUE;
	}
	os_alloc_mem(NULL, &pHSCtrl->QosMapSetIE, (2+(qos_map->num_des * 2) + 16));
	if (pHSCtrl->QosMapSetIE == NULL) {
		OS_SEM_UNLOCK(&pHSCtrl->IeLock);
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR, "QosMapSet Alloc Fail\n");
		return FALSE;
	}
	pos = pHSCtrl->QosMapSetIE;
	*pos = IE_QOS_MAP_SET;
	pos++;
	*pos = 16 + (qos_map->num_des * 2);
	pos++;
	if (qos_map->num_des > 0) {
		memcpy(pos, (PUCHAR)qos_map->dscp_exception, (qos_map->num_des*2));
		pos += qos_map->num_des*2;
	}
	memcpy(pos, (PUCHAR)qos_map->up, 16);
    pHSCtrl->QosMapSetIELen = 2+(qos_map->num_des * 2)+16;
	OS_SEM_UNLOCK(&pHSCtrl->IeLock);
	for (tmp = 0; tmp < 21; tmp++) {
		pHSCtrl->DscpException[tmp] = 0xff;
		pHSCtrl->DscpException[tmp] |= (0xff << 8);
	}
	for (tmp = 0; tmp < 8; tmp++) {
		pHSCtrl->DscpRange[tmp] = 0xff;
		pHSCtrl->DscpRange[tmp] |= (0xff << 8);
	}
	for (tmp = 0; tmp < qos_map->num_des; tmp++) {
		pHSCtrl->DscpException[tmp] = (qos_map->dscp_exception[tmp].dscp) & 0xff;
		pHSCtrl->DscpException[tmp] |= ((qos_map->dscp_exception[tmp].up) & 0xff) << 8;
	}
	for (tmp = 0; tmp < 8; tmp++) {
		pHSCtrl->DscpRange[tmp] = (qos_map->up[tmp].low) & 0xff;
		pHSCtrl->DscpRange[tmp] |= ((qos_map->up[tmp].high) & 0xff) << 8;
	}
	return TRUE;
}
#endif

extern struct wifi_dev_ops ap_wdev_ops;
extern struct wifi_dev_ops go_wdev_ops;

BOOLEAN CFG80211DRV_OpsBeaconAdd(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pBcnCfg)
{
	BOOLEAN bss_info_change = FALSE;
	UINT16 tr_tb_idx;
	PNET_DEV pNetDev;
#ifdef DOT11V_MBSSID_SUPPORT
	BOOLEAN bMakeBeacon = FALSE;
#endif /* DOT11V_MBSSID_SUPPORT */
	UINT apidx = pBeacon->apidx;
	BSS_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apidx];
	struct wifi_dev *wdev = &pMbss->wdev;
	BCN_BUF_STRUCT *bcn_buf = &wdev->bcn_buf;

	tr_tb_idx = wdev->tr_tb_idx;
	pNetDev = pBeacon->pNetDev;
	wdev->Hostapd = Hostapd_CFG;
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE, "80211> ==>\n");

	if (IsHcRadioCurStatOffByWdev(wdev)) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE,
			"80211> radio is off.%s\n",
			wdev->if_dev ? wdev->if_dev->name : "");
		return FALSE;
	}


	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_NOTICE, "80211> ==> apidx=%u, tr_tb_idx=%u\n", apidx, tr_tb_idx);


#ifndef DISABLE_HOSTAPD_BEACON
    pAd->cfg80211_ctrl.beaconIsSetFromHostapd = TRUE; /* set here to prevent MakeBeacon do further modifications about BCN */
#endif
	CFG80211DRV_UpdateApSettingFromBeacon(pAd, apidx, pBeacon, pBcnCfg, &bss_info_change);

	APSecInit(pAd, wdev);
	bcn_buf->pWdev = wdev;

	bcn_buf->bBcnSntReq = TRUE;

#ifndef DISABLE_HOSTAPD_BEACON
	if (wdev->channel > 14)
		wdev->PhyMode = (WMODE_A | WMODE_AN);
	else
		wdev->PhyMode = (WMODE_B | WMODE_G | WMODE_GN);
#endif /* DISABLE_HOSTAPD_BEACON */

	MSDU_FORBID_CLEAR(wdev, MSDU_FORBID_CONNECTION_NOT_READY);
#ifdef BCN_PROTECTION_SUPPORT
	if (wdev->SecConfig.bcn_prot_cfg.bcn_prot_en)
		NdisMoveMemory(&wdev->bss_info_argument.bcn_prot_cfg, &wdev->SecConfig.bcn_prot_cfg, sizeof(wdev->SecConfig.bcn_prot_cfg));
#endif /*BCN_PROTECTION_SUPPORT*/
	/*
		Re-sync freq_oper info due to original bw value
		may be changed after RTMP_DRIVER_80211_CHAN_SET.
	*/
	{
		struct freq_oper OperCh = {0};

		hc_radio_query_by_wdev(wdev, &OperCh);
		/* update freq_oper into bss_info */
		NdisCopyMemory(&wdev->bss_info_argument.chan_oper,
						&OperCh, sizeof(struct freq_oper));
	}
	os_msec_delay(200);
	update_bss_info(pAd, wdev);
	WDEV_BSS_STATE(wdev) = BSS_READY;

#ifdef DISABLE_HOSTAPD_BEACON
	bss_mngr_dev_reg(wdev);
#ifdef DOT11V_MBSSID_SUPPORT
	/* if BSSID is non-transmitted, must do update by transmitted BSSID */
	wdev = mbss_11v_get_tx_wdev(pAd, wdev, pMbss, &bMakeBeacon);
#endif /* DOT11V_MBSSID_SUPPORT */
	/* in case bss'ie may be changed by hostapd
	but bss mngr do not sync, such as ssid
	so do update here */
#ifdef CONFIG_6G_SUPPORT
	pMbss->ShortSSID = Crcbitbybitfast(pMbss->Ssid, pMbss->SsidLen);
#endif
#ifdef DOT11V_MBSSID_SUPPORT
	asic_sync_update_11v_mbssid_info(pAd);
#endif /* DOT11V_MBSSID_SUPPORT */

	/* update ACL rules here */
	ApUpdateAccessControlList(pAd, apidx);

	if (wdev->cfg80211_bcn_rate) {
		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_DISABLE_TX));
		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_ENABLE_TX));
	} else
		UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
#else
	CFG80211_UpdateBeacon(pAd, pBeacon->beacon_head, pBeacon->beacon_head_len,
	pBeacon->beacon_tail, pBeacon->beacon_tail_len, TRUE, pBeacon->apidx);
#endif /*DISABLE_HOSTAPD_BEACON */

	return TRUE;
}

BOOLEAN CFG80211DRV_ApKeyDel(
	VOID                                            *pAdOrg,
	VOID                                            *pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;
	MAC_TABLE_ENTRY *pEntry;
	struct _ASIC_SEC_INFO *info = NULL;
	INT apidx;
	BSS_STRUCT *pMbss;
	struct wifi_dev *pWdev;

	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;
	apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, pKeyInfo->pNetDev);
	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
			"failed - [ERROR] can't find wdev in driver MBSS.\n");
		return FALSE;
	}
	pMbss = &pAd->ApCfg.MBSSID[apidx];
	pWdev  = &pMbss->wdev;

	if (pKeyInfo->bPairwise == FALSE) {
		UINT Wcid = 0;

		GET_GroupKey_WCID(pWdev, Wcid);
		/* Set key material to Asic */
		os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
		if (info == NULL) {
			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
					"Fail to allocate memory!\n");
			return FALSE;
		} else {
			os_zero_mem(info, sizeof(ASIC_SEC_INFO));
			info->Operation = SEC_ASIC_REMOVE_GROUP_KEY;
			info->Wcid = Wcid;
			info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
			/* Set key material to Asic */
			HW_ADDREMOVE_KEYTABLE(pAd, info);
			os_free_mem(info);
		}
	} else {
		pEntry = MacTableLookup(pAd, pKeyInfo->MAC);

		if (pEntry && (pEntry->Aid != 0)) {
			/* Set key material to Asic */
			os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
			if (info == NULL) {
				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
						"Fail to allocate memory!\n");
				return FALSE;
			}
			os_zero_mem(info, sizeof(ASIC_SEC_INFO));
			info->Operation = SEC_ASIC_REMOVE_PAIRWISE_KEY;
			info->Wcid = pEntry->wcid;
			info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
			/* clear the cipher flag to prevent Rx ICV Error log flooding */
			if (!IS_CIPHER_NONE(pEntry->SecConfig.PairwiseCipher))
				SET_CIPHER_NONE(pEntry->SecConfig.PairwiseCipher);
			/* Set key material to Asic */
			HW_ADDREMOVE_KEYTABLE(pAd, info);
			os_free_mem(info);
		}
	}

	return TRUE;
}

VOID CFG80211DRV_RtsThresholdAdd(
	VOID                                            *pAdOrg,
	struct wifi_dev *wdev,
	UINT                                            threshold)
{
	UINT32 len_thld = MAX_RTS_THRESHOLD;

	if ((threshold > 0) && (threshold <= MAX_RTS_THRESHOLD))
		len_thld = (UINT32)threshold;
	wlan_operate_set_rts_len_thld(wdev, len_thld);
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
		"=====>threshold %d\n", len_thld);
}


VOID CFG80211DRV_FragThresholdAdd(
	VOID                                            *pAdOrg,
	struct wifi_dev *wdev,
	UINT                                            threshold)
{
	if (threshold > MAX_FRAG_THRESHOLD || threshold < MIN_FRAG_THRESHOLD)
		threshold =  MAX_FRAG_THRESHOLD;
	else if (threshold % 2 == 1)
		threshold -= 1;
	wlan_operate_set_frag_thld(wdev, threshold);
	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_AP, DBG_LVL_INFO,
		"=====>operate: frag_thld=%d\n", threshold);
}

#ifdef ACK_CTS_TIMEOUT_SUPPORT
BOOLEAN CFG80211DRV_AckThresholdAdd(
	VOID * pAdOrg,
	struct wifi_dev	*wdev,
	UINT threshold)
{
	UINT32 bTimeout = 0;
	UCHAR band_idx = 0;
	PRTMP_ADAPTER pAd = NULL;
	UINT32 bCckCCAtout = 0, bCckMDRDYtout = 0;
	UINT32 bOfdmCCAtout = 0, bOfdmMDRDYtout = 0;
	UINT32 bOfdmaCCAtout = 0, bOfdmaMDRDYtout = 0;
	UINT32 bCCKTimeout = 0, bOFDMTimeout = 0, bOFDMATimeout = 0;

	MTWF_DBG(NULL, DBG_CAT_HW, CATHW_MAC, DBG_LVL_INFO,
			"threshold = %d\n", threshold);

	if ((threshold > 0) && (threshold <= MAX_ACK_THRESHOLD)) {
		bTimeout = (UINT32)threshold;

		pAd = (PRTMP_ADAPTER)pAdOrg;
		if (NULL == wdev || NULL == pAd) {
			MTWF_PRINT("[%s](%d): wdev is null or pAd is null.\n",
			__func__, __LINE__);
			return FALSE;
		}

		band_idx = HcGetBandByWdev(wdev);

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
			return FALSE;
		}

		bCCKTimeout = DEFAULT_TMAC_CDTR_VALUE;
		bCCKTimeout += (bTimeout << 16) | bTimeout;
		if (set_ack_timeout_mode_byband(pAd, bCCKTimeout, band_idx, CCK_TIME_OUT) == FALSE) {
			MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
				"SET CTS/ACK CCK Timeout Fail!!\n");
			return FALSE;
		}

		bOFDMTimeout = DEFAULT_TMAC_ODTR_VALUE;
		bOFDMTimeout += (bTimeout << 16) | bTimeout;
		if (set_ack_timeout_mode_byband(pAd, bOFDMTimeout, band_idx, OFDM_TIME_OUT) == FALSE) {
			MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
				"SET CTS/ACK OFDM Timeout Fail!!\n");
			return FALSE;
		}

		bOFDMATimeout = DEFAULT_TMAC_OMDTR_VALUE;
		bOFDMATimeout += (bTimeout << 16) | bTimeout;
		if (set_ack_timeout_mode_byband(pAd, bOFDMATimeout, band_idx, OFDMA_TIME_OUT) == FALSE) {
			MTWF_DBG(pAd, DBG_CAT_HW, CATHW_MAC, DBG_LVL_WARN,
				"SET CTS/ACK OFDMA Timeout Fail!!\n");
			return FALSE;
		}

		return TRUE;
	} else {
		MTWF_DBG(NULL, DBG_CAT_HW, CATHW_MAC, DBG_LVL_ERROR,
			"Error!! threshold range (%d, %d]\n", 0, MAX_ACK_THRESHOLD);
		return FALSE;
	}
}

#endif/*ACK_CTS_TIMEOUT_SUPPORT*/

BOOLEAN CFG80211DRV_ApKeyAdd(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *pWdev,
	struct __CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo)
{
#ifdef CONFIG_AP_SUPPORT
	MAC_TABLE_ENTRY *pEntry = NULL;
	UINT apidx;
	BSS_STRUCT *pMbss;
#ifdef CONFIG_VLAN_GTK_SUPPORT
	PNET_DEV net_dev;
#endif
	struct _STA_TR_ENTRY *tr_entry = NULL;

	if (!pAd || !pWdev || !pKeyInfo)
		return FALSE;

	if (pWdev->wdev_type == WDEV_TYPE_AP) {
		apidx = pWdev->func_idx;
		if (pWdev->func_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", pWdev->func_idx);
			return FALSE;
		}
	} else
		return FALSE;
	pMbss = &pAd->ApCfg.MBSSID[apidx];
	MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "=====>\n");

	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40
		|| pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104) {
		SET_CIPHER_WEP(pWdev->SecConfig.PairwiseCipher);
		SET_CIPHER_WEP(pWdev->SecConfig.GroupCipher);
		{
			CIPHER_KEY	*pSharedKey;
			POS_COOKIE	pObj;

			pObj = (POS_COOKIE) pAd->OS_Cookie;
			pSharedKey = &pAd->SharedKey[apidx][pKeyInfo->KeyId];
			pSharedKey->KeyLen = pKeyInfo->KeyLen;
			os_move_mem(pSharedKey->Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);

			if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40)
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_WEP64;
			else
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_WEP128;

			AsicAddSharedKeyEntry(pAd, apidx, pKeyInfo->KeyId, pSharedKey);
			if (pKeyInfo->bPairwise == FALSE) {
				struct _ASIC_SEC_INFO *info = NULL;
				UINT Wcid = 0;

				/*if gtk key idx > set pairwise key idx , then not install*/
				if (pAd->ApCfg.MBSSID[apidx].wdev.SecConfig.PairwiseKeyId_set_flag == 1
					&& pAd->ApCfg.MBSSID[apidx].wdev.SecConfig.PairwiseKeyId < pKeyInfo->KeyId)
					return TRUE;
				NdisCopyMemory(&pAd->cfg80211_ctrl.WepKeyInfoBackup,
					pKeyInfo, sizeof(CMD_RTPRIV_IOCTL_80211_KEY));
				pWdev->SecConfig.WepKey[pKeyInfo->KeyId].KeyLen = pKeyInfo->KeyLen;
				os_move_mem(pWdev->SecConfig.WepKey[pKeyInfo->KeyId].Key,
					pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
				pWdev->SecConfig.GroupKeyId = pKeyInfo->KeyId;
				os_move_mem(pWdev->SecConfig.GTK,
					pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
				/* Get a specific WCID to record this MBSS key attribute */
				GET_GroupKey_WCID(pWdev, Wcid);
				/* Set key material to Asic */
				os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
				if (info) {
					os_zero_mem(info, sizeof(ASIC_SEC_INFO));
					info->Operation = SEC_ASIC_ADD_GROUP_KEY;
					info->Direction = SEC_ASIC_KEY_TX;
					info->Wcid = Wcid;
					info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
					info->Cipher = pWdev->SecConfig.GroupCipher;
					info->KeyIdx = pKeyInfo->KeyId;
					os_move_mem(&info->PeerAddr[0], BROADCAST_ADDR, MAC_ADDR_LEN);
					/* Install Shared key */
					os_move_mem(info->Key.Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
					info->Key.KeyLen = pKeyInfo->KeyLen;
					HW_ADDREMOVE_KEYTABLE(pAd, info);
					MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
						"B/MC KEY pKeyInfo->KeyId %d pWdev->SecConfig.WepKey[pKeyInfo->KeyId].KeyLen %d\n"
						, pKeyInfo->KeyId, pWdev->SecConfig.WepKey[pKeyInfo->KeyId].KeyLen);
					os_free_mem(info);
				}
			} else {
				if (!MAC_ADDR_EQUAL(pKeyInfo->MAC, ZERO_MAC_ADDR)) {
#if defined(HOSTAPD_STA_MLD_MAC) && defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
					pEntry = MacTableLookupForTx(pAd, pKeyInfo->MAC, pWdev);
#else
					pEntry = MacTableLookup(pAd, pKeyInfo->MAC);
#endif /* CFG_CFG80211_VERSION */

				}

				if (pEntry) {
					struct _ASIC_SEC_INFO *info = NULL;

					pEntry->SecConfig.PairwiseKeyId = pKeyInfo->KeyId;
					SET_CIPHER_WEP(pEntry->SecConfig.PairwiseCipher);
					/* Set key material to Asic */
					os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
					if (info) {
						os_zero_mem(info, sizeof(ASIC_SEC_INFO));
						info->Operation = SEC_ASIC_ADD_PAIRWISE_KEY;
						info->Direction = SEC_ASIC_KEY_BOTH;
						info->Wcid = pEntry->wcid;
						info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
						info->KeyIdx = pEntry->SecConfig.PairwiseKeyId;
						info->Cipher = pEntry->SecConfig.PairwiseCipher;
						info->KeyIdx = pEntry->SecConfig.PairwiseKeyId;
						os_move_mem(info->Key.Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
						os_move_mem(&info->PeerAddr[0], pEntry->Addr, MAC_ADDR_LEN);
						info->Key.KeyLen = pKeyInfo->KeyLen;
						HW_ADDREMOVE_KEYTABLE(pAd, info);
						MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
								 "UNICAST Info.Key.KeyLen %d pKeyInfo->KeyId %d Info.Key.KeyLen %d\n"
								  , info->Key.KeyLen, pKeyInfo->KeyId, info->Key.KeyLen);
						os_free_mem(info);
					}
				}
			}
		}
	} else if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WPA) {
		if (pKeyInfo->bPairwise == FALSE) {
			struct _ASIC_SEC_INFO *info = NULL;
			USHORT Wcid;

			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
				"GTK pKeyInfo->KeyId=%d\n", pKeyInfo->KeyId);

			/* Get a specific WCID to record this MBSS key attribute */
			GET_GroupKey_WCID(pWdev, Wcid);
			pAd->SharedKey[apidx][pKeyInfo->KeyId].KeyLen = LEN_TK;
			CLEAR_GROUP_CIPHER(&pWdev->SecConfig);
			/*clear group chiper or it will abnormal when first config TKIP*/

			switch (pKeyInfo->cipher) {
			case Ndis802_11GCMP256Enable:
				SET_CIPHER_GCMP256(pWdev->SecConfig.GroupCipher);
				break;

			case Ndis802_11AESEnable:
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_AES;
				SET_CIPHER_CCMP128(pWdev->SecConfig.GroupCipher);
				break;

			case Ndis802_11TKIPEnable:
				pAd->SharedKey[apidx][pKeyInfo->KeyId].CipherAlg = CIPHER_TKIP;
				SET_CIPHER_TKIP(pWdev->SecConfig.GroupCipher);
				break;
			case Ndis802_11GCMP128Enable:
				SET_CIPHER_GCMP128(pWdev->SecConfig.GroupCipher);

				break;

			case Ndis802_11CCMP256Enable:
				SET_CIPHER_CCMP256(pWdev->SecConfig.GroupCipher);

				break;
			}

			/* Get a specific WCID to record this MBSS key attribute */
#ifdef CONFIG_VLAN_GTK_SUPPORT
				net_dev = pKeyInfo->pNetDev;

				if (net_dev->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_AP) {
					/* Get a specific WCID to record this MBSS key attribute */
					GET_GroupKey_WCID(pWdev, Wcid);
				} else if (net_dev->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_AP_VLAN) {
					struct vlan_gtk_info *vg_info = CFG80211_GetVlanInfoByVlandev(pWdev, net_dev);

					if (!vg_info) {
						MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
							"%s(): invalid vlan_dev name=%s addr=%p\n", __func__,
							net_dev->name, net_dev);
						return FALSE;
					}
						Wcid = vg_info->vlan_bmc_idx;
						os_move_mem(vg_info->vlan_gtk, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
						vg_info->gtk_len = pKeyInfo->KeyLen;
					}
#else
			GET_GroupKey_WCID(pWdev, Wcid);
#endif
			/* Set key material to Asic */
			os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
			if (info) {
				os_zero_mem(info, sizeof(ASIC_SEC_INFO));
				info->Operation = SEC_ASIC_ADD_GROUP_KEY;
				info->Direction = SEC_ASIC_KEY_TX;
				info->Wcid = Wcid;
				info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
				info->Cipher = pWdev->SecConfig.GroupCipher;
				info->KeyIdx = pKeyInfo->KeyId;
				pWdev->SecConfig.GroupKeyId = info->KeyIdx;
				pWdev->SecConfig.gtk_Reinstall = 1;

				os_move_mem(&info->PeerAddr[0], BROADCAST_ADDR, MAC_ADDR_LEN);
				/* Install Shared key */
				os_move_mem(pWdev->SecConfig.GTK, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
#ifdef RT_CFG80211_SUPPORT
				pWdev->Is_hostapd_gtk = 1;
				os_move_mem(pWdev->Hostapd_GTK, pWdev->SecConfig.GTK, LEN_MAX_GTK);
#endif
				os_move_mem(info->Key.Key, pWdev->SecConfig.GTK, LEN_MAX_GTK);
				HOST_DBG(pAd, "ApKeyAdd GTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
				HOST_HEXDUMP(pAd, "GTK", (UCHAR *)info->Key.Key, LEN_MAX_GTK);
				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
					"ApKeyAdd GTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
				WPAInstallKey(pAd, info, TRUE, TRUE);
				pWdev->SecConfig.Handshake.GTKState = REKEY_ESTABLISHED;
				os_free_mem(info);
			}
		} else {

#if defined(HOSTAPD_STA_MLD_MAC) && defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
			pEntry = MacTableLookupForTx(pAd, pKeyInfo->MAC, pWdev);
#else
			pEntry = MacTableLookup(pAd, pKeyInfo->MAC);
#endif /* CFG_CFG80211_VERSION */

#ifdef DOT11R_FT_SUPPORT
			if ((!pEntry || (pEntry && (pEntry->wdev != &pAd->ApCfg.MBSSID[apidx].wdev))) &&
				pWdev->FtCfg.FtCapFlag.Dot11rFtEnable &&
				(IS_AKM_FT_WPA2PSK(pWdev->SecConfig.AKMMap) ||
				IS_AKM_FT_WPA2(pWdev->SecConfig.AKMMap) ||
				IS_AKM_FT_SAE_SHA256(pWdev->SecConfig.AKMMap) ||
				IS_AKM_FT_SAE_EXT(pWdev->SecConfig.AKMMap))) {
				if (pEntry && (pEntry->wdev != &pAd->ApCfg.MBSSID[apidx].wdev)) {
					if (pEntry && pEntry->wdev) {
						MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_NOTICE,
							"Delete old entry associated with %02x:%02x:%02x:%02x:%02x:%02x\n",
							PRINT_MAC(pEntry->wdev->bssid));
					}
					MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
				}

				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
					"FT Auth --> insert %02x:%02x:%02x:%02x:%02x:%02x to MAC TABLE\n",
					PRINT_MAC(pKeyInfo->MAC));

				pEntry = MacTableInsertEntry(pAd, pKeyInfo->MAC,
					&pAd->ApCfg.MBSSID[apidx].wdev,
					ENTRY_CLIENT, OPMODE_AP,
					TRUE, MLD_STA_NONE, NULL);
			}
#endif

			if (pEntry) {
				struct _ASIC_SEC_INFO *info = NULL;

				CLEAR_PAIRWISE_CIPHER(&pEntry->SecConfig);

				switch (pKeyInfo->cipher) {
				case Ndis802_11GCMP256Enable:
					SET_CIPHER_GCMP256(pEntry->SecConfig.PairwiseCipher);
					break;

				case Ndis802_11AESEnable:
					SET_CIPHER_CCMP128(pEntry->SecConfig.PairwiseCipher);
					break;

				case Ndis802_11TKIPEnable:
					SET_CIPHER_TKIP(pEntry->SecConfig.PairwiseCipher);
					break;

				case Ndis802_11GCMP128Enable:
					SET_CIPHER_GCMP128(pEntry->SecConfig.PairwiseCipher);
					break;

				case Ndis802_11CCMP256Enable:
					SET_CIPHER_CCMP256(pEntry->SecConfig.PairwiseCipher);
					break;

				}

				NdisCopyMemory(&pEntry->SecConfig.PTK[OFFSET_OF_PTK_TK],
						pKeyInfo->KeyBuf, OFFSET_OF_PTK_TK);


				NdisCopyMemory(&pEntry->SecConfig.PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyBuf, OFFSET_OF_PTK_TK);
				/* Set key material to Asic */
				os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));

				if (info) {
					UINT8 CopyKeySize = 0;

					if ((LEN_MAX_PTK - LEN_PTK_KCK - LEN_PTK_KEK) < sizeof(pKeyInfo->KeyBuf))
						CopyKeySize = LEN_MAX_PTK - LEN_PTK_KCK - LEN_PTK_KEK;
					else
						CopyKeySize = sizeof(pKeyInfo->KeyBuf);
					MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY,
							 DBG_LVL_INFO, "PTK CopyKeySize is %d\n", CopyKeySize);
					os_zero_mem(info, sizeof(ASIC_SEC_INFO));
					NdisCopyMemory(&pEntry->SecConfig.PTK[LEN_PTK_KCK + LEN_PTK_KEK], pKeyInfo->KeyBuf, CopyKeySize);
					info->Operation = SEC_ASIC_ADD_PAIRWISE_KEY;
					info->Direction = SEC_ASIC_KEY_BOTH;
					info->Wcid = pEntry->wcid;
					info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
					info->Cipher = pEntry->SecConfig.PairwiseCipher;
					pEntry->SecConfig.PairwiseKeyId = info->KeyIdx;
					pEntry->SecConfig.ptk_Reinstall = 1;
					info->KeyIdx = (UINT8)(pKeyInfo->KeyId & 0x0fff);
					os_move_mem(&info->PeerAddr[0], pEntry->Addr, MAC_ADDR_LEN);
					os_move_mem(info->Key.Key, &pEntry->SecConfig.PTK[LEN_PTK_KCK + LEN_PTK_KEK], (LEN_TK + LEN_TK2));
					HOST_DBG(pAd, "ApKeyAdd PTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
					HOST_HEXDUMP(pAd, "PTK", (UCHAR *)info->Key.Key, LEN_TK + LEN_TK2);
					MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
						"ApKeyAdd PTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
#ifdef DOT11_EHT_BE
					if (pEntry && pEntry->mlo.mlo_en) {
						mlo_install_key(pEntry, info, FALSE, TRUE);
						mlo_update_port_secure(pEntry, NULL);
					} else if (!pEntry->mlo_join)
#endif
						WPAInstallKey(pAd, info, TRUE, TRUE);

					tr_entry = tr_entry_get(pAd, pEntry->wcid);
#ifdef SW_CONNECT_SUPPORT
					if (IS_CIPHER_CCMP128(pEntry->SecConfig.PairwiseCipher) &&
						(IS_SW_MAIN_STA(tr_entry) || IS_SW_STA(tr_entry))) {
						SET_CIPHER_CCMP128(pEntry->SecConfig.SwPairwiseKey.CipherAlg);
						os_move_mem(&pEntry->SecConfig.SwPairwiseKey.Key, &pEntry->SecConfig.PTK[LEN_PTK_KCK + LEN_PTK_KEK], LEN_TK);
						pEntry->SecConfig.SwPairwiseKey.KeyLen = LEN_TK;
#ifdef CONFIG_LINUX_CRYPTO
						if (pEntry->SecConfig.tfm) {
							mt_os_crypto_aead_key_free(pEntry->SecConfig.tfm);
							pEntry->SecConfig.tfm = NULL;
						}
						pEntry->SecConfig.tfm = mt_os_crypto_aead_key_setup_encrypt("ccm(aes)",
								&pEntry->SecConfig.SwPairwiseKey.Key[0],
								LEN_TK, LEN_CCMP_MIC);
						ASSERT(pEntry->SecConfig.tfm);
#endif /* CONFIG_LINUX_CRYPTO */
						HOST_HEXDUMP(pAd, "AES PTK", &pEntry->SecConfig.PTK[LEN_PTK_KCK + LEN_PTK_KEK], LEN_TK);
						HOST_HEXDUMP(pAd, "PairwiseKey KEY", &pEntry->SecConfig.SwPairwiseKey.Key[0], LEN_TK);
					}
#endif /* SW_CONNECT_SUPPORT */
					if (pEntry && IS_AKM_WPA2(pEntry->SecConfig.AKMMap)) {
						pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
						if (tr_entry != NULL)
							tr_entry->PortSecured = WPA_802_1X_PORT_SECURED;
						pEntry->SecConfig.Handshake.WpaState = AS_PTKINITDONE;
						WifiSysUpdatePortSecur(pAd, pEntry, NULL);
						MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
							"AID:%d, PortSecured\n", pEntry->Aid);
					}
					/* set the flag to FALSE for group re-key handshake */
					if (pEntry->SecConfig.IsFirstKeyHandshake)
						pEntry->SecConfig.IsFirstKeyHandshake = FALSE;
					os_free_mem(info);
				} else {
					MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY,
						DBG_LVL_ERROR, "struct alloc fail\n");
				}
			} else
				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
					"CFG: Set Security. (PAIRWISE) But pEntry NULL\n");
		}
		}
#ifdef DOT11W_PMF_SUPPORT
		else if ((pKeyInfo->KeyType == RT_CMD_80211_KEY_AES_CMAC)
				|| (pKeyInfo->KeyType == RT_CMD_80211_KEY_AES_GMAC256)
				|| (pKeyInfo->KeyType == RT_CMD_80211_KEY_AES_GMAC128)
				|| (pKeyInfo->KeyType == RT_CMD_80211_KEY_AES_CMAC256)) {
			if ((pKeyInfo->bPairwise == FALSE) && (pKeyInfo->KeyId == 4 || pKeyInfo->KeyId == 5)) {
				PPMF_CFG pPmfCfg = &pWdev->SecConfig.PmfCfg;
				struct _ASIC_SEC_INFO *info = NULL;
				USHORT Wcid;

				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
					"PMF IGTK pKeyInfo->KeyId=%d\n", pKeyInfo->KeyId);

				switch (pKeyInfo->KeyType) {
				case RT_CMD_80211_KEY_AES_CMAC:
					SET_CIPHER_BIP_CMAC128(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_CMAC256:
					SET_CIPHER_BIP_CMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_GMAC128:
					SET_CIPHER_BIP_GMAC128(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_GMAC256:
					SET_CIPHER_BIP_GMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;
				}

				/* Get a specific WCID to record this MBSS key attribute */
				GET_GroupKey_WCID(pWdev, Wcid);
				/* Set key material to Asic */
				os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
				if (info) {
					os_zero_mem(info, sizeof(ASIC_SEC_INFO));
					info->Operation = SEC_ASIC_ADD_GROUP_KEY;
					info->Direction = SEC_ASIC_KEY_TX;
					info->Wcid = Wcid;
					info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
					info->Cipher = pWdev->SecConfig.GroupCipher;
					info->Cipher |= pWdev->SecConfig.PmfCfg.igtk_cipher;
					info->KeyIdx = pWdev->SecConfig.GroupKeyId;
					pPmfCfg->IGTK_KeyIdx = pKeyInfo->KeyId;
					pWdev->SecConfig.igtk_Reinstall = 1;
					info->igtk_key_idx = pKeyInfo->KeyId;
					info->IGTKKeyLen = LEN_BIP128_IGTK;
					if (IS_CIPHER_BIP_CMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher)
							|| IS_CIPHER_BIP_GMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher))
						info->IGTKKeyLen = LEN_BIP256_IGTK;
					os_move_mem(&info->PeerAddr[0], BROADCAST_ADDR, MAC_ADDR_LEN);
					os_zero_mem(&pPmfCfg->IPN[pKeyInfo->KeyId - 4][0], LEN_WPA_TSC);
					/* Install Shared key */
					os_move_mem(&pPmfCfg->IGTK[pKeyInfo->KeyId - 4][0], pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
					os_move_mem(info->Key.Key, pWdev->SecConfig.GTK, LEN_MAX_GTK);
					os_move_mem(info->IGTK, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
					HOST_DBG(pAd, "ApKeyAdd IGTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
					HOST_HEXDUMP(pAd, "IGTK", (UCHAR *)info->IGTK, pKeyInfo->KeyLen);
					MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
						"ApKeyAdd IGTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
					WPAInstallKey(pAd, info, TRUE, TRUE);
					pWdev->SecConfig.Handshake.GTKState = REKEY_ESTABLISHED;
					os_free_mem(info);
				}
			}
#ifdef BCN_PROTECTION_SUPPORT
			else if ((pKeyInfo->bPairwise == FALSE) && (pKeyInfo->KeyId == 6 || pKeyInfo->KeyId == 7)) {
				struct _ASIC_SEC_INFO *info = NULL;
				USHORT Wcid;

				MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
					"PMF BIGTK pKeyInfo->KeyId=%d\n", pKeyInfo->KeyId);

				switch (pKeyInfo->KeyType) {
				case RT_CMD_80211_KEY_AES_CMAC:
					SET_CIPHER_BIP_CMAC128(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_CMAC256:
					SET_CIPHER_BIP_CMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_GMAC128:
					SET_CIPHER_BIP_GMAC128(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;

				case RT_CMD_80211_KEY_AES_GMAC256:
					SET_CIPHER_BIP_GMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher);
					break;
				}

				 /* Get a specific WCID to record this MBSS key attribute */
				GET_GroupKey_WCID(pWdev, Wcid);
				/* Set key material to Asic */
				if (pWdev->SecConfig.Bigtk_Reinstall != 1 || (pKeyInfo->KeyId != pWdev->SecConfig.bcn_prot_cfg.bigtk_key_idx)) {
					os_alloc_mem(NULL, (UCHAR **)&info, sizeof(ASIC_SEC_INFO));
					if (info) {
						UCHAR tx_tsc[20];

						os_zero_mem(info, sizeof(ASIC_SEC_INFO));
						NdisZeroMemory(&tx_tsc, sizeof(tx_tsc));
						info->Operation = SEC_ASIC_ADD_GROUP_KEY;
						info->Direction = SEC_ASIC_KEY_TX;
						info->Wcid = Wcid;
						info->BssIndex = pWdev->bss_info_argument.ucBssIndex;
						info->Cipher = pWdev->SecConfig.GroupCipher;
						info->Cipher |= pWdev->SecConfig.PmfCfg.igtk_cipher;
						info->KeyIdx = pWdev->SecConfig.GroupKeyId;
						info->bigtk_key_len = LEN_BIP128_IGTK;
						if (IS_CIPHER_BIP_CMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher)
								|| IS_CIPHER_BIP_GMAC256(pWdev->SecConfig.PmfCfg.igtk_cipher))
							info->bigtk_key_len = LEN_BIP256_IGTK;
						info->bigtk_key_idx = pKeyInfo->KeyId;
						AsicGetTxTsc(pAd, pWdev, TSC_TYPE_BIGTK_PN_MASK, tx_tsc);
						tx_tsc[0]++;
						os_move_mem(&info->tx_tsc, &tx_tsc, LEN_WPA_TSC);
						os_move_mem(&info->PeerAddr[0], BROADCAST_ADDR, MAC_ADDR_LEN);
						os_move_mem(info->Key.Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
						os_move_mem(info->bigtk, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
						/* Install Shared key */
						NdisZeroMemory(&pWdev->bss_info_argument.bcn_prot_cfg, sizeof(pWdev->bss_info_argument.bcn_prot_cfg));
						NdisZeroMemory(&pWdev->SecConfig.bcn_prot_cfg, sizeof(pWdev->SecConfig.bcn_prot_cfg));
						os_move_mem(&pWdev->SecConfig.bcn_prot_cfg.bigtk[pKeyInfo->KeyId - 6][0], pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
						HOST_DBG(pAd, "ApKeyAdd BIGTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
						HOST_HEXDUMP(pAd, "BIGTK", (UCHAR *)pWdev->SecConfig.bcn_prot_cfg.bigtk[pKeyInfo->KeyId - 6], LEN_MAX_BIGTK);
						MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
							"ApKeyAdd BIGTK, wcid=%d, cipher=0x%x, operation=%d\n", info->Wcid, info->Cipher, IS_REMOVEKEY_OPERATION(info));
						os_zero_mem(&pWdev->SecConfig.bcn_prot_cfg.bipn[pKeyInfo->KeyId - 6], LEN_WPA_TSC);
						pWdev->SecConfig.bcn_prot_cfg.bigtk_cipher = info->Cipher;
						pWdev->SecConfig.bcn_prot_cfg.bigtk_key_idx = pKeyInfo->KeyId;
						pWdev->SecConfig.bcn_prot_cfg.bcn_prot_en = 1;
						NdisMoveMemory(&pWdev->bss_info_argument.bcn_prot_cfg, &pWdev->SecConfig.bcn_prot_cfg, sizeof(pWdev->SecConfig.bcn_prot_cfg));
						WPAInstallKey(pAd, info, TRUE, TRUE);
						pWdev->SecConfig.Handshake.GTKState = REKEY_ESTABLISHED;
						os_free_mem(info);
					}
				}
			}
#endif /* BCN_PROTECTION_SUPPORT */
		}
#endif /* DOT11W_PMF_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
	return TRUE;
}


INT CFG80211_StaPortSecured(
	IN VOID                                         *pAdCB,
	IN UCHAR					*pMac,
	IN UINT						flag)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	MAC_TABLE_ENTRY *pEntry;
	STA_TR_ENTRY *tr_entry;

	pEntry = MacTableLookup(pAd, pMac);

	if (!pEntry)
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "Can't find pEntry\n");
	else {
		tr_entry = tr_entry_get(pAd, pEntry->wcid);

		if (flag) {
			/* Update status and set Port as Secured */
			pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
			tr_entry->PortSecured = WPA_802_1X_PORT_SECURED;
			pEntry->SecConfig.Handshake.WpaState = AS_PTKINITDONE;
			WifiSysUpdatePortSecur(pAd, pEntry, NULL);
			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
				"AID:%d, PortSecured\n", pEntry->Aid);
			trigger_garp_for_sta(pAd, pEntry->wdev, pEntry->Addr);
#ifdef MWDS
			MWDSAPPeerEnable(pAd, pEntry);
#endif
#ifdef WIFI_TWT_SUPPORT
			twt_peer_join_btwt_id_0(pEntry->wdev, pEntry);
#endif /* WIFI_TWT_SUPPORT */
#ifdef QOS_R1
#ifdef MSCS_PROPRIETARY
#ifdef DOT11_EHT_BE
			if ((pEntry->mlo.mlo_en && (pEntry->mlo.is_setup_link_entry)) || (!pEntry->mlo.mlo_en))
#endif
			{
				if (IS_QOS_ENABLE(pAd) && pAd->SupportFastPath && pEntry->MSCSSupport) {
					pEntry->dabs_trans_id = 1;
					Send_DABS_Announce(pAd, pEntry->wcid);
					RTMPSetTimer(&pEntry->DABSRetryTimer, DABS_WAIT_TIME);
					OS_SET_BIT(DABS_TIMER_RUNNING, &pEntry->DABSTimerFlag);
				}
			}
#endif
#endif
		} else {
			pEntry->PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
			tr_entry->PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "AID:%d, PortNotSecured\n", pEntry->Aid);
		}
	}
#ifdef ZERO_HANDOFF_ROAMING
	if (pEntry->wdev->is_vbss && !pEntry->is_vbss_entry)
		zr_hdo_rm_add_sta_event_to_daemon(pAd, pEntry->wdev, pEntry);
#endif
	return 0;
}

INT CFG80211_ApStaDel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *pwdev,
	struct mt_priv_ops_sta_del_info *pApStaDelInfo,
	UINT reason)
{
	MAC_TABLE_ENTRY *pEntry;
	PUCHAR pMac = NULL;
	MLME_DISCONNECT_STRUCT mlme_disconn;
	MLME_QUEUE_ELEM *mlme_entry = NULL;
	BOOLEAN enqueue_success = FALSE;

	if (pApStaDelInfo->pSta_MAC != NULL)
		pMac = (PUCHAR)pApStaDelInfo->pSta_MAC;

	if (pMac == NULL) {
		if (!MlmeEnqueueWithWdev(pAd, STA_DEL_MACHINE, 0, 0, NULL, 0, pwdev, TRUE, &mlme_entry)) {
			MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_WTBL, DBG_LVL_ERROR,
				"enqueue MLME STA_DEL_MACHINE Fail!\n");
		} else
			enqueue_success = TRUE;
	} else {
		pEntry = MacTableLookup(pAd, pMac);
		if (pEntry && !MAC_ADDR_EQUAL(pwdev->if_addr, pEntry->wdev->if_addr)) {
			MTWF_DBG(pAd, DBG_CAT_MLME, CATPROTO_BND_STRG, DBG_LVL_NOTICE,
						"STA %pM is belong to bss(%pM, %s), not to bss(%pM, %s), ignore sta del!\n",
						MAC2STR(pMac), MAC2STR(pEntry->wdev->if_addr),
						pEntry->wdev->if_dev->name, MAC2STR(pwdev->if_addr),
						pwdev->if_dev->name);
			return 0;
		}
		if ((pEntry != NULL) || (MAC_ADDR_EQUAL(BROADCAST_ADDR, pMac))) {
			os_move_mem(mlme_disconn.addr, pMac, MAC_ADDR_LEN);

			if (pApStaDelInfo->subtype == SUBTYPE_DISASSOC) {
				if (!reason)
					mlme_disconn.reason = REASON_DISASSOC_STA_LEAVING;
				else
					mlme_disconn.reason = reason;

				if (!MlmeEnqueueWithWdev(pAd, ASSOC_FSM, ASSOC_FSM_MLME_DISASSOC_REQ,
					sizeof(MLME_DISCONNECT_STRUCT), &mlme_disconn, 0, pwdev, TRUE, &mlme_entry)) {
					MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_ASSOC, DBG_LVL_ERROR,
					"enqueue MLME ASSOC_FSM_MLME_DISASSOC_REQ Fail!\n");
				} else
					enqueue_success = TRUE;
			} else {
				if (!reason)
					mlme_disconn.reason = REASON_DEAUTH_STA_LEAVING;
				else
					mlme_disconn.reason = reason;

				if (!MlmeEnqueueWithWdev(pAd, AUTH_FSM, AUTH_FSM_MLME_DEAUTH_REQ,
					sizeof(MLME_DISCONNECT_STRUCT), &mlme_disconn, 0, pwdev, TRUE, &mlme_entry)) {
					MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_AUTH, DBG_LVL_ERROR,
					"enqueue MLME AUTH_FSM_MLME_DEAUTH_REQ Fail!\n");
				} else
					enqueue_success = TRUE;
			}
		} else {
			MTWF_DBG(pAd, DBG_CAT_MLME, CATMLME_WTBL, DBG_LVL_INFO,
				"Can't find pEntry in ApStaDel\n");
		}
	}

	if (enqueue_success) {
		RTMP_MLME_HANDLER(pAd);
		if (!RTMP_OS_WAIT_FOR_COMPLETION_TIMEOUT(&(mlme_entry->mlme_ack_done),
			RTMPMsecsToJiffies(500)))
			MTWF_DBG(pAd, DBG_CAT_MLME, CATMLME_WTBL, DBG_LVL_ERROR,
				"MLME Timeout!!\n");
	}

	return 0;
}

#ifdef HOSTAPD_PMKID_IN_DRIVER_SUPPORT
INT CFG80211_ApUpdateStaPmkid(
	IN VOID			*pAdCB,
	IN VOID			*pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	MAC_TABLE_ENTRY *pEntry = NULL;
	struct mt_priv_pmkid *pApPmkidEntry = NULL;
	UCHAR fake_PMK[LEN_MAX_PMK] = {0};
	struct _NDIS_AP_802_11_PMKID *pmkid_cache;

	pApPmkidEntry = (struct mt_priv_pmkid *)pData;
	pmkid_cache = PD_GET_PMKID_PTR(pAd->physical_dev);

	if (!pApPmkidEntry) {
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
			"pApPmkidEntry is NULL!\n");
		return -1;
	}

	MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
		"update sta pmkid in cache: %d (1-add, 0-remove)\n", pApPmkidEntry->AddRemove);

	/*
	*	add - bssid: MLD addr of AP, link_addr: link addr of STA, sta: MLD addr of STA
	*	remove - bssid: MLD addr of AP, link_addr: link addr of AP, sta: MLD addr of STA
	*/
	if (pApPmkidEntry->AddRemove) /* add */
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
			"bssid="MACSTR", link_addr (STA)="MACSTR", sta="MACSTR"\n",
			MAC2STR(pApPmkidEntry->bssid), MAC2STR(pApPmkidEntry->link_addr), MAC2STR(pApPmkidEntry->sta));
	else /* remove */
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO,
			"bssid="MACSTR", link_addr (AP)="MACSTR", sta="MACSTR"\n",
			MAC2STR(pApPmkidEntry->bssid), MAC2STR(pApPmkidEntry->link_addr), MAC2STR(pApPmkidEntry->sta));

	pEntry = MacTableLookup(pAd, pApPmkidEntry->link_addr);

	/*to do: also check if pEntry belong to same bssid, as sent by hostapd*/
	if (pApPmkidEntry->AddRemove == 0) { /* remove pmkid according to STA's MLD addr */
		if (pApPmkidEntry->akmp & HOSTAPD_AKM_DPP) {
			MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_NOTICE,
			"AKM DPP, skip remove PMKID\n");
			return 0;
		}
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_NOTICE,
			"remove PMKID from peer Address: "MACSTR"\n",
			MAC2STR(pApPmkidEntry->sta));
		OS_SEM_LOCK(&pmkid_cache->pmkid_lock);
		RTMPDeletePMKIDCacheForSTA(pmkid_cache, pApPmkidEntry->bssid, pApPmkidEntry->link_addr, pApPmkidEntry->sta, FALSE);
		OS_SEM_UNLOCK(&pmkid_cache->pmkid_lock);
	} else if (pEntry && (pApPmkidEntry->AddRemove == 1)) { /* add pmkid for existing pEntry */
		NdisCopyMemory(pEntry->PmkidByHostapd, pApPmkidEntry->pmkid, LEN_PMKID);
		MTWF_DBG(NULL, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_NOTICE,
			"add PMKID in pEntryAddress: "MACSTR"\n",
			MAC2STR(pApPmkidEntry->sta));
		hex_dump_with_lvl("Add PMKID", pEntry->PmkidByHostapd, LEN_PMKID, DBG_LVL_INFO);
		OS_SEM_LOCK(&pmkid_cache->pmkid_lock);
		RTMPAddPMKIDCache(pmkid_cache,
				pApPmkidEntry->bssid,
				pApPmkidEntry->sta,
				pApPmkidEntry->pmkid,
				fake_PMK,
				FALSE,
				pEntry->mlo_join,
				LEN_PMK,
#ifndef RT_CFG80211_SUPPORT
				pAd->ApCfg.MBSSID[pEntry->func_tb_idx].PMKCachePeriod
#else
				0
#endif
		);
			OS_SEM_UNLOCK(&pmkid_cache->pmkid_lock);
	} else {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
			"can't find pEntry in MacTable to add PMKID\n");
	}

	return 0;
}
#endif

INT CFG80211_setApDefaultKey(
	IN VOID                    *pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT 					Data
)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	INT32 apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, pNetdev);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
			"failed - [ERROR]can't find wdev in driver MBSS. \n");
		return FALSE;
	}

	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "Set Ap Default Key: %d\n", Data);
	pAd->ApCfg.MBSSID[apidx].wdev.SecConfig.PairwiseKeyId = Data;
#ifdef RT_CFG80211_SUPPORT
	pAd->ApCfg.MBSSID[apidx].wdev.SecConfig.PairwiseKeyId_set_flag = 1;
#endif

	return 0;
}
#ifdef BCN_PROTECTION_SUPPORT
INT CFG80211_setApBeaconDefaultKey(
	IN VOID                    * pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT					Data
)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	INT32 apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, pNetdev);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_WARN,
		"failed - [ERROR]can't find wdev in driver MBSS.\n");
		return FALSE;
	}

	MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "Set Ap Beacon Default Key: %d\n", Data);
	pAd->ApCfg.MBSSID[apidx].wdev.SecConfig.PairwiseKeyId = Data;

	return 0;
}
#endif /*BCN_PROTECTION_SUPPORT*/
#ifdef DOT11W_PMF_SUPPORT
INT CFG80211_setApDefaultMgmtKey(
	IN VOID                    *pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT 					Data
)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	INT32 apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, pNetdev);
	struct wifi_dev *pWdev = NULL;

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR,
			"failed - [ERROR]can't find wdev in driver MBSS. \n");
		return FALSE;
	}
	pWdev = &pAd->ApCfg.MBSSID[apidx].wdev;

	if ((Data == 4) || (Data == 5)) {
		pWdev->SecConfig.PmfCfg.IGTK_KeyIdx = (UINT8)Data;
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_INFO, "Set Ap Default Mgmt KeyId: %d\n", Data);
	} else {
		MTWF_DBG(pAd, DBG_CAT_SEC, CATSEC_KEY, DBG_LVL_ERROR, "fail - [ERROR]Invalid Mgmt KeyId: %d\n", Data);
	}

	return 0;
}
#endif /*DOT11W_PMF_SUPPORT*/

#ifdef CUSTOMISED_MGMT_TRANS_SUPPORT
INT CFG80211_APStaDelSendVendorEvent(PRTMP_ADAPTER pAd, VOID *pEntry, VOID *data, INT len)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data;
	UINT buflen = sizeof(UINT32);
	PMAC_TABLE_ENTRY pDiscEntry;
	PNET_DEV pNetDevIn;
	INT event_len;

	if (!pAd || !pEntry || !data)
		return -1;

	pDiscEntry = (PMAC_TABLE_ENTRY)pEntry;
	pNetDevIn = pDiscEntry->wdev->if_dev;

	if (!pNetDevIn)
		return -1;

	ctrl_data.wl_dev = pNetDevIn->ieee80211_ptr;
	event_len = MAC_ADDR_LEN + sizeof(pDiscEntry->del_reason) + len;

	ctrl_data.wiphy = pAd->pCfg80211_CB->pCfg80211_Wdev->wiphy;

	ctrl_data.uint_val = event_len;
	ctrl_data.data = pDiscEntry->Addr;
	ctrl_data.data_len = MAC_ADDR_LEN;
	ctrl_data.sub_id = pDiscEntry->del_reason;
	ctrl_data.rsp_data = data;
	ctrl_data.rsp_data_len = len;

	mtk_cfg80211_APStaDelSendVendorEvent(&ctrl_data);
	return 0;
}
#endif

INT CFG80211_ApStaDelSendEvent(PRTMP_ADAPTER pAd, const PUCHAR mac_addr, IN PNET_DEV pNetDevIn)
{

	struct wifi_dev *pWdev = NULL;
	INT32 apidx = CFG80211_FindMbssApIdxByNetDevice(pAd, pNetDevIn);

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_DISCON, DBG_LVL_ERROR,
			"failed - [ERROR]can't find wdev in driver MBSS\n");
		return FALSE;
	}

	pWdev = &pAd->ApCfg.MBSSID[apidx].wdev;

#ifdef BCN_PROTECTION_SUPPORT
	pWdev->SecConfig.Bigtk_Reinstall = 0;
#endif
	MTWF_DBG(NULL, DBG_CAT_AP, CATAP_DISCON, DBG_LVL_INFO,
		"SINGLE_DEVICE CFG : GO NOITFY THE CLIENT Disconnected\n");
	CFG80211OS_DelSta(pNetDevIn, mac_addr);


	return 0;
}

int cfg80211_set_acl_policy(PRTMP_ADAPTER pAd, UINT ifIndex, u8 mode)
{
	struct wifi_dev *wdev = &pAd->ApCfg.MBSSID[ifIndex].wdev;

	if (wdev && mode == 1 && (IS_MAP_TURNKEY_ENABLE(pAd)) &&
			(wdev->MAPCfg.DevOwnRole & BIT(MAP_ROLE_BACKHAUL_BSS)) &&
			(!(wdev->MAPCfg.DevOwnRole & BIT(MAP_ROLE_FRONTHAUL_BSS)))) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_ERROR,
				"disallowing ACL for Backhaul BSS, DevOwnRole=%u\n", wdev->MAPCfg.DevOwnRole);
		return -RTMP_IO_EFAULT;
	}

#ifdef ACL_BLK_COUNT_SUPPORT
	if (mode != 2) {
		int count = 0;

		for (count = 0; count < pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num; count++)
			pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[count].Reject_Count = 0;
	}
#endif
#ifdef MAP_R5
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.user_reason_code = 0;
#endif
	switch (mode) {
	case 0: /*Disable */
		pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy = 0;
		break;

	case 1: /* Allow All, and ACL is positive. */
		pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy = 1;
		break;

	case 2: /* Reject All, and ACL is negative. */
		pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy = 2;
		break;
#ifdef MAP_R5
	case 3:
		pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy = 2;
		pAd->ApCfg.MBSSID[ifIndex].AccessControlList.user_reason_code = 1;
		break;
#endif

	default: /*Invalid argument */
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_ERROR,
		"Invalid mode (=%d)\n", mode);
		return -RTMP_IO_EINVAL;
	}

	/* check if the change in ACL affects any existent association */
	ApUpdateAccessControlList(pAd, ifIndex);
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
	"IF(ra%d) Cfg80211_Set_AccessPolicy::(AccessPolicy=%ld)\n",
	ifIndex, pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy);

	return 0;
}

int cfg80211_add_acl_entry(PRTMP_ADAPTER pAd, UINT ifIndex, u8 *mac, u8 mac_cnt)
{
	RT_802_11_ACL *pacl = NULL;
	u8 temp_mac[MAC_ADDR_LEN] = {0};
	int isDuplicate, i, j;
	INT	ACL_length = 12;
	INT	ACL_byteNum = 6;

	if (pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num >= (MAX_NUM_OF_ACL_LIST - 1)) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
		"The AccessControlList is full, and no more entry can join the list!\n");
		return -RTMP_IO_EFAULT;
	}

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&pacl, sizeof(RT_802_11_ACL));

	if (pacl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_ERROR,
		"Allocate memory fail!\n");
		return -RTMP_IO_ENOMEM;
	}

	NdisZeroMemory(pacl, sizeof(RT_802_11_ACL));
	NdisMoveMemory(pacl, &pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));

	if (mac_cnt) {
		for (i = 0; i < mac_cnt; i++) {
			os_move_mem(temp_mac, mac + i * MAC_ADDR_LEN, MAC_ADDR_LEN);
			ACL_length = MAC_ADDR_LEN * 2;

			/* Check if this entry is duplicate. */
			isDuplicate = FALSE;

			for (j = 0; j < pacl->Num; j++) {
				if (pacl->Entry[j].AddrLength == MAC_ADDR_LEN * 2 && memcmp(pacl->Entry[j].Addr, temp_mac, 6) == 0) {
					isDuplicate = TRUE;
					MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
					"You have added an entry before,The duplicate entry is "MACSTR"\n",
					MAC2STR(temp_mac));
				}
			}

			if (!isDuplicate) {
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
				"Add "MACSTR" to list.\n", MAC2STR(temp_mac));
				pacl->Entry[pacl->Num].AddrLength = ACL_length;
				NdisMoveMemory(pacl->Entry[pacl->Num++].Addr, temp_mac, MAC_ADDR_LEN);
			}

			if (pacl->Num == MAX_NUM_OF_ACL_LIST) {
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
				"The AccessControlList is full, and no more entry can join the list!\n");
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
				"The last entry of ACL is "MACSTR"\n", MAC2STR(temp_mac));
				break;
			}

		}
	} else if (strlen(mac) == MAC_ADDR_LEN + 1) {
		os_move_mem(temp_mac, mac, MAC_ADDR_LEN);
		if (mac[MAC_ADDR_LEN] >= '1' &&  mac[MAC_ADDR_LEN] <= '9')
			ACL_length = mac[MAC_ADDR_LEN] - '0';
		else if (mac[MAC_ADDR_LEN] >= 'a' &&  mac[MAC_ADDR_LEN] <= 'c')
			ACL_length = mac[MAC_ADDR_LEN] - 'a' + 10;
		else {
			if (pacl != NULL)
				os_free_mem(pacl);
			return -RTMP_IO_EINVAL;
		}
		/* Check if this entry is duplicate. */
		isDuplicate = FALSE;
		ACL_byteNum = ACL_length / 2;
		for (j = 0; j < pacl->Num; j++) {
			if (pacl->Entry[j].AddrLength == ACL_length && memcmp(pacl->Entry[j].Addr, &temp_mac, ACL_byteNum) == 0) {
				if (ACL_length != ACL_byteNum * 2) {
					if (ACL_byteNum >= 0 && ACL_byteNum < MAC_ADDR_LEN &&
						(pacl->Entry[j].Addr[ACL_byteNum] & 0xf0) == (temp_mac[ACL_byteNum] & 0xf0)) {
						isDuplicate = TRUE;
						MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN, "You have added an entry before :\n");
						MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN, "The duplicate entry is "MACSTR"\n",
								MAC2STR(temp_mac));
					}
				} else {
					isDuplicate = TRUE;
					MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
					"You have added an entry before :\n");
					MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
					"The duplicate entry is "MACSTR"\n",
						MAC2STR(temp_mac));
				}
			}
		}

		if (!isDuplicate) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
			"Add "MACSTR" to list.\n", MAC2STR(temp_mac));
			pacl->Entry[pacl->Num].AddrLength = ACL_length;
			NdisMoveMemory(pacl->Entry[pacl->Num++].Addr, temp_mac, MAC_ADDR_LEN);
		}

		if (pacl->Num == MAX_NUM_OF_ACL_LIST) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
			"The AccessControlList is full, and no more entry can join the list!\n");
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
			"The last entry of ACL is "MACSTR"\n", MAC2STR(temp_mac));
		}
	} else {
		if (pacl != NULL)
			os_free_mem(pacl);
		return -RTMP_IO_EINVAL;
	}

	ASSERT(pacl->Num < MAX_NUM_OF_ACL_LIST);
	NdisZeroMemory(&pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));
	NdisMoveMemory(&pAd->ApCfg.MBSSID[ifIndex].AccessControlList, pacl, sizeof(RT_802_11_ACL));
	/* check if the change in ACL affects any existent association */
	ApUpdateAccessControlList(pAd, ifIndex);
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
			 "Set (cfg80211_Policy=%ld, Entry#=%ld)\n",
			  pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy,
			  pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num);
#ifdef DBG
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
			 "=============== Entry ===============\n");

	for (i = 0; i < pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num; i++) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO, "Entry #%02d: ", i + 1);

		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
				 MACSTR"-%x\n", MAC2STR(pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i].Addr),
				 pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i].AddrLength);
	}

#endif

	if (pacl != NULL)
		os_free_mem(pacl);

	return 0;
}


int cfg80211_del_acl_entry(PRTMP_ADAPTER pAd, UINT ifIndex, u8 *mac, u8 mac_cnt)
{
	RT_802_11_ACL *pacl = NULL;
	int isFound, i, j;
	u8 temp_mac[MAC_ADDR_LEN] = {0};
	UCHAR nullAddr[MAC_ADDR_LEN] = {0};
	INT	ACL_length = 12;
	INT	ACL_byteNum = 6;

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&pacl, sizeof(RT_802_11_ACL));

	if (pacl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_ERROR,
		"Allocate memory fail!!!\n");
		return -1;
	}

	NdisZeroMemory(pacl, sizeof(RT_802_11_ACL));
	NdisMoveMemory(pacl, &pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));
	NdisZeroMemory(nullAddr, MAC_ADDR_LEN);

	if (mac_cnt) {
		for (i = 0; i < mac_cnt; i++) {
			os_move_mem(temp_mac, mac + i * MAC_ADDR_LEN, MAC_ADDR_LEN);

			/* Check if this entry existed. */
			isFound = FALSE;

			for (j = 0; j < pacl->Num; j++) {
				if (pacl->Entry[j].AddrLength == MAC_ADDR_LEN * 2 && memcmp(pacl->Entry[j].Addr, temp_mac, MAC_ADDR_LEN) == 0) {
					isFound = TRUE;
					NdisZeroMemory(pacl->Entry[j].Addr, MAC_ADDR_LEN);
#ifdef ACL_BLK_COUNT_SUPPORT
					pacl->Entry[j].Reject_Count = 0;
#endif
					MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
							"The entry "MACSTR" will be deleted!\n",
							MAC2STR(temp_mac));
				}
			}

			if (!isFound) {
				MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
				"The entry "MACSTR" is not in the list!\n", MAC2STR(temp_mac));
			}
		}
	} else if (strlen(mac) == MAC_ADDR_LEN + 1) {

		os_move_mem(temp_mac, mac, MAC_ADDR_LEN);
		if (mac[MAC_ADDR_LEN] >= '1' &&  mac[MAC_ADDR_LEN] <= '9')
			ACL_length = mac[MAC_ADDR_LEN] - '0';
		else if (mac[MAC_ADDR_LEN] >= 'a' &&  mac[MAC_ADDR_LEN] <= 'c')
			ACL_length = mac[MAC_ADDR_LEN] - 'a' + 10;
		else {
			if (pacl != NULL)
				os_free_mem(pacl);
			return -1;
		}
		/* Check if this entry existed. */
		isFound = FALSE;

		ACL_byteNum = ACL_length / 2;
		for (j = 0; j < pacl->Num; j++) {
			if (pacl->Entry[j].AddrLength == ACL_length && memcmp(pacl->Entry[j].Addr, &temp_mac, ACL_byteNum) == 0) {
				if (ACL_length != ACL_byteNum * 2) {
					if (ACL_byteNum >= 0 && ACL_byteNum < MAC_ADDR_LEN &&
						(pacl->Entry[j].Addr[ACL_byteNum] & 0xf0) == (temp_mac[ACL_byteNum] & 0xf0)) {
						isFound = TRUE;
						NdisZeroMemory(pacl->Entry[j].Addr, MAC_ADDR_LEN);
#ifdef ACL_BLK_COUNT_SUPPORT
						pacl->Entry[j].Reject_Count = 0;
#endif
						MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
						"The entry "MACSTR" will be deleted!\n",
						MAC2STR(temp_mac));
					}
				} else {
					isFound = TRUE;
					NdisZeroMemory(pacl->Entry[j].Addr, MAC_ADDR_LEN);
#ifdef ACL_BLK_COUNT_SUPPORT
					pacl->Entry[j].Reject_Count = 0;
#endif
					MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
					"The entry "MACSTR" will be deleted!\n",
					MAC2STR(temp_mac));
				}
			}
		}

		if (!isFound) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
			"The entry "MACSTR" is not in the list!\n", MAC2STR(temp_mac));
		}

	} else {
		if (pacl != NULL)
			os_free_mem(pacl);
		return -1;
	}

	NdisZeroMemory(&pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy = pacl->Policy;
	ASSERT(pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num == 0);
	i = 0;

	for (j = 0; j < pacl->Num; j++) {
		if (memcmp(pacl->Entry[j].Addr, &nullAddr, MAC_ADDR_LEN) == 0)
			continue;
		else {
			pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i].AddrLength = pacl->Entry[j].AddrLength;
			NdisMoveMemory(&(pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i++]),
			pacl->Entry[j].Addr, MAC_ADDR_LEN);
		}
	}

	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num = i;
	ASSERT(pacl->Num >= pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num);
	/* check if the change in ACL affects any existent association */
	ApUpdateAccessControlList(pAd, ifIndex);
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
	"(Policy=%ld, Entry#=%ld)\n",
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy,
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num);

#ifdef DBG
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
	"=============== Entry ===============\n");

	for (i = 0; i < pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num; i++) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
				 "Entry #%02d: ", i + 1);

		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_INFO,
				 MACSTR"-%x\n", MAC2STR(pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i].Addr),
				 pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Entry[i].AddrLength);
	}

#endif
	os_free_mem(pacl);

	return 0;
}

#define MAX_ACL_DUMP_LEN 4096

/* example of list content
 * policy=1
 * 11:22:33:44:55:66
 * aa:bb:cc:dd:ee:ff
 * ...
*/
int cfg80211_get_acl_list(
	struct _RTMP_ADAPTER *pAd, UINT ifIndex, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	RT_802_11_ACL *pacl = NULL;
	s32 i;
	s32 ret = 0;
	RTMP_STRING *msg;
	UINT left_buf_size;
	INT status = 0;

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&pacl, sizeof(RT_802_11_ACL));

	if (pacl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
		"Allocate memory fail!!!\n");
		return -RTMP_IO_ENOMEM;
	}

	NdisZeroMemory(pacl, sizeof(RT_802_11_ACL));
	NdisMoveMemory(pacl, &pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));

	msg = (char *)ctrl_data->rsp_data;
	NdisZeroMemory(msg, ctrl_data->rsp_data_len);

	/*put policy into msg*/
	left_buf_size = ctrl_data->rsp_data_len - strlen(msg);
	status = snprintf(msg, left_buf_size, "policy=%ld", pacl->Policy);
	if (os_snprintf_error(left_buf_size, status)) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
					"Snprintf failed!\n");
		ret = -RTMP_IO_EFAULT;
		goto ERROR;
	}

	/*put sta mac into msg*/
	for (i = 0; i < pacl->Num; i++) {
		left_buf_size = ctrl_data->rsp_data_len - strlen(msg);
		status = snprintf(msg + strlen(msg), left_buf_size, "\n%pM-%x", pacl->Entry[i].Addr, pacl->Entry[i].AddrLength);
		if (os_snprintf_error(left_buf_size, status)) {
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
						"Snprintf failed!\n");
			ret = -RTMP_IO_EFAULT;
			goto ERROR;
		}
	}
ERROR:
	if (strlen(msg)+1 > MT_MAX_VENDOR_MSG_LEN)
		ret = -RTMP_IO_EFAULT;
	else
		ctrl_data->rsp_data_len = strlen(msg)+1;
	os_free_mem(pacl);
	return ret;
}

int cfg80211_clear_acl_list(PRTMP_ADAPTER pAd, UINT ifIndex)
{
	RT_802_11_ACL *pacl = NULL;

	/* allocate memory */
	os_alloc_mem(NULL, (UCHAR **)&pacl, sizeof(RT_802_11_ACL));

	if (pacl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_ERROR,
		"Allocate memory fail!!!\n");
		return -RTMP_IO_ENOMEM;
	}

	NdisZeroMemory(pacl, sizeof(RT_802_11_ACL));
	NdisMoveMemory(pacl, &pAd->ApCfg.MBSSID[ifIndex].AccessControlList, sizeof(RT_802_11_ACL));

	/* Check if the list is already empty. */
	if (pacl->Num == 0) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_WARN,
		"The Access Control List is empty!\n No need to clear the Access Control List!\n");

		if (pacl != NULL)
			os_free_mem(pacl);

		return 0;
	}

	ASSERT(pacl->Num > 0);

	/* Clear the entry in the list one by one */
	/* Keep the corresponding policy unchanged. */
	do {
		NdisZeroMemory(pacl->Entry[pacl->Num - 1].Addr, MAC_ADDR_LEN);
		pacl->Entry[pacl->Num - 1].AddrLength = 0;
#ifdef ACL_BLK_COUNT_SUPPORT
		pacl->Entry[pacl->Num - 1].Reject_Count = 0;
#endif
		pacl->Num -= 1;
	} while (pacl->Num > 0);

	ASSERT(pacl->Num == 0);
	NdisZeroMemory(&(pAd->ApCfg.MBSSID[ifIndex].AccessControlList), sizeof(RT_802_11_ACL));
	NdisMoveMemory(&(pAd->ApCfg.MBSSID[ifIndex].AccessControlList), pacl, sizeof(RT_802_11_ACL));
	/* check if the change in ACL affects any existent association */
	ApUpdateAccessControlList(pAd, ifIndex);

	if (pacl != NULL)
		os_free_mem(pacl);

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_ACL, DBG_LVL_NOTICE,
	"acl clear done! (Policy=%ld, Entry#=%ld)\n",
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Policy,
	pAd->ApCfg.MBSSID[ifIndex].AccessControlList.Num);

	return 0;
}

INT mtk_cfg80211_set_wireless_mode(RTMP_ADAPTER *pAd, u32 inf_idx, u8 phy_mode)
{
	INT ret = 0;
	USHORT wmode;
	struct wifi_dev *wdev = NULL;
#ifdef CONFIG_AP_SUPPORT
	UINT32 i = 0;
	struct wifi_dev *TmpWdev = NULL;
#endif
#ifdef MLD_GROUP_RESTORE
	struct _BSS_STRUCT *pMbss = NULL;
#endif
	PCHANNEL_CTRL pChCtrl = NULL;
	UCHAR BandIdx;
#ifdef MT_DFS_SUPPORT
	PDFS_PARAM pDfsParam;
#endif
#ifndef MBSS_SUPPORT
	UCHAR *mode_str = NULL;
#endif
	struct _RTMP_CHIP_CAP *cap = NULL;

	wmode = cfgmode_2_wmode((UCHAR)phy_mode);
	BandIdx = hc_get_hw_band_idx(pAd);
	pDfsParam = &pAd->CommonCfg.DfsParameter;
	cap = hc_get_chip_cap(pAd->hdev_ctrl);

	if (!wmode_valid_and_correct(pAd, &wmode)) {
		ret = -RTMP_IO_EINVAL;
		goto error;
	}

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd) {
		if (inf_idx >= MAX_MBSSID_NUM(pAd) || inf_idx >= MAX_BEACON_NUM) {
			ret = -RTMP_IO_EINVAL;
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"Invalid IfIdx: %d!!\n", inf_idx);
			goto error;
		}

		wdev = &pAd->ApCfg.MBSSID[inf_idx].wdev;
		if (wdev == NULL || wdev->if_dev == NULL) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
			"wdev or if_dev is NULL\n");
			return -RTMP_IO_EINVAL;
		}
		if (wmode == wdev->PhyMode) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"phymode can not changed!\n");
			return -RTMP_IO_EINVAL;
		}
		if (WMODE_CAP_6G(wmode) != WMODE_CAP_6G(wdev->PhyMode)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"phymode changed from %d to %d fail!\n",
				wdev->PhyMode, wmode);
			return -RTMP_IO_EINVAL;
		}
		if (WMODE_CAP_5G(wmode) != WMODE_CAP_5G(wdev->PhyMode)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"phymode changed from %d to %d fail!\n",
				wdev->PhyMode, wmode);
			return -RTMP_IO_EINVAL;
		}
		if (WMODE_CAP_2G(wmode) != WMODE_CAP_2G(wdev->PhyMode)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"phymode changed from %d to %d fail!\n",
				wdev->PhyMode, wmode);
			return -RTMP_IO_EINVAL;
		}

#ifdef DOT11_EHT_BE
		/* BE mode, delete link first */
		if (WMODE_CAP_BE(wdev->PhyMode)) {
			struct query_mld_ap_basic bss_mld_info_basic = {0};
			struct reconfig_tmr_to_t reconfig_to_info = {0};

			if (bss_mngr_query_mld_ap_basic(wdev, &bss_mld_info_basic) != NDIS_STATUS_SUCCESS) {
				if (!WMODE_CAP_BE(wmode))
					goto SwitchPhymode;
				else {
					MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
						"Invalid query %s\n", RtmpOsGetNetDevName(wdev->if_dev));
					return -RTMP_IO_EINVAL;
				}
			}

			COPY_MAC_ADDR(reconfig_to_info.mld_addr, bss_mld_info_basic.addr);
			reconfig_to_info.to_link_id_bmap |= BIT(bss_mld_info_basic.link_id);
			reconfig_to_info.args.reconfig_mode = MLD_RECONFIG_MODE_INSTANT;
#ifdef MLD_GROUP_RESTORE
			reconfig_to_info.need_restore = TRUE;
#else
			reconfig_to_info.need_restore = FALSE;
#endif
			if (eht_ap_mld_exec_link_reconfiguration(&reconfig_to_info) != NDIS_STATUS_SUCCESS) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
					"AP (%s) Del Link fail\n", RtmpOsGetNetDevName(wdev->if_dev));
				return -RTMP_IO_EINVAL;
			}

			/* try to destroy AP MLD after ML link deletion */
			if (eht_ap_mld_destroy(wdev, bss_mld_info_basic.mld_grp_idx) != NDIS_STATUS_SUCCESS) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
					"AP MLD not Destroy (grp=%d)\n", bss_mld_info_basic.mld_grp_idx);
			}
		}
#endif /* DOT11_EHT_BE */

SwitchPhymode:

		wdev->PhyMode = wmode;
#ifdef MBSS_SUPPORT

		if ((wmode == WMODE_INVALID) ||
			(WMODE_CAP_6G(wmode) && (!PHY_CAP_6G(cap->phy_caps))) ||
			(WMODE_CAP_5G(wmode) && (!PHY_CAP_5G(cap->phy_caps))) ||
			(WMODE_CAP_2G(wmode) && (!PHY_CAP_2G(cap->phy_caps))) ||
			(WMODE_CAP_N(wmode) && RTMP_TEST_MORE_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DOT_11N))) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"Invalid wireless mode(wmode=0x%x)\n", wmode);
			ret = -RTMP_IO_EINVAL;
			goto error;
		}

		if (WMODE_CAP_5G(wmode) && WMODE_CAP_2G(wmode)) {
			if (pAd->CommonCfg.dbdc_mode == 1) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"AP cannot support 2.4G/5G band mxied mode!\n");
				ret = -RTMP_IO_EINVAL;
				goto error;
			}
		}

		pAd->CommonCfg.cfg_wmode = wmode;

		for (i = 0; i < pAd->ApCfg.BssidNum; i++) {
			TmpWdev = &pAd->ApCfg.MBSSID[i].wdev;

			/*update WmmCapable*/
			if (!wmode_band_equal(TmpWdev->PhyMode, wmode))
				continue;

			TmpWdev->bWmmCapable = pAd->ApCfg.MBSSID[i].bWmmCapableOrg;
		}

		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
		"(BSS%d=%d)\n", inf_idx, wdev->PhyMode);
#else
		if (wmode_band_equal(wdev->PhyMode, wmode) == TRUE)
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"wmode_band_equal(): Band Equal!\n");
		else
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"wmode_band_equal(): Band Not Equal!\n");

		wdev->PhyMode = wmode;
		pAd->CommonCfg.cfg_wmode = wmode;
		mode_str = wmode_2_str(wmode);

		if (mode_str) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"Set WMODE=%s(0x%x)\n", mode_str, wmode);
			os_free_mem(mode_str);
		}

#endif /*MBSS_SUPPORT*/
		hc_update_wdev(wdev);
		/* Change channel state to NONE */
		pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
		hc_set_ChCtrlChListStat(pChCtrl, CH_LIST_STATE_NONE);
#ifdef EXT_BUILD_CHANNEL_LIST
		BuildChannelListEx(pAd, wdev);
#else
#ifdef MT_DFS_SUPPORT
		pDfsParam->NeedSetNewChList = DFS_SET_NEWCH_ENABLED;
#endif
		BuildChannelList(pAd, wdev);
#endif
		RTMPUpdateRateInfo(wmode, &wdev->rate);
#ifdef CONFIG_RA_PHY_RATE_SUPPORT
		rtmpeapupdaterateinfo(wmode, &wdev->rate, &wdev->eap);
#endif /* CONFIG_RA_PHY_RATE_SUPPORT */
		RTMPSetPhyMode(pAd, wdev, wmode);
#ifdef WIFI_TWT_SUPPORT
#ifdef BCN_EXTCAP_VAR_LEN
		wlan_config_set_ext_cap_length(wdev, phy_mode);
#endif /* BCN_EXTCAP_VAR_LEN */
#endif /* WIFI_TWT_SUPPORT */
		/* update new information of BSS to bss manager */
		bss_mngr_dev_update(wdev);
#ifdef DOT11_EHT_BE
		if (WMODE_CAP_BE(wmode)) {
#ifdef MLD_GROUP_RESTORE
			pMbss = wdev->func_dev;
			/* if bss not in mlo but config restore, add to restore mld group */
			if (BSS_NEED_RESTORE_MLD(pMbss)) {
				MTWF_DBG(pAd, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_NOTICE,
						"restroe to mld_group(%d)\n", pMbss->mld_grp_idx_for_restore);
				if (eht_ap_mld_add_link(wdev, pMbss->mld_grp_idx_for_restore) != NDIS_STATUS_SUCCESS) {
					MTWF_DBG(pAd, DBG_CAT_MLO, CATMLO_RECONFIG, DBG_LVL_ERROR,
					"AP (%s) Add Link fail (grp=%d), join SL\n",
					RtmpOsGetNetDevName(wdev->if_dev), pMbss->mld_grp_idx_for_restore);
					if (eht_ap_add_to_single_link_mld(wdev) != NDIS_STATUS_SUCCESS) {
						MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
							"AP(%s) add sl-mld fail!\n", RtmpOsGetNetDevName(wdev->if_dev));
						ret = -RTMP_IO_EINVAL;
						goto error;
					}
				}
			} else {
				if (eht_ap_add_to_single_link_mld(wdev) != NDIS_STATUS_SUCCESS) {
					MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
						"AP(%s) add sl-mld fail!\n", RtmpOsGetNetDevName(wdev->if_dev));
					ret = -RTMP_IO_EINVAL;
					goto error;
				}
			}
#else
			if (eht_ap_add_to_single_link_mld(wdev) != NDIS_STATUS_SUCCESS) {
				MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
					"AP(%s) add sl-mld fail!\n", RtmpOsGetNetDevName(wdev->if_dev));
				ret = -RTMP_IO_EINVAL;
				goto error;
			}
#endif
		} else
#endif
		{
			/* BE mode would disable beacon tx in ML delete link procedure.
			 * enable beacon tx after AP set as legacy target wmode (wmode before BE) */
			wdev->bcn_buf.bBcnSntReq = TRUE;

			UpdateBeaconHandler(pAd, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
		}
	}
#endif /* CONFIG_AP_SUPPORT */

	return ret;
error:
	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
	"parameters out of range\n");
	return ret;
}

NDIS_STATUS cfg80211_update_bssmngr(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
	MLME_QUEUE_ELEM *mlme_entry = NULL;

	pAd->cfg80211_ctrl.cfg80211_mlme_status = WAIT_FOR_OPS_SYNC;

	if (MlmeEnqueueWithWdev(pAd, CFG80211_SYNC_FSM, UPDATE_BSS_MNGR, 0,
		NULL, wdev->func_idx, wdev, TRUE, &mlme_entry)) {

		RTMP_MLME_HANDLER(pAd);
		if (!RTMP_OS_WAIT_FOR_COMPLETION_TIMEOUT(&(mlme_entry->mlme_ack_done),
			RTMPMsecsToJiffies(5000)))
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"MLME Timeout!!\n");
	} else
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR, "enq fail\n");

	return NDIS_STATUS_SUCCESS;
}

VOID bssmngr_update_handler(
	IN RTMP_ADAPTER *pAd,
	IN MLME_QUEUE_ELEM * pElem)
{
	struct wifi_dev *pwdev = (struct wifi_dev *)pElem->wdev;

	if (!pwdev) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MBSS, DBG_LVL_ERROR, "pwdev == NULL\n");
		return;
	}

	bss_mngr_dev_reg(pwdev);
}

VOID cfg80211_StateMachineInit(
	IN PRTMP_ADAPTER pAd,
	IN STATE_MACHINE *S,
	OUT STATE_MACHINE_FUNC	Trans[])
{
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MBSS, DBG_LVL_NOTICE, "\n");
	StateMachineInit(S, (STATE_MACHINE_FUNC *)Trans, MAX_CFG80211_STATE, MAX_CFG80211_MSG,
		(STATE_MACHINE_FUNC)Drop, CFG80211_STATE_BASE, CFG80211_EVENT_BASE);
	StateMachineSetAction(S, WAIT_FOR_OPS_SYNC, UPDATE_BSS_MNGR, (STATE_MACHINE_FUNC)bssmngr_update_handler);
}

UCHAR    OSEN_OUI[4] = {0x50, 0x6F, 0x9A, 0x01};
BOOLEAN cfg80211_ap_parse_rsn_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct _SECURITY_CONFIG *sec_cfg,
	PUCHAR rsn_ie,
	BOOLEAN reset_setting,
	BOOLEAN rsn_override)
{
	PEID_STRUCT pEid;
	PUCHAR pTmp;
	PRSN_IE_HEADER_STRUCT pRsnHeader;
	PCIPHER_SUITE_STRUCT pCipher;
	UCHAR Len;
	UINT32 PairCipherLow = 0; /* Lower unicast cipher */
	BOOLEAN bWPA2 = FALSE;
	USHORT Count;
	PAKM_SUITE_STRUCT pAKM;

	if (!rsn_ie)
		return FALSE;

	pEid = (PEID_STRUCT)rsn_ie;
	Len	= pEid->Len + 2;
	pTmp = (PUCHAR)pEid;
	pRsnHeader = (PRSN_IE_HEADER_STRUCT) pTmp;

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_SEC, DBG_LVL_INFO,
		"reset_setting = %d, rsn_override = %d\n", reset_setting, rsn_override);
	if (rsn_override) {
		pRsnHeader = (PRSN_IE_HEADER_STRUCT) &pEid->Octet[2];
		Len -= VENDOR_OUI_TYPE_LEN;
		pTmp += VENDOR_OUI_TYPE_LEN;
	}

	if (reset_setting) {
		CLEAR_SEC_AKM(sec_cfg->AKMMap);
		CLEAR_PAIRWISE_CIPHER(sec_cfg);
		CLEAR_GROUP_CIPHER(sec_cfg);
	}

	/* 0. Version must be 1 */
	if (le2cpu16(pRsnHeader->Version) == 1) {
		pTmp   += sizeof(RSN_IE_HEADER_STRUCT);
		Len	   -= sizeof(RSN_IE_HEADER_STRUCT);

		/* 1. Check group cipher */
		pCipher = (PCIPHER_SUITE_STRUCT) pTmp;

		if (os_equal_mem(pTmp, RSN_OUI, 3)) {
			MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
				"WPA2 case\n");
			bWPA2 = TRUE;

			/* wdev->AuthMode = Ndis802_11AuthModeOpen; */
			/* SET_AKM_OPEN(wdev->SecConfig.AKMMap); */
			switch (pCipher->Type) {
			case 1:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11GroupWEP40Enabled\n");
				/* wdev->GroupKeyWepStatus  = Ndis802_11GroupWEP40Enabled; */
				SET_CIPHER_WEP40(sec_cfg->GroupCipher);
				break;

			case 5:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11GroupWEP104Enabled\n");
				/* wdev->GroupKeyWepStatus  = Ndis802_11GroupWEP104Enabled; */
				SET_CIPHER_WEP104(sec_cfg->GroupCipher);
				break;

			case 2:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11TKIPEnable\n");
				/* wdev->GroupKeyWepStatus  = Ndis802_11TKIPEnable; */
				SET_CIPHER_TKIP(sec_cfg->GroupCipher);
				break;

			case 4:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11AESEnable\n");
				/* wdev->GroupKeyWepStatus  = Ndis802_11AESEnable; */
				SET_CIPHER_CCMP128(sec_cfg->GroupCipher);
				break;

			case 8:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11GCMP128Enable\n");
				SET_CIPHER_GCMP128(sec_cfg->GroupCipher);
				break;

			case 9:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11GCMP256Enable\n");
				SET_CIPHER_GCMP256(sec_cfg->GroupCipher);
				break;

			case 10:
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Ndis802_11CCMP256Enable\n");
				SET_CIPHER_CCMP256(sec_cfg->GroupCipher);
				break;

			default:
				break;
			}

			/* set to correct offset for next parsing */
			pTmp   += sizeof(CIPHER_SUITE_STRUCT);
			Len    -= sizeof(CIPHER_SUITE_STRUCT);

			/* 2. Get pairwise cipher counts */
			/* Count = *(PUSHORT) pTmp; */
			Count = (pTmp[1] << 8) + pTmp[0];
			pTmp   += sizeof(USHORT);
			Len    -= sizeof(USHORT);

			/* 3. Get pairwise cipher */
			/* Parsing all unicast cipher suite */
			while (Count > 0) {
				/* Skip OUI*/
				pCipher = (PCIPHER_SUITE_STRUCT) pTmp;
				CLEAR_CIPHER(PairCipherLow);

				switch (pCipher->Type) {
				case 1:
				case 5: /* Although WEP is not allowed in WPA related auth mode,
					 * we parse it anywa
					 */
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11WEPEnabled\n");
					SET_CIPHER_WEP104(PairCipherLow);
					SET_CIPHER_WEP104(sec_cfg->PairwiseCipher);
					break;

				case 2:
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11TKIPEnable\n");
					SET_CIPHER_TKIP(PairCipherLow);
					SET_CIPHER_TKIP(sec_cfg->PairwiseCipher);
					break;

				case 4:
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11AESEnable\n");
					SET_CIPHER_CCMP128(PairCipherLow);
					SET_CIPHER_CCMP128(sec_cfg->PairwiseCipher);
					break;

				case 8:
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11GCMP128Enable\n");
					SET_CIPHER_GCMP128(PairCipherLow);
					SET_CIPHER_GCMP128(sec_cfg->PairwiseCipher);
					break;

				case 9:
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11GCMP256Enable\n");
					SET_CIPHER_GCMP256(PairCipherLow);
					SET_CIPHER_GCMP256(sec_cfg->PairwiseCipher);
					break;

				case 10:
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Pairwise Ndis802_11CCMP256Enable\n");
					SET_CIPHER_CCMP256(PairCipherLow);
					SET_CIPHER_CCMP256(sec_cfg->PairwiseCipher);
					break;

				default:
					break;
				}

				/* pMbss->wdev.WepStatus = TmpCipher; */

				if ((sec_cfg->GroupCipher == 0)
					|| (sec_cfg->GroupCipher > PairCipherLow)) {
					sec_cfg->GroupCipher = PairCipherLow;
					MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
						"Replace Group Cipher to lower Pair Cipher (%x)\n",
						sec_cfg->GroupCipher);
				}

				pTmp += sizeof(CIPHER_SUITE_STRUCT);
				Len  -= sizeof(CIPHER_SUITE_STRUCT);
				Count--;
			}

			/* 4. get AKM suite counts */
			/* Count	= *(PUSHORT) pTmp; */
			Count = (pTmp[1] << 8) + pTmp[0];
			pTmp   += sizeof(USHORT);
			Len    -= sizeof(USHORT);

			/* 5. Get AKM ciphers*/
			/* Parsing all AKM ciphers*/
			while (Count > 0) {
				pAKM = (PAKM_SUITE_STRUCT) pTmp;


#ifdef HOSTAPD_HS_R3_SUPPORT
				if (RTMPEqualMemory(pTmp, OSEN_OUI, 4)) {
					SET_AKM_OSEN(sec_cfg->AKMMap);
					pTmp   += sizeof(AKM_SUITE_STRUCT);
					Len    -= sizeof(AKM_SUITE_STRUCT);
					Count--;
					continue;
				}
#endif
				/*add for support hostapd dpp*/
				if (RTMPEqualMemory(pTmp, DPP_OUI, 3)) {
					SET_AKM_DPP(sec_cfg->AKMMap);
					pTmp += sizeof(AKM_SUITE_STRUCT);
					Len -= sizeof(AKM_SUITE_STRUCT);
					Count--;
					continue;
				}

				if (!RTMPEqualMemory(pTmp, RSN_OUI, 3))
					break;

				switch (pAKM->Type) {
				case 0:
					SET_AKM_OPEN(sec_cfg->AKMMap);
					break;

				case 1:
					/* Set AP support WPA-enterprise mode*/
					SET_AKM_WPA2(sec_cfg->AKMMap);
					break;

				case 2:
					/* Set AP support WPA-PSK mode*/
					SET_AKM_WPA2PSK(sec_cfg->AKMMap);
					break;
#ifdef HOSTAPD_11R_SUPPORT
				case 3:
					/* Set AP support FT WPA-enterprise mode*/
					SET_AKM_FT_WPA2(sec_cfg->AKMMap);
					break;
				case 4:
					/* Set AP support WPA-PSK mode*/
					SET_AKM_FT_WPA2PSK(sec_cfg->AKMMap);
					break;
				case 25:
					/*Set AP Support FT-SAE EXT AKM25*/
					SET_AKM_FT_SAE_EXT(sec_cfg->AKMMap);
					break;
#endif /* HOSTAPD_11R_SUPPORT */
				case 5:
					/* Set AP support WPA-PSK-EAP256 mode*/
#ifdef DOT11W_PMF_SUPPORT
					SET_AKM_WPA2(sec_cfg->AKMMap);
					SET_AKM_WPA3(sec_cfg->AKMMap);
					sec_cfg->PmfCfg.Desired_PMFSHA256 = 1;
#else
					SET_AKM_WPA2_SHA256(sec_cfg->AKMMap);
#endif /*DOT11W_PMF_SUPPORT*/
					break;
				case 6:
					/* Set AP support WPA-PSK-SHA256 mode*/
#ifdef DOT11W_PMF_SUPPORT
					SET_AKM_WPA2PSK(sec_cfg->AKMMap);
					sec_cfg->PmfCfg.Desired_PMFSHA256 = 1;
					SET_AKM_WPA2PSK_SHA256(sec_cfg->AKMMap);
#endif /*DOT11W_PMF_SUPPORT*/
					break;

#ifdef HOSTAPD_SAE_SUPPORT
				case 8:
					/*Set AP Support SAE SHA256 */
					SET_AKM_SAE_SHA256(sec_cfg->AKMMap);
					break;

				case 24:
					/*Set AP Support SAE EXT AKM24 */
					SET_AKM_SAE_EXT(sec_cfg->AKMMap);
					break;
#endif
				case 9:
					/*Set AP Support FT-SAE SHA256 */
					SET_AKM_FT_SAE_SHA256(sec_cfg->AKMMap);
					break;

#ifdef HOSTAPD_SUITEB_SUPPORT
				case 11:
					SET_AKM_SUITEB_SHA256(sec_cfg->AKMMap);
					break;

				case 12:
					SET_AKM_SUITEB_SHA384(sec_cfg->AKMMap);
					break;
#endif
#ifdef HOSTAPD_OWE_SUPPORT
				case 18:
					SET_AKM_OWE(sec_cfg->AKMMap);
					break;

#endif
				default:
					SET_AKM_OPEN(sec_cfg->AKMMap);
					break;
				}

				pTmp   += sizeof(AKM_SUITE_STRUCT);
				Len    -= sizeof(AKM_SUITE_STRUCT);
				Count--;
			}

#ifdef DISABLE_HOSTAPD_BEACON
			/*check for no pairwise, pmf, ptksa, gtksa counters */

			if (Len >= 2) {
				memcpy(sec_cfg->RsnCap, pTmp, 2);
#ifdef DOT11W_PMF_SUPPORT
				{
					RSN_CAPABILITIES RsnCap;

					NdisMoveMemory(&RsnCap, pTmp, sizeof(RSN_CAPABILITIES));
					RsnCap.word = cpu2le16(RsnCap.word);
					if (RsnCap.field.MFPC == 1)
						sec_cfg->PmfCfg.Desired_MFPC = 1;
				if (RsnCap.field.MFPR == 1) {
					sec_cfg->PmfCfg.Desired_MFPR = 1;
					sec_cfg->PmfCfg.Desired_PMFSHA256 = 1;
				}
#endif	 /*DOT11W_PMF_SUPPORT*/
				if (RsnCap.field.ocvc == 1)
					sec_cfg->ocv_support = 1;
				}
	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
		"Copied Rsn cap %02x %02x\n", sec_cfg->RsnCap[0], sec_cfg->RsnCap[1]);
			}
			pTmp += sizeof(RSN_CAPABILITIES);
			Len  -= sizeof(RSN_CAPABILITIES);
			/*Extract PMKID list */
			if (Len >= sizeof(UINT16)) {
				INT offset = sizeof(UINT16);

				Count = (pTmp[1] << 8) + pTmp[0];
				if (Count > 0)
					offset += Count*LEN_PMKID;
				pTmp += offset;
				Len -= offset;
			}
#ifdef DOT11W_PMF_SUPPORT
			if (Len >= LEN_OUI_SUITE) {
				UCHAR OUI_PMF_BIP_CMAC_128_CIPHER[4] = {0x00, 0x0F, 0xAC, 0x06};
				UCHAR OUI_PMF_BIP_CMAC_256_CIPHER[4] = {0x00, 0x0F, 0xAC, 0x0d};
				UCHAR OUI_PMF_BIP_GMAC_128_CIPHER[4] = {0x00, 0x0F, 0xAC, 0x0b};
				UCHAR OUI_PMF_BIP_GMAC_256_CIPHER[4] = {0x00, 0x0F, 0xAC, 0x0c};

				if (RTMPEqualMemory(
					pTmp, OUI_PMF_BIP_CMAC_128_CIPHER, LEN_OUI_SUITE))
					SET_CIPHER_BIP_CMAC128(sec_cfg->PmfCfg.igtk_cipher);
				else if (RTMPEqualMemory(pTmp,
					OUI_PMF_BIP_CMAC_256_CIPHER, LEN_OUI_SUITE))
					SET_CIPHER_BIP_CMAC256(sec_cfg->PmfCfg.igtk_cipher);
				else if (RTMPEqualMemory(pTmp,
					OUI_PMF_BIP_GMAC_128_CIPHER, LEN_OUI_SUITE))
					SET_CIPHER_BIP_GMAC128(sec_cfg->PmfCfg.igtk_cipher);
				else if (RTMPEqualMemory(pTmp,
					OUI_PMF_BIP_GMAC_256_CIPHER, LEN_OUI_SUITE))
					SET_CIPHER_BIP_GMAC256(sec_cfg->PmfCfg.igtk_cipher);
				else
					MTWF_DBG(NULL, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"Group Mgmt Cipher Not Supported\n");
			}
#endif
#endif
			MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
				"AuthMode = 0x%x\n", sec_cfg->AKMMap);
		} else {
			MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
				"wpa2 Open/None case\n");
		}
	}
	if (bWPA2)
		return TRUE;
	else
		return FALSE;
}

#endif /* CONFIG_AP_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */

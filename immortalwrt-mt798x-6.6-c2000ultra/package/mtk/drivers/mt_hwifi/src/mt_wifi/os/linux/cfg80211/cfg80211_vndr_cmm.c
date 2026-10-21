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

	All related CFG80211 Vendor Common Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT

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

#ifdef DOT11_EHT_BE
extern struct bss_manager bss_mngr;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#ifdef TR181_SUPPORT
static int mtk_cfg80211_reply_sta(
	struct _RTMP_ADAPTER *pAd,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct wifi_station *sta;

	if (ctrl_data->rsp_data_len < sizeof(struct wifi_station))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct wifi_station);
	sta = (struct wifi_station *)ctrl_data->rsp_data;
	NdisCopyMemory(sta->MacAddr, ctrl_data->data, MAC_ADDR_LEN);
	Get_Sta_Info(pAd, sta);
	return 0;
}
#endif /* TR181_SUPPORT */

#ifdef ANDLINK_V4_0
INT mtk_andlink_get_mld_sta_idx(UCHAR *mac)
{
	struct bmgr_mld_sta *mld_sta = NULL;
	struct bmgr_mld_link *mld_link = NULL;
	u16 i, j;

	if (!mac) {
		MTWF_DBG(NULL, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
			"error:mac is NULL\n");
		return -1;
	}

	for (i = 0; BMGR_VALID_MLD_STA(i); i++) {
		mld_sta = GET_MLD_STA_BY_IDX(i);
		if (mld_sta->valid) {
			for (j = 0; j < BSS_MNGR_MAX_BAND_NUM; j++) {
				mld_link = &mld_sta->mld_link[j];
				if ((mld_link->active || mld_link->requested)
					&& strcmp(mld_link->link_addr, mac) == 0)
					return i;
			}
		}
	}

	MTWF_DBG(NULL, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
		"error:get mld sta idx fail\n");
	return -1;
}

INT	mtk_cfg80211_andlink_get_stainfo_result(
	RTMP_ADAPTER *pAd, INT apidx, void *data, INT offset)
{
	MAC_TABLE_ENTRY *pEnt = NULL;
	INT idx = 0, if_idx = 0;
	UCHAR *dst_ptr = NULL;
	UCHAR *src_ptr = NULL;
#ifdef MAC_REPEATER_SUPPORT
	REPEATER_CLIENT_ENTRY * pRepEnt = NULL;
#endif/*MAC_REPEATER_SUPPORT*/
	INT sta_idx = 0, max_num_of_mac = 33;
	struct mtk_rate_info tx_rate_info;
	struct mtk_rate_info rx_rate_info;
	struct mtk_andlink_wifi_sta_info *sta_info = (struct mtk_andlink_wifi_sta_info *)data;

	for (idx = 0; idx < max_num_of_mac; idx++) {
		pEnt = &pAd->MacTab->Content[idx];
		if (!pEnt || !(IS_ENTRY_CLIENT(pEnt) || IS_ENTRY_PEER_AP(pEnt) || IS_ENTRY_REPEATER(pEnt)))
			continue;
		if (pEnt->wdev
			&& pAd->wdev_list && (pEnt->wdev->channel != pAd->wdev_list[0]->channel))
			continue;
		if (pEnt->Sst == SST_ASSOC) {
			MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_NOTICE,
				"\nsta_idx=%d offset=%d\n", sta_idx, offset);
			sta_idx++;
			if (sta_idx <= offset)
				continue;
#ifdef MAC_REPEATER_SUPPORT
			if ((pAd->ApCfg.bMACRepeaterEn) && IS_REPT_LINK_UP(pEnt->pReptCli)) {
				if ((sta_info->sta_cnt >= ANDLINK_MAX_ASSOC_NUM) ||
					(sta_info->sta_cnt >= ARRAY_SIZE(sta_info->sta_entry))) {
					MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"rept_sta_cnt: %d > %d(MAX_ASSOC_NUM)\n",
					sta_info->sta_cnt, ANDLINK_MAX_ASSOC_NUM);
					break;
				}
				/*rept OMAC*/
				dst_ptr = sta_info->sta_entry[sta_info->sta_cnt].mac_addr;
				src_ptr = pEnt->pReptCli->OriginalAddress;
				NdisMoveMemory(dst_ptr, src_ptr, MAC_ADDR_LEN);

				/*rept VMAC*/
				dst_ptr = sta_info->sta_entry[sta_info->sta_cnt].vmac_addr;
				src_ptr = pEnt->pReptCli->CurrentAddress;
				NdisMoveMemory(dst_ptr, src_ptr, MAC_ADDR_LEN);
			}
#endif/*MAC_REPEATER_SUPPORT*/
			if (pEnt->Sst == SST_ASSOC && pEnt->EntryType == ENTRY_CLIENT) {
				if ((sta_info->sta_cnt >= ANDLINK_MAX_ASSOC_NUM) ||
					(sta_info->sta_cnt >= ARRAY_SIZE(sta_info->sta_entry))) {
					MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"sta_cnt: %d > %d(MAX_ASSOC_NUM)\n",
					sta_info->sta_cnt, ANDLINK_MAX_ASSOC_NUM);
					break;
				}
				/*sta OMAC*/
				dst_ptr = sta_info->sta_entry[sta_info->sta_cnt].mac_addr;
				src_ptr = pEnt->Addr;
				NdisMoveMemory(dst_ptr, src_ptr, MAC_ADDR_LEN);

				MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"\nRmac %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(dst_ptr));
#ifdef MAC_REPEATER_SUPPORT
				pRepEnt = RTMPLookupRepeaterCliEntry(pAd, TRUE, pEnt->Addr, TRUE);
				if (pRepEnt != NULL && pAd->ApCfg.bMACRepeaterEn) {
					/*sta vamc*/
					dst_ptr = sta_info->sta_entry[sta_info->sta_cnt].vmac_addr;
					src_ptr = pRepEnt->CurrentAddress;
					NdisMoveMemory(dst_ptr, src_ptr, MAC_ADDR_LEN);
				} else
#endif/*MAC_REPEATER_SUPPORT*/
				{
					dst_ptr = sta_info->sta_entry[sta_info->sta_cnt].vmac_addr;
					src_ptr = pEnt->Addr;
					NdisMoveMemory(dst_ptr, src_ptr, MAC_ADDR_LEN);
				}
				MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"\nVmac %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(dst_ptr));
				/*mlo_en*/
				if (pEnt->mlo.mlo_en) {
					sta_info->sta_entry[sta_info->sta_cnt].mlo_en = pEnt->mlo.mlo_en;
					sta_info->sta_entry[sta_info->sta_cnt].mlo_idx = (unsigned char)mtk_andlink_get_mld_sta_idx(dst_ptr);
					sta_info->sta_entry[sta_info->sta_cnt].setup_link = pEnt->mlo.is_setup_link_entry;
				} else {
					sta_info->sta_entry[sta_info->sta_cnt].mlo_en = 0;
					sta_info->sta_entry[sta_info->sta_cnt].mlo_idx = 0;
					sta_info->sta_entry[sta_info->sta_cnt].setup_link = 0;
				}
				/* RSSI */
				sta_info->sta_entry[sta_info->sta_cnt].rssi = RTMPMaxRssi(pAd, pEnt->RssiSample.AvgRssi[0],
					pEnt->RssiSample.AvgRssi[1], pEnt->RssiSample.AvgRssi[2]);
				sta_info->sta_entry[sta_info->sta_cnt].uptime = (ULONGLONG)pEnt->StaConnectTime;
				/*Rate*/
				get_sta_rate_info(pAd, pEnt, &tx_rate_info, &rx_rate_info);
				sta_info->sta_entry[sta_info->sta_cnt].tx_rate = tx_rate_info.legacy;/*tx_rate*/
				sta_info->sta_entry[sta_info->sta_cnt].rx_rate = rx_rate_info.legacy;
				/*bw*/
				sta_info->sta_entry[sta_info->sta_cnt].bw = rx_rate_info.bw;
				/*mode*/
				sta_info->sta_entry[sta_info->sta_cnt].mode = pEnt->HTPhyMode.field.MODE;
				/*Rate_rt*/
				sta_info->sta_entry[sta_info->sta_cnt].tx_rate_rt = (pEnt->AvgTxBytes >> 17);
				sta_info->sta_entry[sta_info->sta_cnt].rx_rate_rt = (pEnt->AvgRxBytes >> 17);
				MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"\ntx_rate_rt=%lu rx_rate_rt=%lu\n",
					sta_info->sta_entry[sta_info->sta_cnt].tx_rate_rt,
					sta_info->sta_entry[sta_info->sta_cnt].rx_rate_rt);
				for (if_idx = 0; if_idx < ANDLINK_IF_MAX; if_idx++) {
					sta_info->sta_entry[sta_info->sta_cnt].avg_tx_rate[if_idx] = pEnt->andlink_avg_tx_rate[if_idx];
					sta_info->sta_entry[sta_info->sta_cnt].avg_rx_rate[if_idx] = pEnt->andlink_avg_rx_rate[if_idx];
					sta_info->sta_entry[sta_info->sta_cnt].max_tx_rate[if_idx] = pEnt->andlink_max_tx_rate[if_idx];
					sta_info->sta_entry[sta_info->sta_cnt].max_rx_rate[if_idx] = pEnt->andlink_max_rx_rate[if_idx];
					MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"\navg_tx_rate=%llu max_tx_rate=%llu avg_rx_rate=%llu max_rx_rate=%llu avg_tx_rate =%p\n",
					pEnt->andlink_avg_tx_rate[if_idx],
					pEnt->andlink_max_tx_rate[if_idx],
					pEnt->andlink_avg_rx_rate[if_idx],
					pEnt->andlink_max_rx_rate[if_idx],
					pEnt->andlink_avg_tx_rate[if_idx]);
				}
				sta_info->sta_entry[sta_info->sta_cnt].rx_pkts = (ULONGLONG)pEnt->TxPackets.QuadPart;
				sta_info->sta_entry[sta_info->sta_cnt].rx_bytes = pEnt->TxBytes;
				sta_info->sta_entry[sta_info->sta_cnt].tx_bytes = pEnt->RxBytes;
				sta_info->sta_entry[sta_info->sta_cnt].tx_pkts = (ULONGLONG)pEnt->RxPackets.QuadPart;
				MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"\nsta_cnt =%d rx_pkts=%llu tx_pkts=%llu rx_bytes=%llu tx_bytes=%llu tx_rate_rt=%ld rx_rate_rt=%ld\n",
					sta_info->sta_cnt,
					sta_info->sta_entry[sta_info->sta_cnt].rx_pkts,
					sta_info->sta_entry[sta_info->sta_cnt].tx_pkts,
					sta_info->sta_entry[sta_info->sta_cnt].rx_bytes,
					sta_info->sta_entry[sta_info->sta_cnt].tx_bytes,
					sta_info->sta_entry[sta_info->sta_cnt].tx_rate_rt,
					sta_info->sta_entry[sta_info->sta_cnt].rx_rate_rt);
				sta_info->sta_cnt++;
			}
		}
	}
	return 0;
}
INT	mtk_cfg80211_andlink_get_scan_result(
	RTMP_ADAPTER *pAd, INT apidx, void *data, INT offset)
{
	struct wifi_dev *wdev =  NULL;
	BSS_TABLE *scan_tab = NULL;
	BSS_ENTRY *bss = NULL;
	INT bss_idx = 0;
	INT idx = 0;
	struct mtk_wifi_scan_info *andlink_scan_info = (struct mtk_wifi_scan_info *)data;
	NDIS_802_11_NETWORK_TYPE    wireless_mode;
	int bw20 = 1, bw40 = 2, bw80 = 3, bw160 = 4, bw80_80 = 5;
	int wifi_mode[] = {Ndis802_11A_B, Ndis802_11A_B, Ndis802_11A_B, Ndis802_11B_G, Ndis802_11A_B, Ndis802_11A_N, Ndis802_11B_G_N, Ndis802_11A_N_AC, Ndis802_11B_G_N_AX, Ndis802_11A_N_AC_AX};

	wdev = &pAd->ApCfg.MBSSID[apidx].wdev;
	scan_tab = get_scan_tab_by_wdev(pAd, wdev);
	if (scan_tab == NULL) {
		MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"scan_tab is null(%p).\n", scan_tab);
		return -EFAULT;
	}

	for (idx = 0; idx < scan_tab->BssNr; idx++) {
		if (idx < offset)
			continue;
		bss = &scan_tab->BssEntry[idx];
		if (bss == NULL || bss->SsidLen <= 0 || bss->Channel == 0) {
			MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"bss(%p)  SSID(%s) or channel(%d) is invalid.\n",
					bss, bss->Ssid, bss->Channel);
			continue;
		}
		bss_idx = idx - offset;
		if (bss_idx >= ANDLINK_MAX_WLAN_NEIGHBOR) {
			MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"bss(%p) is null or bss_idx >= %d\n", bss, bss_idx);
			break;
		}
		/*bss*/
		NdisMoveMemory(andlink_scan_info->scan_entry[bss_idx].ssid, bss->Ssid, SSID_LEN-1);
		/*mac_addr*/
		COPY_MAC_ADDR(andlink_scan_info->scan_entry[bss_idx].mac_addr, bss->Bssid);
		andlink_scan_info->scan_entry[bss_idx].channel = bss->Channel;/*channel*/
		andlink_scan_info->scan_entry[bss_idx].rssi = bss->Rssi;/*rssi*/
		/* bandwidth*/
		if (WMODE_CAP_6G(wdev->PhyMode))
			andlink_scan_info->scan_entry[bss_idx].bandwidth = 0;
		else if (WMODE_CAP_5G(wdev->PhyMode)) {
			/*80+80MHz*/
			if (bss->vht_op_ie.vht_op_info.ch_width == 3)
				andlink_scan_info->scan_entry[bss_idx].bandwidth = bw80_80;
			else if (bss->vht_op_ie.vht_op_info.ch_width == 2)
				andlink_scan_info->scan_entry[bss_idx].bandwidth = bw160; /*160MHz*/
			else if (bss->vht_op_ie.vht_op_info.ch_width == 1) {
				/*80MHz*/
				if (bss->vht_op_ie.vht_op_info.ccfs_1 == 0)
					andlink_scan_info->scan_entry[bss_idx].bandwidth = bw80;
				else {
					if (((bss->vht_op_ie.vht_op_info.ccfs_1) - (bss->vht_op_ie.vht_op_info.ccfs_0)) == 8
							|| ((bss->vht_op_ie.vht_op_info.ccfs_1) - (bss->vht_op_ie.vht_op_info.ccfs_0)) == -8)
						andlink_scan_info->scan_entry[bss_idx].bandwidth = bw160; /*160MHz*/
					else
						andlink_scan_info->scan_entry[bss_idx].bandwidth = bw80_80; /*80+80MHz*/
				}
			} else if (bss->vht_op_ie.vht_op_info.ch_width == 0 && bss->vht_op_ie.vht_op_info.ccfs_0 == 1)
				andlink_scan_info->scan_entry[bss_idx].bandwidth = bw40; /*40MHz*/
			else
				andlink_scan_info->scan_entry[bss_idx].bandwidth = bw20; /*20MHz*/
		} else if (bss->HtCapability.HtCapInfo.ChannelWidth == HT_BW_20)
			andlink_scan_info->scan_entry[bss_idx].bandwidth = bw20;/*20*/
		else
			andlink_scan_info->scan_entry[bss_idx].bandwidth = bw20;/*40MHz*/
		/*loadRate*/
		andlink_scan_info->scan_entry[bss_idx].load_rate = bss->QbssLoad.ChannelUtilization * 100 / 255;
		/* wifistandard*/
		wireless_mode = NetworkTypeInUseSanity(bss);
		if (wireless_mode < (ARRAY_SIZE(wifi_mode)))
			andlink_scan_info->scan_entry[bss_idx].wifistandard = wifi_mode[wireless_mode];
		else
			andlink_scan_info->scan_entry[bss_idx].wifistandard = 0;
		/*authmode*/
		snprintf(andlink_scan_info->scan_entry[bss_idx].authmode, ANDLINK_SEC_LEN,
			"%s", GetAuthModeStr(bss->AKMMap));
		MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_NOTICE,
					"load_rate=%d,wifistandard=%d,authmode=%s\n",
					andlink_scan_info->scan_entry[bss_idx].load_rate,
					andlink_scan_info->scan_entry[bss_idx].wifistandard,
					andlink_scan_info->scan_entry[bss_idx].authmode);
	}
	andlink_scan_info->num = bss_idx;

	MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"bss_idx(%d), offset(%d), ssid(%s).\n",
					bss_idx, offset, bss->Ssid);
	return 0;
}

INT	mtk_cfg80211_reply_andlink_sta_info(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	INT apidx = 0;
	INT ret = 0;
	INT offset = 0;
	struct mtk_andlink_wifi_sta_info *sta_info = NULL;
	struct mtk_andlink_wifi_sta_info *sta_info_recv = NULL;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
	} else {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -RTMP_IO_EFAULT;
	}

	if (ctrl_data->data_len < sizeof(struct mtk_andlink_wifi_sta_info))
		return -RTMP_IO_EINVAL;
	if (ctrl_data->rsp_data_len < sizeof(struct mtk_andlink_wifi_sta_info))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct mtk_andlink_wifi_sta_info);
	sta_info = (struct mtk_andlink_wifi_sta_info *)ctrl_data->rsp_data;
	apidx = wdev->func_idx;

	/*get offset*/
	sta_info_recv = (struct mtk_andlink_wifi_sta_info *)ctrl_data->data;
	offset = sta_info_recv->offset;
	NdisZeroMemory(sta_info, sizeof(struct mtk_andlink_wifi_sta_info));
	sta_info->sta_cnt = 0;
	mtk_cfg80211_andlink_get_stainfo_result(pAd, apidx, sta_info, offset);
	return 0;
}

INT	mtk_cfg80211_reply_andlink_bss_info(
	struct _RTMP_ADAPTER *pAd, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	INT status = NDIS_STATUS_SUCCESS;
	struct mtk_andlink_radio_info *radio_info;
	RX_STATISTIC_RXV *rx_stat_rxv = NULL;
	INT apidx = 0;
	INT ret = 0;
	MAC_TABLE_ENTRY *pEnt = NULL;
	INT idx = 0, max_num_of_mac = 33, sta_cnt = 0;
	UINT8 u1Snr[4] = {0}, snr = 0;
	signed char rssi = 0;

	if (ctrl_data->rsp_data_len < sizeof(struct mtk_andlink_radio_info))
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = sizeof(struct mtk_andlink_radio_info);
	radio_info = (struct mtk_andlink_radio_info *)ctrl_data->rsp_data;
	rx_stat_rxv = &pAd->rx_stat_rxv;

	/*band snr/rssi/noise infos*/
	radio_info->rssi = RTMPMaxRssi(pAd,
				rx_stat_rxv->RSSI[0],
				rx_stat_rxv->RSSI[1],
				rx_stat_rxv->RSSI[2]);
	radio_info->snr = rx_stat_rxv->SNR[0];
	radio_info->noise = radio_info->rssi - radio_info->snr;

	for (idx = 0; idx < max_num_of_mac; idx++) {
		pEnt = &pAd->MacTab->Content[idx];
		if (!pEnt
			|| !(IS_ENTRY_CLIENT(pEnt)
			|| IS_ENTRY_PEER_AP(pEnt)
			|| IS_ENTRY_REPEATER(pEnt)))
			continue;
		if (pEnt->Sst == SST_ASSOC) {
			/* RSSI */
			rssi += RTMPMaxRssi(pAd, pEnt->RssiSample.AvgRssi[0],
					pEnt->RssiSample.AvgRssi[1], pEnt->RssiSample.AvgRssi[2]);

			HW_GET_STA_SNR(pAd, pEnt->wcid, u1Snr);
			snr += u1Snr[0];
			sta_cnt++;
		}
	}

	radio_info->rssi = (signed char)(rssi / sta_cnt);
	radio_info->snr = (signed char)(snr / sta_cnt);
	radio_info->noise = radio_info->rssi - radio_info->snr;
	return status;
}
#endif /*ANDLINK_V4_0*/

static UCHAR update_auto_ch_priority_from_prefer_list(
	PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	struct ch_list_info *prefer_list)
{
	if (prefer_list == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"prefer_list is NULL!\n");
		return FALSE;
	}

	if ((prefer_list->num_of_ch == 0) || (prefer_list->num_of_ch > MAX_NUM_OF_CHANNELS)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"invalid prefer_list ch num!\n");
		return FALSE;
	}

	if (!UpdatePreferChannel(pAd, wdev, prefer_list->ch_list, prefer_list->num_of_ch,
		CH_PRIO_OP_OWNER_RUNTIME_CFG)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"UpdateChannelPriority failed!\n");
		return FALSE;
	}

	return TRUE;
}

static UCHAR update_auto_ch_skip_list(PRTMP_ADAPTER pAd, struct ch_list_info *skip_list)
{
	CHANNEL_CTRL *pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
	UCHAR i;
	UCHAR ChIdx;
	UCHAR valid = FALSE;
#ifdef DFS_VENDOR10_CUSTOM_FEATURE
#define MAX_SKIP_CH_NUM 20
#else
#define MAX_SKIP_CH_NUM MAX_NUM_OF_CHANNELS
#endif
	UCHAR skip_ch[MAX_SKIP_CH_NUM] = {0};
	UCHAR skip_count = 0;

	if (skip_list == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"skip_list is NULL!\n");
		return FALSE;
	}

	if (pChCtrl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"pChCtrl is NULL!\n");
		return FALSE;
	}

	/*if skip_list->num_of_ch == 0 originally, means need reset auto ch skip list*/
	if (skip_list->num_of_ch == 0) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"reset auto ch skip list!\n");
		os_zero_mem(pAd->ApCfg.AutoChannelSkipList, MAX_SKIP_CH_NUM);
		pAd->ApCfg.AutoChannelSkipListNum = 0;
		return TRUE;
	}

	/*skip_list->num_of_ch valid check*/
	if (skip_list->num_of_ch > MAX_SKIP_CH_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"invalid skip_list ch num!\n");
		return FALSE;
	}

	/*at least one channel is valid, the skip list will be effective*/
	for (i = 0; i < skip_list->num_of_ch; i++) {
		UCHAR ch;

		ch = skip_list->ch_list[i];
		for (ChIdx = 0; ChIdx < pChCtrl->ChListNum; ChIdx++) {
			if (ch == pChCtrl->ChList[ChIdx].Channel) {
				valid = TRUE;
				break;
			}
		}

		if (valid)
			break;
	}

	if (!valid) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"no valid channel, set skip list fail!\n");
		return FALSE;
	}

	/*when skip list is valid, needs reset the skip list first*/
	os_zero_mem(pAd->ApCfg.AutoChannelSkipList, MAX_SKIP_CH_NUM);
	pAd->ApCfg.AutoChannelSkipListNum = 0;

	/*the channels which are in skip list, should also be in ChCtrl list*/
	for (i = 0; i < skip_list->num_of_ch; i++) {
		UCHAR ch;
		UCHAR find = FALSE;

		ch = skip_list->ch_list[i];
		for (ChIdx = 0; ChIdx < pChCtrl->ChListNum; ChIdx++) {
			if (ch == pChCtrl->ChList[ChIdx].Channel) {
				find = TRUE;
				break;
			}
		}

		if (find) {
			skip_ch[skip_count] = ch;
			skip_count++;
		}
	}

	memcpy(&pAd->ApCfg.AutoChannelSkipList[0], &skip_ch[0], skip_count);
	pAd->ApCfg.AutoChannelSkipListNum = skip_count;

	for (ChIdx = 0; ChIdx < pAd->ApCfg.AutoChannelSkipListNum; ChIdx++)
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
			"get skip list, ch = %d\n", pAd->ApCfg.AutoChannelSkipList[ChIdx]);

	return TRUE;
}

#ifdef DOT11_EHT_BE
static int mtk_cfg80211_reply_bss_mlo_info(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int payload_len = 0;
	struct bss_mlo_info *ml_info; /* defined in mtk_vendor_nl80211.h */

	if (wdev->func_idx < 0 || wdev->func_idx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"invalid apidx:%d\n", wdev->func_idx);
		return -RTMP_IO_EINVAL;
	}

	payload_len = sizeof(struct bss_mlo_info);
	if (ctrl_data->rsp_data_len < payload_len)
		return -RTMP_IO_ENOMEM;
	ctrl_data->rsp_data_len = payload_len;
	ml_info = ctrl_data->rsp_data;
	if (pAd->ApCfg.MBSSID[wdev->func_idx].mld_grp_idx != 0
		&& pAd->ApCfg.MBSSID[wdev->func_idx].mld_grp_idx != MLD_GROUP_NONE) {
		ml_info->link_cnt = wdev->ml_event.event_id;
		ml_info->mld_grp_idx = wdev->ml_event.mld_grp_idx;
		ml_info->link_id = wdev->ml_event.link_id;
		NdisCopyMemory(ml_info->addr, wdev->ml_event.addr, MAC_ADDR_LEN);
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "mld_grp_idx=%d, link_id=%d\n",
			ml_info->mld_grp_idx, ml_info->link_id);

	return 0;
}

extern INT get_mac_entry_trx_statistics(PMAC_TABLE_ENTRY pEntry, UCHAR *tx_phymode,
	UCHAR *rx_phymode, UCHAR *tx_bw, UCHAR *rx_bw, UCHAR *tx_nss, UCHAR *rx_nss,
	UCHAR *tx_mcs, UCHAR *rx_mcs, ULONG *tx_rate, ULONG *rx_rate, UCHAR *tx_sgi,
	UCHAR *rx_sgi, UCHAR *snr);

static int mtk_cfg80211_fill_connected_sta_mld(
	struct _RTMP_ADAPTER *pAd,
	struct bmgr_mld_sta *mld_sta,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int i, j, ret = 0;
	MAC_TABLE_ENTRY *pEntry = NULL;
	struct station_information *sta;
	PRTMP_ADAPTER ad;
#ifdef VOW_SUPPORT
	int ac;
	struct _RTMP_CHIP_DBG *chip_dbg = hc_get_chip_dbg(pAd->hdev_ctrl);
#endif
	struct mt_peer_mld_sta_info *peer_mld;
	struct mt_peer_mld_link_info *peer_mld_link;

	if (MT_MAX_BAND_NUM != BSS_MNGR_MAX_BAND_NUM) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"MT_MAX_BAND_NUM != BSS_MNGR_MAX_BAND_NUM.\n");
		return -RTMP_IO_EINVAL;
	}


	os_alloc_mem(NULL, (UCHAR **)&peer_mld, sizeof(struct mt_peer_mld_sta_info));
	if (!peer_mld)
		return -RTMP_IO_ENOMEM;
	NdisZeroMemory(peer_mld, sizeof(struct mt_peer_mld_sta_info));
	os_zero_mem(peer_mld, sizeof(struct mt_peer_mld_sta_info));
	ctrl_data->rsp_data = peer_mld;

	peer_mld->mld_grp_idx = mld_sta->mld_grp_idx;
	os_move_mem(peer_mld->mld_addr, mld_sta->mld_addr, MAC_ADDR_LEN);

	if (GET_DOT11BE_ML_BASIC_CMM_EML_CAP_EMLSR_SUP(mld_sta->eml_caps))
		peer_mld->emlsr_sup = 1;

	if (GET_DOT11BE_ML_BASIC_CMM_EML_CAP_EMLMR_SUP(mld_sta->eml_caps))
		peer_mld->emlmr_sup = 1;

	if (GET_DOT11BE_ML_BASIC_CMM_MLD_CAP_FREQ_SEP_STR(mld_sta->mld_caps))
		peer_mld->freq_sep_str = 1;

	pEntry = (MAC_TABLE_ENTRY *)mld_sta->mld_link[0].priv_ptr;
	if (pEntry && pEntry->wdev)
		peer_mld->sta_connect_time = pEntry->StaConnectTime;

	for (i = 0; i < MT_MAX_BAND_NUM; i++) {
		peer_mld_link = &peer_mld->mld_link[i];
		sta = &peer_mld_link->sta_info;
		peer_mld_link->active = 0;
		if (!mld_sta->mld_link[i].active)
			continue;

		pEntry = (MAC_TABLE_ENTRY *)mld_sta->mld_link[i].priv_ptr;

		if (!pEntry || !pEntry->wdev) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"pEntry NULL. i = %d link_addr "MACSTR"\n", i,
				MAC2STR(mld_sta->mld_link[i].link_addr));
			continue;
		}
		if (get_mac_entry_trx_statistics(pEntry, &(sta->tx_phymode), &(sta->rx_phymode),
			&(sta->tx_bw),  &(sta->rx_bw), &(sta->tx_nss), &(sta->rx_nss),
			&(sta->tx_mcs), &(sta->rx_mcs), &(sta->tx_rate), &(sta->rx_rate),
			&peer_mld_link->tx_sgi, &peer_mld_link->rx_sgi, &peer_mld_link->snr)) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"get_mac_entry_trx_statistics fail!! i = %d link_addr "MACSTR"\n",
				i, MAC2STR(mld_sta->mld_link[i].link_addr));
			continue;
		}

		ad = (PRTMP_ADAPTER)(pEntry->pAd);
		if (!ad) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"pAd is NULL!!. i = %d link_addr "MACSTR"\n", i,
				MAC2STR(mld_sta->mld_link[i].link_addr));
			continue;
		}
		peer_mld_link->active = 1;

		for (j = 0; j < 4; j++)
			peer_mld_link->sta_rssi[j] = -127;
		rtmp_get_rssi(ad, pEntry->wcid,
			peer_mld_link->sta_rssi, MCS_NSS_CAP(ad)->max_path[MAX_PATH_RX]);
		os_move_mem(
			peer_mld_link->link_addr, mld_sta->mld_link[i].link_addr, MAC_ADDR_LEN);
		os_move_mem(
			peer_mld_link->link_bssid, pEntry->wdev->if_addr, MAC_ADDR_LEN);
		peer_mld_link->link_id = pEntry->mlo.link_info.link_id;
		peer_mld_link->tid_map_ul = pEntry->mlo.link_info.tid_map_ul;
		peer_mld_link->tid_map_dl = pEntry->mlo.link_info.tid_map_dl;
		peer_mld_link->tx_bytes = pEntry->TxBytes;
		peer_mld_link->rx_bytes = pEntry->RxBytes;
		peer_mld_link->rx_packets = pEntry->RxPackets.u.LowPart;
		peer_mld_link->tx_packets = pEntry->TxPackets.u.LowPart;
		peer_mld_link->tx_fail_count = pEntry->TxFailCount;
		peer_mld_link->tx_msdu_retry_cnt = pEntry->txMsduRetryCnt;
		RtmpDrvMaxRateGet(pAd, pEntry->MaxHTPhyMode.field.MODE,
			pEntry->HTPhyMode.field.ShortGI,
			pEntry->MaxHTPhyMode.field.BW, (pEntry->MaxHTPhyMode.field.MCS & 0xf),
			(pEntry->MaxHTPhyMode.field.MCS >> 4) + 1, &peer_mld_link->data_rate);
#ifdef VOW_SUPPORT
		peer_mld_link->tx_time = 0;
		peer_mld_link->rx_time = 0;
		if (chip_dbg && chip_dbg->get_sta_airtime) {
			for (ac = 0; ac < WMM_NUM; ac++) {
				peer_mld_link->tx_time += chip_dbg->get_sta_airtime(
								pAd, pEntry->wcid, ac, TRUE);
				peer_mld_link->rx_time += chip_dbg->get_sta_airtime(
								pAd, pEntry->wcid, ac, FALSE);
			}
		}
#endif
	}
	ret = mtk_cfg80211_reply_connected_sta_mld_info(ctrl_data);
	if (peer_mld)
		os_free_mem(peer_mld);
	return ret;
}

int mtk_cfg80211_reply_ap_mld(
	struct bmgr_mlo_dev *mld,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int i = 0;
	struct bmgr_entry *bss;
	struct mt_mlo_dev_info *mld_dev_info;
	struct mt_affiliated_aps *aps;

	mld_dev_info = (struct mt_mlo_dev_info *)ctrl_data->rsp_data;
	mld_dev_info->mld_grp = mld->mld_grp;
	os_move_mem(mld_dev_info->mld_addr, mld->mld_addr, MAC_ADDR_LEN);
	mld_dev_info->mld_type = mld->mld_type;

	for (i = 0; i < MT_BSS_MNGR_MAX_BAND_NUM; i++) {
		aps = &mld_dev_info->aps[i];
		if (!BMGR_VALID_BSS_IDX(mld->bss_idx_mld[i])) {
			aps->valid = 0;
			continue;
		}

		bss = GET_BSS_ENTRY_BY_IDX(mld->bss_idx_mld[i]);
		if (!BMGR_VALID_MLO_BSS_ENTRY(bss) || !bss->pwdev) {
			aps->valid = 0;
			continue;
		}

		aps->valid = 1;
		os_move_mem(aps->bssid, bss->pwdev->bssid, MAC_ADDR_LEN);
		aps->link_id = bss->link_id;
		if (wlan_config_get_eht_dscb_enable(bss->pwdev)) {
			aps->dis_subch_enable = 1;
			aps->dis_subch_bitmap = wlan_config_get_eht_dis_subch_bitmap(bss->pwdev);
		} else
			aps->dis_subch_enable = 0;

		aps->emlsr_mr = wlan_config_get_emlsr_mr(bss->pwdev);
	}

	return 0;
}

static int mtk_cfg80211_reply_connected_sta_mld(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data,
	struct bmgr_mlo_dev *mld)
{
	int ret = 0, i, j;
	struct query_mld_conn *mld_conn = NULL;
	u32 mem_size;
	struct query_mld_sta *mld_sta_info;
	struct bmgr_mld_sta *mld_sta;

	mem_size = sizeof(struct query_mld_conn);
	mem_size += BMGR_MAX_MLD_STA_CNT * sizeof(struct query_mld_sta);

	os_alloc_mem(NULL, (UCHAR **)&mld_conn, mem_size);
	if (!mld_conn)
		return -RTMP_IO_ENOMEM;

	NdisZeroMemory(mld_conn, mem_size);

	mld_conn->mld_group_idx = mld->mld_grp;
	ret = bss_mngr_query_mld_conn(mld_conn);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
			"MLD Conn Query failed: mld_group_idx(%d)\n",
			mac_ad->ApCfg.link_recom.recom_mld_grp);
		goto fail;
	}

	mld_sta_info = (struct query_mld_sta *)(mld_conn->mld_sta_info);
	for (i = 0; i < mld_conn->mld_sta_num; i++) {
		for (j = 0; BMGR_VALID_MLD_STA(j); j++) {
			mld_sta = GET_MLD_STA_BY_IDX(j);
			if (mld_sta->valid && mld_sta_info->mld_sta_idx == mld_sta->idx) {
				if (mtk_cfg80211_fill_connected_sta_mld(
					mac_ad, mld_sta, ctrl_data)) {
					MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
						"failed to reply sta mld(%u).\n",
						mld_sta_info->mld_sta_idx);
				}
				break;
			}
		}
		mld_sta_info++;
	}

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		"reply mld mld_grp_idx connected sta mlds=%u\n", mld->mld_grp);

fail:
	if (mld_conn)
		os_free_mem(mld_conn);
	return ret;
}

/* wrapper function */
int mtk_cfg80211_send_bss_ml_event(struct wifi_dev *wdev, enum CFG80211_ML_EVENT event_id)
{
	mtk_fill_bss_ml_info_event(wdev,
		&wdev->bss_info_argument.mld_info, event_id);
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"ml_event ===> event(%d) grp(%d) link(%d) addr(%pM)\n",
		wdev->ml_event.event_id,
		wdev->ml_event.mld_grp_idx,
		wdev->ml_event.link_id,
		wdev->ml_event.addr);
	return mtk_cfg80211_event_bss_ml_info(wdev->if_dev, wdev->ml_event);
}
#endif /* DOT11_EHT_BE */

static int mtk_cfg80211_reply_radio_stats(
	struct _RTMP_ADAPTER *pAd, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct wifi_radio_stats *radio_stats;
	int ret = 0, payload_len = 0;

	payload_len = sizeof(struct wifi_radio_stats);
	if (ctrl_data->rsp_data_len < payload_len)
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = payload_len;
	radio_stats = (struct wifi_radio_stats *)ctrl_data->rsp_data;
	ret = Get_Radio_Statistics(pAd, radio_stats);
	if (ret) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"fail to Get_Radio_Statistics.\n");
		return -RTMP_IO_EINVAL;
	}
	return ret;
}

extern INT get_station_info_from_mac_entry(
	struct wifi_dev *wdev, UINT32 ent_type, const UCHAR *mac_addr,
	struct station_information *sta);

static int mtk_cfg80211_get_sta(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct station_information *sta;

	if (ctrl_data->rsp_data_len < sizeof(struct station_information))
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = sizeof(struct station_information);
	sta = (struct station_information *)ctrl_data->rsp_data;

	if (get_station_info_from_mac_entry(wdev, ENTRY_CLIENT, ctrl_data->data, sta))
		return -RTMP_IO_EINVAL;
	return 0;
}

#ifdef CONFIG_WLAN_SERVICE
static int parse_hqa_cmd_frame(
	struct _RTMP_ADAPTER *pAd,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct hqa_frame *hqa_frame;
	struct hqa_frame_ctrl local_hqa;
	unsigned int rsp_date_len = 0;
	int ret = 0;
	char *data = ctrl_data->data;

	if (ctrl_data->rsp_data_len < sizeof(struct hqa_frame)) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"memory not enough(rsp_data_len = %d)\n", ctrl_data->rsp_data_len);
		return -RTMP_IO_ENOMEM;
	}

	hqa_frame = (struct hqa_frame *)ctrl_data->rsp_data;
	if (!hqa_frame) {
		ret = -RTMP_IO_ENOMEM;
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"hqa_frame fail\n");
		goto err;
	}

	if (sizeof(struct hqa_frame) < ctrl_data->data_len) {
		ret = -RTMP_IO_EINVAL;
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"nl data len too long: %d, %lu\n",
			ctrl_data->data_len, sizeof(struct hqa_frame));
		goto err;
	}

	NdisZeroMemory(hqa_frame, sizeof(*hqa_frame));
	NdisCopyMemory(hqa_frame, data, ctrl_data->data_len);

	/* 0 means eth type hqa frame */
	local_hqa.type = 0;
	local_hqa.hqa_frame_eth = hqa_frame;
	ret = mt_agent_hqa_cmd_handler(&pAd->serv, (struct hqa_frame_ctrl *)&local_hqa);
	if (ret)	{
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"hqa cmd handler fail, err:0x%08x\n", ret);
		ctrl_data->rsp_data_len = 0;
		ret = -RTMP_IO_EFAULT;
		goto err;
	}

	/* Update cmd_frame length */
	if (OS_NTOHS((hqa_frame)->length) > SERV_IOCTLBUFF) {
		ret = -RTMP_IO_EFAULT;
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"hqa fram length too long: %d\n", OS_NTOHS((hqa_frame)->length));
		goto err;
	}

	rsp_date_len = sizeof((hqa_frame)->magic_no) + sizeof((hqa_frame)->type)
				+ sizeof((hqa_frame)->id) + sizeof((hqa_frame)->length)
				+ sizeof((hqa_frame)->sequence) + OS_NTOHS((hqa_frame)->length);
	ctrl_data->rsp_data_len = rsp_date_len;
	/* hex_dump("hqa_cmd_frame", (UCHAR *)local_hqa.hqa_frame_eth, rsp_date_len); */
	return 0;
err:
	return ret;
}
#endif /* CONFIG_WLAN_SERVICE */

#ifdef SWACI_MECHANISM
INT32 mtk_cfg80211_set_swaci_lna_enable(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));
		param.swaci.en.fgEnable = (UINT8)data[0];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_ENABLE);
		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_scenario(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.scen.u1InterfScenario = (UINT8)data[0];
		param.swaci.scen.u1LimitRfTtGain = (UINT8)data[1];
		param.swaci.scen.u1LimitRfTtGainAddRfdgc = (UINT8)data[2];
		param.swaci.scen.iBw20EdccaTh = (INT8)data[3];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_SCENARIO);
		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_duty_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.duty_th.u1DutyAciTh = (UINT8)data[0];
		param.swaci.duty_th.u1DutyAaciTh = (UINT8)data[1];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_DUTY_TH);
		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_scen_adc_rcpi_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.d_th.u1AdcRcpiDiffTh = (UINT8)data[0];
		param.swaci.d_th.u1AdcValidTh = (UINT8)data[1];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_COND_D_TH);

		return Ret;
}


INT32 mtk_cfg80211_set_swaci_lna_aci_ib_rcpi_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.e_th.u1IbRcpiDiffAciTh = (UINT8)data[0];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_COND_E_TH);

		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_aaci_ib_rcpi_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.f_th.u1AdcIbDiffTh = (UINT8)data[0];
		param.swaci.f_th.u1IbRcpiDiffAaciTh = (UINT8)data[1];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_COND_F_TH);

		return Ret;
}


INT32 mtk_cfg80211_set_swaci_lna_inst_ib_rcpi_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.g_th.u1InstIbRcpiDiffTh = (UINT8)data[0];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_COND_G_TH);

		return Ret;
}


INT32 mtk_cfg80211_set_swaci_lna_interf_cnt_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.if_cnt.u1AciHitCntTh = (UINT8)data[0];
		param.swaci.if_cnt.u1AciHitLCntTh = (UINT8)data[1];
		param.swaci.if_cnt.u1AaciHitCntTh = (UINT8)data[2];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_INTERF_CNT_TH);

		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_valid_rcpi_th(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		param.swaci.v_rcpi_th.i4Bw20RcpiValidTh = (UINT8)data[0];
		param.swaci.v_rcpi_th.i4Bw40RcpiValidTh = (UINT8)data[1];
		param.swaci.v_rcpi_th.i4Bw80RcpiValidTh = (UINT8)data[2];
		param.swaci.v_rcpi_th.i4Bw160RcpiValidTh = (UINT8)data[3];
		Ret = UniCmdSwAciLnaCtrl(pAd, &param, UNI_CMD_SWACI_VALID_RCPI_TH);

		return Ret;
}

INT32 mtk_cfg80211_set_swaci_lna_conf(struct _RTMP_ADAPTER *pAd, INT *data, UINT32 len)
{
		INT32 Ret = TRUE;
		struct _UNI_CMD_SWACI_LNA_CTRL_T param;

		os_zero_mem((uint8_t *)&param, sizeof(param));

		return Ret;
}
#endif /* SWACI_MECHANISM */

/*************************************************************************/
/* GPL Refine Begin */
struct cfg80211_drv_vndr_cmd_handler {
	enum mt_cfg80211_vndr_attr_type attr_type;
	int (*drv_vndr_cmd_handler)(
		struct _RTMP_ADAPTER *mac_ad, struct wifi_dev *wdev, void *ctrl_data_);
};

#ifdef WAPP_SUPPORT
static int mtk_cfg80211_reply_cac_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
#ifdef DFS_CAC_R2
	struct DOT11_H *pDot11h = NULL;
	PDFS_PARAM pDfsParam = NULL;
	UCHAR bandIdx;

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_CAC_CAP\n");

	pDfsParam = &mac_ad->CommonCfg.DfsParameter;
	NdisZeroMemory(&wdev->cac_capability, sizeof(wdev->cac_capability));
	wapp_get_cac_cap(mac_ad, wdev, &wdev->cac_capability);
	NdisMoveMemory(&wdev->cac_capability.country_code[0],
		mac_ad->CommonCfg.CountryCode, 2);
	wdev->cac_capability.rdd_region = mac_ad->CommonCfg.RDDurRegion;

	/* CAC Ongoing Update */
	bandIdx = HcGetBandByWdev(wdev);
	pDot11h = wdev->pDot11_H;
	if (pDot11h == NULL)
		return -RTMP_IO_EINVAL;

	wdev->cac_capability.active_cac = FALSE;
	if (pDot11h->ChannelMode == CHAN_SILENCE_MODE) {
		wdev->cac_capability.active_cac = TRUE;
		if (pDfsParam->band_bw == BW_80 ||
		pDfsParam->band_bw == BW_160)
			wdev->cac_capability.ch_num =
				DfsPrimToCent(pDfsParam->PrimCh, pDfsParam->band_bw);
		else
			wdev->cac_capability.ch_num = pDfsParam->PrimCh;
		if (pDot11h->cac_time > pDot11h->RDCount)
			wdev->cac_capability.remain_time =
				(pDot11h->cac_time - pDot11h->RDCount);
		else
			wdev->cac_capability.remain_time = 0;
	}
	wdev->cac_capability.cac_mode = get_cac_mode(mac_ad, pDfsParam, wdev);

	ctrl_data->rsp_data_len = sizeof(wdev->cac_capability);
	ctrl_data->rsp_data = &wdev->cac_capability;

	return 0;
#else /* DFS_CAC_R2 */
	return -EOPNOTSUPP;
#endif /* DFS_CAC_R2 */
}

static int mtk_cfg80211_reply_misc_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_misc_cap *misc_cap;

	if (ctrl_data->rsp_data_len < sizeof(wdev_misc_cap))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(wdev_misc_cap);
	misc_cap = (wdev_misc_cap *)ctrl_data->rsp_data;
	misc_cap->max_num_of_cli = 64;
	misc_cap->max_num_of_bss = mac_ad->ApCfg.BssidNum;
	misc_cap->num_of_bss = mac_ad->ApCfg.BssidNum;
	misc_cap->max_num_of_block_cli = BLOCK_LIST_NUM;
	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_MISC_CAP\n");

	return 0;
}

static int mtk_cfg80211_reply_ht_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_ht_cap *ht_cap;

	if (ctrl_data->rsp_data_len < sizeof(wdev_ht_cap))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(wdev_ht_cap);
	ht_cap = (wdev_ht_cap *)ctrl_data->rsp_data;
	ht_cap->tx_stream = wlan_config_get_tx_stream(wdev);
	ht_cap->rx_stream = wlan_config_get_rx_stream(wdev);
	ht_cap->sgi_20 = (wlan_config_get_ht_gi(wdev) == GI_400) ? 1:0;
	ht_cap->sgi_40 = (wlan_config_get_ht_gi(wdev) == GI_400) ?  1:0;
	ht_cap->ht_40 = (wlan_operate_get_ht_bw(wdev) == BW_40) ? 1:0;
	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_HT_CAP\n");

	return 0;
}

static int mtk_cfg80211_reply_vht_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	UINT sup_tx_mcs_size = 0, sup_rx_mcs_size = 0;
	wdev_vht_cap *vht_cap;
	VHT_CAP_INFO drv_vht_cap;
	VHT_OP_IE drv_vht_op;

	if (ctrl_data->rsp_data_len < sizeof(wdev_vht_cap))
		return -RTMP_IO_EINVAL;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE, "wdev is not an AP\n");
		return -EINVAL;
	}

	ctrl_data->rsp_data_len = sizeof(wdev_vht_cap);
	vht_cap = (wdev_vht_cap *)ctrl_data->rsp_data;
	NdisZeroMemory(&drv_vht_cap, sizeof(VHT_CAP_INFO));
	NdisZeroMemory(&drv_vht_op, sizeof(VHT_OP_IE));
	NdisCopyMemory(&drv_vht_cap, &mac_ad->CommonCfg.vht_cap_ie.vht_cap, sizeof(VHT_CAP_INFO));
	mt_WrapSetVHTETxBFCap(mac_ad, wdev, &drv_vht_cap);
	drv_vht_op.basic_mcs_set.mcs_ss1 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss2 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss3 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss4 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss5 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss6 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss7 = VHT_MCS_CAP_NA;
	drv_vht_op.basic_mcs_set.mcs_ss8 = VHT_MCS_CAP_NA;
	switch	(wlan_operate_get_rx_stream(wdev)) {
	case 4:
		drv_vht_op.basic_mcs_set.mcs_ss4 = MCS_NSS_CAP(mac_ad)->max_vht_mcs;
		fallthrough;
	case 3:
		drv_vht_op.basic_mcs_set.mcs_ss3 = MCS_NSS_CAP(mac_ad)->max_vht_mcs;
		fallthrough;
	case 2:
		drv_vht_op.basic_mcs_set.mcs_ss2 = MCS_NSS_CAP(mac_ad)->max_vht_mcs;
		fallthrough;
	case 1:
		drv_vht_op.basic_mcs_set.mcs_ss1 = MCS_NSS_CAP(mac_ad)->max_vht_mcs;
		break;
	}
	sup_tx_mcs_size = sizeof(vht_cap->sup_tx_mcs) < sizeof(drv_vht_op.basic_mcs_set)
				? sizeof(vht_cap->sup_tx_mcs) : sizeof(drv_vht_op.basic_mcs_set);
	sup_rx_mcs_size = sizeof(vht_cap->sup_rx_mcs) < sizeof(drv_vht_op.basic_mcs_set)
				? sizeof(vht_cap->sup_tx_mcs) : sizeof(drv_vht_op.basic_mcs_set);

	NdisMoveMemory(vht_cap->sup_tx_mcs,
				&drv_vht_op.basic_mcs_set,
				sup_tx_mcs_size);
	NdisMoveMemory(vht_cap->sup_rx_mcs,
				&drv_vht_op.basic_mcs_set,
				sup_rx_mcs_size);
	vht_cap->tx_stream = wlan_config_get_tx_stream(wdev);
	vht_cap->rx_stream = wlan_config_get_tx_stream(wdev);
	vht_cap->sgi_80 = (wlan_config_get_ht_gi(wdev) == GI_400) ? 1:0;
	vht_cap->sgi_160 = (wlan_config_get_ht_gi(wdev) == GI_400) ? 1:0;
	vht_cap->vht_160 = (wlan_operate_get_vht_bw(wdev) == BW_160) ? 1:0;
	vht_cap->vht_8080 = (wlan_operate_get_vht_bw(wdev) == BW_8080) ? 1:0;
	vht_cap->su_bf = (drv_vht_cap.bfer_cap_su) ? 1:0;
	vht_cap->mu_bf = (drv_vht_cap.bfer_cap_mu) ? 1:0;

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_VHT_CAP\n");

	return 0;
}

extern UINT16 he_mcs_map(UINT8 nss, UINT8 he_mcs);
static int mtk_cfg80211_reply_he_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_he_cap *he_cap;
	UINT16 he_max_mcs_nss, *he_mcs_pos, he_mcs_len = 0;
	UINT8 he_bw;
	struct he_bf_info he_bf_struct;
	struct mcs_nss_caps *mcs_nss;

	if (ctrl_data->rsp_data_len < sizeof(wdev_he_cap))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(wdev_he_cap);
	he_cap = (wdev_he_cap *)ctrl_data->rsp_data;
	mcs_nss = wlan_config_get_mcs_nss_caps(wdev);
	he_bw = wlan_config_get_he_bw(wdev);
#ifdef EM_PLUS_SUPPORT
	/* Report MAX BW to wapp for ap capability */
	if (WMODE_CAP_2G(wdev->PhyMode))
		he_bw = HE_BW_2040;
	else if (WMODE_CAP_5G(wdev->PhyMode))
		he_bw = HE_BW_160;
	else if (WMODE_CAP_6G(wdev->PhyMode))
		he_bw = HE_BW_160;
#endif /* EM_PLUS_SUPPORT */
	he_cap->tx_stream = wlan_config_get_tx_stream(wdev);
	he_cap->rx_stream = wlan_config_get_rx_stream(wdev);
	he_mcs_pos = (UINT16 *)he_cap->he_mcs;
	he_max_mcs_nss = he_mcs_map(he_cap->tx_stream, HE_MCS_0_11);
	NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
	he_mcs_pos++;
	he_max_mcs_nss = he_mcs_map(he_cap->rx_stream, HE_MCS_0_11);
	NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
	he_mcs_pos++;
	he_mcs_len += 4;
	if (he_bw > HE_BW_80) {
		if (he_cap->tx_stream > mcs_nss->bw160_max_nss)
			he_cap->tx_stream = mcs_nss->bw160_max_nss;
		if (he_cap->rx_stream > mcs_nss->bw160_max_nss)
			he_cap->rx_stream = mcs_nss->bw160_max_nss;
		he_max_mcs_nss = he_mcs_map(he_cap->tx_stream, HE_MCS_0_11);
		NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
		he_mcs_pos++;
		he_max_mcs_nss = he_mcs_map(he_cap->rx_stream, HE_MCS_0_11);
		NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
		he_mcs_pos++;
		he_mcs_len += 4;
		he_cap->he_160 = 1;
		if (he_bw > HE_BW_160) {
			he_max_mcs_nss = he_mcs_map(he_cap->tx_stream, HE_MCS_0_11);
			NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
			he_mcs_pos++;
			he_max_mcs_nss = he_mcs_map(he_cap->rx_stream, HE_MCS_0_11);
			NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
			he_mcs_pos++;
			he_mcs_len += 4;
			he_cap->he_8080 = 1;
		}
	}
	he_cap->he_mcs_len = he_mcs_len;

	NdisZeroMemory(&he_bf_struct, sizeof(struct he_bf_info));
	mt_wrap_get_he_bf_cap(wdev, &he_bf_struct);

	if (he_bf_struct.bf_cap & HE_SU_BFER)
		he_cap->su_bf_cap = 1;
	if (he_bf_struct.bf_cap & HE_MU_BFER)
		he_cap->mu_bf_cap = 1;
	if (wlan_config_get_mu_dl_mimo(wdev)) {
		if (wdev->wdev_type == WDEV_TYPE_AP)
			he_cap->dl_mu_mimo_ofdma_cap = 1;
		he_cap->dl_ofdma_cap = 1; /*To see*/
	}
	if (wlan_config_get_mu_ul_mimo(wdev)) {
		if (wdev->wdev_type == WDEV_TYPE_AP) {
			he_cap->ul_mu_mimo_ofdma_cap = 1;
			he_cap->ul_mu_mimo_cap = 1;
			he_cap->ul_ofdma_cap = 1; /*To see*/
		}
	}
	he_cap->gi = wlan_config_get_he_gi(wdev);

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_HE_CAP\n");

	return 0;
}

static int mtk_cfg80211_reply_wdev(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev_in,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	INT i;
	struct wifi_dev *wdev;
	wapp_dev_info *dev_info;

	if (ctrl_data->rsp_data_len < sizeof(wapp_dev_info))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(wapp_dev_info);
	dev_info = (wapp_dev_info *)ctrl_data->rsp_data;
	for (i = 0; i < WDEV_NUM_MAX; i++) {
		if (mac_ad->wdev_list[i] != NULL) {
			wdev = mac_ad->wdev_list[i];

			if (wdev->if_dev
				&& wdev_in->if_dev
				&& (RtmpOsGetNetIfIndex(wdev->if_dev)
					== RtmpOsGetNetIfIndex(wdev_in->if_dev))) {
				dev_info->ifindex = RtmpOsGetNetIfIndex(wdev->if_dev);
				dev_info->dev_type = wdev->wdev_type;
				COPY_MAC_ADDR(dev_info->mac_addr, wdev->if_addr);
				NdisCopyMemory(dev_info->ifname,
					RtmpOsGetNetDevName(wdev->if_dev), IFNAMSIZ);
				dev_info->radio_id = HcGetBandByWdev(wdev);
				dev_info->adpt_id = (uintptr_t) mac_ad;
				dev_info->wireless_mode = wdev->PhyMode;
				dev_info->dev_active = HcIsRadioAcq(wdev);
				MTWF_DBG(mac_ad, DBG_CAT_FW, CATFW_DEVINFO, DBG_LVL_NOTICE,
					"incoming index %d - %d driver index name %s\n",
					RtmpOsGetNetIfIndex(wdev_in->if_dev),
					(RtmpOsGetNetIfIndex(wdev->if_dev)),
					RtmpOsGetNetDevName(wdev->if_dev));
				if (IS_MAP_TURNKEY_ENABLE(mac_ad) &&
						wdev->wdev_type == WDEV_TYPE_STA) {
					if (wdev->func_idx >= MAX_APCLI_NUM)
						continue;
					SetApCliEnableByWdev(mac_ad, wdev, FALSE);
				}
			}
		}
	}
	return 0;
}

#ifdef CONFIG_MAP_SUPPORT
static int mtk_cfg80211_reply_eht_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct wdev_eht_cap *eht_cap;
	struct _RTMP_CHIP_CAP *cap;
	UINT8 eht_bw, prim_ch;
	UCHAR ch_band, eht_cen_ch;
#ifdef MAP_R6
	UINT8 eht_tx_nss, eht_rx_nss, eht_max_mcs;
	struct eht_txrx_mcs_nss eht_mcs_nss = {0};
	struct eht_txrx_mcs_nss_20 eht_mcs_nss_bw20 = {0};
#endif

	if (ctrl_data->rsp_data_len < sizeof(struct wdev_eht_cap))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct wdev_eht_cap);
	eht_cap = (struct wdev_eht_cap *)ctrl_data->rsp_data;
	cap = hc_get_chip_cap(mac_ad->hdev_ctrl);
	prim_ch = wdev->channel;
	eht_bw = wlan_config_get_eht_bw(wdev);
#ifdef EM_PLUS_SUPPORT
	/* Report MAX BW to wapp for ap capability */
	if (WMODE_CAP_2G(wdev->PhyMode))
		eht_bw = EHT_BW_2040;
	else if (WMODE_CAP_5G(wdev->PhyMode))
		eht_bw = EHT_BW_160;
	else if (WMODE_CAP_6G(wdev->PhyMode))
		eht_bw = EHT_BW_320;
#endif /* EM_PLUS_SUPPORT */
	eht_cap->eht_ch_width = eht_bw;
	ch_band = wlan_config_get_ch_band(wdev);
	eht_cen_ch = eht_cent_ch_freq(wdev, wdev->channel, eht_bw, ch_band);
#ifdef MAP_R6
	eht_cap->interface_index = wdev->wdev_idx;
	eht_cap->eht_operation_information_valid = 1;
	eht_cap->eht_default_pe_duration = 1;
	eht_cap->group_addressed_BU_indication_limit = 1;
	eht_cap->group_addressed_BU_indication_exponent = 1;
	eht_tx_nss = wlan_config_get_eht_tx_nss(wdev);
	eht_rx_nss = wlan_config_get_eht_rx_nss(wdev);
	eht_max_mcs = wlan_config_get_eht_max_mcs(wdev);
	eht_cap->control = eht_bw;

	if (eht_max_mcs == MCS_0)
		eht_max_mcs = MCS_13;

	if (eht_bw == EHT_BW_20) {
		if (wdev->wdev_type == WDEV_TYPE_STA) {
			eht_mcs_map_bw20(eht_tx_nss, eht_rx_nss, eht_max_mcs, &eht_mcs_nss_bw20);
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs0_7_nss =
				eht_mcs_nss_bw20.max_tx_rx_mcs0_7_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs8_9_nss =
				eht_mcs_nss_bw20.max_tx_rx_mcs8_9_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs10_11_nss =
				eht_mcs_nss_bw20.max_tx_rx_mcs10_11_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs12_13_nss =
				eht_mcs_nss_bw20.max_tx_rx_mcs12_13_nss;
		} else {
			eht_mcs_map(eht_tx_nss, eht_rx_nss, eht_max_mcs, &eht_mcs_nss);
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs0_7_nss =
				eht_mcs_nss.max_tx_rx_mcs0_9_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs8_9_nss =
				eht_mcs_nss.max_tx_rx_mcs0_9_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs10_11_nss =
				eht_mcs_nss.max_tx_rx_mcs10_11_nss;
			eht_cap->eht_mcs_nss_set.max_tx_rx_mcs12_13_nss =
				eht_mcs_nss.max_tx_rx_mcs12_13_nss;
		}
	} else {
		eht_mcs_map(eht_tx_nss, eht_rx_nss, eht_max_mcs, &eht_mcs_nss);
		eht_cap->eht_mcs_nss_set.max_tx_rx_mcs0_7_nss = eht_mcs_nss.max_tx_rx_mcs0_9_nss;
		eht_cap->eht_mcs_nss_set.max_tx_rx_mcs8_9_nss = eht_mcs_nss.max_tx_rx_mcs0_9_nss;
		eht_cap->eht_mcs_nss_set.max_tx_rx_mcs10_11_nss = eht_mcs_nss.max_tx_rx_mcs10_11_nss;
		eht_cap->eht_mcs_nss_set.max_tx_rx_mcs12_13_nss = eht_mcs_nss.max_tx_rx_mcs12_13_nss;
	}
	eht_cap->dscb_bitmap = 0;
#endif
	switch (eht_bw) {
	case EHT_BW_320:
		eht_cap->ccfs0 = GET_BW320_PRIM160_CENT(prim_ch, eht_cen_ch);
		eht_cap->ccfs1 = eht_cen_ch;
		break;
	case EHT_BW_160:
		eht_cap->ccfs0 = GET_BW160_PRIM80_CENT(prim_ch, eht_cen_ch);
		eht_cap->ccfs1 = eht_cen_ch;
		break;
	case EHT_BW_80:
		eht_cap->ccfs0 = eht_cen_ch;
		eht_cap->ccfs1 = 0;
		break;
	case EHT_BW_2040:
		eht_cap->ccfs0 = eht_cen_ch;
		eht_cap->ccfs1 = 0;
		break;
	default:
		eht_cap->ccfs0 = eht_cen_ch;
		eht_cap->ccfs1 = 0;
	}
	eht_cap->tx_stream = wlan_config_get_tx_stream(wdev);
	eht_cap->rx_stream = wlan_config_get_tx_stream(wdev);

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_EHT_CAP\n");
	return 0;
}
#endif /* CONFIG_MAP_SUPPORT */

#if (defined(MTK_MLO_MAP_SUPPORT) && defined(DOT11_EHT_BE))
static struct bmgr_entry *get_bss_entry_by_netdev(
		IN PNET_DEV pNetDev)
{
	struct bmgr_entry *entry = NULL;
	int i;

	for (i = 0; BMGR_VALID_BSS_IDX(i); i++) {
		entry = bss_mngr.entry[i];
		if (entry && (entry->pNetDev == pNetDev))
			return entry;
	}

	return NULL;
}

static int mtk_cfg80211_reply_mlo_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct wdev_mlo_caps *mlo_cap;
	struct _RTMP_CHIP_CAP *cap;
	int ret = 0, i;
	struct bmgr_entry *entry = NULL;
	u8 mld_grp, mld_type;
	struct bmgr_reg_info *reg_info = NULL;

	if (ctrl_data->rsp_data_len < sizeof(struct wdev_mlo_caps))
		return -RTMP_IO_EINVAL;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
	"Driver: NL80211_VENDOR_ATTR_GET_CAP_MLO_CAPABILITY is called from WAPP in driver\n");
	cap = hc_get_chip_cap(mac_ad->hdev_ctrl);

	ctrl_data->rsp_data_len = sizeof(struct wdev_mlo_caps);
	mlo_cap = (struct wdev_mlo_caps *)ctrl_data->rsp_data;
	if (IF_COMBO_HAVE_AP_STA(mac_ad))
		mlo_cap->RoleNum = 2;
	else
		mlo_cap->RoleNum = 1;
#define MLO_AP_INFO 0
#define MLO_APCLI_INFO 1

	for (i = 0; i < mlo_cap->RoleNum; i++) {
		if (i == 0) {
			if (wdev->wdev_type != WDEV_TYPE_AP) {
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
						"wdev is not an AP\n");
				continue;
			}
			mlo_cap->MloCapPerRole[i].DeviceRole = MLO_AP_INFO;
		}
#ifdef APCLI_SUPPORT
		else if (i == 1) {
			UCHAR b_idx = 0;

			b_idx = HcGetBandByWdev(wdev);
			if (mac_ad->ApCfg.ApCliInfRunned) {
				if (MAX_MULTI_STA >= 1)
					wdev = &mac_ad->StaCfg[0].wdev;
			}
			if (!wdev) {
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
						"wdev is NULL\n");
				continue;
			}
			mlo_cap->MloCapPerRole[i].DeviceRole = MLO_APCLI_INFO;
			mlo_cap->MloCapPerRole[i].EmlCap.emlsr_supp = wlan_config_get_emlsr_mr(wdev);

			if (mac_ad->map_apcli_mlo_disable == 0)
				mlo_cap->MloCapPerRole[i].MloEnable = 1;

		}
#endif
		entry = get_bss_entry_by_netdev(wdev->if_dev);
		if (BMGR_VALID_BSS_ENTRY(entry)) {
			if (!BMGR_VALID_MLO_DEV(entry->mld_ptr)) {
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_ERROR,
					"Entry not affiliated to MLO GRP\n");
				ret = -1;
				return ret;
			}

			reg_info = &entry->entry_info;
			if (reg_info)
				mld_grp = reg_info->mld_grp;
			mld_type = entry->mld_ptr->mld_type;

			mlo_cap->MloCapPerRole[i].StatPunctureSupport = 1;
#ifndef MAP_R6
			if ((i == 0) && (mld_type == BMGR_MLD_TYPE_MULTI ||
				mld_type == BMGR_MLD_TYPE_SINGLE))
				mlo_cap->MloCapPerRole[i].MloEnable = 1;
#else
			if ((i == 0) && ((mld_type == BMGR_MLD_TYPE_MULTI
					|| mld_type == BMGR_MLD_TYPE_SINGLE)
					&& mld_grp < BMGR_INVALID_MLD_GRP
					&& mld_grp != 0)) {
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"mld_type = %d mld_grp %d\n", mld_type, mld_grp);
				mlo_cap->MloCapPerRole[i].MloEnable = 1;
			} else {
				MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
					"non-mld type mld_type = %d, mld_grp %d or sta %d\n",
					mld_type, mld_grp, i);
			}
#endif
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_INFO,
				"MLO CAP :device role %d StatPunc Support %d, MloEnable %d\n",
				mlo_cap->MloCapPerRole[i].DeviceRole,
				mlo_cap->MloCapPerRole[i].StatPunctureSupport,
				mlo_cap->MloCapPerRole[i].MloEnable);
			if (i != 1)
				mlo_cap->MloCapPerRole[i].EmlCap.emlsr_supp =
					entry->mld_ptr->attr.eml_caps.emlsr_supp;

			mlo_cap->MloCapPerRole[i].EmlCap.emlsr_padding =
					entry->mld_ptr->attr.eml_caps.eml_padding_delay;
			mlo_cap->MloCapPerRole[i].EmlCap.emlsr_trans_delay =
					entry->mld_ptr->attr.eml_caps.eml_trans_delay;
			mlo_cap->MloCapPerRole[i].EmlCap.emlmr_supp =
					entry->mld_ptr->attr.eml_caps.emlmr_supp;
			mlo_cap->MloCapPerRole[i].EmlCap.emlmr_delay = 0;
			mlo_cap->MloCapPerRole[i].EmlCap.trans_to =
					entry->mld_ptr->attr.eml_caps.trans_to;
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_INFO,
				"MLO EML CAP :EMLSR Support %d padding %d Delay %d\n",
				mlo_cap->MloCapPerRole[i].EmlCap.emlsr_supp,
				mlo_cap->MloCapPerRole[i].EmlCap.emlsr_padding,
				mlo_cap->MloCapPerRole[i].EmlCap.emlsr_trans_delay);
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_INFO,
				"MLO EML CAP : EMLMR Support %d Delay %d trans timeout %d\n",
				mlo_cap->MloCapPerRole[i].EmlCap.emlmr_supp,
				mlo_cap->MloCapPerRole[i].EmlCap.emlmr_delay,
				mlo_cap->MloCapPerRole[i].EmlCap.trans_to);
			mlo_cap->MloCapPerRole[i].MldCap.max_simul_link =
				(entry->mld_ptr->mld_link_cnt - 1);
			mlo_cap->MloCapPerRole[i].MldCap.srs_supp =
				entry->mld_ptr->attr.mld_caps.srs_supp;
			mlo_cap->MloCapPerRole[i].MldCap.t2l_nego_supp =
				entry->mld_ptr->attr.mld_caps.t2l_nego_supp;
			mlo_cap->MloCapPerRole[i].MldCap.freq_sep_str =
				entry->mld_ptr->attr.mld_caps.freq_sep_str;
			mlo_cap->MloCapPerRole[i].MldCap.aar_supp = 0;
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_INFO,
				"MLO MLD CAP : max_simul_link %d srs supp %d t2lnego %d\n",
				mlo_cap->MloCapPerRole[i].MldCap.max_simul_link,
				mlo_cap->MloCapPerRole[i].MldCap.srs_supp,
				mlo_cap->MloCapPerRole[i].MldCap.t2l_nego_supp);
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_BMGR, DBG_LVL_INFO,
				"MLO MLD CAP : Freq Sep Support %d AAR Supp %d\n",
				mlo_cap->MloCapPerRole[i].MldCap.freq_sep_str,
				mlo_cap->MloCapPerRole[i].MldCap.aar_supp);
		}
	}
	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_MLO_CAP\n");
	return ret;
}

static int  mtk_cfg80211_reply_mlo_mld_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct _wdev_mlo_mld_info *mlo_info;
	struct bmgr_entry *entry = NULL;
	int ret = 0;
#ifdef MLD_GROUP_RESTORE
	struct _BSS_STRUCT *pMbss = wdev->func_dev;
	struct bmgr_mlo_dev *mld;
#endif


	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_NOTICE, "wdev is not an AP\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->rsp_data_len < sizeof(struct _wdev_mlo_mld_info))
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_MLO_MLD_INFO\n");

	ctrl_data->rsp_data_len = sizeof(struct _wdev_mlo_mld_info);
	mlo_info = (struct _wdev_mlo_mld_info *)ctrl_data->rsp_data;

	entry = get_bss_entry_by_netdev(wdev->if_dev);

	if (BMGR_VALID_MLO_BSS_ENTRY(entry)) {
		if (!BMGR_VALID_MLO_DEV(entry->mld_ptr)) {
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
				"Entry not affiliated to MLO GRP\n");
			ret = -1;
			goto out;
		}
			mlo_info->mld_idx = entry->mld_ptr->mld_grp;
			NdisMoveMemory(mlo_info->mld_addr, entry->mld_ptr->mld_addr, MAC_ADDR_LEN);
#ifdef MAP_R6
			mlo_info->link_id = entry->link_id;
			mlo_info->ap_str_support = 1;
			mlo_info->ap_nstr_support = 0;
			mlo_info->ap_emlsr_support = entry->mld_ptr->attr.eml_caps.emlsr_supp;
			mlo_info->ap_emlmr_support = entry->mld_ptr->attr.eml_caps.emlmr_supp;
#endif
	}
#ifdef MLD_GROUP_RESTORE
	/* if bss not in mlo but config restore, alse response mld info */
	else if (BSS_NEED_RESTORE_MLD(pMbss)) {
		mlo_info->mld_idx = pMbss->mld_grp_idx_for_restore;
#ifdef MLD_GROUP_ID_REMAPPING_V1
		mld = mld_idx_mapping_v1_load(mlo_info->mld_idx);
#else
		mld = GET_MLD_BY_GRP_IDX(mlo_info->mld_idx);
#endif
		if (!BMGR_VALID_MLO_DEV(mld)) {
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
				"mld is not valid, mld_group_idx=%d\n", mlo_info->mld_idx);
			ret = -1;
			goto out;
		}
		NdisMoveMemory(mlo_info->mld_addr, mld->mld_addr, MAC_ADDR_LEN);
#ifdef MAP_R6
		mlo_info->link_id = entry->link_id;
		mlo_info->ap_str_support = 1;
		mlo_info->ap_nstr_support = 0;
		mlo_info->ap_emlsr_support = mld->attr.eml_caps.emlsr_supp;
		mlo_info->ap_emlmr_support = mld->attr.eml_caps.emlmr_supp;
#endif
	}
#endif

out:
	return ret;
}
#endif /* (defined(MTK_MLO_MAP_SUPPORT) && defined(DOT11_EHT_BE)) */

#ifdef DOT11_HE_AX
#ifdef MAP_R3
static int mtk_cfg80211_reply_wf6_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_wf6_cap_roles *wf6_cap;
	UINT16 i = 0;

	if (ctrl_data->rsp_data_len < sizeof(wdev_wf6_cap_roles))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(wdev_wf6_cap_roles);
	wf6_cap = (wdev_wf6_cap_roles *)ctrl_data->rsp_data;
	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_INFO,
	"NL80211_VENDOR_ATTR_GET_CAP_INFO_WF6_CAPABILITY is called from WAPP in driver\n");
	if (IF_COMBO_HAVE_AP_STA(mac_ad))
		wf6_cap->role_supp = 2;
	else
		wf6_cap->role_supp = 1;
#define WF6_AP_INFO 0
#define WF6_APCLI_INFO 1
	/* Role supported as 1 when previously used*/

	for (i = 0; i < wf6_cap->role_supp; i++) {
		UINT16 he_max_mcs_nss = 0, *he_mcs_pos = NULL, he_mcs_len = 0;
		struct mcs_nss_caps *mcs_nss = NULL;
		UINT8 he_bw = 0;
		struct he_bf_info he_bf_struct;
#ifdef APCLI_SUPPORT
		UCHAR b_idx = 0;
#endif
		if (!wdev) {
			wf6_cap->role_supp = 1;
			continue;
		}
		if (!wdev->pHObj) {
			wf6_cap->role_supp = 1;
			continue;
		}
#ifdef APCLI_SUPPORT
		b_idx = HcGetBandByWdev(wdev);
		if ((i == 1) && (mac_ad->ApCfg.ApCliInfRunned)) {
			if (MAX_MULTI_STA >= 1)
				wdev = &mac_ad->StaCfg[0].wdev;
		} else if (i == 1) {
			wf6_cap->role_supp = 1;
			continue;
		}
		if (!wdev) {
			wf6_cap->role_supp = 1;
			continue;
		}
		if (!wdev->pHObj) {
			wf6_cap->role_supp = 1;
			continue;
		}
#endif
		mcs_nss = wlan_config_get_mcs_nss_caps(wdev);
		he_bw = wlan_config_get_he_bw(wdev);

		wf6_cap->wf6_role[i].tx_stream = wlan_config_get_tx_stream(wdev);
		wf6_cap->wf6_role[i].rx_stream = wlan_config_get_rx_stream(wdev);

		he_mcs_pos = (UINT16 *)wf6_cap->wf6_role[i].he_mcs;

		he_max_mcs_nss = he_mcs_map(wf6_cap->wf6_role[i].tx_stream, HE_MCS_0_11);
		NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
		he_mcs_pos++;

		he_max_mcs_nss = he_mcs_map(wf6_cap->wf6_role[i].rx_stream, HE_MCS_0_11);
		NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
		he_mcs_pos++;

		he_mcs_len += 4;

		if (he_bw > HE_BW_80
#ifdef MAP_R3
			|| ((IS_MAP_ENABLE(mac_ad)
				&& IS_MAP_CERT_ENABLE(mac_ad)
				&& IS_MAP_R3_ENABLE(mac_ad)))
#endif

		) {
			if (wf6_cap->wf6_role[i].tx_stream > mcs_nss->bw160_max_nss)
				wf6_cap->wf6_role[i].tx_stream = mcs_nss->bw160_max_nss;
			if (wf6_cap->wf6_role[i].rx_stream > mcs_nss->bw160_max_nss)
				wf6_cap->wf6_role[i].rx_stream = mcs_nss->bw160_max_nss;

			he_max_mcs_nss = he_mcs_map(wf6_cap->wf6_role[i].tx_stream, HE_MCS_0_11);
			NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
			he_mcs_pos++;

			he_max_mcs_nss = he_mcs_map(wf6_cap->wf6_role[i].rx_stream, HE_MCS_0_11);
			NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
			he_mcs_pos++;

			he_mcs_len += 4;
			wf6_cap->wf6_role[i].he_8080 = 1;

			if (he_bw > HE_BW_160
#ifdef MAP_R3
				|| ((IS_MAP_ENABLE(mac_ad)
				&& IS_MAP_CERT_ENABLE(mac_ad)
				&& IS_MAP_R3_ENABLE(mac_ad)))
#endif
			) {
				he_max_mcs_nss = he_mcs_map(
					wf6_cap->wf6_role[i].tx_stream, HE_MCS_0_11);
				NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
				he_mcs_pos++;

				he_max_mcs_nss = he_mcs_map(
					wf6_cap->wf6_role[i].rx_stream, HE_MCS_0_11);
				NdisMoveMemory(he_mcs_pos, &he_max_mcs_nss, sizeof(UINT16));
				he_mcs_pos++;

				he_mcs_len += 4;
				wf6_cap->wf6_role[i].he_160 = 1;
			}
		}

		wf6_cap->wf6_role[i].he_mcs_len = he_mcs_len;
		he_mcs_len = 0;

		NdisZeroMemory(&he_bf_struct, sizeof(struct he_bf_info));
		mt_wrap_get_he_bf_cap(wdev, &he_bf_struct);

		if (he_bf_struct.bf_cap & HE_SU_BFER)
			wf6_cap->wf6_role[i].su_bf_cap = 1;
		if (he_bf_struct.bf_cap & HE_MU_BFER)
			wf6_cap->wf6_role[i].mu_bf_cap = 1;
		if (wlan_config_get_mu_dl_mimo(wdev)) {
			if (wdev->wdev_type == WDEV_TYPE_AP)
				wf6_cap->wf6_role[i].dl_mu_mimo_ofdma_cap = 1;
		}
		if (wlan_config_get_mu_ul_mimo(wdev)) {
			if (wdev->wdev_type == WDEV_TYPE_AP) {
				wf6_cap->wf6_role[i].ul_mu_mimo_ofdma_cap = 1;
				wf6_cap->wf6_role[i].ul_mu_mimo_cap = 1;
			}
		}
		/* agent_role = 0 for AP, agent_role = 1 for APCLI */
		wf6_cap->wf6_role[i].agent_role = i;
		wf6_cap->wf6_role[i].su_beamformee_status = 0;
		wf6_cap->wf6_role[i].beamformee_sts_less80 = 0;
		wf6_cap->wf6_role[i].beamformee_sts_more80 = 0;
		wf6_cap->wf6_role[i].max_user_dl_tx_mu_mimo = 0;
		wf6_cap->wf6_role[i].max_user_ul_rx_mu_mimo = 0;
		wf6_cap->wf6_role[i].max_user_dl_tx_ofdma = 0;
		wf6_cap->wf6_role[i].max_user_ul_rx_ofdma = 0;
		wf6_cap->wf6_role[i].rts_status = 0;
		wf6_cap->wf6_role[i].mu_rts_status = 0; /* Not Supported */
		wf6_cap->wf6_role[i].m_bssid_status = 0;
		wf6_cap->wf6_role[i].mu_edca_status = 0;
		wf6_cap->wf6_role[i].twt_requester_status = 0;
		wf6_cap->wf6_role[i].twt_responder_status = 0;
		wf6_cap->wf6_role[i].ul_ofdma_cap = 0;
		wf6_cap->wf6_role[i].dl_ofdma_cap = 0;
#ifdef HE_TXBF_SUPPORT
		if (he_bf_struct.bf_cap & HE_SU_BFEE)
			wf6_cap->wf6_role[i].su_beamformee_status = 1;

		if (he_bf_struct.bfee_sts_le_eq_bw80)
			wf6_cap->wf6_role[i].beamformee_sts_less80 = 1;

		if (he_bf_struct.bfee_sts_gt_bw80)
			wf6_cap->wf6_role[i].beamformee_sts_more80 = 1;
#endif /*HE_TXBF_SUPPORT*/
#ifdef WIFI_TWT_SUPPORT
		if (wlan_config_get_asic_twt_caps(wdev) &&
				(TWT_SUPPORT_ITWT(wlan_config_get_he_twt_support(wdev)))) {
			if (i == WF6_AP_INFO) {
				wf6_cap->wf6_role[i].twt_requester_status = 0;
				wf6_cap->wf6_role[i].twt_responder_status = 1;
			} else {
				wf6_cap->wf6_role[i].twt_requester_status = 1;
				wf6_cap->wf6_role[i].twt_responder_status = 0;
			}
		}
#endif /*WIFI_TWT_SUPPORT*/

		if (wlan_config_get_he_mu_edca(wdev))
			wf6_cap->wf6_role[i].mu_edca_status = 1;

		if (wlan_config_get_mu_dl_ofdma(wdev) &&
			wdev->wdev_type == WDEV_TYPE_AP) {
			wf6_cap->wf6_role[i].dl_ofdma_cap = 1;
			wf6_cap->wf6_role[i].max_user_dl_tx_ofdma =
				wlan_config_get_ofdma_user_cnt(wdev);
#ifdef CFG_SUPPORT_FALCON_MURU
			if (!wf6_cap->wf6_role[i].max_user_dl_tx_ofdma) {
			/*iwpriv ra0 set set_muru_manual_config=ul_comm_user_cnt:*/
				wf6_cap->wf6_role[i].max_user_dl_tx_ofdma =
					(UINT8)(mac_ad->CommonCfg.HE_OfdmaUserNum);
			}
#endif
		}

		if (wlan_config_get_mu_ul_ofdma(wdev) &&
				wdev->wdev_type == WDEV_TYPE_AP) {
			wf6_cap->wf6_role[i].ul_ofdma_cap = 1;
			wf6_cap->wf6_role[i].max_user_ul_rx_ofdma =
				wlan_config_get_ofdma_user_cnt(wdev);
#ifdef CFG_SUPPORT_FALCON_MURU
			if (!wf6_cap->wf6_role[i].max_user_ul_rx_ofdma)
				wf6_cap->wf6_role[i].max_user_ul_rx_ofdma =
				(UINT8)(mac_ad->CommonCfg.HE_OfdmaUserNum);
#endif
		}

#ifdef MBSS_SUPPORT
		if (MAX_MBSSID_NUM(mac_ad) > 1)
			wf6_cap->wf6_role[i].m_bssid_status = 1;
#endif /*MBSS_SUPPORT*/

		if (mac_ad->mcli_ctl.c2s_only == TRUE)
			wf6_cap->wf6_role[i].rts_status = 1;

		if (wlan_config_get_mu_dl_mimo(wdev)) {
			if (wdev->wdev_type == WDEV_TYPE_AP)
				wf6_cap->wf6_role[i].max_user_dl_tx_mu_mimo =
					wf6_cap->wf6_role[i].max_user_dl_tx_ofdma;
		}
		if (wlan_config_get_mu_ul_mimo(wdev)) {
			if (wdev->wdev_type == WDEV_TYPE_AP)
				wf6_cap->wf6_role[i].max_user_ul_rx_mu_mimo =
					wf6_cap->wf6_role[i].max_user_ul_rx_ofdma;
		}
	}

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_BCN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_HE_CAP\n");
	return 0;
}
#endif /* MAP_R3 */
#endif /* DOT11_HE_AX */

static int mtk_cfg80211_reply_wapp_max_num_of_sta(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	UINT16 *MaxStaNum = 0;

	if (ctrl_data->rsp_data_len < sizeof(UINT16))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(UINT16);
	MaxStaNum = ctrl_data->rsp_data;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_MAX_NUM_OF_STA\n");

	*MaxStaNum = GET_MAX_UCAST_NUM(mac_ad);
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"Driver MaxStaNum = %d\n", *MaxStaNum);
	return 0;
}

static int mtk_cfg80211_reply_wapp_chan_list(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int i = 0;
	CHANNEL_CTRL *pChCtrl = NULL;
	wdev_chn_info *chn_list;

	if (ctrl_data->rsp_data_len < sizeof(wdev_chn_info))
		return -RTMP_IO_EINVAL;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR, "wdev is not an AP\n");
		return -RTMP_IO_EINVAL;
	}

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_CHAN_LIST\n");

	ctrl_data->rsp_data_len = sizeof(wdev_chn_info);
	chn_list = (wdev_chn_info *)ctrl_data->rsp_data;

	pChCtrl = hc_get_channel_ctrl(mac_ad->hdev_ctrl);
	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO, "(sync) Msg received!\n");
	mac_ad->ChannelListNum = pChCtrl->ChListNum;
	for (i = 0; i < pChCtrl->ChListNum; i++) {
		mac_ad->ChannelList[i].Channel = pChCtrl->ChList[i].Channel;
		mac_ad->ChannelList[i].DfsReq = pChCtrl->ChList[i].DfsReq;
	}
	chn_list->band = wdev->PhyMode;
	chn_list->op_ch = wlan_operate_get_prim_ch(wdev);
	chn_list->op_class = get_regulatory_class(mac_ad, wdev->channel, wdev->PhyMode, wdev);
	chn_list->ch_list_num = mac_ad->ChannelListNum;
	chn_list->dl_mcs = wdev->HTPhyMode.field.MCS;
	setChannelList(mac_ad, wdev, chn_list);
#ifdef CONFIG_MAP_SUPPORT
	chn_list->non_op_chn_num = getNonOpChnNum(mac_ad, wdev, chn_list->op_class);
	setNonOpChnList(mac_ad,
			wdev,
			chn_list->non_op_ch_list,
			chn_list->op_class,
			chn_list->non_op_chn_num);

	setAutoChannelSkipList(mac_ad, wdev, chn_list);
#endif /* CONFIG_MAP_SUPPORT */

	return 0;
}

static int mtk_cfg80211_reply_wapp_nop_chan_list(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct nop_channel_list_s *nop_channels;

	if (ctrl_data->rsp_data_len < sizeof(struct nop_channel_list_s))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct nop_channel_list_s);
	nop_channels = (struct nop_channel_list_s *)ctrl_data->rsp_data;
	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_NOP_CHANNEL_LIST\n");

	wapp_prepare_nop_channel_list(mac_ad, nop_channels);
	return 0;
}

static int mtk_cfg80211_reply_wapp_op_class(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
#ifndef MAP_6E_SUPPORT
	struct _wdev_op_class_info *op_class;
#else
	struct _wdev_op_class_info_ext *op_class;
#endif
	unsigned int data_size;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "wdev is not an AP\n");
		return -RTMP_IO_EINVAL;
	}

#ifndef MAP_6E_SUPPORT
	data_size = sizeof(struct _wdev_op_class_info);
#else
	data_size = sizeof(struct _wdev_op_class_info_ext);
#endif

	if (ctrl_data->rsp_data_len < data_size)
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_OP_CLASS\n");

	ctrl_data->rsp_data_len = data_size;
#ifndef MAP_6E_SUPPORT
	op_class = (struct _wdev_op_class_info *)ctrl_data->rsp_data;
#else
	op_class = (struct _wdev_op_class_info_ext *)ctrl_data->rsp_data;
#endif
#ifdef CONFIG_MAP_SUPPORT
#ifdef MAP_6E_SUPPORT
	if (WMODE_CAP(wdev->PhyMode, WMODE_AX_6G))
		op_class->num_of_op_class = map_set_op_class_info_6g(mac_ad, wdev, op_class);
	else
#endif
		op_class->num_of_op_class = map_set_op_class_info(mac_ad, wdev, op_class);

#endif /* CONFIG_MAP_SUPPORT */
	return 0;
}

static int mtk_cfg80211_reply_wapp_bss_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_bss_info *bss_info;
	BSS_STRUCT *mbss;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_FW, CATFW_BSSINFO, DBG_LVL_ERROR, "wdev is not an AP\n");
		return -RTMP_IO_EINVAL;
	}
	if (ctrl_data->rsp_data_len < sizeof(wdev_bss_info))
		return -RTMP_IO_EINVAL;

	mbss = wdev->func_dev;
	MTWF_DBG(mac_ad, DBG_CAT_FW, CATFW_BSSINFO, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_BSS_INFO\n");
	ctrl_data->rsp_data_len = sizeof(wdev_bss_info);
	bss_info = (wdev_bss_info *)ctrl_data->rsp_data;

	bss_info->SsidLen = mbss->SsidLen;
	NdisMoveMemory(bss_info->ssid,
			mbss->Ssid,
			(MAX_LEN_OF_SSID+1));
	NdisMoveMemory(bss_info->bssid, wdev->bssid, MAC_ADDR_LEN);
	NdisMoveMemory(bss_info->if_addr, wdev->if_addr, MAC_ADDR_LEN);
#ifdef CONFIG_MAP_SUPPORT
	bss_info->map_role = wdev->MAPCfg.DevOwnRole;
#endif
	bss_info->auth_mode = wdev->SecConfig.AKMMap;
	bss_info->enc_type = wdev->SecConfig.PairwiseCipher;
#ifdef WSC_AP_SUPPORT
	bss_info->key_len = strlen(wdev->WscControl.WpaPsk);
	NdisMoveMemory(bss_info->key,
		wdev->WscControl.WpaPsk, bss_info->key_len);
#else
#ifndef RT_CFG80211_SUPPORT
	bss_info->key_len = strlen(mbss->PSK);
	NdisMoveMemory(bss_info->key, mbss->PSK, bss_info->key_len);
#endif
#endif
	bss_info->hidden_ssid = mbss->bHideSsid;

	return 0;
}

#ifdef WSC_INCLUDED
static int mtk_cfg80211_reply_wapp_wsc_profiles(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int TotalLen = 0;
	wsc_apcli_config_msg *wsc_config = NULL;
	POS_COOKIE pObj = NULL;
	unsigned int ifIndex;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_WAPP_WSC_PROFILES\n");

	pObj = (POS_COOKIE) mac_ad->OS_Cookie;
	ifIndex = pObj->ioctl_if;

	TotalLen = Fill_OID_WSC_PROFILE(mac_ad, ifIndex, &wsc_config);
	if (wsc_config == NULL) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"wsc_config is NULL return FALSE\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->rsp_data_len < TotalLen) {
		os_free_mem(wsc_config);
		return -RTMP_IO_EINVAL;
	}

	ctrl_data->rsp_data_len = TotalLen;
	os_move_mem(ctrl_data->rsp_data, wsc_config, TotalLen);
	os_free_mem(wsc_config);

	return 0;
}
#endif /* WSC_INCLUDED */
#ifdef REPORT_CH_TXPWR_SUPPORT
#define IS_20M(__opclass) \
	((__opclass == 115) || (__opclass == 118) || (__opclass == 121) || (__opclass == 125) \
	|| (__opclass == 131) || (__opclass == 81) || (__opclass == 82))

#endif
static int mtk_cfg80211_reply_wapp_tx_pwr(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct _wdev_tx_power_ext *tx_pwr = NULL;
	UINT ifIndex;
	POS_COOKIE pObj = NULL;
#ifndef MAP_6E_SUPPORT
	wdev_op_class_info *op_class;
#else
	struct _wdev_op_class_info_ext *op_class;
#endif
#ifdef REPORT_CH_TXPWR_SUPPORT
	struct _wdev_tx_power_limit *ptxpwrinfo = NULL;
	UINT8 i = 0;
#endif
	UINT8 op = 0, ch = 0, pwr_num = 0;
	UINT8 channel_set_num = 0, MaxTxPwr = 0, ChSetMinLimPwr;
	PUCHAR channel_set = NULL;

	pObj = (POS_COOKIE) mac_ad->OS_Cookie;
	ifIndex = pObj->ioctl_if;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "wdev is not an AP\n");
		return -RTMP_IO_EINVAL;
	}
	if (ctrl_data->rsp_data_len < sizeof(wdev_tx_power))
		return -RTMP_IO_EINVAL;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_TX_PWR\n");

#ifndef MAP_6E_SUPPORT
	os_alloc_mem(mac_ad, (UCHAR **)&op_class, sizeof(wdev_op_class_info));
#else
	os_alloc_mem(mac_ad, (UCHAR **)&op_class, sizeof(struct _wdev_op_class_info_ext));
#endif

	if (op_class == NULL)
		return -RTMP_IO_EINVAL;
#ifndef MAP_6E_SUPPORT
	NdisZeroMemory(op_class, sizeof(wdev_op_class_info));
#else
	NdisZeroMemory(op_class, sizeof(struct _wdev_op_class_info_ext));
#endif

#ifdef CONFIG_MAP_SUPPORT
#ifdef MAP_6E_SUPPORT
	if (WMODE_CAP(wdev->PhyMode, WMODE_AX_6G))
		op_class->num_of_op_class = map_set_op_class_info_6g(mac_ad, wdev, op_class);
	else
#endif
		op_class->num_of_op_class = map_set_op_class_info(mac_ad, wdev, op_class);

#endif
	ctrl_data->rsp_data_len = sizeof(struct _wdev_tx_power_ext);
	tx_pwr = (struct _wdev_tx_power_ext *)ctrl_data->rsp_data;
	tx_pwr->num_of_op_class = op_class->num_of_op_class;
#ifdef REPORT_CH_TXPWR_SUPPORT
	if (ifIndex == 1) {
		os_alloc_mem(mac_ad, (UCHAR **)&ptxpwrinfo, sizeof(struct _wdev_tx_power_limit));
		NdisZeroMemory(ptxpwrinfo, sizeof(struct _wdev_tx_power_limit));
		tx_pwr->band =  HcGetBandByWdev(wdev);
		tx_pwr->tx_power_set = 1;
		MtGetMaxTxPwrLimit(mac_ad, tx_pwr->band, ptxpwrinfo, POWER_LIMIT_TABLE_TYPE_SKU);
	} else {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_DEBUG, "ifindex not one\n");
		tx_pwr->tx_power_set = 0;
	}
#endif
	for (op = 0; op < op_class->num_of_op_class; op++) {
#ifndef MAP_6E_SUPPORT
		if (op_class->opClassInfo[op].op_class == 0)
#else
		if (op_class->opClassInfoExt[op].op_class == 0)
#endif
			continue;
#ifndef MAP_6E_SUPPORT
		tx_pwr->tx_pwr_limit[pwr_num].op_class = op_class->opClassInfo[op].op_class;
		channel_set = get_channelset_by_reg_class(
				mac_ad, op_class->opClassInfo[op].op_class, wdev->PhyMode);
#else
		tx_pwr->tx_pwr_limit[pwr_num].op_class = op_class->opClassInfoExt[op].op_class;
		channel_set = get_channelset_by_reg_class(
				mac_ad, op_class->opClassInfoExt[op].op_class, wdev->PhyMode);
#endif
		channel_set_num = get_channel_set_num(channel_set);
		/* no match channel set. */
		if (channel_set == NULL)
			continue;
		/* empty channel set. */
		if (channel_set_num == 0 || channel_set_num > MAX_NUM_OF_CHANNELS)
			continue;
		/*
			There is many channel which have different limit tx power
			we choose the minimum
		*/
#ifdef REPORT_CH_TXPWR_SUPPORT
		tx_pwr->tx_pwr_limit[pwr_num].ch_num = channel_set_num;
#endif
		ChSetMinLimPwr = 0xFF;
		for (ch = 0; ch < channel_set_num; ch++) {
			MaxTxPwr = GetRegulatoryMaxTxPwr(mac_ad, channel_set[ch], wdev);
			if (!strncmp((RTMP_STRING *) mac_ad->CommonCfg.CountryCode, "CN", 2))
				MaxTxPwr = mac_ad->MaxTxPwr;/*for CN CountryCode*/
			if (MaxTxPwr < ChSetMinLimPwr)
				ChSetMinLimPwr = MaxTxPwr;
			if (ChSetMinLimPwr == 0xff)
				ChSetMinLimPwr = mac_ad->MaxTxPwr;
#ifdef REPORT_CH_TXPWR_SUPPORT
			if (IS_20M(tx_pwr->tx_pwr_limit[pwr_num].op_class) == 0
				|| (tx_pwr->tx_power_set == 0)) {
				tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].channel = 0;
				tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].pwrlimit = 0;
				tx_pwr->tx_pwr_limit[pwr_num].ch_num = 0;
				continue;
			}

			tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].channel = channel_set[ch];
			for (i = 0; i < ptxpwrinfo->ch_num; i++) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_DEBUG,
					"ch:%d pwr:%d/%d/%d/%d/%d\n", ptxpwrinfo->tx_pwr[ch].channel,
					ptxpwrinfo->tx_pwr[ch].pwrlimit[0], ptxpwrinfo->tx_pwr[ch].pwrlimit[1],
					ptxpwrinfo->tx_pwr[ch].pwrlimit[2], ptxpwrinfo->tx_pwr[ch].pwrlimit[3],
					ptxpwrinfo->tx_pwr[ch].pwrlimit[4]);
				if (ptxpwrinfo->tx_pwr[i].channel == tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].channel) {
					tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].pwrlimit = ptxpwrinfo->tx_pwr[i].pwrlimit[0];
					MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_DEBUG,
					"TX POWER ch:%u, power:%u, op_class:%u!\n",
					tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].channel,
					tx_pwr->tx_pwr_limit[pwr_num].ch_pwr[ch].pwrlimit,
					tx_pwr->tx_pwr_limit[pwr_num].op_class);
					break;

				}
			}
#endif
		}
		tx_pwr->tx_pwr_limit[pwr_num].max_pwr = ChSetMinLimPwr;
		pwr_num++;
	}

	if (op_class)
		os_free_mem(op_class);
#ifdef REPORT_CH_TXPWR_SUPPORT
	if (ptxpwrinfo && ifIndex == 1)
		os_free_mem(ptxpwrinfo);
#endif
	return 0;
}

static int mtk_cfg80211_reply_wapp_ap_metrics(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	wdev_ap_metric *ap_metric;
	BSS_STRUCT *mbss;

	if (wdev->wdev_type != WDEV_TYPE_AP) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "wdev is not an AP\n");
		return -RTMP_IO_EFAULT;
	}
	if (ctrl_data->rsp_data_len < sizeof(wdev_ap_metric))
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = sizeof(wdev_ap_metric);
	ap_metric = (wdev_ap_metric *)ctrl_data->rsp_data;
	mbss = wdev->func_dev;
	NdisMoveMemory(ap_metric->bssid, wdev->bssid, MAC_ADDR_LEN);
	ap_metric->cu = get_channel_utilization(mac_ad, wdev->func_idx);
	NdisCopyMemory(ap_metric->ESPI_AC[ESPI_BE], mbss->ESPI_AC_BE, sizeof(mbss->ESPI_AC_BE));
	NdisCopyMemory(ap_metric->ESPI_AC[ESPI_BK], mbss->ESPI_AC_BK, sizeof(mbss->ESPI_AC_BK));
	NdisCopyMemory(ap_metric->ESPI_AC[ESPI_VO], mbss->ESPI_AC_VO, sizeof(mbss->ESPI_AC_VO));
	NdisCopyMemory(ap_metric->ESPI_AC[ESPI_VI], mbss->ESPI_AC_VI, sizeof(mbss->ESPI_AC_VI));
	return 0;
}

static int mtk_cfg80211_reply_wapp_support_ver(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	char *wapp_support_ver;
	int ret;

	if (ctrl_data->rsp_data_len < strlen(WAPP_SUPPORT_VERSION) + 1)
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_WAPP_SUPPORT_VER\n");

	ctrl_data->rsp_data_len = strlen(WAPP_SUPPORT_VERSION) + 1;
	wapp_support_ver = (char *)ctrl_data->rsp_data;
	ret = snprintf(wapp_support_ver, ctrl_data->rsp_data_len, "%s", WAPP_SUPPORT_VERSION);
	if (os_snprintf_error(ctrl_data->rsp_data_len, ret)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"wapp_support_ver snprintf error!\n");
		return -RTMP_IO_EINVAL;
	}
	return 0;
}

static int mtk_cfg80211_reply_wifi_ver(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	char *driver_ver;
	int ret;

	if (ctrl_data->rsp_data_len < strlen(AP_DRIVER_VERSION) + 1)
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
		"get MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_WIFI_VER\n");

	ctrl_data->rsp_data_len = strlen(AP_DRIVER_VERSION) + 1;
	driver_ver = (char *)ctrl_data->rsp_data;
	ret = snprintf(&driver_ver[0], ctrl_data->rsp_data_len, "%s", AP_DRIVER_VERSION);
	if (os_snprintf_error(ctrl_data->rsp_data_len, ret)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"driver_ver snprintf error!\n");
		return -EINVAL;
	}
	return 0;
}
#endif /* WAPP_SUPPORT */

#if (defined(OFFCHANNEL_SCAN_FEATURE) && defined(ZERO_PKT_LOSS_SUPPORT))
static int mtk_cfg80211_reply_current_channel_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct msg_channel_list *pRsp = NULL;
	CHANNEL_CTRL *pChCtrl = NULL;
	int i = 0;

	if (ctrl_data->rsp_data_len < sizeof(struct msg_channel_list))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct msg_channel_list);
	pRsp = (struct msg_channel_list *)ctrl_data->rsp_data;

	pChCtrl = hc_get_channel_ctrl(mac_ad->hdev_ctrl);
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_DEBUG,
		"(sync) Msg received !!\n");
	mac_ad->ChannelListNum = pChCtrl->ChListNum;

	pRsp->CHANNELLIST[0].channel = wlan_operate_get_prim_ch(wdev);
	pRsp->CHANNELLIST[0].channel_idx = Channel2Index(mac_ad, pRsp->CHANNELLIST[0].channel);
	pRsp->CHANNELLIST[0].dfs_req = mac_ad->ChannelList[pRsp->CHANNELLIST[0].channel_idx].DfsReq;
	pRsp->CHANNELLIST[0].tx_time = mac_ad->Ch_Stats.Tx_Time;
	pRsp->CHANNELLIST[0].rx_time = mac_ad->Ch_Stats.Rx_Time;
	pRsp->CHANNELLIST[0].obss_time = mac_ad->Ch_Stats.Obss_Time;
	pRsp->CHANNELLIST[0].channel_busy_time = mac_ad->Ch_BusyTime;
	pRsp->CHANNELLIST[0].NF = -2;
	/* Convert MeasurementDuration to milli seconds */
	pRsp->CHANNELLIST[0].actual_measured_time = (mac_ad->ChannelStats.MeasurementDuration / 1000);

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
	"Current ch: %d Current ch_id : %d Current ch dfs_req : %d NF : %d Ch_bsy_time : %u!!\n",
	pRsp->CHANNELLIST[0].channel, pRsp->CHANNELLIST[0].channel_idx,
	pRsp->CHANNELLIST[0].dfs_req, pRsp->CHANNELLIST[0].NF,
	pRsp->CHANNELLIST[0].channel_busy_time);

	for (i = 1; i <= pChCtrl->ChListNum; i++) {
		pRsp->CHANNELLIST[i].channel = pChCtrl->ChList[i-1].Channel;
		mac_ad->ChannelList[i-1].Channel = pChCtrl->ChList[i-1].Channel;
		pRsp->CHANNELLIST[i].channel_idx = i-1;
		pRsp->CHANNELLIST[i].dfs_req = pChCtrl->ChList[i-1].DfsReq;
	}

	return 0;
}

static int mtk_cfg80211_reply_operating_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct operating_info *Info;

	if (ctrl_data->rsp_data_len < sizeof(OPERATING_INFO))
		return -RTMP_IO_EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
		"(sync) Msg received for current channel info!!\n");

	ctrl_data->rsp_data_len = sizeof(OPERATING_INFO);
	Info = (struct operating_info *)ctrl_data->rsp_data;
	Info->cfg_ht_bw = wlan_config_get_ht_bw(wdev);
	Info->cfg_vht_bw = wlan_config_get_vht_bw(wdev);
	Info->RDDurRegion = mac_ad->CommonCfg.RDDurRegion;
	Info->region = GetCountryRegionFromCountryCode(mac_ad->CommonCfg.CountryCode);
	Info->channel = wlan_operate_get_prim_ch(wdev);
	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
		"Channel: %d Info.cfg_ht_bw : %d Info.cfg_vht_bw : %d !!\n",
		Info->channel, Info->cfg_ht_bw, Info->cfg_vht_bw);
	return 0;
}
#endif /* OFFCHANNEL_SCAN_FEATURE && ZERO_PKT_LOSS_SUPPORT */

#ifdef TR181_SUPPORT
static int mtk_cfg80211_reply_channel_last_change(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	UINT32 Time, *ChannelLastChange;
	ULONG TNow;
	struct hdev_ctrl *ctrl = NULL;

	if (ctrl_data->rsp_data_len < sizeof(UINT32))
		return -RTMP_IO_EINVAL;

	ctrl = (struct hdev_ctrl *)mac_ad->hdev_ctrl;
	if (!ctrl) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
			"hdev_ctrl is invalid.\n");
		return -EINVAL;
	}
	ctrl_data->rsp_data_len = sizeof(UINT32);
	ChannelLastChange = ctrl_data->rsp_data;

	NdisGetSystemUpTime(&TNow);
	Time = jiffies_to_usecs(TNow);
	*ChannelLastChange = Time - ctrl->rdev.pRadioCtrl->CurChannelUpTime;

	return 0;
}

static int mtk_cfg80211_reply_max_num_of_ssids(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	UCHAR MaxSupportedSSIDs, *data_ptr = ctrl_data->rsp_data;

	MaxSupportedSSIDs = MAX_MBSSID_NUM(mac_ad);
	if (ctrl_data->rsp_data_len)
		data_ptr[0] = MaxSupportedSSIDs;
	else
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = sizeof(MaxSupportedSSIDs);
	return 0;
}

static int mtk_cfg80211_reply_extension_channel(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int apidx;
	UCHAR extensionChannel, *data_ptr = ctrl_data->rsp_data;

	apidx = wdev->func_idx;
	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
		"apidx is %d.\n", apidx);

	extensionChannel = wlan_config_get_ext_cha(wdev);

	if (ctrl_data->rsp_data_len)
		data_ptr[0] = extensionChannel;
	else
		return -RTMP_IO_EINVAL;
	ctrl_data->rsp_data_len = sizeof(extensionChannel);
	return 0;
}
#endif /* TR181_SUPPORT */

#ifdef ANDLINK_V4_0
INT	mtk_cfg80211_andlink_get_uplink_info(
	RTMP_ADAPTER *pAd, INT apidx, void *data)
{
	struct wifi_dev *wdev =  NULL;
	struct mtk_wifi_uplink_info *andlink_uplink_info = (struct mtk_wifi_uplink_info *)data;
	INT idx = 0, if_idx = 0;

	MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR, "==>\n");
	/*init andlink_uplink_info*/
	wdev = &pAd->StaCfg[apidx].wdev;
	if (!wdev || !(HcGetBandByWdev(wdev) < CFG_WIFI_RAM_BAND_NUM) ||
		pAd->CommonCfg.andlink_enable != TRUE) {
		MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"wdev is null or andlink enbable(%d) in band(%d).\n",
					pAd->CommonCfg.andlink_enable,
					HcGetBandByWdev(wdev));
		return -RTMP_IO_EFAULT;
	}

	/*find apcli entry*/
	if (((GetAssociatedAPByWdev(pAd, wdev)) != NULL) && (pAd->StaCfg[apidx].SsidLen != 0)) {
		for (idx = 0; VALID_UCAST_ENTRY_WCID(pAd, idx); idx++) {
			PMAC_TABLE_ENTRY pEntry = &pAd->MacTab->Content[idx];

			if (pEntry->wdev != &pAd->StaCfg[apidx].wdev)
				continue;

			if (IS_ENTRY_PEER_AP(pEntry) && (pEntry->Sst == SST_ASSOC)) {
				PMAC_TABLE_ENTRY apcli_entry = NULL;
				mtk_rate_info_t tx_rate;
				mtk_rate_info_t rx_rate;

				if (pAd->StaCfg[apidx].SsidLen > 0)
					MTWF_DBG(pAd,
					DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
					"ssid %s\n", pAd->StaCfg[apidx].Ssid);

				NdisMoveMemory(&andlink_uplink_info->ssid, pAd->StaCfg[apidx].Ssid,
				pAd->StaCfg[apidx].SsidLen > MAX_LEN_OF_SSID
					? MAX_LEN_OF_SSID : pAd->StaCfg[apidx].SsidLen);
				/*channel*/
				andlink_uplink_info->channel = pAd->StaCfg[apidx].wdev.channel;
				if ((apidx >= 0)
					&& IS_WCID_VALID(pAd, pAd->StaCfg[apidx].MacTabWCID))
					apcli_entry =
					&pAd->MacTab->Content[pAd->StaCfg[apidx].MacTabWCID];
				if (apcli_entry) {
					/*rssi*/
					andlink_uplink_info->rssi =
						RTMPMaxRssi(pAd,
							apcli_entry->RssiSample.AvgRssi[0],
							apcli_entry->RssiSample.AvgRssi[1],
							apcli_entry->RssiSample.AvgRssi[2]);
					/*noise*/
					andlink_uplink_info->noise =
						RTMPMaxRssi(pAd,
							pAd->ApCfg.RssiSample.AvgRssi[0],
							pAd->ApCfg.RssiSample.AvgRssi[1],
							pAd->ApCfg.RssiSample.AvgRssi[2]) -
						RTMPMinSnr(pAd,
							pAd->ApCfg.RssiSample.AvgSnr[0],
							pAd->ApCfg.RssiSample.AvgSnr[1]);

					/*snr*/
					andlink_uplink_info->snr =
						andlink_uplink_info->rssi - andlink_uplink_info->noise;
					/*broadcast and multicast*/
					if (apcli_entry->pMbss) {
						andlink_uplink_info->mcBytesTx =
							MBSS_GET(pEntry->pMbss)->mcBytesTx;
						andlink_uplink_info->bcBytesTx =
							MBSS_GET(pEntry->pMbss)->bcBytesTx;
					}
				} else {
					MTWF_DBG(pAd,
						DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
						"apcli_entry null\n");
				}
				if (get_sta_rate_info(pAd, pEntry, &tx_rate, &rx_rate) == TRUE) {
					/*tx/rx_rate*/
					andlink_uplink_info->tx_rate = (UINT)tx_rate.legacy;
					andlink_uplink_info->rx_rate = (UINT)rx_rate.legacy;
				}

				/*tx/rx_rate_rt Mbps*/
				andlink_uplink_info->tx_rate_rt = (UINT32)pEntry->OneSecTxBytes;
				andlink_uplink_info->rx_rate_rt = (UINT32)pEntry->OneSecRxBytes;
				for (if_idx = 0; if_idx < ANDLINK_IF_MAX; if_idx++) {
					/*avg_tx/rx_rt*/
					andlink_uplink_info->avg_tx_rate[if_idx] =
						pEntry->andlink_avg_tx_rate[if_idx] / 128;
					andlink_uplink_info->avg_rx_rate[if_idx] =
						pEntry->andlink_avg_rx_rate[if_idx] / 128;
					/*max_tx/rx_rt*/
					andlink_uplink_info->max_tx_rate[if_idx] =
						pEntry->andlink_max_tx_rate[if_idx] / 128;
					andlink_uplink_info->max_rx_rate[if_idx] =
						pEntry->andlink_max_rx_rate[if_idx] / 128;
					MTWF_DBG(pAd, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_INFO,
						"if_idx=%d avg_tx_rate=%llu avg_rx_rate=%llu max_tx_rate=%llu max_rx_rate=%llu\n",
						if_idx, pEntry->andlink_avg_tx_rate[if_idx],
						pEntry->andlink_avg_rx_rate[if_idx],
						pEntry->andlink_max_tx_rate[if_idx],
						pEntry->andlink_max_rx_rate[if_idx]);
				}
			}
			break;
		}
	}

	return 0;
}

INT	mtk_cfg80211_reply_andlink_uplink_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct mtk_wifi_uplink_info *andlink_uplink_info;
	int apidx = wdev->func_idx;

	if (ctrl_data->rsp_data_len < sizeof(struct mtk_wifi_uplink_info))
		return -RTMP_IO_EINVAL;

	ctrl_data->rsp_data_len = sizeof(struct mtk_wifi_uplink_info);
	andlink_uplink_info = (struct mtk_wifi_uplink_info *)ctrl_data->rsp_data;
	mtk_cfg80211_andlink_get_uplink_info(mac_ad, apidx, &andlink_uplink_info);
	return 0;
}

INT	mtk_cfg80211_reply_andlink_scan_result_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	INT apidx = 0, ret = 0, offset = 0;
	INT status = NDIS_STATUS_SUCCESS;
	struct mtk_wifi_scan_info *andlink_scan_info_recev = NULL;
	struct mtk_wifi_scan_info *andlink_scan_info = NULL;

	apidx = wdev->func_idx;
	if (!VALID_MBSS(mac_ad, apidx)) {
		MTWF_DBG(mac_ad, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN,
					"VALID_MBSS\n");
		return -RTMP_IO_EFAULT;
	}
	if (ctrl_data->rsp_data_len < sizeof(struct mtk_wifi_scan_info))
		return -RTMP_IO_ENOMEM;
	andlink_scan_info = (struct mtk_wifi_scan_info *)ctrl_data->rsp_data;
	andlink_scan_info_recev = (struct mtk_wifi_scan_info *)ctrl_data->data;
	offset = andlink_scan_info_recev->offset;
	MTWF_DBG(mac_ad, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_WARN, "offset(%d).\n", offset);
	ret = mtk_cfg80211_andlink_get_scan_result(mac_ad, apidx, andlink_scan_info, offset);
	if (ret != 0) {
		MTWF_DBG(mac_ad, DBG_CAT_ANDLINK, CATANDLINK_CFG, DBG_LVL_ERROR,
			"error:reply scan result fail!\n");
		ctrl_data->rsp_data_len = 0;
	} else
		ctrl_data->rsp_data_len = sizeof(struct mtk_wifi_scan_info);
	return ret;
}

#endif /* ANDLINK_V4_0 */

#ifdef CONFIG_6G_AFC_SUPPORT
static int mtk_cfg80211_reply_afc_data(
	struct _RTMP_ADAPTER *mac_ad,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int ret = 0;

	ret = nl80211_afc_daemon_channel_info(mac_ad, ctrl_data);
	if (ret != 0)
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_AFC, DBG_LVL_ERROR,
			"reply_msg fail!\n");
	return ret;
}
#endif /* CONFIG_6G_AFC_SUPPORT */

UCHAR ac_queue_map[] = {
	TxQ_IDX_AC1, /* ACI:0 AC_BE */
	TxQ_IDX_AC0, /* ACI:1 AC_BK */
	TxQ_IDX_AC2, /* ACI:2 AC_VI */
	TxQ_IDX_AC3, /* ACI:3 AC_VO */
};

static INT mtk_cfg80211_Set_APAifsn_Proc(
	struct _RTMP_ADAPTER *pAd, UCHAR *aifsn_value)
{
	UCHAR i;
	UCHAR wmm_idx = 0;
	UCHAR ac_index;
	INT success = 0;
	struct wifi_dev *wdev = NULL;
	EDCA_PARM *wmm_edca_param = NULL;
#ifdef CONFIG_AP_SUPPORT
	POS_COOKIE	pObj = (POS_COOKIE)pAd->OS_Cookie;
	UINT ap_idx = pObj->ioctl_if;

	if (ap_idx >= MAX_BEACON_NUM) {
		MTWF_PRINT("invalid mbss id(%d)\n", ap_idx);
		return -RTMP_IO_EFAULT;
	}
	wdev = &pAd->ApCfg.MBSSID[ap_idx].wdev;
#endif
	if (wdev == NULL) {
		MTWF_PRINT("wdev is NULL\n");
		return -RTMP_IO_EFAULT;
	}
	wmm_idx = HcGetWmmIdx(pAd, wdev);
	wmm_edca_param = hwifi_get_edca(pAd, wdev);

	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		if ((aifsn_value[i] < 1) || (aifsn_value[i] > 15)) {
			MTWF_PRINT(
			"APAifsn[%d]=%d is invalid, APAifsn should be configured from 1 to 15\n",
			i, aifsn_value[i]);
			return -RTMP_IO_EFAULT;
		}
	}

	MTWF_PRINT("start to configure APAifsn\n");
	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Aifsn[i] = aifsn_value[i];
		if (wmm_edca_param)
			wmm_edca_param->Aifsn[i] = aifsn_value[i];
		MTWF_PRINT("APAifsn[%d]=%d\n",
			i, pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Aifsn[i]);
		ac_index = ac_queue_map[i];
		if (AsicSetWmmParam(pAd, wdev, wmm_idx, i,
			WMM_PARAM_AIFSN, aifsn_value[i]) == NDIS_STATUS_FAILURE) {
			success = -RTMP_IO_EFAULT;
			MTWF_PRINT("fails to set APAifsn[%d]\n", i);
		}
	}

	return success;
}

static INT mtk_cfg80211_Set_APCwmin_Proc(
	struct _RTMP_ADAPTER *pAd, UCHAR *cwmin_value)
{
	UCHAR i;
	UCHAR wmm_idx = 0;
	UCHAR ac_index;
	INT success = 0;
	struct wifi_dev *wdev = NULL;
	EDCA_PARM *wmm_edca_param = NULL;
#ifdef CONFIG_AP_SUPPORT
	POS_COOKIE	pObj = (POS_COOKIE)pAd->OS_Cookie;
	UINT ap_idx = pObj->ioctl_if;

	if (ap_idx >= MAX_BEACON_NUM) {
		MTWF_PRINT("invalid mbss id(%d)\n", ap_idx);
		return -RTMP_IO_EFAULT;
	}
	wdev = &pAd->ApCfg.MBSSID[ap_idx].wdev;
#endif
	if (wdev == NULL) {
		MTWF_PRINT("wdev is NULL\n");
		return -RTMP_IO_EFAULT;
	}
	wmm_idx = HcGetWmmIdx(pAd, wdev);
	wmm_edca_param = hwifi_get_edca(pAd, wdev);

	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		if ((cwmin_value[i] > 16)) {
			MTWF_PRINT("APCwmin[%d]=%d is invalid, APCwmin should not be over 16\n",
				i, cwmin_value[i]);
			return -RTMP_IO_EFAULT;
		}
	}

	MTWF_PRINT("start to configure APCwmin\n");
	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Cwmin[i] = cwmin_value[i];
		if (wmm_edca_param)
			wmm_edca_param->Cwmin[i] = cwmin_value[i];
		MTWF_PRINT("APCwmin[%d]=%d\n",
			i, pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Cwmin[i]);
		ac_index = ac_queue_map[i];
		if (AsicSetWmmParam(pAd, wdev, wmm_idx, i,
			WMM_PARAM_CWMIN, cwmin_value[i]) == NDIS_STATUS_FAILURE) {
			success = -RTMP_IO_EFAULT;
			MTWF_PRINT("fails to set APCwmin[%d]\n", i);
		}
	}

	return success;
}

static INT mtk_cfg80211_Set_APCwmax_Proc(
	struct _RTMP_ADAPTER *pAd, UCHAR *cwmax_value)
{
	UCHAR i;
	UCHAR wmm_idx = 0;
	UCHAR ac_index;
	INT success = 0;
	struct wifi_dev *wdev = NULL;
	EDCA_PARM *wmm_edca_param = NULL;
#ifdef CONFIG_AP_SUPPORT
	POS_COOKIE	pObj = (POS_COOKIE)pAd->OS_Cookie;
	UINT ap_idx = pObj->ioctl_if;

	if (ap_idx >= MAX_BEACON_NUM) {
		MTWF_PRINT("invalid mbss id(%d)\n", ap_idx);
		return -RTMP_IO_EFAULT;
	}
	wdev = &pAd->ApCfg.MBSSID[ap_idx].wdev;
#endif
	if (wdev == NULL) {
		MTWF_PRINT("wdev is NULL\n");
		return -RTMP_IO_EFAULT;
	}
	wmm_idx = HcGetWmmIdx(pAd, wdev);
	wmm_edca_param = hwifi_get_edca(pAd, wdev);

	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		if ((cwmax_value[i] > 16)) {
			MTWF_PRINT("APCwmax[%d]=%d is invalid, APCwmax should not be over 16\n",
				i, cwmax_value[i]);
			return -RTMP_IO_EFAULT;
		}
	}

	MTWF_PRINT("start to configure APCwmax\n");
	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Cwmax[i] = cwmax_value[i];
		if (wmm_edca_param)
			wmm_edca_param->Cwmax[i] = cwmax_value[i];
		MTWF_PRINT("APCwmax[%d]=%d\n",
			i, pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Cwmax[i]);
		ac_index = ac_queue_map[i];
		if (AsicSetWmmParam(pAd, wdev, wmm_idx, i,
			WMM_PARAM_CWMAX, cwmax_value[i]) == NDIS_STATUS_FAILURE) {
			success = -RTMP_IO_EFAULT;
			MTWF_PRINT("fails to set APCwmax[%d]\n", i);
		}
	}

	return success;
}

static INT mtk_cfg80211_Set_APTxop_Proc(
	struct _RTMP_ADAPTER *pAd, UCHAR *txop_value)
{
	UCHAR i;
	UCHAR wmm_idx = 0;
	UCHAR ac_index;
	INT success = 0;
	struct wifi_dev *wdev = NULL;
	EDCA_PARM *wmm_edca_param = NULL;
#ifdef CONFIG_AP_SUPPORT
	POS_COOKIE	pObj = (POS_COOKIE)pAd->OS_Cookie;
	UINT ap_idx = pObj->ioctl_if;

	if (ap_idx >= MAX_BEACON_NUM) {
		MTWF_PRINT("invalid mbss id(%d)\n", ap_idx);
		return -RTMP_IO_EFAULT;
	}
	wdev = &pAd->ApCfg.MBSSID[ap_idx].wdev;
#endif
	if (wdev == NULL) {
		MTWF_PRINT("wdev is NULL\n");
		return -RTMP_IO_EFAULT;
	}
	wmm_idx = HcGetWmmIdx(pAd, wdev);
	wmm_edca_param = hwifi_get_edca(pAd, wdev);

	MTWF_PRINT("start to configure APTxop\n");
	for (i = 0; i < WMM_NUM_OF_AC; i++) {
		pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Txop[i] = txop_value[i];
		if (wmm_edca_param)
			wmm_edca_param->Txop[i] = txop_value[i];
		MTWF_PRINT("APTxop[%d]=%d\n", i, pAd->CommonCfg.APEdcaParm[wdev->EdcaIdx].Txop[i]);
		ac_index = ac_queue_map[i];
		if (AsicSetWmmParam(pAd, wdev, wmm_idx, i,
			WMM_PARAM_TXOP, txop_value[i]) == NDIS_STATUS_FAILURE) {
			success = -RTMP_IO_EFAULT;
			MTWF_PRINT("fails to set APTxop[%d]\n", i);
		}
	}

	return success;
}

static INT mtk_cfg80211_set_wmm_cap(
	struct _RTMP_ADAPTER *pAd, BOOLEAN bWmmCapable)
{
	POS_COOKIE	pObj = (POS_COOKIE)pAd->OS_Cookie;
	struct wifi_dev *wdev;
	UINT ifIndex = pObj->ioctl_if;

	if (ifIndex >= MAX_BEACON_NUM) {
		MTWF_PRINT("invalid ioctl_if %d\n", ifIndex);
		return -RTMP_IO_EINVAL;
	}

	wdev = &pAd->ApCfg.MBSSID[ifIndex].wdev;

	if (bWmmCapable == 1)
		wdev->bWmmCapable = TRUE;
	else if (bWmmCapable == 0)
		wdev->bWmmCapable = FALSE;
	else
		return -RTMP_IO_EINVAL;  /*Invalid argument */

	pAd->ApCfg.MBSSID[ifIndex].bWmmCapableOrg =
		pAd->ApCfg.MBSSID[ifIndex].wdev.bWmmCapable;

#ifdef DOT11_N_SUPPORT
	/*Sync with the HT relate info. In N mode, we should re-enable it */
	SetCommonHtVht(pAd, wdev);
#endif /* DOT11_N_SUPPORT */
	MTWF_PRINT("IF(ra%d) (bWmmCapable=%d)\n", ifIndex, wdev->bWmmCapable);
	return 0;
}

#ifdef AIR_MONITOR
static INT mtk_cfg80211_set_monitor_enable(
	struct _RTMP_ADAPTER *pAd, unsigned char monitor_enable)
{
	UCHAR band_idx;
	INT ret;

	pAd->MntEnable = monitor_enable;
	band_idx = hc_get_hw_band_idx(pAd);
	ret = asic_set_air_mon_enable(pAd, monitor_enable, band_idx);
	if (ret != TRUE)
		return -RTMP_IO_EFAULT;

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
		"[Band%d]MntEnable=0x%0x\n", band_idx, monitor_enable);
	return 0;
}

static INT mtk_cfg80211_set_monitor_rule(
	struct _RTMP_ADAPTER *pAd, unsigned char *rx_rule)
{
	UCHAR band_idx;
	INT ret;

	band_idx = hc_get_hw_band_idx(pAd);
	ret = asic_set_air_mon_rule(pAd, rx_rule, band_idx);
	if (ret != TRUE)
		return -RTMP_IO_EFAULT;

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
		"[Band%d]MntRule=%d:%d:%d\n", band_idx, rx_rule[0], rx_rule[1], rx_rule[2]);
	return 0;
}

static INT mtk_cfg80211_set_monitor_target(
	struct _RTMP_ADAPTER *pAd, unsigned char *mac_addr)
{
	NdisCopyMemory(pAd->curMntAddr, mac_addr, MAC_ADDR_LEN);
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
		"MntTarget=%02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(MAC2STR(pAd->curMntAddr)));
	return 0;
}

static INT mtk_cfg80211_set_monitor_index(
	struct _RTMP_ADAPTER *pAd, unsigned char mnt_idx)
{
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	UCHAR band_idx;
	INT ret;
	struct wifi_dev *wdev = NULL;

	wdev = get_wdev_by_ioctl_idx_and_iftype(pAd, pObj->ioctl_if, pObj->ioctl_if_type);
	if (!wdev)
		return -RTMP_IO_EINVAL;

	band_idx = hc_get_hw_band_idx(pAd);
	ret = asic_set_air_mon_idx(pAd, wdev, mnt_idx, band_idx);
	if (ret != TRUE)
		return -RTMP_IO_EFAULT;

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
		"[Band%d]MntIdx=%d\n", band_idx, mnt_idx);
	return 0;
}

static INT mtk_cfg80211_set_monitor_clear(
	struct _RTMP_ADAPTER *pAd)
{
	UCHAR i;
	MNT_STA_ENTRY *pEntry = NULL;

	for (i = 0; i < MAX_NUM_OF_MONITOR_STA; i++) {
		pEntry = pAd->MntTable + i;
		pEntry->data_cnt = 0;
		pEntry->mgmt_cnt = 0;
		pEntry->cntl_cnt = 0;
		pEntry->Count = 0;
		NdisZeroMemory(&pEntry->RssiSample, sizeof(RSSI_SAMPLE));
	}
	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
		"Clear all Monitor STA Entry\n");
	return 0;
}

static INT mtk_cfg80211_set_monitor_sta_index(
	struct _RTMP_ADAPTER *pAd,
	struct mnt_sta *req_sta)
{
	UCHAR sta_mac[MAC_ADDR_LEN] = {0};
	UCHAR sta_idx = 0;
	INT ret = -RTMP_IO_EINVAL;

	NdisCopyMemory(sta_mac, req_sta->sta_mac, MAC_ADDR_LEN);
	sta_idx = req_sta->sta_id;

	mtk_cfg80211_set_monitor_target(pAd, sta_mac);
	if (sta_idx < MAX_NUM_OF_MONITOR_STA)
		ret = mtk_cfg80211_set_monitor_index(pAd, sta_idx);
	else {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_ERROR,
				 "The index is over the maximum limit.\n");
		ret = -RTMP_IO_EINVAL;
	}
	return ret;
}

static INT mtk_cfg80211_get_air_mnt_result(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	int ret = 0;
	unsigned int air_mnt_len = 0;
	wapp_mnt_info *mnt_info = NULL;
	MNT_STA_ENTRY *pTmpEntry = NULL;
	UCHAR band_idx = 0;
	UCHAR i;
	UINT32 pEntryLen = 0;

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE, "GET:: AIR MONITOR REQ\n");

	air_mnt_len = (unsigned int) (MAX_NUM_OF_MONITOR_STA * sizeof(wapp_mnt_info));
	if (ctrl_data->rsp_data_len < air_mnt_len)
		return -RTMP_IO_ENOMEM;
	band_idx = hc_get_hw_band_idx(pAd);
	mnt_info = (wapp_mnt_info *)ctrl_data->rsp_data;
	if (mnt_info == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_ERROR,
				"Query::OID_GET_AIR_MONITOR_RESULT alloc memory failed!\n");
		return -RTMP_IO_EFAULT;
	}
	NdisZeroMemory(mnt_info, MAX_NUM_OF_MONITOR_STA * sizeof(wapp_mnt_info));

	for (i = 0; i < MAX_NUM_OF_MONITOR_STA; i++) {
		pTmpEntry = &pAd->MntTable[i];
		if (pTmpEntry->Count != 0 && pTmpEntry->Count < pAd->MntMaxPktCnt) {
			NdisCopyMemory(mnt_info[pEntryLen].sta_addr, pTmpEntry->addr, MAC_ADDR_LEN);
			mnt_info[pEntryLen].rssi = (char)(pTmpEntry->RssiTotal/(LONG)pTmpEntry->Count);
			MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
				"STA ["MACSTR"] packet received %ld total rssi %ld avg rssi %d\n",
				MAC2STR(pTmpEntry->addr),
				pTmpEntry->Count,
				pTmpEntry->RssiTotal,
				mnt_info[pEntryLen].rssi);
			pEntryLen += 1;
		}
	}
	ctrl_data->rsp_data_len = air_mnt_len;
	return ret;
}

static INT mtk_cfg80211_get_air_mnt_sta_stats(
	struct _RTMP_ADAPTER *pAd,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	INT ret = 0;
	UCHAR i = 0, bidx = 0;
	UINT32 pEntryLen = 0;
	unsigned int air_mnt_len = 0;
	wapp_mnt_info *mnt_info = NULL;
	MNT_STA_ENTRY *pEntry = NULL;

	air_mnt_len = (unsigned int) (MAX_NUM_OF_MONITOR_STA * sizeof(wapp_mnt_info));
	if (ctrl_data->rsp_data_len < air_mnt_len)
		return -RTMP_IO_ENOMEM;
	bidx = hc_get_hw_band_idx(pAd);
	mnt_info = (wapp_mnt_info *)ctrl_data->rsp_data;
	if (mnt_info == NULL) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_ERROR,
				"Query::OID_GET_AIR_MONITOR_RESULT alloc memory failed!\n");
		return -RTMP_IO_EFAULT;
	}

	NdisZeroMemory(mnt_info, MAX_NUM_OF_MONITOR_STA * sizeof(wapp_mnt_info));
	MTWF_PRINT("  Monitor Enable: BAND%d: %d\n", bidx, pAd->MntEnable);
	MTWF_PRINT("  Index last set: BAND%d: %d\n", bidx, pAd->MntIdx);
	MTWF_PRINT("  BAND%d Count: %d\n", bidx, pAd->MonitrCnt);

	for (i = 0; i < MAX_NUM_OF_MONITOR_STA; i++) {
		pEntry = &pAd->MntTable[i];
		MTWF_PRINT("pEntry->bValid %d:\n", pEntry->bValid);
		if (pEntry->bValid) {
			MTWF_PRINT("Band%d: Monitor STA[%d]\t", pEntry->Band, i);
			MTWF_PRINT(MACSTR"\t", MAC2STR(pEntry->addr));
			MTWF_PRINT("[DATA]=%08lu\t", pEntry->data_cnt);
			MTWF_PRINT("[MGMT]=%08lu\t", pEntry->mgmt_cnt);
			MTWF_PRINT("[CNTL]=%08lu\t", pEntry->cntl_cnt);
			MTWF_PRINT("[TOTAL]=%08lu\t", pEntry->Count);
			MTWF_PRINT("RSSI:%d,%d,%d,%d\n",
			pEntry->RssiSample.AvgRssi[0], pEntry->RssiSample.AvgRssi[1],
			pEntry->RssiSample.AvgRssi[2], pEntry->RssiSample.AvgRssi[3]);
			pEntry->RssiTotal =
				(pEntry->RssiSample.AvgRssi[0] + pEntry->RssiSample.AvgRssi[1] +
				pEntry->RssiSample.AvgRssi[2] + pEntry->RssiSample.AvgRssi[3]);
			MTWF_PRINT("[TOtal Rssi]=%ld\t", pEntry->RssiTotal);
			NdisCopyMemory(mnt_info[pEntryLen].sta_addr, pEntry->addr, MAC_ADDR_LEN);
			mnt_info[pEntryLen].rssi = (char)(pEntry->RssiTotal/4);
			pEntryLen += 1;
		}
	}

	MTWF_DBG(pAd, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
			"RUDRA GET:: AIR MONITOR REQ_33 air_mnt_len[%d]\n", air_mnt_len);
	ctrl_data->rsp_data_len = air_mnt_len;
	return ret;
}

#endif /* AIR_MONITOR */

static INT mtk_cfg80211_set_ht_ba_wsize(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, USHORT ba_wsize)
{
	UINT16 max_ba_wsize = 0;

	max_ba_wsize = ba_get_default_max_ba_wsize(wdev, pAd);

	if (ba_wsize == 0 || ba_wsize > BA_WIN_SZ_1024)
		return -RTMP_IO_EINVAL;

	wlan_config_set_ba_tx_wsize(wdev, ba_wsize, max_ba_wsize);
	SetCommonHtVht(pAd, wdev);

	MTWF_PRINT("(HtBaWinSize=%d)\n", ba_wsize);
	return 0;
}

static INT mtk_cfg80211_set_ht_auto_ba(
	struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, UCHAR ba_en)
{
	wlan_config_set_ba_enable(wdev, ba_en);
	SetCommonHtVht(pAd, wdev);
	MTWF_PRINT("(HtAutoBa=%d)\n", ba_en);
	return 0;
}

static INT mtk_cfg80211_set_ba_setup(
	struct _RTMP_ADAPTER *pAd, unsigned char *mac_addr, UCHAR tid)
{
	MAC_TABLE_ENTRY *pEntry = NULL;
	STA_TR_ENTRY *tr_entry = NULL;
	struct BA_INFO *ba_info;

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	pEntry = MacTableLookup(pAd, mac_addr);
#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	pEntry = MacTableLookup2(pAd, mac_addr, NULL);
#endif

	if (!IS_VALID_ENTRY(pEntry)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_WARN,
			"%s():Can't find STA, MAC("MACSTR"), TID = %d\n",
			__func__, MAC2STR(mac_addr), tid);
		return -RTMP_IO_EINVAL;
	}

	tr_entry = tr_entry_get(pAd, pEntry->wcid);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_NOTICE,
		"%s():Setup Ori BA session: TID = %d, MAC("MACSTR")\n",
		__func__, tid, MAC2STR(mac_addr));

	ba_info = &pEntry->ba_info;

#ifdef DOT11_EHT_BE
	mt_os_lockless_enter(os_lockless_ctx_get());

	if (IS_ENTRY_MLO(pEntry)) {
		struct mld_entry_t *mld_entry;

		mld_entry = mt_os_lockless_access_pointer(os_lockless_ctx_get(), pEntry->mld_entry);
		if (!mld_entry) {
			mt_os_lockless_exit(os_lockless_ctx_get());
			return -EINVAL;
		}
		ba_info = &mld_entry->ba_info;
	}
#endif /* DOT11_EHT_BE */

	ba_info->AutoTest = TRUE;
	ba_ori_session_start(pAd, tr_entry->wcid, tid);

#ifdef DOT11_EHT_BE
	mt_os_lockless_exit(os_lockless_ctx_get());
#endif
	return 0;
}

static INT mtk_cfg80211_set_ba_ori_teardown(
	struct _RTMP_ADAPTER *pAd, unsigned char *mac_addr, UCHAR tid)
{
	MAC_TABLE_ENTRY *pEntry = NULL;
	struct BA_INFO *ba_info;
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	pEntry = MacTableLookup(pAd, (PUCHAR) mac_addr);
#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	pEntry = MacTableLookup2(pAd, (PUCHAR) mac_addr, NULL);
#endif

	if (!IS_VALID_ENTRY(pEntry)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_WARN,
			"%s():Can't find STA, MAC("MACSTR"), TID = %d\n",
			__func__, MAC2STR(mac_addr), tid);
		return -RTMP_IO_EINVAL;
	}

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_NOTICE,
		"%s():Tear down Ori BA session: TID = %d, MAC("MACSTR")\n",
		__func__, tid, MAC2STR(mac_addr));

	ba_info = &pEntry->ba_info;

#ifdef DOT11_EHT_BE
	mt_os_lockless_enter(os_lockless_ctx_get());

	if (IS_ENTRY_MLO(pEntry)) {
		struct mld_entry_t *mld_entry;

		mld_entry = mt_os_lockless_access_pointer(os_lockless_ctx_get(), pEntry->mld_entry);
		if (!mld_entry) {
			mt_os_lockless_exit(os_lockless_ctx_get());
			return FALSE;
		}
		ba_info = &mld_entry->ba_info;
	}
#endif /* DOT11_EHT_BE */

	ba_info->AutoTest = FALSE;
	ba_ori_session_tear_down(pAd, pEntry->wcid, tid, FALSE);

#ifdef DOT11_EHT_BE
	mt_os_lockless_exit(os_lockless_ctx_get());
#endif
	return 0;
}

static INT mtk_cfg80211_set_ba_rec_teardown(
	struct _RTMP_ADAPTER *pAd, unsigned char *mac_addr, UCHAR tid)
{
	MAC_TABLE_ENTRY *pEntry = NULL;
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	pEntry = MacTableLookup(pAd, (PUCHAR) mac_addr);
#endif
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	pEntry = MacTableLookup2(pAd, (PUCHAR) mac_addr, NULL);
#endif


	if (pEntry) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_NOTICE,
			"%s():Tear down Rec BA session: TID = %d, MAC("MACSTR")\n",
			__func__, tid, MAC2STR(mac_addr));
		ba_rec_session_tear_down(pAd, pEntry->wcid, tid, FALSE);
	} else {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_WARN,
			"%s():Can't find STA, MAC("MACSTR"), TID = %d\n",
			__func__, MAC2STR(mac_addr), tid);
		return -EINVAL;
	}
	return 0;
}

static int cfg80211_drv_vndr_show_handler(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	char *cmd_string;
	RTMP_STRING *msg = NULL;
	int msg_len = 0;
	int free_len;
	char *tmp = NULL;
	RTMP_STRING *this_char, *value = NULL;
	int ret;
	int status = NDIS_STATUS_SUCCESS;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	cmd_string = (char *)ctrl_data->data;
	msg = ctrl_data->rsp_data;
	free_len = ctrl_data->rsp_data_len;
	tmp = cmd_string;
	while ((this_char = strsep((char **)&tmp, ",")) != NULL) {
		if (!*this_char)
			continue;

		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
			"Before check, this_char=%s\n", this_char);
		value = strchr(this_char, '=');

		if (value) {
			if (strlen(value) > 1) {
				*value = 0;
				value++;
			} else
				value = NULL;
		}

		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
			"After check, this_char=%s, value=%s\n",
			this_char, (value == NULL ? "" : value));

		for (PRTMP_PRIVATE_AP_SHOW_PROC = RTMP_PRIVATE_AP_SHOW_SUPPORT_PROC;
			PRTMP_PRIVATE_AP_SHOW_PROC->name;
			PRTMP_PRIVATE_AP_SHOW_PROC++) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
				"PRTMP_PRIVATE_AP_SHOW_PROC->name=%s\n",
				PRTMP_PRIVATE_AP_SHOW_PROC->name);
			if (rtstrcasecmp(this_char, PRTMP_PRIVATE_AP_SHOW_PROC->name) == TRUE) {
				if (value) {
					ret = snprintf(msg + msg_len, free_len, value);
					if (os_snprintf_error(free_len, ret)) {
						MTWF_DBG(mac_ad,
							DBG_CAT_CFG80211,
							CATCFG80211_VNDR, DBG_LVL_WARN,
							"msg snprintf error!\n");
						status = -RTMP_IO_EINVAL;
						break;
					}
					msg_len += ret;
					free_len -= ret;
				}

				if (!PRTMP_PRIVATE_AP_SHOW_PROC->set_proc(mac_ad, msg)) {
					/*FALSE:Set private failed then return Invalid argument */
					status = -RTMP_IO_EINVAL;
				}
				break;  /*Exit for loop. */
			}
		}

		if (PRTMP_PRIVATE_AP_SHOW_PROC->name == NULL) {
			/*Not found argument */
			status = -RTMP_IO_EINVAL;
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"nl80211::(mwctl) Command not Support [%s=%s]\n", this_char, value);
			break;
		}

		msg_len = strlen(msg);
	}

	ctrl_data->rsp_data_len = msg_len;
	return status;
}

static int cfg80211_drv_vndr_set_handler(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	char *cmd_string;
	RTMP_STRING *this_char, *value;
	UCHAR *tmp = NULL;
	INT status = NDIS_STATUS_SUCCESS;
	struct PRIV_SET_PROC *SET_PROC;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"mac_ad not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	cmd_string = (char *)ctrl_data->data;
	tmp = cmd_string;
	while ((this_char = strsep((char **)&tmp, "\0")) != NULL) {
		if (!*this_char)
			continue;
		value = strchr(this_char, '=');

		if (value != NULL)
			*value++ = 0;

		if (!value
#ifdef WSC_AP_SUPPORT
			&& (
				(strcmp(this_char, "WscStop") != 0) &&
				(strcmp(this_char, "ser") != 0) &&
				(strcmp(this_char, "WscGenPinCode") != 0)
			)
#endif /* WSC_AP_SUPPORT */
		   )
			continue;

		for (SET_PROC = RTMP_PRIVATE_AP_SUPPORT_PROC; SET_PROC->name; SET_PROC++) {
			if (rtstrcasecmp(this_char, SET_PROC->name) == TRUE) {
				if (!SET_PROC->set_proc(mac_ad, value)) {
					/*FALSE:Set private failed then return Invalid argument */
					status = -RTMP_IO_EINVAL;
				}

				break;  /*Exit for loop. */
			}
		}

		if (SET_PROC->name == NULL) {
			/*Not found argument */
			status = -RTMP_IO_EINVAL;
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_NOTICE,
				"nl80211::(mwctl) Command not Support [%s=%s]\n", this_char,
				 value);
			break;
		}
	}

	return status;
}

static int cfg80211_drv_vndr_mac_handler(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = 0;
	UINT32 IdMac, map_addr, mac_s = 0, mac_e = 0;
	UINT32 macVal = 0, max_len = MT_MAX_VENDOR_MSG_LEN;
	BOOLEAN IsFound;
	RTMP_STRING *msg;
	UINT32 msg_len = ctrl_data->rsp_data_len;
	UINT32 free_len;
	struct mac_param *param; /* defined in mtk_vendor_nl80211.h */

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (max_len > msg_len)
		free_len = (max_len - msg_len);
	else
		goto error;
	msg = (RTMP_STRING *)ctrl_data->rsp_data;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MAC_WRITE_PARAM) {
		param = (struct mac_param *)ctrl_data->data;
		mac_s = param->start;
		macVal = param->value;
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
				"write: dsp_addr=0x%08x, val=0x%08x\n",
				mac_s, macVal);
#ifdef WIFI_UNIFIED_COMMAND
		/* WF_PHY_DSP CR via WM */
		if ((mac_s & 0x86000000) == 0x86000000) {
			RTMP_REG_PAIR dsp_reg;

			dsp_reg.Register = mac_s;
			dsp_reg.Value = macVal;
			UniCmdMultipleMacRegAccessWrite(mac_ad, &dsp_reg, 1);
		} else
#endif
			RTMP_IO_WRITE32(mac_ad->hdev_ctrl, mac_s, macVal);
		map_addr = mac_s;
		IsFound = mt_mac_cr_range_mapping(mac_ad->physical_dev, &map_addr);
		ret = snprintf(msg + msg_len, free_len, "[0x%04x]:%08x  ",
			map_addr, macVal);
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"msg snprintf error!(free_len=%d, ret=%d)\n", free_len, ret);
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_NOTICE,
			"MacAddr=0x%x, MacValue=0x%x, IsRemap=%d\n", map_addr, macVal, !IsFound);
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MAC_SHOW_PARAM) {
		param = (struct mac_param *)ctrl_data->data;
		mac_s = param->start;
		mac_e = param->end;
		for (IdMac = mac_s; IdMac <= mac_e; IdMac += 4) {
#ifdef WIFI_UNIFIED_COMMAND
			/* WF_PHY_DSP CR via WM */
			if ((mac_s & 0x86000000) == 0x86000000) {
				RTMP_REG_PAIR dsp_reg;

				dsp_reg.Register = IdMac;
				dsp_reg.Value = 0;
				UniCmdMultipleMacRegAccessRead(mac_ad, &dsp_reg, 1);
				macVal = dsp_reg.Value;
			} else
#endif
				RTMP_IO_READ32(mac_ad->hdev_ctrl, IdMac, &macVal);

			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
				"read: dsp_addr=0x%08x, val=0x%08x\n",
				IdMac, macVal);
			map_addr = IdMac;
			IsFound = mt_mac_cr_range_mapping(mac_ad->physical_dev, &map_addr);
			ret = snprintf(msg + msg_len, free_len,
				"[0x%04x]:%08x  ", map_addr, macVal);
			if (os_snprintf_error(free_len, ret)) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
					"msg snprintf error!(free_len=%d, ret=%d)\n",
					free_len, ret);
				goto error;
			}
			msg_len += ret;
			free_len -= ret;
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_NOTICE,
				"MacAddr=0x%x, MacValue=0x%x, IsRemap=%d, ret=%d, msg_len=%d\n",
				map_addr, macVal, !IsFound, ret, msg_len);
		}
	} else
		goto error;

	if (msg_len == 0) {
		ret = snprintf(msg, free_len, "===>Error command format!");
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"msg snprintf error!\n");
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
	}
	ctrl_data->rsp_data_len = msg_len;
	return 0;
error:
	return -RTMP_IO_EFAULT;
}

int RTMPGetStatisticsStr(
	struct _RTMP_ADAPTER *pAd, POS_COOKIE pObj, RTMP_STRING *msg, int msg_buf_len);

static int cfg80211_drv_vndr_statistic_handler(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	POS_COOKIE pObj;
	RTMP_STRING *msg;
	UINT max_len = ctrl_data->rsp_data_len;
	INT32 msg_len = 0;
	int ret;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"mac_ad not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}
	msg = (RTMP_STRING *)ctrl_data->rsp_data;
	pObj = (POS_COOKIE)mac_ad->OS_Cookie;
	msg_len = RTMPGetStatisticsStr(mac_ad, pObj, msg, max_len);
	if (msg_len < 0) {
		ret = -RTMP_IO_EFAULT;
		goto error;
	} else if (msg_len == 0) {
		ret = snprintf(msg, max_len, "===>Error Statictics string!");
		if (os_snprintf_error(max_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"msg snprintf error!\n");
			ret = -RTMP_IO_EINVAL;
			goto error;
		}
		msg_len += ret;
	}
	ctrl_data->rsp_data_len = msg_len;
	return 0;
error:
	return ret;
}

#define FREQ_STR_LEN 8
static int mtk_cfg80211_reply_band_info_freqlist(
	struct _RTMP_ADAPTER *mac_ad, char *rsp_data, unsigned int rsp_data_len)
{
	INT Status = 0, ret = 0, i = 0;
	CHANNEL_CTRL *pChCtrl;
	RTMP_STRING *msg = rsp_data;
	UINT32 TotalLen, LeftBufSize;

	pChCtrl = hc_get_channel_ctrl(mac_ad->hdev_ctrl);

	TotalLen = FREQ_STR_LEN * (pChCtrl->FreqListNum + 1);
	if (TotalLen > rsp_data_len)
		return -RTMP_IO_EINVAL;

	for (i = 0; (i < pChCtrl->FreqListNum)
			&& (pChCtrl->FreqListNum <= MAX_NUM_OF_CHANNELS); i++) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_INFO,
				"Freq = %d, channel = %d\n", pChCtrl->FreqList[i],
				frequency_to_channel(pChCtrl->FreqList[i]));

		LeftBufSize = TotalLen - strlen(msg);
		if (i == 0)
			Status = snprintf(msg + strlen(msg),
				LeftBufSize, "%d", frequency_to_channel(pChCtrl->FreqList[i]));
		else
			Status = snprintf(msg + strlen(msg),
				LeftBufSize, ", %d", frequency_to_channel(pChCtrl->FreqList[i]));

		if (os_snprintf_error(LeftBufSize, Status)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
					"Snprintf failed!\n");
			ret = -RTMP_IO_EFAULT;
			break;
		}
	}

	if (ret != 0)
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"reply_msg fail!\n");
	return ret;
}

static int cfg80211_drv_vndr_get_band_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret;
	struct mtk_mac_dev *mac_dev = NULL;
	struct mtk_mac_phy *mac_phy = NULL;
	char *band_str[BAND_SELECT_BAND_MAX] = {"N/A", "2G", "5G", "6G", "5G-Low",
		"5G-High", "6G-Low", "6G-High"};

	switch (ctrl_data->sub_id) {
	case MTK_NL80211_VENDOR_ATTR_GET_BAND_INFO_BAND:
		if (mac_ad->BandSel <= BAND_SELECT_BAND_NONE ||
			mac_ad->BandSel >= BAND_SELECT_BAND_MAX)
			return -RTMP_IO_EFAULT;
		ret = snprintf(ctrl_data->rsp_data, ctrl_data->rsp_data_len,
				"%s", band_str[mac_ad->BandSel]);
		if (os_snprintf_error(ctrl_data->rsp_data_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"msg snprintf error!(free_len=%d, ret=%d)\n",
				ctrl_data->rsp_data_len, ret);
			return -RTMP_IO_EFAULT;
		}
		ctrl_data->rsp_data_len = strlen(band_str[mac_ad->BandSel])+1;
		break;
	case MTK_NL80211_VENDOR_ATTR_GET_BAND_INFO_FREQLIST:
		ret = mtk_cfg80211_reply_band_info_freqlist(
			mac_ad, ctrl_data->rsp_data, ctrl_data->rsp_data_len);
		if (!ret)
			ctrl_data->rsp_data_len = strlen(ctrl_data->rsp_data)+1;
		else
			ctrl_data->rsp_data_len = 0;
		break;
	case MTK_NL80211_VENDOR_ATTR_GET_BAND_INFO_BANDWIDTH:
		mac_dev = hc_get_mac_dev(mac_ad);
		mac_phy = &mac_dev->mac_phy;
		ctrl_data->rsp_data = &mac_phy->bw;
		ctrl_data->rsp_data_len = sizeof(u8);
		break;
	case MTK_NL80211_VENDOR_ATTR_GET_BAND_INFO_CHANNEL:
		mac_dev = hc_get_mac_dev(mac_ad);
		mac_phy = &mac_dev->mac_phy;
		ctrl_data->rsp_data = &mac_phy->chan;
		ctrl_data->rsp_data_len = sizeof(u8);
		break;
	default:
		return -RTMP_IO_EOPNOTSUPP;
	}

	return 0;
}

static int cfg80211_drv_vndr_get_cap(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	POS_COOKIE pObj;
	int ret;

	pObj = (POS_COOKIE) mac_ad->OS_Cookie;
	if (wdev->if_up_down_state == FALSE) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"failed - [ERROR] Interface is down .\n");
		return -RTMP_IO_EFAULT;
	}

#ifdef WAPP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_CAC_CAP) {
		ret = mtk_cfg80211_reply_cac_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_cac_cap failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_MISC_CAP) {
		ret = mtk_cfg80211_reply_misc_cap(mac_ad, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_misc_cap failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_HT_CAP) {
		ret = mtk_cfg80211_reply_ht_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_ht_cap failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_VHT_CAP) {
		ret = mtk_cfg80211_reply_vht_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_vht_cap failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_HE_CAP) {
		ret = mtk_cfg80211_reply_he_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_he_cap failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_WDEV) {
		ret = mtk_cfg80211_reply_wdev(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wdev failed.\n");
	}
#ifdef CONFIG_MAP_SUPPORT
	else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_EHT_CAP) {
		ret = mtk_cfg80211_reply_eht_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_eht_cap failed.\n");
	}
#endif
#if (defined(MTK_MLO_MAP_SUPPORT) && defined(DOT11_EHT_BE))
	else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_MLO_CAP) {
		ret = mtk_cfg80211_reply_mlo_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_mlo_cap failed.\n");
	}
#endif
#ifdef DOT11_HE_AX
#ifdef MAP_R3
	else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_CAP_INFO_WF6_CAPABILITY) {
		ret = mtk_cfg80211_reply_wf6_cap(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wf6_cap failed.\n");
	}
#endif
#endif
	else
		return -RTMP_IO_EINVAL;

	return 0;
#else /* WAPP_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !WAPP_SUPPORT */
}

static int cfg80211_drv_vndr_get_runtime_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = 0;

	if (wdev->if_up_down_state == FALSE) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_NOTICE,
			"failed - [ERROR] Interface is down .\n");
		return -RTMP_IO_ENETDOWN;
	}

#if (defined(OFFCHANNEL_SCAN_FEATURE) && defined(ZERO_PKT_LOSS_SUPPORT))
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_CURRENT_CHANNEL_INFO) {
		ret = mtk_cfg80211_reply_current_channel_info(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"--error in sending current channel info--\n");
		else
			return 0;
	}
#endif /* OFFCHANNEL_SCAN_FEATURE && ZERO_PKT_LOSS_SUPPORT */

#ifdef WAPP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_MAX_NUM_OF_STA) {
		ret = mtk_cfg80211_reply_wapp_max_num_of_sta(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_max_num_of_sta failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_CHAN_LIST) {
		ret = mtk_cfg80211_reply_wapp_chan_list(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_chan_list failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_NOP_CHANNEL_LIST) {
		ret = mtk_cfg80211_reply_wapp_nop_chan_list(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_nop_chan_list failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_OP_CLASS) {
		ret = mtk_cfg80211_reply_wapp_op_class(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_op_class failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_BSS_INFO) {
		ret = mtk_cfg80211_reply_wapp_bss_info(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_bss_info failed.\n");
			return ret;
		}
	}
#ifdef WSC_INCLUDED
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_WAPP_WSC_PROFILES) {
		ret = mtk_cfg80211_reply_wapp_wsc_profiles(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_wapp_wsc_profiles failed.\n");
			return ret;
		}
	}
#endif /* WSC_INCLUDED */
#endif /* WAPP_SUPPORT */
#ifdef TR181_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_CHANNEL_LAST_CHANGE) {
		ret = mtk_cfg80211_reply_channel_last_change(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_channel_last_change failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_MAX_NUM_OF_SSIDS) {
		ret = mtk_cfg80211_reply_max_num_of_ssids(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_max_num_of_ssids failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_EXTENSION_CHANNEL) {
		ret = mtk_cfg80211_reply_extension_channel(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_extension_channel failed.\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_TRANSMITPOWER) {
		ctrl_data->rsp_data_len = sizeof(UCHAR);
		ctrl_data->rsp_data = &mac_ad->CommonCfg.ucTxPowerPercentage;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_80211H) {
		ctrl_data->rsp_data_len = sizeof(UCHAR);
		ctrl_data->rsp_data = &mac_ad->CommonCfg.bIEEE80211H;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_BASIC_RATE) {
		ctrl_data->rsp_data_len = sizeof(ULONG);
		ctrl_data->rsp_data = &mac_ad->CommonCfg.BasicRateBitmap;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_SUPP_RATE) {
		ctrl_data->rsp_data = &wdev->rate.legacy_rate.sup_rate;
		ctrl_data->rsp_data_len = wdev->rate.legacy_rate.sup_rate_len;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_ACS_REFRESH_PERIOD) {
		ctrl_data->rsp_data = &mac_ad->ApCfg.AcsCfg.ACSCheckTime;
		ctrl_data->rsp_data_len = sizeof(UINT32);
	}
#endif /* TR181_SUPPORT */
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_MLO_MLD_INFO) {
#if (defined(MTK_MLO_MAP_SUPPORT) && defined(DOT11_EHT_BE))
		ret = mtk_cfg80211_reply_mlo_mld_info(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
				"mtk_cfg80211_reply_mld_mlo_info failed.\n");
			return ret;
		}
#else
		ret = -RTMP_IO_EOPNOTSUPP;
#endif
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_WMODE) {
		ctrl_data->rsp_data = &wdev->PhyMode;
		ctrl_data->rsp_data_len = sizeof(USHORT);
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_BW) {
		static UCHAR bw_oper;

		bw_oper = wlan_operate_get_bw(wdev);
		ctrl_data->rsp_data = &bw_oper;
		ctrl_data->rsp_data_len = sizeof(UCHAR);
		ret = 0;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RUNTIME_INFO_GET_COUNTRYCODE) {
		ctrl_data->rsp_data = &mac_ad->CommonCfg.CountryCode;
		ctrl_data->rsp_data_len = sizeof(mac_ad->CommonCfg.CountryCode);
		ret = 0;
	}
	return ret;
}

static int cfg80211_drv_vndr_get_statistic(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = -RTMP_IO_EOPNOTSUPP;

	if (wdev->if_up_down_state == FALSE) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"failed - [ERROR] Interface is down .\n");
		return -RTMP_IO_ENETDOWN;
	}

#ifdef ANDLINK_V4_0
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_UPLINK_STATISTICS_ANDLINK_FORMAT) {
		ret = mtk_cfg80211_reply_andlink_uplink_info(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_andlink_uplink_info failed.\n");
	}
#endif/*ANDLINK_V4_0*/
#ifdef WAPP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_TX_PWR) {
		ret = mtk_cfg80211_reply_wapp_tx_pwr(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_wapp_tx_pwr failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_AP_METRICS) {
		ret = mtk_cfg80211_reply_wapp_ap_metrics(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_wapp_ap_metrics failed.\n");
	}
#endif /* WAPP_SUPPORT */

	return ret;
}

static int cfg80211_drv_vndr_get_static_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = 0;

	if (wdev->if_up_down_state == FALSE) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"failed - [ERROR] Interface is down .\n");
		return -RTMP_IO_ENETDOWN;
	}

#ifdef WAPP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_WAPP_SUPPORT_VER) {
		ret = mtk_cfg80211_reply_wapp_support_ver(mac_ad, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_wapp_support_ver failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_WIFI_VER) {
		ret = mtk_cfg80211_reply_wifi_ver(mac_ad, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_wifi_ver failed.\n");
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_CHIP_ID) {
		ctrl_data->rsp_data = &mac_ad->ChipID;
		ctrl_data->rsp_data_len = sizeof(mac_ad->ChipID);
		ret = 0;
	}
#endif /* WAPP_SUPPORT */

#ifdef CONFIG_6G_AFC_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_WIFI_AFC_DATA) {
		ret = mtk_cfg80211_reply_afc_data(mac_ad, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_AFC_DATA failed.\n");
	}
#endif

#if (defined(OFFCHANNEL_SCAN_FEATURE) && defined(ZERO_PKT_LOSS_SUPPORT))
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_OPERATING_INFO) {
		ret = mtk_cfg80211_reply_operating_info(mac_ad, wdev, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG,
				DBG_LVL_ERROR, "--error in sending operating info--\n");
	}
#endif /* OFFCHANNEL_SCAN_FEATURE && ZERO_PKT_LOSS_SUPPORT */

#if (defined(VENDOR_FEATURE6_SUPPORT) || defined(CONFIG_MAP_SUPPORT))
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_STATIC_INFO_COEXISTENCE) {
		ctrl_data->rsp_data = &mac_ad->CommonCfg.bBssCoexEnable;
		ctrl_data->rsp_data_len = sizeof(mac_ad->CommonCfg.bBssCoexEnable);
	}
#endif

	return ret;

}

static int cfg80211_drv_vndr_wapp_req(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef WAPP_SUPPORT
	int ret = -RTMP_IO_EINVAL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WAPP_REQ) {
		struct wapp_req *req;

		if (ctrl_data->data_len < sizeof(struct wapp_req))
			return -RTMP_IO_EINVAL;

		req = (struct wapp_req *)ctrl_data->data;
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			   "req_id %d\n", req->req_id);
		ret = wapp_event_handle(mac_ad, wdev, req);
	}

	return ret;
#else /* WAPP_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !WAPP_SUPPORT */
}

int cfg80211_drv_vndr_set_mcast_snoop(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#if defined(IGMP_SNOOP_SUPPORT)
	int ret = -RTMP_IO_EINVAL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}
	if (ctrl_data->data == NULL)
		return -RTMP_IO_EINVAL;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENABLE) {
		if (ctrl_data->u8_val == 0xf) {
			ctrl_data->rsp_data = &wdev->IgmpSnoopEnable;
			ctrl_data->rsp_data_len = sizeof(wdev->IgmpSnoopEnable);
			MTWF_PRINT("%s: %s\n", __func__, wdev->IgmpSnoopEnable ?
				"multicast snooping is enabled" : "multicast snooping is disabled");
			ret = 0;
		} else
			ret = Set_IgmpSn_Enable(mac_ad, wdev, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_UNKNOWN_PLCY) {
		ret = Set_IgmpSn_Allow_Non_Memb_Enable(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENTRY_ADD) {
		char *group_entry = (char *)ctrl_data->data;
		int grp_entry_len = ctrl_data->data_len;

		if (grp_entry_len >= 60
			|| strlen(group_entry) >= grp_entry_len)
			return -RTMP_IO_EINVAL;

		ret = Set_IgmpSn_AddEntry(mac_ad, group_entry);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_ENTRY_DEL) {
		char *group_entry = (char *)ctrl_data->data;
		int grp_entry_len = ctrl_data->data_len;

		if (grp_entry_len >= 60
			|| strlen(group_entry) >= grp_entry_len)
			return -RTMP_IO_EINVAL;

		ret = Set_IgmpSn_DelEntry(mac_ad, group_entry);
		if (ret < 0)
			return ret;
	}

#ifdef IGMP_SNOOPING_DENY_LIST
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_DENY_LIST) {
		char *deny_entry = (char *)ctrl_data->data;
		int deny_entry_len = ctrl_data->data_len;

		if (!deny_entry)
			return -RTMP_IO_EINVAL;

		if (deny_entry_len >= 64
			|| strlen(deny_entry) >= deny_entry_len)
			return -RTMP_IO_EINVAL;

		ret = Set_IgmpSn_Deny(mac_ad, deny_entry);
		if (ret < 0)
			return ret;
	}
#endif /* IGMP_SNOOPING_DENY_LIST */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_FLOODINGCIDR) {
		char *white_list = (char *)ctrl_data->data;
		int white_list_len = ctrl_data->data_len;

		if (white_list_len >= 21
			|| strlen(white_list) >= white_list_len)
			return -RTMP_IO_EINVAL;

		ret = Set_Igmp_Flooding_CIDR_Proc(mac_ad, white_list);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MCAST_SNOOP_CFGPERBANDSIZE) {
		char *per_band_list = (char *)ctrl_data->data;
		int per_band_list_len = ctrl_data->data_len;

		if (per_band_list_len >= 16
			|| strlen(per_band_list) >= per_band_list_len)
			return -RTMP_IO_EINVAL;

		ret = Set_IgmpSn_CfgPerBandSize_Proc(mac_ad, per_band_list);
		if (ret == FALSE)
			return -RTMP_IO_EINVAL;
	}

	return 0;
#else
	return -RTMP_IO_EOPNOTSUPP;
#endif /* IGMP_SNOOP_SUPPORT */
}

int cfg80211_drv_vndr_set_country(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	BOOLEAN b_set_code = FALSE, b_set_region = FALSE, b_set_name = FALSE;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO, "====>\n");

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_COUNTRY_SET_CODE)
		b_set_code = TRUE;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_COUNTRY_SET_REGION)
		b_set_region = TRUE;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_COUNTRY_SET_NAME)
		b_set_name = TRUE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		 "b_set_code=%d, b_set_region=%d, b_set_name=%d\n",
		 b_set_code, b_set_region, b_set_name);

	if (b_set_code) {
		ret = mtk_cfg80211_vndr_cmd_set_country_code(mac_ad, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"Set country code failed! ret=%x\n", ret);
			return ret;
		}

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"Set country code to %s succeed.\n", (char *)ctrl_data->data);
	}

	if (b_set_region) {
		ret = mtk_cfg80211_vndr_cmd_set_country_region(mac_ad, ctrl_data->uint_val);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"Set region failed! ret=%x\n", ret);
			return ret;
		}

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"Set region to %d succeed.\n", ctrl_data->uint_val);
	}

	if (b_set_name) {
		ret = mtk_cfg80211_vndr_cmd_set_country_string(mac_ad, ctrl_data->data);

		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"Set country string failed! ret=%x\n", ret);

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"Set country string to %s succeed.\n", (char *)ctrl_data->data);
	}

	RTEnqueueInternalCmd(mac_ad, CMDTHREAD_REG_HINT, &(mac_ad->CommonCfg.CountryCode),
						sizeof(mac_ad->CommonCfg.CountryCode));
	return ret;
}

int cfg80211_drv_vndr_set_channel(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	UCHAR chan = 0, ht_coex = 0, ext_chan = 0, cac_req = 0;
	UINT32 set_flag = 0;
	UINT32 bw = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_ch_switch_param *ch_switch_params_ptr = NULL;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (ctrl_data->data_len < sizeof(struct mt_ch_switch_param))
		return -RTMP_IO_EINVAL;

	ch_switch_params_ptr = (struct mt_ch_switch_param *)ctrl_data->data;

	if (ch_switch_params_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_CHAN_SET_NUM)) {
		set_flag |= MTK_CFG80211_CHAN_SET_FLAG_CHAN;
		chan = ch_switch_params_ptr->ch;
	}

	if (ch_switch_params_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_CHAN_SET_BW)) {
		set_flag |= MTK_CFG80211_CHAN_SET_FLAG_BW;
		bw = ch_switch_params_ptr->bw;
	}

	if (ch_switch_params_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_CHAN_SET_HT_EXTCHAN)) {
		set_flag |= MTK_CFG80211_CHAN_SET_FLAG_EXT_CHAN;
		ext_chan = ch_switch_params_ptr->ext_ch;
	}

	if (ch_switch_params_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_CHAN_SET_HT_COEX)) {
		set_flag |= MTK_CFG80211_CHAN_SET_FLAG_COEX;
		ht_coex = ch_switch_params_ptr->ht_coex;
	}

	if (ch_switch_params_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_CHAN_SET_BYPASS_CAC)) {
		set_flag |= MTK_CFG80211_CHAN_SET_FLAG_CAC_REQUIRED;
		cac_req = ch_switch_params_ptr->cac_required;
	}

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "set_flag=0x%x\n", set_flag);
	if (set_flag != 0) {
		ret = mtk_cfg80211_vndr_cmd_set_channel_attributes(
			mac_ad, set_flag, chan, bw, ext_chan, ht_coex, cac_req);

		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"Update channel attribute failed! ret=%x\n", ret);
			return ret;
		}

		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE,
			"Update channel attribute succeed.\n");
	}

	return ret;
}

int cfg80211_drv_vndr_set_acl(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (VALID_MBSS(mac_ad, wdev->func_idx) == FALSE) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
		"invalid ioctl_if %d\n", wdev->func_idx);
		return FALSE;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ACL_POLICY) {
		ret = cfg80211_set_acl_policy(mac_ad, wdev->func_idx, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ACL_ADD_MAC) {
		u8 mac_cnt = 0;

		if (ctrl_data->data_len % MAC_ADDR_LEN == 0)
			mac_cnt = ctrl_data->data_len / MAC_ADDR_LEN;

		ret = cfg80211_add_acl_entry(mac_ad, wdev->func_idx, ctrl_data->data, mac_cnt);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ACL_DEL_MAC) {
		u8 mac_cnt = 0;

		if (ctrl_data->data_len % MAC_ADDR_LEN == 0)
			mac_cnt = ctrl_data->data_len / MAC_ADDR_LEN;

		ret = cfg80211_del_acl_entry(mac_ad, wdev->func_idx, ctrl_data->data, mac_cnt);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ACL_LIST_INFO) {
		ret = cfg80211_get_acl_list(mac_ad, wdev->func_idx, ctrl_data);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ACL_CLEAR_ALL) {
		ret = cfg80211_clear_acl_list(mac_ad, wdev->func_idx);
		if (ret < 0)
			return ret;
	}

	return 0;
}

#ifdef VOW_SUPPORT
static int mtk_cfg80211_reply_vow_info(
	PRTMP_ADAPTER pAd, UINT32 group, struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	struct vow_info *vow_info;

	if (group >= VOW_MAX_GROUP_NUM) {
		MTWF_DBG(pAd, DBG_CAT_AP, CATAP_VOW, DBG_LVL_WARN,
			"set command failed. group(=%d)\n", group);
		return -RTMP_IO_EINVAL;
	}
	if (ctrl_data->rsp_data_len < sizeof(struct vow_info))
		return -RTMP_IO_EINVAL;
	vow_info = (struct vow_info *)ctrl_data->rsp_data;
	vow_info->atf_en = pAd->vow_cfg.en_airtime_fairness;
	vow_info->bw_en = pAd->vow_cfg.en_bw_ctrl;
	vow_info->ratio = pAd->vow_bss_cfg[group].max_airtime_ratio;

	return 0;
}
#endif /* VOW_SUPPORT */

int cfg80211_drv_vndr_set_vow(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef VOW_SUPPORT
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_ATF_EN_INFO) {
		ret = mtk_cfg80211_set_vow_atf_en(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_BW_EN_INFO) {
		ret = mtk_cfg80211_set_vow_bw_en(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_ATC_EN_INFO) {
		struct vow_group_en_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_group_en_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_group_en_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_atc_en(mac_ad, param->group, param->en);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_BW_CTL_EN_INFO) {
		struct vow_group_en_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_group_en_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_group_en_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_bw_ctrl_en(mac_ad, param->group, param->en);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_MIN_RATE_INFO) {
		struct vow_rate_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_rate_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_rate_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_min_rate(mac_ad, param->group, param->rate);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_MAX_RATE_INFO) {
		struct vow_rate_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_rate_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_rate_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_max_rate(mac_ad, param->group, param->rate);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_MIN_RATIO_INFO) {
		struct vow_ratio_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_ratio_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_ratio_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_min_ratio(mac_ad, param->group, param->ratio);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_MAX_RATIO_INFO) {
		struct vow_ratio_param *param; /* defined in mtk_vendor_nl80211.h */

		if (ctrl_data->data_len < sizeof(struct vow_ratio_param))
			return -RTMP_IO_EINVAL;

		param = (struct vow_ratio_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_vow_max_ratio(mac_ad, param->group, param->ratio);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_VOW_GET_INFO) {
		ret = mtk_cfg80211_reply_vow_info(mac_ad, ctrl_data->uint_val, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_vow_info failed.\n");
	}
	return 0;
#else /* VOW_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !VOW_SUPPORT */
}

int cfg80211_drv_vndr_set_mwds(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef MWDS
	INT apidx, apcli_idx;
	BOOLEAN isAp;
	UCHAR if_idx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		isAp = TRUE;
		apidx = wdev->func_idx;
	} else if (wdev->wdev_type == WDEV_TYPE_STA) {
		isAp = FALSE;
		apcli_idx = wdev->func_idx;
	} else {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"incorrect wdev type(=0x%x)\n", wdev->wdev_type);
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MWDS_ENABLE) {
		if (isAp)
			if_idx = apidx;
		else
			if_idx = apcli_idx;
		mtk_cfg80211_set_mwds_cap(mac_ad, if_idx, isAp, ctrl_data->u8_val);
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MWDS_INFO) { /*for MWDS autotest*/
		INT i;
		PMAC_TABLE_ENTRY pEntry;

		for (i = 0; VALID_UCAST_ENTRY_WCID(mac_ad, i); i++) {
			pEntry = entry_get(mac_ad, i);

			if (IS_ENTRY_A4(pEntry)) {
				if (ctrl_data->u8_val == 0) {
					pEntry->MWDSInfo.Addr4PktNum = 0;
					pEntry->MWDSInfo.Addr3PktNum = 0;
					pEntry->MWDSInfo.NullPktNum = 0;
					pEntry->MWDSInfo.bcPktNum = 0;
				} else
					MTWF_PRINT(
				"MAC: "MACSTR" A4: %d, A3: %d, NULL: %d, bc/mc: %d, total: %d\n",
				MAC2STR(pEntry->Addr), pEntry->MWDSInfo.Addr4PktNum,
				pEntry->MWDSInfo.Addr3PktNum,
				pEntry->MWDSInfo.NullPktNum, pEntry->MWDSInfo.bcPktNum,
				(pEntry->MWDSInfo.NullPktNum + pEntry->MWDSInfo.bcPktNum));
			}
		}
	} else {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			   "invalid mwds cmd\n");
		return -RTMP_IO_EINVAL;
	}
	return 0;
#else /* MWDS */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !MWDS */
}

int cfg80211_drv_vndr_set_auto_ch_sel(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	AUTO_CH_CTRL *pAutoChCtrl = NULL;
	int ret = 0;
	BOOLEAN need_trigger_acs = FALSE;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_auto_ch_sel_param *auto_ch_sel_param_ptr = NULL;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->data_len < sizeof(struct mt_auto_ch_sel_param)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
			"ctrl_data->data_len < mt_auto_ch_sel_param's length, return!!\n");
		return -RTMP_IO_EFAULT;
	}

	auto_ch_sel_param_ptr = (struct mt_auto_ch_sel_param *)ctrl_data->data;

	if (auto_ch_sel_param_ptr == NULL) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
			"auto_ch_sel_param_ptr is NULL, return\n");
		return -RTMP_IO_EFAULT;
	}


	pAutoChCtrl = HcGetAutoChCtrl(mac_ad);
	if (pAutoChCtrl == NULL) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
			"pAutoChCtrl is NULL, return\n");
		return -RTMP_IO_EFAULT;
	}

	/*if ACS is running, the ACS parameter should not be changed*/
	if (GetCurrentChannelOpOwner(mac_ad, wdev) == CH_OP_OWNER_ACS) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
				"Failed!!!ACS is running, please try again after ACS done!\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_INFO,
		"start to parsing cmd, flags= 0x%lx\n", auto_ch_sel_param_ptr->flags);
#ifdef CONFIG_6G_SUPPORT
	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_6G_PSC)) {
		if (wlan_config_get_ch_band(wdev) != CMD_CH_BAND_6G) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
				"not 6G BAND, no need configure PSC ACS.\n");
		} else {
			if (auto_ch_sel_param_ptr->is_6g_psc > 0)
				pAutoChCtrl->AutoChSelCtrl.PSC_ACS = TRUE;
			else
				pAutoChCtrl->AutoChSelCtrl.PSC_ACS = FALSE;

			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
					"6G PSC_ACS is %d\n", pAutoChCtrl->AutoChSelCtrl.PSC_ACS);
		}
	}
#endif

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_SKIP_LIST)) {
		struct ch_list_info *skip_list; /* defined in mtk_vendor_nl80211.h */

		if (auto_ch_sel_param_ptr->skip_list_len < sizeof(struct ch_list_info))
			return -RTMP_IO_EINVAL;
		skip_list = (struct ch_list_info *)auto_ch_sel_param_ptr->skip_list;
		if (update_auto_ch_skip_list(mac_ad, skip_list)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
			"skip_list is set!\n");
		}
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_PREFER_LIST)) {
		struct ch_list_info *prefer_list; /* defined in mtk_vendor_nl80211.h */

		if (auto_ch_sel_param_ptr->prefer_list_len < sizeof(struct ch_list_info))
			return -RTMP_IO_EINVAL;
		prefer_list = (struct ch_list_info *)auto_ch_sel_param_ptr->prefer_list;
		if (update_auto_ch_priority_from_prefer_list(mac_ad, wdev, prefer_list)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
			"prefer_list is set!\n");
		}
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_PARTIAL)) {
		if (auto_ch_sel_param_ptr->partial > 0)
			pAutoChCtrl->AutoChSelCtrl.PartialScanACS = TRUE;
		else
			pAutoChCtrl->AutoChSelCtrl.PartialScanACS = FALSE;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"PartialScanACS is %d\n",
				pAutoChCtrl->AutoChSelCtrl.PartialScanACS);
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_SCANNING_DWELL)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Scanning Dwell=%d\n", auto_ch_sel_param_ptr->scanning_dwell);
		pAutoChCtrl->AutoChSelCtrl.ScanningDwell = auto_ch_sel_param_ptr->scanning_dwell;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_RESTORE_DWELL)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Restoring Dwell=%d\n", auto_ch_sel_param_ptr->restore_dwell);
		pAutoChCtrl->AutoChSelCtrl.RestoreDwell = auto_ch_sel_param_ptr->restore_dwell;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_NUM)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Partial Scan ACS ScanningChNum=%d\n", auto_ch_sel_param_ptr->partial_scan_ch_num);
		pAutoChCtrl->AutoChSelCtrl.ScanningChNum = auto_ch_sel_param_ptr->partial_scan_ch_num;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_SEL)) {
		switch (auto_ch_sel_param_ptr->sel_alg) {
		case 7:
			mac_ad->ApCfg.auto_ch_score_flag = TRUE;
			fallthrough;
		case 3:
			mac_ad->ApCfg.AutoChannelAlg = ChannelAlgBusyTime;
			need_trigger_acs = TRUE;
			break;
		case 8:
			mac_ad->ApCfg.AutoChannelAlg = ChannelAlgIntlgntMaxMin;
			need_trigger_acs = TRUE;
			break;
		case 9:
			mac_ad->ApCfg.AutoChannelAlg = ChannelAlgIntlgntAvg;
			need_trigger_acs = TRUE;
			break;
		default:
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
					"Algo(%d) error!\n", auto_ch_sel_param_ptr->sel_alg);
			ret = -RTMP_IO_EINVAL;
		}
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
			"use Algo %d!\n", mac_ad->ApCfg.AutoChannelAlg);
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_CHECK_TIME)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"ACS check time=%d\n", auto_ch_sel_param_ptr->periodic_check_time);
		mac_ad->ApCfg.AcsCfg.ACSCheckTime = auto_ch_sel_param_ptr->periodic_check_time;
		pAutoChCtrl->AutoChSelCtrl.AutoCheckStartTime = 0;
		if (auto_ch_sel_param_ptr->periodic_check_time == 0) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"disable Periodic ACS\n");
			pAutoChCtrl->AutoChSelCtrl.ACSCheckCount = 0;
		} else if (pAutoChCtrl->AutoChSelCtrl.ACSCheckCount >= auto_ch_sel_param_ptr->periodic_check_time) {
			pAutoChCtrl->AutoChSelCtrl.ACSCheckCount = 0;
#ifdef EM_PLUS_SUPPORT
			if (!IS_MAP_TURNKEY_ENABLE(pAd))
#endif /* EM_PLUS_SUPPORT */
				need_trigger_acs = TRUE;
		}
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_STA_NUM_THR)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Periodic ACS StaNumThr=%d\n", auto_ch_sel_param_ptr->periodic_sta_num_thr);
		mac_ad->ApCfg.AcsCfg.StaNumThr = auto_ch_sel_param_ptr->periodic_sta_num_thr;
	}

	if (auto_ch_sel_param_ptr->flags & (1 <<  MTK_NL80211_VENDOR_ATTR_AUTO_CH_DATA_RATE_WT)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Intlgnt ACS data_rate_wt=%d\n", auto_ch_sel_param_ptr->data_rate_wt);
		pAutoChCtrl->AutoChSelCtrl.DataRateWeight = auto_ch_sel_param_ptr->data_rate_wt;
	}

	if (auto_ch_sel_param_ptr->flags & (1 <<  MTK_NL80211_VENDOR_ATTR_AUTO_CH_PRIO_WT)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Set Intlgnt ACS PrioWeight=%d\n", auto_ch_sel_param_ptr->ch_prio_wt);
		pAutoChCtrl->AutoChSelCtrl.PrioWeight = auto_ch_sel_param_ptr->ch_prio_wt;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_TX_POWER_CONS)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"is Intlgnt ACS TXPowerCons =%d\n", auto_ch_sel_param_ptr->tx_power_cons);
		if (auto_ch_sel_param_ptr->tx_power_cons)
			pAutoChCtrl->AutoChSelCtrl.TXPowerCons = TRUE;
		else
			pAutoChCtrl->AutoChSelCtrl.TXPowerCons = FALSE;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_CH_UTIL_THR)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"ACS ch_util_thr =%d\n", auto_ch_sel_param_ptr->ch_util_thr);
		mac_ad->ApCfg.AcsCfg.ChUtilThr = auto_ch_sel_param_ptr->ch_util_thr;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_ICE_CH_UTIL_THR)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"ACS ice_ch_util_thr =%d\n", auto_ch_sel_param_ptr->ice_ch_util_thr);
		mac_ad->ApCfg.AcsCfg.ACSIceChUtilThr = auto_ch_sel_param_ptr->ice_ch_util_thr;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_MAX_ACS_TIMES)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"ACS max_acs_times =%d\n", auto_ch_sel_param_ptr->max_acs_times);
		mac_ad->ApCfg.AcsCfg.ACSMaxACSTimes = auto_ch_sel_param_ptr->max_acs_times;
	}

	if (auto_ch_sel_param_ptr->flags & (1 << MTK_NL80211_VENDOR_ATTR_AUTO_CH_SWITCH_THR)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"ACS sw_ch_thr =%d\n", auto_ch_sel_param_ptr->switch_thr);
		mac_ad->ApCfg.AcsCfg.ACSSwChThr = auto_ch_sel_param_ptr->switch_thr;
	}

	if (need_trigger_acs) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_NOTICE,
				"Start to trigger acs scan.\n");
		if (!mtk_cfg80211_auto_ch_sel(mac_ad, wdev)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
					"Trigger acs scan failed.\n");
			return -RTMP_IO_EINVAL;
		}
	}
	return ret;
}

int cfg80211_drv_vndr_set_scan(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef SCAN_SUPPORT
	SCAN_CTRL *ScanCtrl = NULL;
	int ret = 0;
	UCHAR ap_scan = TRUE;
	UCHAR scan_type;
	NDIS_802_11_SSID Ssid;
	int ssid_len = 0;
	char *p = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_off_ch_scan_param *off_ch_scan;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	ScanCtrl = &mac_ad->ScanCtrl;
	if (ScanCtrl == NULL) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"failed - [ERROR]ScanCtrl is NULL\n");
		return -RTMP_IO_EFAULT;
	}

	NdisZeroMemory(&Ssid, sizeof(NDIS_802_11_SSID));

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_SCAN_RESULT) {
		/* No need to dump scan result if we are doing scanning */
		if (scan_in_run_state(mac_ad, wdev) || (ScanCtrl->PartialScan.bScanning == TRUE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"scan is ongoing, no need to dump scan table\n");
			return -RTMP_IO_EBUSY;
		}

		ret = mtk_cfg80211_get_scan_result(
			mac_ad, ScanCtrl, ctrl_data->uint_val, ctrl_data);
		return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SCAN_CLEAR) {
		/* Don't clear the scan table if we are doing scanning */
		if (scan_in_run_state(mac_ad, wdev) || (ScanCtrl->PartialScan.bScanning == TRUE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"scan is ongoing, can not clear scan table\n");
			return -RTMP_IO_EBUSY;
		}

		BssTableInit(&ScanCtrl->ScanTab);
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"clear scan table!\n");
		return 0;
	}
#ifdef CONFIG_6G_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_6G_PSC_SCAN_EN) {
		if (ctrl_data->u8_val > 0)
			ScanCtrl->psc_scan_en = TRUE;
		else
			ScanCtrl->psc_scan_en = FALSE;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"6G PSC scan is %d\n", ScanCtrl->psc_scan_en);
		return 0;
	}
#endif /* CONFIG_6G_SUPPORT */
#ifdef ANDLINK_V4_0
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_SCAN_RESULT_ANDLINK_FORMAT) {
		ret = mtk_cfg80211_reply_andlink_scan_result_info(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_WARN,
			"failed - [ERROR]mtk_cfg80211_reply_andlink_scan_result_info fail.\n");
			return -RTMP_IO_EINVAL;
		}
		return 0;
	}
#endif/*ANDLINK_V4_0*/
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SCAN_TYPE) {
		scan_type = ctrl_data->u8_val;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"scan_type = %d\n", scan_type);
	} else
		return -RTMP_IO_EINVAL;

	/*do dfs cac check for 5G band*/
#ifdef MT_DFS_SUPPORT
	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G) {
		struct DOT11_H *pDot11h = &mac_ad->Dot11_H;

		if (pDot11h && (pDot11h->ChannelMode == CHAN_SILENCE_MODE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"still in silence mode, can not trigger scan\n");
			return -RTMP_IO_EFAULT;
		}
	}
#endif /* MT_DFS_SUPPORT */

	if (wdev->wdev_type == WDEV_TYPE_STA) {
		ap_scan = FALSE;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
			"not ap_scan\n");
	}

	switch (scan_type) {
	case mt_scan_type_full:
		if (ctrl_data->data_len) {
			ssid_len = ctrl_data->data_len;
			p = ctrl_data->data;

			if (ssid_len > 32 || ssid_len < 0) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"invalid ssid_len=%d, force to do passive scan\n", ssid_len);
				ssid_len = 0;
			}
			if (ssid_len == 0)
				Ssid.SsidLength = 0;
			else {
				Ssid.SsidLength = ssid_len;
				memcpy(Ssid.Ssid, p, ssid_len);
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
					"SsidLength = %d, ssid : %s\n", Ssid.SsidLength, Ssid.Ssid);
			}
		}

		if (ap_scan) {
			if (ssid_len == 0)
				ApSiteSurvey_by_wdev(mac_ad, &Ssid, SCAN_PASSIVE, FALSE, wdev);
			else
				ApSiteSurvey_by_wdev(mac_ad, &Ssid, SCAN_ACTIVE, FALSE, wdev);
		} else {
			/*To do SiteSurvey, need TakeChannelOpCharge first*/
			if (!TakeChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN, FALSE)) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
				"TakeChannelOpCharge fail for SCAN!!\n");
				return -RTMP_IO_EBUSY;
			}
			if ((wdev->func_idx >= 0)
				&& (wdev->func_idx < MAX_MULTI_STA))
				mac_ad->StaCfg[wdev->func_idx].bSkipAutoScanConn = TRUE;
			else {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
					"invalid ioctl if\n");
				return -RTMP_IO_EBUSY;
			}
			StaSiteSurvey(mac_ad, &Ssid, SCAN_ACTIVE, wdev);
		}

		break;

	case mt_scan_type_partial:
		if (ctrl_data->uint_val) {
			ScanCtrl->PartialScan.TimerInterval = ctrl_data->uint_val;
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"set partial scan timer interval = %d\n", ctrl_data->uint_val);
		}

		if (ctrl_data->ushort_val
			&& ctrl_data->ushort_val <= 255) {
			ScanCtrl->PartialScan.NumOfChannels = ctrl_data->ushort_val;
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"set partial scan Number of channels = %d\n", ctrl_data->ushort_val);
		}

		if (ScanCtrl->PartialScan.bScanning) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"partial scan is ongoing, cannot trigger again\n");
			return -RTMP_IO_EBUSY;
		}
		/*To set ParitalScan flag TRUE, need TakeChannelOpCharge first*/
		if (!TakeChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_PARTIAL_SCAN, TRUE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"TakeChannelOpCharge fail for PARTIAL SCAN!!\n");
			return -RTMP_IO_EBUSY;
		}

		ScanCtrl->PartialScan.pwdev = wdev;
		wdev->ScanInfo.LastScanChannel = 0;
		ScanCtrl->PartialScan.bScanning = TRUE;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
			"set partial scan bScanning = %d\n", ScanCtrl->PartialScan.bScanning);
		break;

#ifdef OFFCHANNEL_SCAN_FEATURE
	case mt_scan_type_off_ch:
		if (ctrl_data->data_len < sizeof(struct mt_off_ch_scan_param))
			return -RTMP_IO_EINVAL;

		off_ch_scan = (struct mt_off_ch_scan_param *)ctrl_data->data;
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
				"channel number to trigger off-ch scan:%d\n",
				off_ch_scan->target_ch);
		if (!IsValidChannel(mac_ad, off_ch_scan->target_ch, NULL)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
					"wrong channel number to trigger off-ch scan:%d\n",
					off_ch_scan->target_ch);
			return -RTMP_IO_EINVAL;
		}
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
			"active to trigger off-ch scan:%d\n", off_ch_scan->active);
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
			"duration to trigger off-ch scan:%d\n", off_ch_scan->duration);

		/*To do OffChannelScan, need TakeChannelOpCharge first*/
		if (!TakeChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN, TRUE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
				"TakeChannelOpCharge fail for Off-Channel SCAN!!\n");
			return -RTMP_IO_EBUSY;
		}
		ScanCtrl->Num_Of_Channels = 1;
		ScanCtrl->ScanTime[0] = 0;
		ScanCtrl->CurrentGivenChan_Index = 0;
		ScanCtrl->state = OFFCHANNEL_SCAN_START;
		if (off_ch_scan->active)
			ApSiteSurveyNew_by_wdev(
				mac_ad, off_ch_scan->target_ch, off_ch_scan->duration,
				SCAN_ACTIVE, FALSE, wdev);
		else
			ApSiteSurveyNew_by_wdev(
				mac_ad, off_ch_scan->target_ch, off_ch_scan->duration,
				SCAN_PASSIVE, FALSE, wdev);
		break;
#endif
	case NL80211_2040_OVERLAP_SCAN:
		if (!mtk_cfg80211_trigger_2040_overlap_scan(mac_ad, wdev)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
					"fail to trigger 2040 overlap scan, return\n");
			return -RTMP_IO_EFAULT;
		}

		break;
	default:
		return -RTMP_IO_EINVAL;
	}

	return 0;
#else /* SCAN_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !SCAN_SUPPORT */

}

int cfg80211_drv_vndr_ft_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef HOSTAPD_11R_SUPPORT
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_FTIE) {
		struct ap_11r_params *ap_ft_params; /* defined in mtk_vendor_nl80211.h */
		PFT_CFG pFtCfg;
		UINT apidx = 0, size_tmp = 0;

		ap_ft_params = (struct ap_11r_params *)ctrl_data->data;

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"OwnMac: %02x%02x%02x%02x%02x%02x\n",
				PRINT_MAC(ap_ft_params->own_mac));

		apidx = get_apidx_by_addr(mac_ad, ap_ft_params->own_mac);
		if (apidx >= mac_ad->ApCfg.BssidNum) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"apidx invalid");
			return -1;
		}

		pFtCfg = &mac_ad->ApCfg.MBSSID[apidx].wdev.FtCfg;

		if (pFtCfg->FtCapFlag.Dot11rFtEnable) {
			NdisZeroMemory(pFtCfg->FtR0khId, sizeof(pFtCfg->FtR0khId));
			size_tmp = ARRAY_SIZE(pFtCfg->FtR0khId) < ARRAY_SIZE(ap_ft_params->nas_identifier)
					? ARRAY_SIZE(pFtCfg->FtR0khId) : ARRAY_SIZE(ap_ft_params->nas_identifier);
			size_tmp = ap_ft_params->nas_id_len < size_tmp ? ap_ft_params->nas_id_len : size_tmp;
			NdisMoveMemory(pFtCfg->FtR0khId, ap_ft_params->nas_identifier, size_tmp);
			pFtCfg->FtR0khIdLen = size_tmp;

			NdisZeroMemory(pFtCfg->FtR1khId, MAC_ADDR_LEN);
			NdisMoveMemory(pFtCfg->FtR1khId, ap_ft_params->r1_key_holder, MAC_ADDR_LEN);

			pFtCfg->AssocDeadLine = ap_ft_params->reassociation_deadline;

			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"R1KH: %02x%02x%02x%02x%02x%02x\n",
				PRINT_MAC(ap_ft_params->r1_key_holder));
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"NAS-ID: %s\n", ap_ft_params->nas_identifier);
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"AssocDeadline: %d\n", ap_ft_params->reassociation_deadline);
		} else
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"11R disabled in driver\n");
	}

	return 0;
#else /* HOSTAPD_11R_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !HOSTAPD_11R_SUPPORT */
}

int cfg80211_drv_vndr_mlo_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_AUTH_MLO_IE) {
		struct wpa_mlo_ie_parse *peer_mlo_ie; /* defined in mtk_vendor_nl80211.h */
		UINT8 *construct_peer_mlo_ie;

		peer_mlo_ie = (struct wpa_mlo_ie_parse *)ctrl_data->data;
		construct_peer_mlo_ie = wdev->hostapd_peer_ml_ie;
		os_zero_mem(construct_peer_mlo_ie, sizeof(wdev->hostapd_peer_ml_ie));
		construct_peer_mlo_ie[0] = IE_WLAN_EXTENSION;
		construct_peer_mlo_ie[1] = 10;
		construct_peer_mlo_ie[2] = EID_EXT_EHT_MULTI_LINK;
		construct_peer_mlo_ie[3] = 0; /* ml common control */
		construct_peer_mlo_ie[4] = 0;
		construct_peer_mlo_ie[5] = 7; /* len = 7 */
		NdisMoveMemory(&construct_peer_mlo_ie[6], peer_mlo_ie->ml_addr, MAC_ADDR_LEN);
		hex_dump_with_lvl("mlo_ie",
			(unsigned char *) construct_peer_mlo_ie, 12, DBG_LVL_INFO);

#if defined(HOSTAPD_STA_MLD_MAC) \
	&& defined(CFG_CFG80211_VERSION) \
	&& (KERNEL_VERSION(6, 0, 0) <= CFG_CFG80211_VERSION)
		if (12 + sizeof(struct wpa_mlo_ie_parse) < sizeof(wdev->hostapd_peer_ml_ie)) {
			NdisMoveMemory(&construct_peer_mlo_ie[12],
				peer_mlo_ie, sizeof(struct wpa_mlo_ie_parse));
			hex_dump_with_lvl("construct_peer_mlo_ie",
				(unsigned char *) construct_peer_mlo_ie,
				12+sizeof(struct wpa_mlo_ie_parse), DBG_LVL_INFO);
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
				"mld mac addr: "MACSTR". setuplink addr: "MACSTR", valid:%d\n",
				MAC2STR(peer_mlo_ie->ml_addr),
				MAC2STR(peer_mlo_ie->profiles[0].addr),
				peer_mlo_ie->profiles[0].mac_addr_present);
		}
#endif/*(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 0, 0) <= CFG_CFG80211_VERSION)*/
	}

	return 0;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

#if defined(DOT11_EHT_BE)
static int hostapd_ft_action_req(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_cfg80211_vndr_ctrl *ctrl_data)
{
	BSS_STRUCT *pMbss;
	PEID_STRUCT eid_ptr;
	MAC_TABLE_ENTRY *pEntry;
	UCHAR *mlo_ie = NULL;
	uint16_t mld_sta_idx = MLD_STA_NONE;
	UINT8 mld_addr[MAC_ADDR_LEN] = {0};
	UINT8 sta_addr[MAC_ADDR_LEN] = {0};
	UINT8 sta_mld_addr[MAC_ADDR_LEN] = {0};
	UCHAR *data = (UCHAR *)ctrl_data->data;
	UINT8 mlo_ie_flag = FALSE;
#ifdef DOT11R_FT_SUPPORT
	PFT_CFG pFtCfg = NULL;
#endif /* DOT11R_FT_SUPPORT */

	MTWF_DBG(mac_ad, DBG_CAT_HW, CATMLME_AUTH, DBG_LVL_NOTICE,
		"FT ACTION - %s, data len %d\n", __func__, ctrl_data->data_len);
	if (!data) {
		MTWF_DBG(mac_ad, DBG_CAT_HW, CATMLME_AUTH, DBG_LVL_ERROR,
			"FT ACTION - %s, data is null\n", __func__);
		return -RTMP_IO_EINVAL;
	}

	NdisCopyMemory(sta_addr, data, MAC_ADDR_LEN);
	NdisCopyMemory(sta_mld_addr, data + 16, MAC_ADDR_LEN);
	pMbss = (struct _BSS_STRUCT *)wdev->func_dev;
	if (!pMbss) {
		MTWF_DBG(mac_ad, DBG_CAT_MLME, CATMLME_AUTH, DBG_LVL_ERROR,
			"%s(%d), wdev is null\n", __func__, __LINE__);
		return -RTMP_IO_EFAULT;
	}

#ifdef DOT11R_FT_SUPPORT
	pFtCfg = &(wdev->FtCfg);
	if (pFtCfg)
		pFtCfg->update_sta_mac = FALSE;
#endif /* DOT11R_FT_SUPPORT */

	for (eid_ptr = (PEID_STRUCT) (data + 28);
			eid_ptr->Len + 2 + (UCHAR *) eid_ptr - data <= ctrl_data->data_len;
			eid_ptr = (PEID_STRUCT) ((UCHAR *) eid_ptr + eid_ptr->Len + 2)) {

		if (eid_ptr->Eid == IE_WLAN_EXTENSION &&
				eid_ptr->Len && eid_ptr->Octet[0] == EID_EXT_EHT_MULTI_LINK) {

			mlo_ie = (UCHAR *)eid_ptr;

			if (mlo_ie) {
				mlo_ie_flag = TRUE;
				wdev->do_not_send_sta_del_event_flag = 1;
				mld_sta_idx = eht_build_multi_link_conn_req(wdev,
						SUBTYPE_AUTH, mlo_ie, NULL, NULL, sta_addr, mld_addr, 0);
				wdev->do_not_send_sta_del_event_flag = 0;

				if (mld_sta_idx == MLD_STA_NONE) {
					MTWF_DBG(mac_ad, DBG_CAT_MLME, CATMLME_AUTH, DBG_LVL_ERROR,
						"Fail: mld conn request (bss:%s, setup addr:%pM)\n",
						RtmpOsGetNetDevName(wdev->if_dev), sta_addr);
					return 1;
				}
#ifdef DOT11R_FT_SUPPORT
				if (pFtCfg) {
					NdisZeroMemory(pFtCfg->sta_mld_addr, MAC_ADDR_LEN);
					COPY_MAC_ADDR(pFtCfg->sta_mld_addr, sta_addr);
				}
#endif /* DOT11R_FT_SUPPORT */
			}

			pEntry = MacTableLookup(mac_ad, sta_addr);

			if (!pEntry) {
				pEntry = MacTableInsertEntry(
						mac_ad, sta_addr,
						wdev, ENTRY_CLIENT, OPMODE_AP,
						TRUE, mld_sta_idx, mld_addr);
			}
			break;
		}
	}

	if (!mlo_ie_flag) {
		pEntry = MacTableLookup(mac_ad, sta_mld_addr);
#ifdef DOT11R_FT_SUPPORT
		if (pFtCfg) {
			NdisZeroMemory(pFtCfg->sta_mld_addr, MAC_ADDR_LEN);
			COPY_MAC_ADDR(pFtCfg->sta_mld_addr, sta_mld_addr);
		}
#endif /* DOT11R_FT_SUPPORT */
		if (!pEntry) {
			pEntry = MacTableInsertEntry(
					mac_ad, sta_mld_addr,
					wdev, ENTRY_CLIENT, OPMODE_AP,
					TRUE, MLD_STA_NONE, NULL);
		}
	}

	return 0;
}
#endif /* defined(DOT11_EHT_BE) */

static int cfg80211_drv_vndr_del_mlo_sta_entry(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DEL_MLO_STA_ENTRY_MAX) {
		ret = hostapd_ft_action_req(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
					"mtk_cfg80211_command failed.\n");
			return -RTMP_IO_EINVAL;
		}
	}

	return 0;
#else /* DOT11_EHT_BE */
#endif /* !DOT11_EHT_BE */
}

int cfg80211_drv_vndr_wapp_cosr_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	return -RTMP_IO_EOPNOTSUPP;
}

int cfg80211_drv_vndr_set_pmkid(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef HOSTAPD_PMKID_IN_DRIVER_SUPPORT
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_PMKSA_CACHE) {
		if (ctrl_data->data_len < sizeof(struct mt_priv_pmkid))
			return -RTMP_IO_EINVAL;
		hex_dump_with_lvl(
			"pmkid_cache", (unsigned char *) ctrl_data->data,
			sizeof(struct mt_priv_pmkid), DBG_LVL_INFO);
		ret = CFG80211_ApUpdateStaPmkid(mac_ad, ctrl_data->data);
	}
	return ret;
#else /* HOSTAPD_PMKID_IN_DRIVER_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !HOSTAPD_PMKID_IN_DRIVER_SUPPORT */
}

int cfg80211_drv_vndr_set_ap_ptk(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	MAC_TABLE_ENTRY *pEntry = NULL;
	struct RT_CMD_AP_IOCTL_UPDATE_AP_PTK *pApPtkEntry = NULL;

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_AP_PTK) {
		if (ctrl_data->data_len < sizeof(struct RT_CMD_AP_IOCTL_UPDATE_AP_PTK))
			return -RTMP_IO_EINVAL;

		hex_dump_always("ap ptk dump", (unsigned char *)ctrl_data->data, sizeof(struct RT_CMD_AP_IOCTL_UPDATE_AP_PTK));
		pApPtkEntry = (struct RT_CMD_AP_IOCTL_UPDATE_AP_PTK *)ctrl_data->data;
		pEntry = MacTableLookupForTx(mac_ad, pApPtkEntry->sta_mac, NULL);
		if (!pEntry) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pEntry is null\n");
			return -RTMP_IO_EINVAL;
		}

		memcpy(pEntry->SecConfig.PTK, pApPtkEntry->ptk, pApPtkEntry->ptk_len);
	}
	return ret;
}


int cfg80211_drv_vndr_set_wmm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WMM_AP_AIFSN_INFO) {
		UCHAR aifsn_value[WMM_NUM_OF_AC] = {0};
		int aifsn_len = ctrl_data->data_len;
		UCHAR *aifsn_temp = ctrl_data->data;

		if (aifsn_len >= WMM_NUM_OF_AC)
			aifsn_len = WMM_NUM_OF_AC;
		else
			return -RTMP_IO_EINVAL;
		memcpy(aifsn_value, aifsn_temp, aifsn_len);
		ret = mtk_cfg80211_Set_APAifsn_Proc(mac_ad, aifsn_value);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WMM_AP_CWMIN_INFO) {
		UCHAR cwmin_value[WMM_NUM_OF_AC] = {0};
		int cwmin_len = ctrl_data->data_len;
		UCHAR *cwmin_temp = ctrl_data->data;

		if (cwmin_len >= WMM_NUM_OF_AC)
			cwmin_len = WMM_NUM_OF_AC;
		else
			return -RTMP_IO_EINVAL;
		memcpy(cwmin_value, cwmin_temp, cwmin_len);
		ret = mtk_cfg80211_Set_APCwmin_Proc(mac_ad, cwmin_value);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WMM_AP_CWMAX_INFO) {
		UCHAR cwmax_value[WMM_NUM_OF_AC] = {0};
		int cwmax_len = ctrl_data->data_len;
		UCHAR *cwmax_temp = ctrl_data->data;

		if (cwmax_len >= WMM_NUM_OF_AC)
			cwmax_len = WMM_NUM_OF_AC;
		else
			return -RTMP_IO_EINVAL;

		memcpy(cwmax_value, cwmax_temp, cwmax_len);
		ret = mtk_cfg80211_Set_APCwmax_Proc(mac_ad, cwmax_value);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WMM_AP_TXOP_INFO) {
		UCHAR txop_value[WMM_NUM_OF_AC] = {0};
		int txop_len = ctrl_data->data_len;
		UCHAR *txop_temp = ctrl_data->data;

		if (txop_len >= WMM_NUM_OF_AC)
			txop_len = WMM_NUM_OF_AC;
		else
			return -RTMP_IO_EINVAL;

		memcpy(txop_value, txop_temp, txop_len);

		ret = mtk_cfg80211_Set_APTxop_Proc(mac_ad, txop_value);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_WMM_AP_CAP_INFO) {
		switch (ctrl_data->u8_val) {
		case 0:
		case 1:
			ret = mtk_cfg80211_set_wmm_cap(mac_ad, ctrl_data->u8_val);
			break;
		case 0xf:
			ctrl_data->u8_val = wdev->bWmmCapable;
			break;
		default:
			return -EINVAL;
		}

		if (ret < 0)
			return ret;
	}

	return 0;
}

int cfg80211_drv_vndr_set_monitor(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef AIR_MONITOR
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"mac_ad not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_ENABLE) {
		ret = mtk_cfg80211_set_monitor_enable(mac_ad, ctrl_data->u8_val);
		if (ret)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_RULE) {
		ret = mtk_cfg80211_set_monitor_rule(mac_ad, ctrl_data->data);
		if (ret)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_STA)
		mtk_cfg80211_set_monitor_target(mac_ad, ctrl_data->data);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_IDX) {
		ret = mtk_cfg80211_set_monitor_index(mac_ad, ctrl_data->u8_val);
		if (ret)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_CLR)
		mtk_cfg80211_set_monitor_clear(mac_ad);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_STA0) {
		mtk_cfg80211_set_monitor_target(mac_ad, ctrl_data->data);
		mtk_cfg80211_set_monitor_index(mac_ad, 0);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_STA_ID) {
		struct mnt_sta *req_sta = (struct mnt_sta *)ctrl_data->data;

		if (ctrl_data->data_len < sizeof(struct mnt_sta))
			return -RTMP_IO_EINVAL;
		ret = mtk_cfg80211_set_monitor_sta_index(mac_ad, req_sta);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR]mtk_cfg80211_set_monitor_sta_index fail\n");
			return ret;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_MAX_PKT) {
		struct mnt_max_pkt *mnt_pkt = (struct mnt_max_pkt *)ctrl_data->data;

		if (ctrl_data->data_len < sizeof(struct mnt_max_pkt))
			return -RTMP_IO_EINVAL;
		mac_ad->MntMaxPktCnt = mnt_pkt->pkt_number;
		MTWF_DBG(mac_ad, DBG_CAT_AP, CATAP_MNT, DBG_LVL_NOTICE,
			"MntMaxPktCnt=%u\n", mac_ad->MntMaxPktCnt);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_GET_RESULT) {
		ret = mtk_cfg80211_get_air_mnt_result(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR]mtk_cfg80211_get_air_mnt_result fail\n");
			return ret;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MONITOR_GET_STA_STATS) {
		ret = mtk_cfg80211_get_air_mnt_sta_stats(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR]mtk_cfg80211_get_air_mnt_result fail\n");
			return ret;
		}
	}
	return 0;
#else /* AIR_MONITOR */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !AIR_MONITOR */
}

int cfg80211_drv_vndr_clean_sta(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct _MAC_TABLE_ENTRY *entry = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"mac_ad not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_STA_CLEAN_MAC) {
		UCHAR *mac_addr = ctrl_data->data;

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_VENDOR, DBG_LVL_NOTICE,
			"Clean STA: %02x:%02x:%02x:%02x:%02x:%02x\n", PRINT_MAC(mac_addr));
		entry = MacTableLookup(mac_ad, mac_addr);
		if (entry != NULL)
			mac_entry_delete(mac_ad, entry, TRUE);
	}
	return 0;
}

int cfg80211_drv_vndr_set_ba(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BA_DECLINE_INFO) {
		if (ctrl_data->u8_val == 0xf)
			ctrl_data->u8_val = wlan_config_get_ba_decline(wdev);
		else {
			wlan_config_set_ba_decline(wdev, ctrl_data->u8_val);
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_NOTICE,
				"(BADecline=%d)\n", wlan_config_get_ba_decline(wdev));
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_HT_BA_WSIZE_INFO) {
		ret = mtk_cfg80211_set_ht_ba_wsize(mac_ad, wdev, ctrl_data->ushort_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BA_EN_INFO) {
		if (ctrl_data->u8_val == 0xf)
			ctrl_data->u8_val = wlan_config_get_ba_enable(wdev);
		else
			mtk_cfg80211_set_ht_auto_ba(mac_ad, wdev, ctrl_data->u8_val);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BA_SETUP_INFO) {
		/* defined in mtk_vendor_nl80211.h */
		struct ba_mactid_param *ba_setup = (struct ba_mactid_param *)ctrl_data->data;

		ret = mtk_cfg80211_set_ba_setup(mac_ad, ba_setup->mac_addr, ba_setup->tid);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BA_ORITEARDOWN_INFO) {
		/* defined in mtk_vendor_nl80211.h */
		struct ba_mactid_param *ba_ori_td = (struct ba_mactid_param *)ctrl_data->data;

		ret = mtk_cfg80211_set_ba_ori_teardown(mac_ad, ba_ori_td->mac_addr, ba_ori_td->tid);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_BA_RECTEARDOWN_INFO) {
		/* defined in mtk_vendor_nl80211.h */
		struct ba_mactid_param *ba_rec_td = (struct ba_mactid_param *)ctrl_data->data;

		ret = mtk_cfg80211_set_ba_rec_teardown(mac_ad, ba_rec_td->mac_addr, ba_rec_td->tid);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int cfg80211_drv_vndr_set_action(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_BA, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_ACTION_OFFCHAN_ACTION_FRAME) {
#ifdef CHANNEL_SWITCH_MONITOR_CONFIG
		struct action_frm_data *off_ch_action_data;
		int len = ctrl_data->data_len;

		if (len < sizeof(struct action_frm_data)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR] invalid data length %d\n", ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}
		off_ch_action_data = (struct action_frm_data *)ctrl_data->data;
		if (len < (sizeof(struct action_frm_data) + off_ch_action_data->frm_len)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR] invalid frm length %u\n",
				off_ch_action_data->frm_len);
			return -RTMP_IO_EINVAL;
		}
		mtk_send_offchannel_action_frame(mac_ad, wdev, off_ch_action_data);
#endif /* CHANNEL_SWITCH_MONITOR_CONFIG */
	}


	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_ACTION_QOS_ACTION_FRAME) {
#ifdef QOS_R1
		struct action_frm_data *qos_action_data;
		int data_len;

		data_len = ctrl_data->data_len;
		if ((data_len < sizeof(*qos_action_data)) ||
		    (data_len > MAX_MGMT_PKT_LEN)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR] invalid data length %d\n", data_len);
			return -RTMP_IO_EINVAL;
		}

		qos_action_data = (struct action_frm_data *)ctrl_data->data;
		if (qos_action_data->frm_len != (data_len - sizeof(*qos_action_data))) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"failed - [ERROR] invalid frm length %u\n",
				qos_action_data->frm_len);
			return -RTMP_IO_EINVAL;
		}

		QoS_send_action_frame(mac_ad, wdev, qos_action_data);
#endif /* QOS_R1 */
	}
#ifdef CONFIG_MAP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_ACTION_STATIC_PUNCTURE_BITMAP) {
		MTWF_PRINT("%s static punc bitmap value from wapp:%d\n",
			__func__, ctrl_data->ushort_val);
		ret = mtk_nl80211_set_punc_bitmap(mac_ad, ctrl_data->ushort_val);
		if (ret) {
			MTWF_PRINT("%s failed - [ERROR]mtk_nl80211_set_punc_bitmap fail\n",
				__func__);
			return ret;
		}
	}
#endif /* CONFIG_MAP_SUPPORT */
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_ACTION_SEND_T2LM_REQUEST) {
		struct nl_80211_t2lm_cmd *t2lm_cmd;
		int data_len;

		data_len = ctrl_data->data_len;
		if (data_len != sizeof(*t2lm_cmd)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR] invalid t2lm_cmd length %d\n", data_len);
			return -RTMP_IO_EINVAL;
		}

		t2lm_cmd = (struct nl_80211_t2lm_cmd *)ctrl_data->data;
		mtk_cfg80211_nt2lm_request(mac_ad, wdev, t2lm_cmd);

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"%s, t2lm_cmd->ta_addr:%pM, t2lm_cmd->ra_addr: %pM\n",
				wdev->if_dev->name, t2lm_cmd->ta_addr, t2lm_cmd->ra_addr);
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_ACTION_SET_T2LM_MAPPING) {
		int data_len;

		data_len = ctrl_data->data_len;
		if (data_len != sizeof(struct nl_80211_at2lm_map)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR] invalid t2lm_cmd t2lmmap %d\n", data_len);
			return -RTMP_IO_EINVAL;
		}
		set_ap_at2lm_setting(mac_ad, ctrl_data->data);
	}
	return 0;
}

int cfg80211_drv_vndr_qos(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

#ifdef QOS_R1
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_QOS_UP_TUPLE_EXPIRED_NOTIFY) {
#ifdef MSCS_PROPRIETARY
		PMAC_TABLE_ENTRY pEntry;
		struct wapp_vend_spec_classifier_para_report *classifier_para = NULL;

		if (ctrl_data->data_len < sizeof(struct wapp_vend_spec_classifier_para_report))
			return -RTMP_IO_EINVAL;

		classifier_para = (struct wapp_vend_spec_classifier_para_report *)ctrl_data->data;

		if (classifier_para->id > MAX_QOS_PARAM_TBL) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
					"para id is out of range.\n");
		} else {
			pEntry = MacTableLookup(mac_ad, classifier_para->sta_mac);
			if (pEntry) {
				delete_qos_param(mac_ad, classifier_para->id);
				QoS_send_mscs_rsp(mac_ad, pEntry->wcid, 0,
					TCLAS_PROCESSING_TERMINATED, classifier_para);
			}
		}
#endif /* MSCS_PROPRIETARY */
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_PMK_BY_PEER_MAC) {
		PUCHAR pStaMacAddr = NULL;
		PMAC_TABLE_ENTRY  pEntry = NULL;

		pStaMacAddr = ctrl_data->data;
		if (ctrl_data->data_len != 6u) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"macAddrLen length check fail!\n");
			return -RTMP_IO_EINVAL;
		}

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"get pmk of STA:"MACSTR".\n", MAC2STR(pStaMacAddr));

		pEntry = MacTableLookup(mac_ad, pStaMacAddr);
		if (pEntry) {
			ctrl_data->rsp_data = &pEntry->SecConfig.PMK[0];
			ctrl_data->rsp_data_len = LEN_PMK;
		} else
			return -RTMP_IO_EINVAL;
	}
#ifdef QOS_R3
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_QOS_CHARATERISTICS_IE) {
		struct qos_characteristics *qosdata = NULL;

		qosdata = (struct qos_characteristics *)ctrl_data->data;
		if (ctrl_data->data_len < sizeof(*qosdata)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"qosdata length=%d check fail!\n", ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}
		QoS_set_qos_characteristics_ie(mac_ad, qosdata, wdev);
	}
#endif /* QOS_R3 */
#endif /* QOS_R1 */
#ifdef EPCS_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EPCS_STATE_SYNC) {
		struct epcs_sta *sta = NULL;

		sta = (struct epcs_sta *)ctrl_data->data;
		if (ctrl_data->data_len < sizeof(*sta)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"qosdata length=%d check fail!\n", ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}
		epcs_sta_state_sync(mac_ad, wdev, sta);
	}
#endif /* EPCS_SUPPORT */

	return 0;
}

int cfg80211_drv_vndr_offchannel_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef OFFCHANNEL_SCAN_FEATURE
	int i = 0;
	OFFCHANNEL_SCAN_MSG msg;
	int offchannel_len;
	SCAN_CTRL *ScanCtrl = NULL;
	int Status = CHANNEL_MONITOR_STRG_SUCCESS;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G) {
		struct DOT11_H *pDot11h = &mac_ad->Dot11_H;

		if (pDot11h && (pDot11h->ChannelMode == CHAN_SILENCE_MODE
				|| pDot11h->ChannelMode == CHAN_NOP_MODE)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
				"radio is down, can not trigger scan!!!\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SUBCMD_OFF_CHANNEL_INFO) {
		offchannel_len = ctrl_data->data_len;
		if (offchannel_len < sizeof(OFFCHANNEL_SCAN_MSG)) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
				"offchannel_len(=%d) error\n", offchannel_len);
			return -RTMP_IO_EINVAL;
		}

		memcpy(&msg,
			ctrl_data->data,
			offchannel_len);

		ScanCtrl = get_scan_ctrl_by_wdev(mac_ad, wdev);
		if (!ScanCtrl)
			return -RTMP_IO_EFAULT;
		memcpy(ScanCtrl->if_name, msg.ifrn_name, IFNAMSIZ);
		switch (msg.Action) {
		case GET_OFFCHANNEL_INFO:
		{
			/*To do OffChannelScan, need TakeChannelOpCharge first*/
			if (!TakeChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN, TRUE)) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
					"TakeChannelOpCharge fail for Off-Channel SCAN!!\n");
				return -RTMP_IO_EFAULT;
			}

			if (scan_in_run_state(mac_ad, wdev) == TRUE) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
					 "Scan in running State\n");
				ReleaseChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN);
				return -RTMP_IO_EBUSY;
			}

			if (ScanCtrl->state != OFFCHANNEL_SCAN_INVALID) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_WARN,
					"failed because offchannel scan is still ongoing\n");
				ReleaseChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN);
				return -RTMP_IO_EBUSY;
			} else {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
						"num of away channel to scan = %d\n",
						 msg.data.offchannel_param.Num_of_Away_Channel);

				if (msg.data.offchannel_param.Num_of_Away_Channel > MAX_AWAY_CHANNEL) {
					MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
						 "Invalid Argument\n");
					ReleaseChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SCAN);
					return -RTMP_IO_EINVAL;
				}

				ScanCtrl->Num_Of_Channels =
					msg.data.offchannel_param.Num_of_Away_Channel;
				ScanCtrl->Off_Ch_Scan_BW = msg.data.offchannel_param.bw;
				/* Fillup the parameters received for all channels */
				for (i = 0; i < msg.data.offchannel_param.Num_of_Away_Channel; i++) {
					ScanCtrl->ScanGivenChannel[i] =
						msg.data.offchannel_param.channel[i];
					/* TODO: Raghav: update on the first ch scan also */
					ScanCtrl->Offchan_Scan_Type[i] =
						msg.data.offchannel_param.scan_type[i];
					ScanCtrl->ScanTime[i] =
						msg.data.offchannel_param.scan_time[i];
					MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_INFO,
						"channel = %d:scan type = %d:scan time = %d\n",
						ScanCtrl->ScanGivenChannel[i],
						ScanCtrl->Offchan_Scan_Type[i],
						ScanCtrl->ScanTime[i]);
				}
#ifdef ZERO_PKT_LOSS_SUPPORT
				ScanCtrl->state = OFFCHANNEL_SCAN_START;
#endif
				Status = ApSiteSurveyNew_by_wdev(mac_ad,
					msg.data.offchannel_param.channel[0],
					msg.data.offchannel_param.scan_time[0],
					msg.data.offchannel_param.scan_type[0], FALSE, wdev);
			}
		}
			break;
		case TRIGGER_DRIVER_CHANNEL_SWITCH:
			/*To do set channel, need TakeChannelOpCharge first*/
			if (!TakeChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SET_CHN, TRUE)) {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
					"TakeChannelOpCharge fail for SET channel!!\n");
				return -RTMP_IO_EFAULT;
			}
			mac_ad->ApCfg.iwpriv_event_flag = TRUE;

			RTMP_OS_REINIT_COMPLETION(&mac_ad->ApCfg.set_ch_aync_done);
			Status = rtmp_set_channel(mac_ad, wdev, msg.data.channel_data.channel);

			/*non-csa flow should release ChannelOpCharge here,
			CSA flow will release ChannelOpCharge when CSA count down to zero*/
			if (mac_ad->ApCfg.set_ch_async_flag == FALSE) {
				mac_ad->ApCfg.iwpriv_event_flag = FALSE;
				ReleaseChannelOpCharge(mac_ad, wdev, CH_OP_OWNER_SET_CHN);
			}
			break;
		case UPDATE_DRIVER_SORTED_CHANNEL_LIST:
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				 "num of channels in sorted channel list received from App =%d\n",
				  msg.data.sorted_channel_list.size);
			if (msg.data.sorted_channel_list.size > 0) {
				mac_ad->sorted_list.size = msg.data.sorted_channel_list.size;
				for (i = 0; i < msg.data.sorted_channel_list.size; i++) {
					mac_ad->sorted_list.SortedMaxChannelBusyTimeList[i] = msg.data.sorted_channel_list.SortedMaxChannelBusyTimeList[i];
					MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
						"channel[%d] = %d\n",
						i,
						mac_ad->sorted_list.SortedMaxChannelBusyTimeList[i]);
					mac_ad->sorted_list.SortedMinChannelBusyTimeList[i] =
						msg.data.sorted_channel_list.SortedMinChannelBusyTimeList[i];
				}
				Status = CHANNEL_MONITOR_STRG_SUCCESS;
			} else {
				MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
				"num of channels in sorted channel list received is invalid\n");
				Status = CHANNEL_MONITOR_STRG_FAILURE;
			}
			break;
		default:
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
					"unknown action code. (%d)\n", msg.Action);
			break;
		}

		if (Status != CHANNEL_MONITOR_STRG_SUCCESS)
			return -RTMP_IO_EINVAL;
	}

	return 0;
#else /* OFFCHANNEL_SCAN_FEATURE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !OFFCHANNEL_SCAN_FEATURE */
}

int cfg80211_drv_vndr_dfs(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

#ifdef MT_DFS_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DFS_SET_ZERO_WAIT) {
		if (ctrl_data->data_len < sizeof(union dfs_zero_wait_msg))
			return -RTMP_IO_EINVAL;

		ZeroWaitDfsMsgHandle(mac_ad, (unsigned char *)ctrl_data->data);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DFS_GET_ZERO_WAIT) {
		union dfs_zero_wait_msg *msg;

		if (ctrl_data->rsp_data_len < sizeof(union dfs_zero_wait_msg))
			return -RTMP_IO_EINVAL;
		msg = (union dfs_zero_wait_msg *)ctrl_data->rsp_data;
#ifdef MAP_R2
		msg->aval_channel_list_msg.Action = QUERY_AVAL_CH_LIST;
#endif /* MAP_R2 */
		ZeroWaitDfsQueryMsgHandle(mac_ad, (char *)msg);
	}
#endif /* MT_DFS_SUPPORT */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DFS_CAC_STOP) {
		int i = 0;
		UCHAR BandIdx = 0;
		BOOLEAN bSupport5G = HcIsRfSupport(mac_ad, RFIC_5GHZ);

		BandIdx = HcGetBandByWdev(wdev);
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_INFO,
			 "OID_802_11_CAC_STOP!!\n");

#ifdef A_BAND_SUPPORT
		if (bSupport5G && (mac_ad->CommonCfg.bIEEE80211H == 1)) {
			BOOLEAN BandInCac[CFG_WIFI_RAM_BAND_NUM];
			struct DOT11_H *pDot11hTest = NULL;

			for (i = 0; i < CFG_WIFI_RAM_BAND_NUM; i++)
				BandInCac[i] = FALSE;

			pDot11hTest = &mac_ad->Dot11_H;
			if (pDot11hTest == NULL) {
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_DFS, DBG_LVL_ERROR,
					"pDot11hTest == NULL!\n");
				return -EINVAL;
			}

#ifdef MT_DFS_SUPPORT
			if (pDot11hTest->ChannelMode == CHAN_SILENCE_MODE) {
#ifdef MAP_R2
				BandInCac[BandIdx] = TRUE;
#endif /* MAP_R2 */
				pDot11hTest->RDCount = 0;
				MlmeEnqueue(mac_ad,
					DFS_STATE_MACHINE, DFS_CAC_END, 0, NULL,
					HcGetBandByWdev(wdev));
				UPDATE_CH_MODE(&pDot11hTest->ChannelMode, CHAN_NORMAL_MODE);
			}
		}
#endif /* MT_DFS_SUPPORT */
#endif /* A_BAND_SUPPORT */
	}
	return 0;
}

int cfg80211_drv_vndr_set_vlan(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef VLAN_SUPPORT
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_VLAN_EN_INFO) {
		ret = mtk_cfg80211_Set_VLANEn_Proc(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_VLAN_ID_INFO) {
		ret = mtk_cfg80211_Set_VLANID_Proc(mac_ad, ctrl_data->ushort_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_VLAN_PRIORITY_INFO) {
		ret = mtk_cfg80211_Set_VLANPriority_Proc(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_VLAN_TAG_INFO) {
		ret = mtk_cfg80211_Set_VLAN_TAG_Proc(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_VLAN_POLICY_INFO) {
		struct vlan_policy_param *param;

		param = (struct vlan_policy_param *)ctrl_data->data;

		ret = mtk_cfg80211_Set_VLAN_Policy_Proc(mac_ad, param->direction, param->policy);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /* VLAN_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !VLAN_SUPPORT */

}

int cfg80211_drv_vndr_set_mbo(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef MBO_SUPPORT
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MBO, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_MBO_NPC) {
		UCHAR npc_value[MBO_NPC_LEN] = {0};

		if (ctrl_data->data_len != MBO_NPC_LEN)
			return -RTMP_IO_EINVAL;

		memcpy(npc_value, ctrl_data->data, ctrl_data->data_len);

		ret = mtk_cfg80211_Set_mbo_npc_Proc(mac_ad, npc_value);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /* MBO_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !MBO_SUPPORT */
}

int cfg80211_drv_vndr_mlo_preset_linkid(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_SEC, CATSEC_MLO, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MLO_PRESET_LINKID_INFO) {
		ret = mtk_cfg80211_mlo_preset_link_id(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

int cfg80211_drv_vndr_tx_power(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_PERCENTAGE) {
		ret = mtk_cfg80211_set_txpower(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_MAX) {
		ret = mtk_cfg80211_set_maxtxpwr(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_INFO) {
		ret = mtk_cfg80211_set_txpwr_info(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_PERCENTAGE_EN) {
		ret = mtk_cfg80211_set_percentage_ctrl(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_DROP_CTRL) {
		ret = mtk_cfg80211_set_pwr_drop_ctrl(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_DECREASE) {
		ret = mtk_cfg80211_set_decrease_pwrctrl(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_SKU_CTRL) {
		ret = mtk_cfg80211_set_sku_ctrl(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_SKU_INFO) {
		ret = mtk_cfg80211_set_sku_info(mac_ad);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_MU) {
		struct mu_power_param *ppwr;

		ppwr = (struct mu_power_param *)ctrl_data->data;
		ret = mtk_cfg80211_set_mutxpwr(mac_ad, ppwr->en, ppwr->value);
		if (ret < 0)
			return ret;
	}
#ifdef MGMT_TXPWR_CTRL
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_MGMT) {
		CHAR *pMgmtPwr;
		int len;

		pMgmtPwr = ctrl_data->data;
		len = ctrl_data->data_len;

		if (strlen(pMgmtPwr) > len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid pMgmtPwr length%lu,%d\n",
				strlen(pMgmtPwr), len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_cfg80211_set_mgmt_frame_power(mac_ad, pMgmtPwr);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_GET_MGMT)
		ctrl_data->u8_val =
			mac_ad->ApCfg.MgmtTxPwr + mac_ad->ApCfg.EpaFeGain + (wdev->TxPwrDelta);
#endif /* MGMT_TXPWR_CTRL */
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_PSU_SKU_CTRL) {
		ret = mtk_cfg80211_set_psu_sku_ctrl(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TXPWR_PSU_SKU_TABLE_IDX) {
		ret = mtk_cfg80211_set_psu_sku_tbl_idx(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int cfg80211_drv_vndr_edca(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TX_BURST) {
		ret = mtk_cfg80211_set_txburst_proc(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int cfg80211_drv_vndr_set_vie(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CUSTOMER_VENDOR_IE_SUPPORT
	int ret;
	INT ifIndex = wdev->func_idx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_ERROR,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_BEACON_VIE) {
		if (wdev->wdev_type != WDEV_TYPE_AP) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"error: only ap ineterface can config beacon vie!\n");
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_cfg80211_config_beacon_vie(
			mac_ad, ifIndex, ctrl_data->data, ctrl_data->data_len);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_PROBE_RSP_VIE) {
		if (wdev->wdev_type != WDEV_TYPE_AP) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"error: only ap ineterface can config beacon vie!\n");
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_cfg80211_config_probe_rsp_vie(
			mac_ad, ifIndex, ctrl_data->data, ctrl_data->data_len);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_PROBE_REQ_VIE) {
		if (wdev->wdev_type != WDEV_TYPE_STA) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"error: only apcli ineterface can config beacon vie!\n");
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_cfg80211_config_probe_req_vie(
			mac_ad, ifIndex, ctrl_data->data, ctrl_data->data_len);

		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_OUI_FILTER) {
		if (wdev->wdev_type != WDEV_TYPE_AP) {
			MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_SCAN, DBG_LVL_NOTICE,
				"error: only ap ineterface can config OUI!\n");
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_cfg80211_config_oui_filter(
			mac_ad, ifIndex, ctrl_data->data, ctrl_data->data_len);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /* CUSTOMER_VENDOR_IE_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !CUSTOMER_VENDOR_IE_SUPPORT */
}

int cfg80211_drv_vndr_set_radio_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int apidx = wdev->func_idx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"failed - [ERROR]can't find wdev in driver MBSS.\n");
		return -RTMP_IO_EINVAL;
	} else if (apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"apidx is invalid\n");
		return -RTMP_IO_EINVAL;
	}
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		"apidx is %d.\n", apidx);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_RADIO_SET_STATS_MEASURING_METHOD) {
		struct wifi_radio_stats_measure *stats_measure;

		if (ctrl_data->data_len < sizeof(struct wifi_radio_stats_measure))
			return -RTMP_IO_EINVAL;

		stats_measure = (struct wifi_radio_stats_measure *)ctrl_data->data;
		mac_ad->radio_measure_rate = stats_measure->radio_stats_measuring_rate;
		mac_ad->radio_measure_interval = stats_measure->radio_stats_measuring_interval;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_RADIO_SET_MEASURE_ENABEL)
		mac_ad->radio_measure_enable = ctrl_data->u8_val;

	return 0;
}

int cfg80211_drv_vndr_get_radio_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int apidx = wdev->func_idx, ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (apidx == WDEV_NOT_FOUND) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"failed - [ERROR]can't find wdev in driver MBSS.\n");
		return -RTMP_IO_EINVAL;
	} else if (apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"apidx is invalid\n");
		return -RTMP_IO_EINVAL;
	}
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		"apidx is %d.\n", apidx);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_RADIO_STATS) {
		ret = mtk_cfg80211_reply_radio_stats(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_radio_stats failed. ret=%x\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RADIO_CPU_TEMPRETURE) {
		RTMP_GET_TEMPERATURE(mac_ad, HcGetBandByWdev(wdev), &mac_ad->temperature);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
				 "CurrentTemperature = %d\n", mac_ad->temperature);
		ctrl_data->uint_val = mac_ad->temperature;
	}

#ifdef ANDLINK_V4_0
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RADIO_BSS_INFO_ANDLINK_FORMAT) {
		ret = mtk_cfg80211_reply_andlink_bss_info(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_cfg80211_reply_andlink_bss_info fail.\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif/*ANDLINK_V4_0*/

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_RADIO_RSSI) {
		PhyStatGetRssi(
			mac_ad, HcGetBandByWdev(wdev),
			ctrl_data->rsp_data, (UINT8 *)&ctrl_data->rsp_data_len);
	}

	return ret;
}

int cfg80211_drv_vndr_get_sta(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

#ifdef TR181_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_STA) {
		ret = mtk_cfg80211_reply_sta(mac_ad, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"mtk_cfg80211_reply_sta failed.ret=%x\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}
#endif /* TR181_SUPPORT */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_STA_MAC) {
		ret = mtk_cfg80211_get_sta(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"mtk_cfg80211_reply_sta failed.ret=%x\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}

#ifdef ANDLINK_V4_0
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_STA_INFO_ANDLINK_FORMAT) {
		ret = mtk_cfg80211_reply_andlink_sta_info(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"mtk_cfg80211_reply_andlink_sta_info failed.\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif/*ANDLINK_V4_0*/

	return ret;
}

int cfg80211_drv_vndr_hqa(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_WLAN_SERVICE
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (VIRTUAL_IF_NUM(mac_ad) == 0) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "No intf up in band\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_HQA) {
		ret = parse_hqa_cmd_frame(mac_ad, ctrl_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_wapp_event failed.\n");
	} else
		return -RTMP_IO_EINVAL;

	return 0;
#else /* CONFIG_WLAN_SERVICE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !CONFIG_WLAN_SERVICE */
}

int cfg80211_drv_vndr_set_v10converter(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONVERTER_MODE_SWITCH_SUPPORT
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_V10CONVERTER]) {
		ret = mtk_cfg80211_Set_V10ConverterMode(mac_ad, ctrl_data->u8_val);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /*CONVERTER_MODE_SWITCH_SUPPORT*/
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !CONVERTER_MODE_SWITCH_SUPPORT */
}

int cfg80211_drv_vndr_set_apropxyrefresh(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef A4_CONN
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SET_APPROXYREFRESH) {
		/* Enable a4 need refresh based on input from application */
		mac_ad->a4_need_refresh = ctrl_data->u8_val;
	}

	return 0;
#else /* A4_CONN */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !A4_CONN */

}

int cfg80211_drv_vndr_e2p(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = 0, i = 1;
	UINT32 eepAddr, e2p_s = 0, e2p_e = 0;
	UINT32 max_len = MT_EEPROM_SIZE;
	USHORT e2pVal = 0;
	RTMP_STRING *msg;
	struct e2p_param *param;
	int msg_len = ctrl_data->rsp_data_len;
	int free_len = max_len;
	struct _RTMP_CHIP_CAP *chip_cap;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	chip_cap = hc_get_chip_cap(mac_ad->hdev_ctrl);
	if (max_len > msg_len)
		free_len = (max_len - msg_len);
	else
		goto error;
	msg = (RTMP_STRING *)ctrl_data->rsp_data;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
		"%d nla_type(attr)=%d\n", __LINE__, ctrl_data->sub_id);
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_E2P_WRITE_PARAM &&
		ctrl_data->data_len >= sizeof(*param)) {
		param = (struct e2p_param *)ctrl_data->data;
		eepAddr = param->start;
		e2pVal = param->value;

		if (eepAddr > get_dev_eeprom_size(mac_ad->physical_dev)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"start addr (%u) > eeprom size (%u)\n", eepAddr, get_dev_eeprom_size(mac_ad));
			goto error;
		}

		RT28xx_EEPROM_WRITE16(mac_ad, eepAddr, e2pVal);
		ret = snprintf(msg + msg_len, free_len, "[0x%04X]:0x%04X  ", eepAddr, e2pVal);
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}

		msg_len += ret;
		free_len -= ret;
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
			"E2p=0x%x, e2pVal=0x%x\n", eepAddr, e2pVal);
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_E2P_SHOW_PARAM &&
		ctrl_data->data_len >= sizeof(*param)) {
		param = (struct e2p_param *)ctrl_data->data;
		e2p_s = param->start;
		e2p_e = param->end;
		for (eepAddr = e2p_s; eepAddr <= e2p_e; eepAddr += 2) {
			RT28xx_EEPROM_READ16(mac_ad, eepAddr, e2pVal);
			ret = snprintf(msg + msg_len, free_len,
				"[0x%04X]:0x%04X  %s", eepAddr, e2pVal, ((i % 4 == 0)?"\n" : ""));
			if (os_snprintf_error(free_len, ret)) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
					"msg snprintf error!\n");
				goto error;
			}
			i += 1;
			msg_len += ret;
			free_len -= ret;
		}
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_E2P_DUMP_ALL_PARAM) {
		e2p_s = 0;
		e2p_e = chip_cap->EEPROM_DEFAULT_BIN_SIZE;

		if (e2p_e >= MT_EEPROM_SIZE)
			e2p_e = MT_EEPROM_SIZE;

		for (eepAddr = e2p_s; eepAddr <= e2p_e; eepAddr += 2) {
			RT28xx_EEPROM_READ16(mac_ad, eepAddr, e2pVal);
			MTWF_PRINT("[0x%04X]:%04X  %s",
				eepAddr, e2pVal, ((eepAddr & 0x6) == 0x06) ? "\n" : "");
		}

		ret = snprintf(msg, free_len, "dump done");
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
	}

	if (msg_len == 0) {
		ret = snprintf(msg, free_len, "===>Error command format!");
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
	}
	ctrl_data->rsp_data_len = msg_len;
	return 0;
error:
	return -EFAULT;
}

int cfg80211_drv_vndr_testengine(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef CONFIG_WLAN_SERVICE
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	int ret = 0;
	UINT32 rsp_data = 0;
	UINT32 max_len = MT_MAX_SERV_IOCTLBUFF;
	RTMP_STRING *msg;
	struct testengine_param *param; /* defined in mtk_vendor_nl80211.h */
	int msg_len = ctrl_data->rsp_data_len;
	int free_len = max_len;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (max_len > msg_len)
		free_len = (max_len - msg_len);
	else
		goto error;
	msg = (RTMP_STRING *)ctrl_data->rsp_data;

	MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_INFO,
		"%d nla_type(attr)=%d\n", __LINE__, ctrl_data->sub_id);
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TESTENGINE_SET &&
		ctrl_data->data_len >= sizeof(*param)) {
		param = (struct testengine_param *)ctrl_data->data;

		ret = MtCmdSetTestEngine(mac_ad, param->atidx, param->value);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_INFO,
			"Set ret: 0x%x, atidx: %d  Data: 0x%x\n", ret, param->atidx, param->value);

		ret = snprintf(msg + msg_len, free_len,
			"set=> atidx:%u   value:%u\n", param->atidx, param->value);
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}

		msg_len += ret;
		free_len -= ret;
	} else if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_TESTENGINE_GET &&
		ctrl_data->data_len >= sizeof(*param)) {
		param = (struct testengine_param *)ctrl_data->data;

		ret = MtCmdGetTestEngine(mac_ad, param->atidx, param->value, &rsp_data);
		if (ret)
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_INFO,
			"Get ret: 0x%x, atidx: %d  Data: 0x%x\n", ret, param->atidx, param->value);

		ret = snprintf(msg + msg_len, free_len,
			"get=> atidx:%u   value:%u\n", param->atidx, rsp_data);
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
	}

	if (msg_len == 0) {
		ret = snprintf(msg, free_len, "===>Error command format!");
		if (os_snprintf_error(free_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_ATE, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			goto error;
		}
		msg_len += ret;
		free_len -= ret;
	}
	ctrl_data->rsp_data_len = msg_len;
	return 0;
error:
	return -RTMP_IO_EFAULT;
#else /* CONFIG_WLAN_SERVICE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !CONFIG_WLAN_SERVICE */
}

int cfg80211_drv_vndr_mlo_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MLO_INFO_SHOW_CMD_STR) {
		if (ctrl_data->u8_val == 1)
			show_sta_mlo_link_info(mac_ad, NULL);
		else {
			MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_WARN,
					"error value=%d\n", ctrl_data->u8_val);
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DUMP_APCLI_MLD) {
		ret = mtk_cfg80211_reply_apcli_mld(mac_ad, ctrl_data);
		if (ret)
			return -RTMP_IO_EINVAL;
	}

	return 0;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

int cfg80211_drv_vndr_mlo_switch(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_MLO_SWITCH_SET)
		mtk_cfg80211_vndr_cmd_set_mlo_switch_attributes(mac_ad, ctrl_data->u8_val);

	return 0;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

int cfg80211_drv_vndr_get_bss_mlo_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	int ret = 0;
	UCHAR mld_index = 0, *mld_mac;
	struct bmgr_mlo_dev *mld;
	struct query_mld_basic mld_basic = {0};
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->func_idx == WDEV_NOT_FOUND) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"failed - [ERROR]can't find wdev in driver MBSS.\n");
		return -RTMP_IO_EINVAL;
	} else if (wdev->func_idx >= MAX_BEACON_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR, "apidx is invalid.\n");
		return -RTMP_IO_EINVAL;
	}

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATCFG_DBGLOG, DBG_LVL_INFO,
		"apidx is %d.\n", wdev->func_idx);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_BSS_MLO_INFO) {
		ret = mtk_cfg80211_reply_bss_mlo_info(mac_ad, wdev, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"mtk_cfg80211_reply_bss_mlo_info failed. ret=%x.\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}

	/*get ap mld information by mld group index*/
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MLD_INDEX)
		mld_index = ctrl_data->u8_val;

	/*get ap mld information by mld mac*/
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MLD_ADDRESS) {
		UINT8 mld_mac_len = ctrl_data->data_len;

		mld_mac = ctrl_data->data;
		if (mld_mac && mld_mac_len == MAC_ADDR_LEN) {
			COPY_MAC_ADDR(mld_basic.addr, mld_mac);
			if (bss_mngr_query_mld_basic(&mld_basic) != NDIS_STATUS_SUCCESS) {
				MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
					"MLD Lookup failed: MLD MAC (%pM)\n", mld_mac);
				return -RTMP_IO_EINVAL;
			}

			mld_index = mld_basic.mld_grp_idx;
		}
	}

	if (mld_index) {
		if (!BMGR_VALID_MLD_GRP_IDX(mld_index)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"invalid mld index=%u\n", mld_index);
			return -RTMP_IO_EINVAL;
		}
#ifdef MLD_GROUP_ID_REMAPPING_V1
		mld = mld_idx_mapping_v1_load(mld_index);
#else
		mld = GET_MLD_BY_GRP_IDX(mld_index);
#endif
		if (!BMGR_VALID_MLO_DEV(mld)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"invalid mld\n");
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_cfg80211_reply_ap_mld(mld, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_bss_mlo_info failed. ret=%x.\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}

	/*dump all ap mld information*/
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MLD_DUMP) {
		if (ctrl_data->ushort_val >= BMGR_MAX_MLD_GRP_CNT) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_VENDOR, DBG_LVL_ERROR,
				"Invalid mld grp idx(=%d)\n", ctrl_data->ushort_val);
			return -RTMP_IO_EINVAL;
		}
#ifdef MLD_GROUP_ID_REMAPPING_V1
		mld = mld_idx_mapping_v1_load(ctrl_data->ushort_val);
#else
		mld = GET_MLD_BY_GRP_IDX(ctrl_data->ushort_val);
#endif
		if (!BMGR_VALID_MLO_DEV(mld)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
				"invalid mld\n");
			return 1;
		}
		if (mld->mld_type != BMGR_MLD_TYPE_MULTI)
			return 1;
		ret = mtk_cfg80211_reply_ap_mld(mld, ctrl_data);
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
				"reply mld[%u] %02x:%02x:%02x:%02x:%02x:%02x\n",
				mld->mld_grp, PRINT_MAC(mld->mld_addr));
		return ret;
	}

	return ret;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

static int cfg80211_drv_vndr_offload_acs(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct mt_phy_cap *phy_cap;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}

	if (ctrl_data->rsp_data_len < sizeof(struct mt_phy_cap))
		return -RTMP_IO_EINVAL;

	phy_cap = (struct mt_phy_cap *)ctrl_data->rsp_data;
	phy_cap->channel = wdev->channel;
	if (WMODE_CAP_6G(wdev->PhyMode))
		phy_cap->phy_mode = MTK_PHY_MODE_6G;
	else if (WMODE_CAP_2G(wdev->PhyMode))
		phy_cap->phy_mode = MTK_PHY_MODE_2G;
	else if (WMODE_CAP_5G(wdev->PhyMode))
		phy_cap->phy_mode = MTK_PHY_MODE_5G;

	return 0;
}

int cfg80211_drv_vndr_bh_status(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DFS_SLAVE_SUPPORT
	struct BH_STATUS_MSG *pMsg = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_DFS, DBG_LVL_ERROR,
			"mac_ad not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_DFS_SLAVE_BH_STATUS) {
		if (ctrl_data->data_len < sizeof(struct BH_STATUS_MSG))
			return -RTMP_IO_EINVAL;

		pMsg = (struct BH_STATUS_MSG *)ctrl_data->data;

		if (pMsg->fBhStatus)
			slave_bcn_ctrl(mac_ad, TRUE);
		else
			slave_bcn_ctrl(mac_ad, FALSE);
	}

	return 0;
#else /* DFS_SLAVE_SUPPORT */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DFS_SLAVE_SUPPORT */
}

int cfg80211_drv_vndr_get_connected_sta_mld(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef DOT11_EHT_BE
	int apidx = 0, ret = 0, i;
	UCHAR mld_index;
	struct bmgr_mlo_dev *mld;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		if (wdev->func_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
				"invalid ioctl_if %d\n", wdev->func_idx);
			return -RTMP_IO_EFAULT;
		}
		apidx = wdev->func_idx;
	} else {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_WARN,
			"no this device.(inf_type=0x%x, inf_idx=%d)\n",
			wdev->wdev_type, wdev->func_idx);
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(mac_ad, DBG_CAT_AP, CATCFG_DBGLOG, DBG_LVL_INFO, "apidx is %d.\n", apidx);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_AP_MLD_INDEX_TO_DUMP) {
		mld_index = ctrl_data->u8_val;
		if (!BMGR_VALID_MLD_GRP_IDX(mld_index)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"invalid mld index=%u\n", mld_index);
			return -EINVAL;
		}
#ifdef MLD_GROUP_ID_REMAPPING_V1
		mld = mld_idx_mapping_v1_load(mld_index);
#else
		mld = GET_MLD_BY_GRP_IDX(mld_index);
#endif
		if (!BMGR_VALID_MLO_DEV(mld)) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"invalid mld\n");
			return -EINVAL;
		}

		ret = mtk_cfg80211_reply_connected_sta_mld(mac_ad, ctrl_data, mld);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"reply_connected_sta_mld failed. ret=%x.\n", ret);
			return -EINVAL;
		}
	} else {
		for (i = 0; i < BMGR_MAX_MLD_GRP_CNT; i++) {
#ifdef MLD_GROUP_ID_REMAPPING_V1
			mld = mld_idx_mapping_v1_load(i);
#else
			mld = GET_MLD_BY_GRP_IDX(i);
#endif
			if (!BMGR_VALID_MLO_DEV(mld)) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_INFO,
					"invalid mld\n");
				continue;
			}
			if (mld->mld_type != BMGR_MLD_TYPE_MULTI)
				continue;

			ret = mtk_cfg80211_reply_connected_sta_mld(mac_ad, ctrl_data, mld);
			if (ret) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
					"reply_connected_sta_mld failed. ret=%x.\n", ret);
				continue;
			}
		}
	}

	return ret;
#else /* DOT11_EHT_BE */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !DOT11_EHT_BE */
}

int cfg80211_drv_vndr_swaci_lna(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
#ifdef SWACI_MECHANISM
	int ret;
	UINT32 len;
	INT *data = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_ENABLE) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("enable data:%d, len:%d", data[0], len);

		ret = mtk_cfg80211_set_swaci_lna_enable(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_SCENARIO) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("scenario data:%d, %d, %d, %d, len:%d",
			data[0], data[1], data[2], data[3], len);

		ret = mtk_cfg80211_set_swaci_lna_scenario(mac_ad, data, len);

		if (ret < 0)
			return ret;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_DUTY_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("duty data:%d, %d, len:%d", data[0], data[1], len);

		ret = mtk_cfg80211_set_swaci_lna_duty_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_SCEN_ADC_RCPI_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Cond D data:%d, %d, len:%d", data[0], data[1], len);

		ret = mtk_cfg80211_set_swaci_lna_scen_adc_rcpi_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_ACI_IB_RCPI_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Cond E data:%d, len:%d", data[0], len);

		ret = mtk_cfg80211_set_swaci_lna_aci_ib_rcpi_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_AACI_IB_RCPI_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Cond F data:%d, %d, len:%d", data[0], data[1], len);

		ret = mtk_cfg80211_set_swaci_lna_aaci_ib_rcpi_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_INST_IB_RCPI_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Cond G data:%d, len:%d", data[0], len);

		ret = mtk_cfg80211_set_swaci_lna_inst_ib_rcpi_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_INTERF_CNT_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Interf Cnt data:%d, %d, %d, len:%d ", data[0], data[1], data[2], len);

		ret = mtk_cfg80211_set_swaci_lna_interf_cnt_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_VALID_RCPI_TH) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("ValideRcpi data:%d, %d, %d, %d, len:%d ",
				data[0], data[1], data[2], data[3], len);

		ret = mtk_cfg80211_set_swaci_lna_valid_rcpi_th(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_SWACI_LNA_CONF) {
		len = ctrl_data->data_len;
		data = ctrl_data->data;
		MTWF_PRINT("Conf data:%d, len:%d", data[0], len);

		ret = mtk_cfg80211_set_swaci_lna_conf(mac_ad, data, len);
		if (ret < 0)
			return ret;
	}

	return 0;
#else /* SWACI_MECHANISM */
	return -RTMP_IO_EOPNOTSUPP;
#endif /* !SWACI_MECHANISM */
}

static int
cfg80211_drv_vndr_mld_create_multi_grp(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	u8 tgt_mld_grp_idx)
{
	int ret;
	struct eht_mld_param mld_param = {0};
	u8 mld_addr[MAC_ADDR_LEN];
	u8 mld_type = BMGR_MLD_TYPE_MULTI;
	struct _BSS_STRUCT *pMbss = NULL;

	pMbss = &pAd->ApCfg.MBSSID[wdev->func_idx];
	pMbss->mld_addr_by_cfg = false;
	pMbss->unique_mld_addr_enable = true;

	ret = eht_ap_mld_available_address_search(
			wdev,
			tgt_mld_grp_idx,
			mld_addr,
			mld_type);
	if (!ret) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"unique mld addr get failed for mld grp idx:%u!!\n", tgt_mld_grp_idx);
		return NDIS_STATUS_FAILURE;
	}

	eht_ap_mld_fill_mld_param_from_selected_link(wdev, &mld_param);

	ret = eht_ap_mld_create(wdev,
			&tgt_mld_grp_idx,
			mld_addr,
			&mld_param,
			&mld_type);

	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"multi-link mld grp create failed: idx:%u!!\n", tgt_mld_grp_idx);
	}

	return ret;
}

static int
cfg80211_drv_vndr_set_ap_mld_index_single2multi(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct eht_mld_ap_info *src_mld_ptr,
	u8 tgt_mld_grp_idx,
	struct eht_mld_ap_info *tgt_mld_ptr)
{
	int ret;

	if (!tgt_mld_ptr) {
		ret = cfg80211_drv_vndr_mld_create_multi_grp(pAd, wdev, tgt_mld_grp_idx);
		if (ret != 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"multi-link mld grp create failed: idx:%u!!\n", tgt_mld_grp_idx);
			return ret;
		}
	}

	return eht_ap_mld_link_transfer(wdev, tgt_mld_grp_idx);
}

static int
cfg80211_drv_vndr_set_ap_mld_index_multi2multi(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct eht_mld_ap_info *src_mld_ptr,
	u8 tgt_mld_grp_idx,
	struct eht_mld_ap_info *tgt_mld_ptr)
{
	int ret;

	if (!tgt_mld_ptr) {
		ret = cfg80211_drv_vndr_mld_create_multi_grp(pAd, wdev, tgt_mld_grp_idx);
		if (ret != 0) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"multi-link mld grp create failed: idx:%u!!\n",
				tgt_mld_grp_idx);
			return ret;
		}
	}

	return eht_ap_mld_link_transfer(wdev, tgt_mld_grp_idx);
}

static int
cfg80211_drv_vndr_set_ap_mld_index(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	BSS_STRUCT *pMbss;
	u8 src_mld_grp_idx;
	u8 tgt_mld_grp_idx;
	struct eht_mld_ap_info src_mld;
	struct eht_mld_ap_info tgt_mld;
	struct eht_mld_ap_info *src_mld_ptr = NULL;
	struct eht_mld_ap_info *tgt_mld_ptr = NULL;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	pMbss = (struct _BSS_STRUCT *)wdev->func_dev;
	src_mld_grp_idx = pMbss->mld_grp_idx;
	tgt_mld_grp_idx = ctrl_data->u8_val;

	if (tgt_mld_grp_idx != 0) {
		if (tgt_mld_grp_idx > MAX_MLD_NUM) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"invalid mld_grp_idx:%u!!\n", tgt_mld_grp_idx);
			return -RTMP_IO_EINVAL;
		}
		ret = eht_ap_mld_get(wdev, tgt_mld_grp_idx, &tgt_mld);
		if (ret == 0)
			tgt_mld_ptr = &tgt_mld;
		else
			tgt_mld_ptr = NULL;
	}

	if (src_mld_grp_idx != 0) {
		ret = eht_ap_mld_get(wdev, src_mld_grp_idx, &src_mld);
		if (ret != 0) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
				"mld get failed, mld_grp_idx:%u!!\n", src_mld_grp_idx);
			return ret;
		}
		src_mld_ptr = &src_mld;
	}

	/* tgt is single */
	if (tgt_mld_grp_idx == 0) {
		/* src is multi */
		if (src_mld_ptr && src_mld_ptr->mld_type == BMGR_MLD_TYPE_MULTI) {
			ret = eht_ap_mld_link_transfer(wdev, 0);
			if (ret != 0) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
					"change to singlelink mld failed!!\n");
				return ret;
			}
		} else {
			return 0;
		}
	} else { /* tgt is multi */
		if (tgt_mld_ptr) {
			int link_idx;
			struct wifi_dev *sibling_wdev;

			for (link_idx = 0; link_idx < MLD_LINK_MAX; link_idx++) {
				if (!tgt_mld_ptr->aps[link_idx].valid)
					continue;
				sibling_wdev = WdevSearchByBssid(mac_ad,
						tgt_mld_ptr->aps[link_idx].bssid);
				if (!sibling_wdev)
					continue;
				ret = eht_ap_mld_link_transfer(sibling_wdev, 0);
				if (ret != 0) {
					MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
						"change sibling wdev to singlelink mld failed!!\n");
					return ret;
				}
			}
		}
		if (src_mld_ptr && src_mld_ptr->mld_type == BMGR_MLD_TYPE_MULTI) {
			if (src_mld_ptr->mld_grp == tgt_mld_grp_idx)
				return 0;
			ret = cfg80211_drv_vndr_set_ap_mld_index_multi2multi(
								mac_ad,
								wdev,
								src_mld_ptr,
								tgt_mld_grp_idx,
								tgt_mld_ptr);
		} else {
			ret = cfg80211_drv_vndr_set_ap_mld_index_single2multi(
								mac_ad,
								wdev,
								src_mld_ptr,
								tgt_mld_grp_idx,
								tgt_mld_ptr);
		}
	}


	return ret;
}

static int
cfg80211_drv_vndr_set_ap_mld_address(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	UINT32 mld_addr_len;
	u8 *mld_addr = NULL;
	u8 mld_grp_idx;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	mld_addr_len = ctrl_data->data_len;
	mld_addr = ctrl_data->data;
	mld_grp_idx = ctrl_data->u8_val;

	if (!mld_addr || mld_addr_len != MAC_ADDR_LEN) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"invalid mld_addr:%p, mld_addr_len:%u!!\n", mld_addr, mld_addr_len);
		return NDIS_STATUS_FAILURE;
	}

	ret = eht_ap_mld_set_addr(wdev, mld_grp_idx, mld_addr);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"mld addr set failed, mld_grp_idx:%u!!\n", mld_grp_idx);
	}

	return ret;
}

static int
cfg80211_drv_vndr_set_ap_mld_eml_mode(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	u8 mld_grp_idx;
	struct eht_mld_op_ctrl op_ctrl = {0};
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	mld_grp_idx = ctrl_data->u8_val;
	op_ctrl.pres_bmap |= MLD_OP_PARAM_PRES_GROUP_IDX;
	op_ctrl.mld_grp = mld_grp_idx;

	if (ctrl_data->uint_val == MTK_NL80211_VENDOR_ATTR_AP_MLD_EMLSR)
		op_ctrl.mld_param.eml_mode = EMLSR;
	else if (ctrl_data->uint_val == MTK_NL80211_VENDOR_ATTR_AP_MLD_EMLMR)
		op_ctrl.mld_param.eml_mode = EMLMR;
	else
		op_ctrl.mld_param.eml_mode = EML_DISABLE;
	op_ctrl.pres_bmap |= MLD_OP_PARAM_PRES_EML_MODE;

	ret = eht_ap_mld_attr_set(wdev, mld_grp_idx, &op_ctrl);
	if (ret != NDIS_STATUS_SUCCESS) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_WARN,
			"mld attr set failed, mld_grp_idx:%u!!\n", mld_grp_idx);
	}

	return ret;
}

static int
cfg80211_drv_vndr_set_ap_mld(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	switch (ctrl_data->sub_id) {
	case MTK_NL80211_VENDOR_ATTR_SET_AP_MLD_INDEX:
		return cfg80211_drv_vndr_set_ap_mld_index(mac_ad, wdev, ctrl_data);
	case MTK_NL80211_VENDOR_ATTR_SET_AP_MLD_ADDRESS:
		return cfg80211_drv_vndr_set_ap_mld_address(mac_ad, wdev, ctrl_data);
	case MTK_NL80211_VENDOR_ATTR_SET_AP_MLD_EML_MODE:
		return cfg80211_drv_vndr_set_ap_mld_eml_mode(mac_ad, wdev, ctrl_data);
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int cfg80211_drv_vndr_set_zr_hdo_rm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
#ifdef ZERO_HANDOFF_ROAMING
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	UINT32 len;
	UCHAR *data = NULL;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	len = ctrl_data->data_len;
	data = ctrl_data->data;


	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_SET_ALMAC)
		memcpy(mac_ad->physical_dev->al_mac, data, len);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_START_ROAMING)
		ret = mtk_cfg80211_trigger_start_zr_hd_rm(mac_ad, wdev, data, len);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_ADD_STA)
		ret = mtk_cfg80211_add_vbss_entry(mac_ad, wdev, data, len);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_DELETE_STA) {

		struct _MAC_TABLE_ENTRY *pEntry;


		pEntry = MacTableLookup(mac_ad, data);
		if (!pEntry) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"pEntry is already delete, ignore\n");
		return RTMP_IO_EINVAL;
		}
		mac_entry_delete(mac_ad, pEntry, TRUE);

	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_SET_VBSS) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_SET_VBSS\n");
		set_vbss(mac_ad, wdev);
		UpdateBeaconHandler(mac_ad, wdev, BCN_UPDATE_IE_CHG);
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_SET_AID) {
		unsigned short *aid;

		aid = (unsigned short *)data;

		MTWF_PRINT(" start aid:%d,end aid: %d, len:%d", aid[0], aid[1], len);
		ret = mtk_cfg80211_set_zf_bss_aid(mac_ad, wdev, aid[0], aid[1]);
	}
#endif
	return ret;
}
int cfg80211_drv_vndr_get_zr_hdo_rm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0;
#ifdef ZERO_HANDOFF_ROAMING
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;


	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
			"pAd not start up. return!!\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_ZR_HDO_RM_GET_STA_INFO)
		ret = mtk_cfg80211_reply_sta_info(mac_ad, wdev, ctrl_data);
#endif
	return ret;
}


struct cfg80211_drv_vndr_cmd_handler drv_vndr_cmd_handler[] = {
	{mt_cfg80211_vndr_attr_show, cfg80211_drv_vndr_show_handler},
	{mt_cfg80211_vndr_attr_set, cfg80211_drv_vndr_set_handler},
	{mt_cfg80211_vndr_attr_mac, cfg80211_drv_vndr_mac_handler},
	{mt_cfg80211_vndr_attr_statistic, cfg80211_drv_vndr_statistic_handler},
	{mt_cfg80211_vndr_attr_get_band_info, cfg80211_drv_vndr_get_band_info},
	{mt_cfg80211_vndr_attr_get_cap, cfg80211_drv_vndr_get_cap},
	{mt_cfg80211_vndr_attr_get_runtime_info, cfg80211_drv_vndr_get_runtime_info},
	{mt_cfg80211_vndr_attr_get_statistic, cfg80211_drv_vndr_get_statistic},
	{mt_cfg80211_vndr_attr_get_static_info, cfg80211_drv_vndr_get_static_info},
	{mt_cfg80211_vndr_attr_wapp_req, cfg80211_drv_vndr_wapp_req},
	{mt_cfg80211_vndr_attr_mcast_snoop, cfg80211_drv_vndr_set_mcast_snoop},
#ifdef FTM_SUPPORT
	{mt_cfg80211_vndr_attr_set_ftm, cfg80211_drv_vndr_set_ftm},
	{mt_cfg80211_vndr_attr_set_get_ftm_stat, cfg80211_drv_vndr_get_ftm_stat},
#endif /* FTM_SUPPORT */
	{mt_cfg80211_vndr_attr_set_set_ap_wireless, cfg80211_drv_vndr_set_ap_wireless},
	{mt_cfg80211_vndr_attr_set_ap_rfeature, cfg80211_drv_vndr_set_ap_rfeature},
	{mt_cfg80211_vndr_attr_set_country, cfg80211_drv_vndr_set_country},
	{mt_cfg80211_vndr_attr_set_channel, cfg80211_drv_vndr_set_channel},
	{mt_cfg80211_vndr_attr_set_acl_policy, cfg80211_drv_vndr_set_acl},
	{mt_cfg80211_vndr_attr_set_vow, cfg80211_drv_vndr_set_vow},
	{mt_cfg80211_vndr_attr_set_mwds, cfg80211_drv_vndr_set_mwds},
	{mt_cfg80211_vndr_attr_set_auto_ch_sel, cfg80211_drv_vndr_set_auto_ch_sel},
	{mt_cfg80211_vndr_attr_set_scan, cfg80211_drv_vndr_set_scan},
	{mt_cfg80211_vndr_attr_set_bss, cfg80211_drv_vndr_set_bss},
	{mt_cfg80211_vndr_attr_set_radio, cfg80211_drv_vndr_set_radio},
	{mt_cfg80211_vndr_attr_ft_ie, cfg80211_drv_vndr_ft_ie},
	{mt_cfg80211_vndr_attr_mlo_ie, cfg80211_drv_vndr_mlo_ie},
	{mt_cfg80211_vndr_attr_del_mlo_sta_entry, cfg80211_drv_vndr_del_mlo_sta_entry},
	{mt_cfg80211_vndr_attr_set_cosr_ie, cfg80211_drv_vndr_set_cosr_bcn_ie},
	{mt_cfg80211_vndr_attr_wapp_cosr_info, cfg80211_drv_vndr_wapp_cosr_info},
	{mt_cfg80211_vndr_attr_set_pmkid, cfg80211_drv_vndr_set_pmkid},
	{mt_cfg80211_vndr_attr_set_mac_addr, cfg80211_drv_vndr_set_mac_addr},
	{mt_cfg80211_vndr_attr_set_seq_num, cfg80211_drv_vndr_set_seq_num},
	{mt_cfg80211_vndr_attr_set_wmm, cfg80211_drv_vndr_set_wmm},
	{mt_cfg80211_vndr_attr_set_monitor, cfg80211_drv_vndr_set_monitor},
	{mt_cfg80211_vndr_attr_clean_sta, cfg80211_drv_vndr_clean_sta},
	{mt_cfg80211_vndr_attr_set_ba, cfg80211_drv_vndr_set_ba},
	{mt_cfg80211_vndr_attr_set_action, cfg80211_drv_vndr_set_action},
	{mt_cfg80211_vndr_attr_qos, cfg80211_drv_vndr_qos},
	{mt_cfg80211_vndr_attr_offchannel_info, cfg80211_drv_vndr_offchannel_info},
	{mt_cfg80211_vndr_attr_dfs, cfg80211_drv_vndr_dfs},
#ifdef CONFIG_MAP_SUPPORT
	{mt_cfg80211_vndr_attr_easymesh, cfg80211_drv_vndr_easymesh},
#endif /* CONFIG_MAP_SUPPORT */
	{mt_cfg80211_vndr_attr_set_vlan, cfg80211_drv_vndr_set_vlan},
	{mt_cfg80211_vndr_attr_set_mbo, cfg80211_drv_vndr_set_mbo},
	{mt_cfg80211_vndr_attr_mlo_preset_linkid, cfg80211_drv_vndr_mlo_preset_linkid},
	{mt_cfg80211_vndr_attr_tx_power, cfg80211_drv_vndr_tx_power},
	{mt_cfg80211_vndr_attr_edca, cfg80211_drv_vndr_edca},
	{mt_cfg80211_vndr_attr_vie, cfg80211_drv_vndr_set_vie},
	{mt_cfg80211_vndr_attr_set_radio_stats, cfg80211_drv_vndr_set_radio_stats},
	{mt_cfg80211_vndr_attr_get_radio_stats, cfg80211_drv_vndr_get_radio_stats},
	{mt_cfg80211_vndr_attr_get_bss_stats, cfg80211_drv_vndr_get_bss_stats},
	{mt_cfg80211_vndr_attr_get_sta, cfg80211_drv_vndr_get_sta},
	{mt_cfg80211_vndr_attr_hqa, cfg80211_drv_vndr_hqa},
	{mt_cfg80211_vndr_attr_set_v10converter, cfg80211_drv_vndr_set_v10converter},
	{mt_cfg80211_vndr_attr_set_apropxyrefresh, cfg80211_drv_vndr_set_apropxyrefresh},
	{mt_cfg80211_vndr_attr_e2p, cfg80211_drv_vndr_e2p},
	{mt_cfg80211_vndr_attr_testengine, cfg80211_drv_vndr_testengine},
	{mt_cfg80211_vndr_attr_mlo_info, cfg80211_drv_vndr_mlo_info},
	{mt_cfg80211_vndr_attr_mlo_switch, cfg80211_drv_vndr_mlo_switch},
	{mt_cfg80211_vndr_attr_get_bss_mlo_info, cfg80211_drv_vndr_get_bss_mlo_info},
	{mt_cfg80211_vndr_attr_offload_acs, cfg80211_drv_vndr_offload_acs},
	{mt_cfg80211_vndr_attr_bh_status, cfg80211_drv_vndr_bh_status},
	{mt_cfg80211_vndr_attr_get_connected_sta_mld, cfg80211_drv_vndr_get_connected_sta_mld},
	{mt_cfg80211_vndr_attr_set_config, cfg80211_drv_vndr_set_config},
	{mt_cfg80211_vndr_attr_swaci_lna, cfg80211_drv_vndr_swaci_lna},
	{mt_cfg80211_vndr_attr_set_ap_mld, cfg80211_drv_vndr_set_ap_mld},
	{mt_cfg80211_vndr_attr_set_zr_hdo_rm, cfg80211_drv_vndr_set_zr_hdo_rm},
	{mt_cfg80211_vndr_attr_get_zr_hdo_rm, cfg80211_drv_vndr_get_zr_hdo_rm},
	{mt_cfg80211_vndr_attr_set_ap_ptk, cfg80211_drv_vndr_set_ap_ptk},
	{0, }
};

int cfg80211_drv_vndr_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	int ret = -RTMP_IO_EOPNOTSUPP;
	int cmd_attr_id = ioctl_cfg->subcmd;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ioctl_cfg->data;
	struct cfg80211_drv_vndr_cmd_handler *vndr_handler;

	if (!ctrl_data) {
		MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_NOTICE,
			"no input contrl data\n");
		return -RTMP_IO_EFAULT;
	}

	MTWF_DBG(NULL, DBG_CAT_CFG80211, CATCFG80211_VNDR, DBG_LVL_DEBUG,
		"mac_ad: %p, cmd_attr_id = %d\n", mac_ad, cmd_attr_id);
	for (vndr_handler = drv_vndr_cmd_handler; (vndr_handler->attr_type != 0); vndr_handler++) {
		if (vndr_handler->attr_type == cmd_attr_id)
			ret = vndr_handler->drv_vndr_cmd_handler(mac_ad, wdev, ctrl_data);
	}
	ctrl_data->status = ret;
	return ret;
}


#endif /* RT_CFG80211_SUPPORT */

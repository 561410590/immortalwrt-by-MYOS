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

	All related CFG80211 FTM Vendor Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT

#ifdef FTM_SUPPORT

#include "rt_config.h"


static VOID copy_peer_info(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UINT8 newInd)
{
	UINT8 status = pAd->ftm_peer[newInd].status;
	OS_SEM_LOCK(&pAd->ftm_lock);
	pAd->ftm_peer[newInd].num_burst_exponent = pAd->ftm_peer[0].num_burst_exponent;
	pAd->ftm_peer[newInd].burst_duration = pAd->ftm_peer[0].burst_duration;
	pAd->ftm_peer[newInd].min_delta_ftm = pAd->ftm_peer[0].min_delta_ftm;
	pAd->ftm_peer[newInd].partial_tsf = pAd->ftm_peer[0].partial_tsf;
	pAd->ftm_peer[newInd].ptsf_no_perference = pAd->ftm_peer[0].ptsf_no_perference;
	pAd->ftm_peer[newInd].asap = pAd->ftm_peer[0].asap;
	pAd->ftm_peer[newInd].ftms_per_burst = pAd->ftm_peer[0].ftms_per_burst;
	pAd->ftm_peer[newInd].fmt_and_bw = pAd->ftm_peer[0].fmt_and_bw;
	pAd->ftm_peer[newInd].preamble = pAd->ftm_peer[0].preamble;
	pAd->ftm_peer[newInd].bandwidth = pAd->ftm_peer[0].bandwidth;
	pAd->ftm_peer[newInd].burst_period = pAd->ftm_peer[0].burst_period;
	NdisCopyMemory(&pAd->ftm_peer[newInd].Responder,
					&pAd->ftm_peer[0].Responder, MAC_ADDR_LEN);
	pAd->ftm_peer[newInd].non_asap_delay_time = pAd->ftm_peer[0].non_asap_delay_time;


	OS_SEM_UNLOCK(&pAd->ftm_lock);
	pAd->ftm_peer[newInd].status = status;
}

static UINT8 find_peer_num(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev)
{
	UINT8 ind = 0, num = 0, newInd = 0;
	void *pPeerInfoArray;
	struct _FTM_PEER_INFO *pPeerInfo, *pPeerInfoNew;

	pPeerInfoArray = &pAd->ftm_peer;
	pPeerInfoNew = (struct _FTM_PEER_INFO *)pPeerInfoArray;

	for (ind = 1; ind < MAX_FTM_TBL_SIZE; ind++) {
		pPeerInfo = (struct _FTM_PEER_INFO *)pPeerInfoArray + ind;
		if (NdisCmpMemory(pPeerInfoNew->Responder, pPeerInfo->Responder, MAC_ADDR_LEN) == 0) {
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
				"ind=%hhu\n", ind);
			return ind;
		}
	}

	num = wdev->FtmCtrl.LastAssignedISTA + 1;
	for (ind = num; ind < num+MAX_FTM_TBL_SIZE; ind++) {
		newInd = ind % MAX_FTM_TBL_SIZE;
		if (newInd == 0)
			continue;

		pPeerInfo = (struct _FTM_PEER_INFO *)pPeerInfoArray + newInd;
		if (pPeerInfo->status == FTMISTA_UNUSED) {
			wdev->FtmCtrl.LastAssignedISTA = newInd;
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
				"newInd=%hhu\n", newInd);
			return newInd;
		}
	}
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"No enough iSTA peerInfo to use.\n");
	wdev->FtmCtrl.LastAssignedISTA = 0;
	return 0;
}


int cfg80211_drv_vndr_set_ftm(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	unsigned char range_req = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	UINT16 iSTA_exist_time = 0;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_EFAULT;
	}
	if (ctrl_data->data == NULL) {
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"ctrl_data->data is NULL\n");
		return -RTMP_IO_EINVAL;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_TOAE_CFG) {
		mtk_cfg80211_set_ftm_toae_cfg(mac_ad, wdev, (unsigned char *)ctrl_data->data);
		return 0;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_FTMR_TIMEOUT) {
		mtk_cfg80211_set_ftm_FTMR_timeout(mac_ad, wdev, ctrl_data->uint_val);
		return 0;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_TESTMODE)
		mtk_cfg80211_set_ftm_test_mode(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_ENABLE)
		mtk_cfg80211_set_ftm_enable(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_ROLE)
		mtk_cfg80211_set_ftm_role(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_BURST_EXP)
		mtk_cfg80211_set_ftm_burst_exp(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_BURST_DUR)
		mtk_cfg80211_set_ftm_burst_dur(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_MIN_DELTA)
		mtk_cfg80211_set_ftm_min_delta(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_PARTIAL_TSF)
		mtk_cfg80211_set_ftm_ptsf(mac_ad, wdev, ctrl_data->ushort_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_PTSF_NO_PREFERENCE)
		mtk_cfg80211_set_ftm_ptsf_no_perfer(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_ASAP)
		mtk_cfg80211_set_ftm_asap(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_NON_ASAP_DELAY_TIME)
		mtk_cfg80211_set_non_asap_delay_time(mac_ad, wdev, ctrl_data->ushort_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_NUM)
		mtk_cfg80211_set_ftm_num(mac_ad, wdev, ctrl_data->u8_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_FMT_AND_BW) {
		if (mtk_cfg80211_set_fmt_and_bw(
			mac_ad, wdev, ctrl_data->u8_val) == -RTMP_IO_EINVAL) {
			mtk_cfg80211_dump_ftm_parm(mac_ad, wdev);
			return 0;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_BURST_PERIOD)
		mtk_cfg80211_set_ftm_burst_period(mac_ad, wdev, ctrl_data->ushort_val);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_TARGET_MAC) {
		if (ctrl_data->data_len < MAC_ADDR_LEN)
			return -RTMP_IO_EINVAL;
		mtk_cfg80211_set_ftm_target_mac(mac_ad, wdev, (unsigned char *)ctrl_data->data);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_DEBUG) {
		if (ctrl_data->data_len < MAC_ADDR_LEN)
			return -RTMP_IO_EINVAL;
		mtk_cfg80211_set_ftm_debug(mac_ad, wdev, (unsigned char *)ctrl_data->data);
		return 0;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_DUMP_PARAMS)
		mtk_cfg80211_dump_ftm_parm(mac_ad, wdev);

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_RANG_REQ) {
		if (wdev->FtmCtrl.FTMInit == false) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"haven't enable FTM initiator: mwctl dev <devname> set ftm enable=2\n");
			return 0;
		}
		if (mac_ad->ftm_peer[0].preamble == LOC_PREAMBLE_INVALID ||
			mac_ad->ftm_peer[0].bandwidth == LOC_MEAS_BW_INVALID) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
				"bandwidth or preamble is invalid\n");
			return 0;
		}

		range_req = ctrl_data->u8_val;
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_NOTICE,
			"range_req=%d\n", range_req);

		if (range_req == 0 || range_req == 1) {
			struct _FTM_PEER_INFO PeerInfo;
			UINT8 *pAddr;
			UINT8 peer_num = find_peer_num(mac_ad, wdev);

			wdev->FtmCtrl.LastAssignedISTA = peer_num;
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"peer_num=%hhu\n", peer_num);

			copy_peer_info(mac_ad, wdev, peer_num);
			PeerInfo = mac_ad->ftm_peer[peer_num];

			if (wdev->wdev_type == WDEV_TYPE_STA) {
				PSTA_ADMIN_CONFIG pStaCfg = NULL;

				pStaCfg = GetStaCfgByWdev(mac_ad, wdev);

				if (!INFRA_ON(pStaCfg)
					|| RTMPCompareMemory(
						pStaCfg->Bssid,
						PeerInfo.Responder, MAC_ADDR_LEN) != 0) {
					MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
						"APCLI only support with connected peer\n");
					return 0;
				}
			}

			iSTA_exist_time = ktime_to_ms(ktime_sub(mt_ktime_get(), (mac_ad->ftm_ista_start_time)));
			if (mac_ad->ftm_ista_status != FTMISTA_UNUSED && iSTA_exist_time < 1000) {
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"[FTM][ISTA] sb still in burst. iSTA_exist_time=%d\n", iSTA_exist_time);
				return 0;
			}

			mac_ad->ftm_ista_status = FTMISTA_IN_BURST;
			mac_ad->ftm_ista_start_time = mt_ktime_get();

			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"wdev->wdev_idx=%hhu, PeerInfo.status=%hhu\n",
					wdev->wdev_idx, PeerInfo.status);
			mac_ad->ftm_peer[peer_num].status = FTMISTA_FIRST_BURST;

			pAddr = PeerInfo.Responder;
			ftm_create_mac_entry(mac_ad, wdev, pAddr, &mac_ad->ftm_peer[peer_num]);

			mtk_cfg80211_range_req_mc(mac_ad, wdev, range_req, peer_num);
		}
	}
	return 0;
}

VOID mtk_cfg80211_set_ftm_toae_cfg(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UCHAR *toae_cfg)
{
	int matches;
	struct ftm_toae_cfg *p_toae_cfg;
	UINT8 band_idx;

	p_toae_cfg = &wdev->FtmCtrl.toae_cfg;
	matches = sscanf(toae_cfg, "%hhu-%hhu-%hhu-%hhu-%hu",
		&p_toae_cfg->fgBiasTuning, &p_toae_cfg->u1AntIdx, &p_toae_cfg->u1SpeIdxFtm,
		&p_toae_cfg->u1SpeIdxFtmAck, &p_toae_cfg->u2ChainSel);

	if (matches != 5) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"wrong format of toae_cfg\n");
		return;
	}

	band_idx = hc_get_hw_band_idx(pAd);
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_TOAE_CAL_CFG, 0, band_idx, NULL);
}

VOID mtk_cfg80211_range_req_mc_after_scan(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev)
{
	UINT8 band_idx;
	BSS_TABLE *scan_tab;
	ULONG bss_idx;
	UINT8 peer_num = wdev->FtmCtrl.LastAssignedISTA;
	struct _FTM_PEER_INFO *pPeerInfo;

	pPeerInfo = &pAd->ftm_peer[peer_num];

	/* get target channel */
	scan_tab = get_scan_tab_by_wdev(pAd, wdev);
	if (scan_tab == NULL) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"scan_tab is null(%p).\n", scan_tab);
		return;
	}

	bss_idx = BssSsidTableSearchByBSSID(scan_tab, pPeerInfo->Responder);
	if (bss_idx < MAX_LEN_OF_BSS_TABLE) {
		pPeerInfo->target_channel = scan_tab->BssEntry[bss_idx].Channel;
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_NOTICE,
				"ori_channel=%hhu, target_channel=%hhu\n",
				pPeerInfo->ori_channel, pPeerInfo->target_channel);
		FtmSwitchChannel(pAd, wdev, pPeerInfo->target_channel, ACTION_GO_TO);
		return;
	}

	pPeerInfo->target_channel = pPeerInfo->ori_channel;

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_NOTICE,
				"ori_channel=%hhu, target_channel=%hhu\n",
				pPeerInfo->ori_channel, pPeerInfo->target_channel);

	band_idx = hc_get_hw_band_idx(pAd);
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RANGE_REQ_MC, pPeerInfo->trigger_mc_req, band_idx, NULL);
}

VOID mtk_cfg80211_range_req_mc(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT enable,
	UINT8 peer_num)
{
	UINT8 band_idx;
	struct _FTM_PEER_INFO *pPeerInfo;
	BSS_TABLE *scan_tab;
	ULONG bss_idx;

	pPeerInfo = &pAd->ftm_peer[peer_num];

	/* reset distance */
	pPeerInfo->distanceMM = 0;

	/* restore origin channel */
	pPeerInfo->ori_channel = wdev->channel;

	/* get target channel */
	scan_tab = get_scan_tab_by_wdev(pAd, wdev);
	if (scan_tab == NULL) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"scan_tab is null(%p).\n", scan_tab);
		pPeerInfo->status = FTMISTA_UNUSED;
		pAd->ftm_ista_status = FTMISTA_UNUSED;
		return;
	}

	bss_idx = BssSsidTableSearchByBSSID(scan_tab, pPeerInfo->Responder);
	if (bss_idx < MAX_LEN_OF_BSS_TABLE) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"scan_tab bss_idx=%ld < MAX_LEN_OF_BSS_TABLE\n", bss_idx);
		pPeerInfo->target_channel = scan_tab->BssEntry[bss_idx].Channel;
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_NOTICE,
				"ori_channel=%hhu, target_channel=%hhu\n",
				pPeerInfo->ori_channel, pPeerInfo->target_channel);
		FtmSwitchChannel(pAd, wdev, pPeerInfo->target_channel, ACTION_GO_TO);
		return;
	} else if (pPeerInfo->create_STAREC != 1) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"exist STAREC, but not in scan table, might be STA.\n");
		pPeerInfo->target_channel = wdev->channel;
	} else {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
					"bss_idx=%ld >= MAX_LEN_OF_BSS_TABLE\n", bss_idx);
		wdev->FtmCtrl.trigger_scan = 1;
		pPeerInfo->trigger_mc_req = enable;
		Set_PartialScan_Proc(pAd, "1");
		return;
	}

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_NOTICE,
				"ori_channel=%hhu, target_channel=%hhu\n",
				pPeerInfo->ori_channel, pPeerInfo->target_channel);

	band_idx = hc_get_hw_band_idx(pAd);
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RANGE_REQ_MC, enable, band_idx, NULL);
}

VOID mtk_cfg80211_dump_ftm_parm(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev)
{
	FTM_CTRL ftmCtrl = wdev->FtmCtrl;
	struct _FTM_PEER_INFO PeerInfo;
	UINT8 *pAddr;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole == 1)
		PeerInfo = wdev->FtmCtrl.rSTA_pinfo[peer_num];
	else if (wdev->FtmCtrl.FTMRole == 2)
		PeerInfo = pAd->ftm_peer[peer_num];
	else {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"FTMRole=%hhu is invalid\n", wdev->FtmCtrl.FTMRole);
		return;
	}

	pAddr = PeerInfo.Responder;

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"Enable Role (FTM Resp=%d, FTM Init=%d)\n",
			ftmCtrl.FTMResp, ftmCtrl.FTMInit);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"Role Params: %d (1 for Responder, 2 for Initiator)\n", ftmCtrl.FTMRole);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"burst_exp=%d burst_dur=%d min_delta=%d ptsf=%d no_perfer=%d\n",
			PeerInfo.num_burst_exponent, PeerInfo.burst_duration, PeerInfo.min_delta_ftm,
			PeerInfo.partial_tsf, PeerInfo.ptsf_no_perference);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"asap=%d non_asap_delay_time=%hu ftm_num=%d burst_period=%d\n",
			PeerInfo.asap, PeerInfo.non_asap_delay_time, PeerInfo.ftms_per_burst, PeerInfo.burst_period);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"fmt_bw=%d preamble=%hhu bandwidth=%hhu\n",
			PeerInfo.fmt_and_bw, PeerInfo.preamble, PeerInfo.bandwidth);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"target=%02x:%02x:%02x:%02x:%02x:%02x\n",
			pAddr[0], pAddr[1], pAddr[2], pAddr[3], pAddr[4], pAddr[5]);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"status=%hhu, wdev->wdev_idx=%hhu\n",
			PeerInfo.status, wdev->wdev_idx);
}

VOID mtk_cfg80211_set_ftm_FTMR_timeout(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UINT32 FTMR_timeout)
{
	UINT8 band_idx = hc_get_hw_band_idx(pAd);

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"timeout=%u\n", FTMR_timeout);
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_FTMR_TIMEOUT, FTMR_timeout, band_idx, NULL);
}

VOID mtk_cfg80211_set_non_asap_delay_time(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UINT16 non_asap_delay_time)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (non_asap_delay_time < 1 || non_asap_delay_time > 65535) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->non_asap_delay_time = non_asap_delay_time;
}

VOID mtk_cfg80211_set_ftm_test_mode(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT enable)
{
	UINT8 band_idx = hc_get_hw_band_idx(pAd);

	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RSP_ENABLE, 3, band_idx, NULL);
}


VOID FtmInitBiasTable(
	IN PRTMP_ADAPTER	pAd
)
{
	RTMP_CHIP_OP *pChipOps = hc_get_chip_ops(pAd->hdev_ctrl);
	PUCHAR ftmBias = (PUCHAR)&pAd->ftm_bias_tbl;
	int i = 0;
	USHORT value = 0;

	if (get_dev_eeprom_size(pAd->physical_dev) >= FTM_BIAS_OFFSET) {
		// check if wf0 enable for 11mc
		for (i = 0; i < sizeof(struct loc_cal_info); i++) {
			pChipOps->eeread(pAd, FTM_BIAS_OFFSET+i, &value);
			memcpy((ftmBias+i), &value, 1);
		}
		if (pAd->ftm_bias_tbl.ftm_cal_info.WF0_Enable == 1) {
			for (i = 0; i < sizeof(struct loc_bias_table); i++) {
				pChipOps->eeread(pAd, FTM_BIAS_OFFSET+i, &value);
				memcpy((ftmBias+i), &value, 1);
			}
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
				"11mc load wf0 bias table done\n");
		} else
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"wf0 didn't enable for 11mc\n");
	} else
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"eeprom_size: %x, FTM_BIAS_OFFSET: %x\n",
				get_dev_eeprom_size(pAd->physical_dev), FTM_BIAS_OFFSET);
}


VOID mtk_cfg80211_set_ftm_enable(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT ftm_enable)
{
	UINT8 band_idx = hc_get_hw_band_idx(pAd);
	USHORT wmode = wdev->PhyMode;

	/* Disable FTM
	 * if 2G in legacy mode
	 * if 5G in legacy, HT mode */
	if (wmode == WMODE_INVALID) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"wmode is WMODE_INVALID, FTM disable\n");
		ftm_enable = 0;
	}
#ifdef DOT11_EHT_BE
	else if (WMODE_CAP_BE(wmode))
		;
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	else if (WMODE_CAP_AX(wmode))
		;
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	else if (WMODE_CAP_AC(wmode))
		;
#endif /* DOT11_VHT_AC */
#ifdef DOT11_N_SUPPORT
	else if (WMODE_CAP_N(wmode) && band_idx == BAND1) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"not support FTM in 5/6G HT mode, FTM disable\n");
		ftm_enable = 0;
	}
#endif
	else if (wmode & (WMODE_A|WMODE_B|WMODE_G)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"not support FTM in Legacy mode, FTM disable\n");
		ftm_enable = 0;
	}

{
	struct _CMD_GET_LOC_CAP_T *pLocCap;

	/* Get Capability */
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_GET_CAPA, 0, band_idx, NULL);

	pLocCap = &wdev->FtmCtrl.loc_cap;

	wdev->FtmCtrl.FTMResp = (ftm_enable >> 0) & 0x1;
	// test if FW support 11mc-resp (bit 1)
	if (!(pLocCap->u4ResponderSupported & (1 << 1))) {
		wdev->FtmCtrl.FTMResp = 0;
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"FW not support 11mc-resp, u4ResponderSupported=%hu\n", pLocCap->u4ResponderSupported);
	}

	wdev->FtmCtrl.FTMInit = (ftm_enable >> 1) & 0x1;
	// test if FW support 11mc-init (bit 1)
	if (!(pLocCap->u4LOCInitSupported & (1 << 1))) {
		wdev->FtmCtrl.FTMInit = 0;
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"FW not support 11mc-init, u4LOCInitSupported=%hu\n", pLocCap->u4LOCInitSupported);
	}
}

	if (ftm_enable == 0)
		UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RSP_ENABLE, 0, band_idx, NULL);
	else {
		if (wdev->FtmCtrl.FTMResp)
			UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RSP_ENABLE, 1, band_idx, NULL);
		if (wdev->FtmCtrl.FTMInit)
			UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_RSP_ENABLE, 2, band_idx, NULL);
		FtmInitBiasTable(pAd);
	}
}

VOID mtk_cfg80211_set_ftm_role(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT role)
{
	if (role < 1 || role > 2) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	wdev->FtmCtrl.FTMRole = role;
}


VOID mtk_cfg80211_set_ftm_burst_exp(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT burst_exp)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (burst_exp < 0 || burst_exp > 15) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->num_burst_exponent = burst_exp;
}

VOID mtk_cfg80211_set_ftm_burst_dur(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT burst_dur)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (burst_dur < 0 || burst_dur > 15) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	} else if ((burst_dur >= 0 && burst_dur <= 1) ||
				(burst_dur >= 12 && burst_dur <= 14)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"burst duration=%d is reversed\n", burst_dur);
		return;
	}

	pPeerInfo->burst_duration = burst_dur;
}

VOID mtk_cfg80211_update_ftm_min_delta(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;
	UINT8 min_delta = 0;

	// rSTA
	pPeerInfo = &wdev->FtmCtrl.rSTA_pinfo[peer_num];
	min_delta = pPeerInfo->min_delta_ftm;
	if (min_delta < wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"FW support min delta time should >= %d\n",
			wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket);
		min_delta = wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket;
		pPeerInfo->min_delta_ftm = min_delta;
	}

	// iSTA
	pPeerInfo = &pAd->ftm_peer[peer_num];
	min_delta = pPeerInfo->min_delta_ftm;
	if (min_delta < wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"FW support min delta time should >= %d\n",
			wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket);
		min_delta = wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket;
		pPeerInfo->min_delta_ftm = min_delta;
	}
}

VOID mtk_cfg80211_set_ftm_min_delta(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT min_delta)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (min_delta < 1 || min_delta > 255) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	if (min_delta < wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"FW support min delta time should >= %d\n",
			wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket);
		min_delta = wdev->FtmCtrl.loc_cap.u4MinDeltaTimePerPacket;
	}

	pPeerInfo->min_delta_ftm = min_delta;
}


VOID mtk_cfg80211_set_ftm_ptsf(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT ptsf)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (ptsf < 0 || ptsf > 65535) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->partial_tsf = ptsf;
}


VOID mtk_cfg80211_set_ftm_ptsf_no_perfer(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT ptsf_no_perfer)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (ptsf_no_perfer < 0 || ptsf_no_perfer > 1) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->ptsf_no_perference = ptsf_no_perfer;
}


VOID mtk_cfg80211_set_ftm_asap(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT asap)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (asap < 0 || asap > 1) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->asap = asap;
}


VOID mtk_cfg80211_set_ftm_num(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT ftms_per_burst)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (ftms_per_burst < 0 || ftms_per_burst == 1 || ftms_per_burst > 31) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->ftms_per_burst = ftms_per_burst;
}


static struct _FTM_FMT_MAP ftmFmtMap[] = {
	{FTM_BW_NO_PREFERENCE, LOC_PREAMBLE_NO_PREFER, LOC_MEAS_BW_20},
	{FTM_BW_NONHT_BW5, LOC_PREAMBLE_LEGACY, LOC_MEAS_BW_5},
	{FTM_BW_NONHT_BW10, LOC_PREAMBLE_LEGACY, LOC_MEAS_BW_10},
	{FTM_BW_NONHT_BW20, LOC_PREAMBLE_LEGACY, LOC_MEAS_BW_20},
	{FTM_BW_HT_BW20, LOC_PREAMBLE_HT, LOC_MEAS_BW_20},
	{FTM_BW_VHT_BW20, LOC_PREAMBLE_VHT, LOC_MEAS_BW_20},
	{FTM_BW_HT_BW40, LOC_PREAMBLE_HT, LOC_MEAS_BW_40},
	{FTM_BW_VHT_BW40, LOC_PREAMBLE_VHT, LOC_MEAS_BW_40},
	{FTM_BW_VHT_BW80, LOC_PREAMBLE_VHT, LOC_MEAS_BW_80},
	{FTM_BW_VHT_BW80_80, LOC_PREAMBLE_VHT, LOC_MEAS_BW_160},
	{FTM_BW_VHT_BW160_2RFLO, LOC_PREAMBLE_VHT, LOC_MEAS_BW_160},
	{FTM_BW_VHT_BW160_1RFLO, LOC_PREAMBLE_VHT, LOC_MEAS_BW_160},
	{FTM_BW_DMG_BW2160, LOC_PREAMBLE_INVALID, LOC_MEAS_BW_20},
};


static INT LocGetChannelWidth(
	struct wifi_dev *wdev,
	UINT_8 preamble)
{
	USHORT wmode = wdev->PhyMode;
	UCHAR bw = wlan_operate_get_bw(wdev);
	int chWidth;

	// mapping
	switch (bw) {
	case BW_20:
		chWidth = WIFI_CHAN_WIDTH_20;
		break;
	case BW_40:
		chWidth = WIFI_CHAN_WIDTH_40;
		break;
	case BW_80:
		chWidth = WIFI_CHAN_WIDTH_80;
		break;
	case BW_160:
		chWidth = WIFI_CHAN_WIDTH_160;
		break;
	case BW_10:
		return WIFI_CHAN_WIDTH_10;
	case BW_5:
		return WIFI_CHAN_WIDTH_5;
	case BW_8080:
		chWidth = WIFI_CHAN_WIDTH_80P80;
		break;
	default:
		chWidth = WIFI_CHAN_WIDTH_INVALID;
		break;
	}

	if (wmode == WMODE_INVALID)
		return WIFI_CHAN_WIDTH_INVALID;
#ifdef DOT11_EHT_BE
	else if (WMODE_CAP_BE(wmode))
		return chWidth;
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	else if (WMODE_CAP_AX(wmode)) {
		if (preamble & LOC_PREAMBLE_EHT)
			return WIFI_CHAN_WIDTH_INVALID;
		return chWidth;
	}
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	else if (WMODE_CAP_AC(wmode)) {
		if (preamble & (LOC_PREAMBLE_EHT|LOC_PREAMBLE_HE))
			return WIFI_CHAN_WIDTH_INVALID;
		return chWidth;
	}
#endif /* DOT11_VHT_AC */
#ifdef DOT11_N_SUPPORT
	else if (WMODE_CAP_N(wmode)) {
		if (preamble & (LOC_PREAMBLE_EHT|LOC_PREAMBLE_HE|LOC_PREAMBLE_VHT))
			return WIFI_CHAN_WIDTH_INVALID;
		return chWidth;
	}
#endif
	else if (wmode & (WMODE_A|WMODE_B|WMODE_G)) {
		if (preamble & (LOC_PREAMBLE_EHT|LOC_PREAMBLE_HE|LOC_PREAMBLE_VHT|LOC_PREAMBLE_HT))
			return WIFI_CHAN_WIDTH_INVALID;
		return chWidth;
	}

	return WIFI_CHAN_WIDTH_INVALID;
}

static INT ftm_check_format_and_bw(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	struct _FTM_PEER_INFO *pPeerInfo)
{
	USHORT wmode = wdev->PhyMode;
	INT8 channelWidth;
	UCHAR *temp_wmode = NULL;

	temp_wmode = wmode_2_str(wmode);
	if (temp_wmode == NULL) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR, "wmode is NULL\n");
		return -EINVAL;
	}

	channelWidth = LocGetChannelWidth(wdev, pPeerInfo->preamble);
	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_DEBUG,
		"channelWidth = %hhd, wmode: %s\n", channelWidth, temp_wmode);
	os_free_mem(temp_wmode);
	temp_wmode = NULL;

	if (channelWidth == WIFI_CHAN_WIDTH_INVALID) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"pPeerInfo->preamble exceed AP capability\n");
		return -EINVAL;
	}

	switch (channelWidth) {
	case WIFI_CHAN_WIDTH_5:
	{
		if (pPeerInfo->bandwidth & LOC_MEAS_BW_5)
			break;

		pPeerInfo->bandwidth = LOC_MEAS_BW_INVALID;
		return -EINVAL;
	}
	case WIFI_CHAN_WIDTH_10:
	{
		if (pPeerInfo->bandwidth & (LOC_MEAS_BW_5|LOC_MEAS_BW_10))
			break;

		pPeerInfo->bandwidth = LOC_MEAS_BW_INVALID;
		return -EINVAL;
	}
	case WIFI_CHAN_WIDTH_20:
	{
		if (pPeerInfo->bandwidth & (LOC_MEAS_BW_5|LOC_MEAS_BW_10|LOC_MEAS_BW_20))
			break;

		pPeerInfo->bandwidth = LOC_MEAS_BW_INVALID;
		return -EINVAL;
	}
	case WIFI_CHAN_WIDTH_40:
	{
		if (pPeerInfo->bandwidth & (LOC_MEAS_BW_5|LOC_MEAS_BW_10|LOC_MEAS_BW_20|LOC_MEAS_BW_40))
			break;

		pPeerInfo->bandwidth = LOC_MEAS_BW_INVALID;
		return -EINVAL;
	}
	case WIFI_CHAN_WIDTH_80:
	{
		if (pPeerInfo->bandwidth & (LOC_MEAS_BW_5|LOC_MEAS_BW_10|LOC_MEAS_BW_20|LOC_MEAS_BW_40|LOC_MEAS_BW_80))
			break;

		pPeerInfo->bandwidth = LOC_MEAS_BW_INVALID;
		return -EINVAL;
	}
	case WIFI_CHAN_WIDTH_160:
	case WIFI_CHAN_WIDTH_80P80:
	default:
		break;
	}

	return 0;
}

INT mtk_cfg80211_set_fmt_and_bw(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT fmt_and_bw)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;
	int x;
	UINT8 band_idx = hc_get_hw_band_idx(pAd);

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (fmt_and_bw < 0 || fmt_and_bw > 63) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return -RTMP_IO_EINVAL;
	}

	pPeerInfo->fmt_and_bw = fmt_and_bw;

	// mapping
	pPeerInfo->preamble = LOC_PREAMBLE_INVALID;
	pPeerInfo->bandwidth = LOC_MEAS_BW_20;

	if (pPeerInfo->fmt_and_bw == 0) {
		int check_assoc = 1;

		if (wdev->wdev_type == WDEV_TYPE_STA) {
			PSTA_ADMIN_CONFIG pStaCfg = NULL;

			pStaCfg = GetStaCfgByWdev(pAd, wdev);
			if (!INFRA_ON(pStaCfg) || RTMPCompareMemory(pStaCfg->Bssid, pPeerInfo->Responder, MAC_ADDR_LEN) != 0)
				check_assoc = 0;
		} else if (wdev->wdev_type == WDEV_TYPE_AP) {
			MAC_TABLE_ENTRY *pEntry = NULL;

			pEntry = MacTableLookup(pAd, pPeerInfo->Responder);
			if (pEntry) {
				if (!IS_ENTRY_CLIENT(pEntry) || (IS_ENTRY_CLIENT(pEntry) && pEntry->Sst != SST_ASSOC))
					check_assoc = 0;
			} else
				check_assoc = 0;
		} else {
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"unexpected wdev_type: %u\n", wdev->wdev_type);
			check_assoc = 0;
		}

		if (check_assoc == 0) {
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"fmt_bw = no prefer only use in associated state.\n");
			return 0;
		}
		pPeerInfo->preamble = LOC_PREAMBLE_NO_PREFER;
		return 0;
	}

	for (x = 0; x < ARRAY_SIZE(ftmFmtMap); x++) {
		if (pPeerInfo->fmt_and_bw == ftmFmtMap[x].ftm_fmt_and_bw) {
			pPeerInfo->preamble = ftmFmtMap[x].loc_cfg_preamble;
			pPeerInfo->bandwidth = ftmFmtMap[x].loc_cfg_bandwidth;
			break;
		}
	}

{
	struct _CMD_GET_LOC_CAP_T *pLocCap;

	pLocCap = &wdev->FtmCtrl.loc_cap;

	// check fw capability
	if ((pPeerInfo->preamble != LOC_PREAMBLE_NO_PREFER) &&
			!(pPeerInfo->preamble & pLocCap->u4PreambleSupport)) {
		if (pPeerInfo->preamble == 0 && wdev->FtmCtrl.FTMInit == 0 && wdev->FtmCtrl.FTMResp == 0)
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"please enable ftm first: mwctl dev raX0 set ftm enable=2\n");
		else
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"FW didn't support. preamble: %d, FW: %d\n",
				pPeerInfo->preamble, pLocCap->u4PreambleSupport);
		return -EINVAL;
	}

	if (!(pPeerInfo->bandwidth & pLocCap->u4BwSupport)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"FW didn't support. bandwidth: %d, FW: %d\n", pPeerInfo->bandwidth, pLocCap->u4BwSupport);
		return -EINVAL;
	}
}

	if (ftm_check_format_and_bw(pAd, wdev, pPeerInfo) == -EINVAL) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"ftm_check_format_and_bw is -EINVAL\n");
		return -RTMP_IO_EINVAL;
	}


	if ((pPeerInfo->preamble == LOC_PREAMBLE_HT) && band_idx == BAND1) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"not support FTM in 5/6G HT mode\n");
		pPeerInfo->preamble = LOC_PREAMBLE_INVALID;
		return -RTMP_IO_EINVAL;
	} else if (pPeerInfo->preamble == LOC_PREAMBLE_LEGACY) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"not support FTM in Legacy mode\n");
		pPeerInfo->preamble = LOC_PREAMBLE_INVALID;
		return -RTMP_IO_EINVAL;
	}

	return 0;
}


VOID mtk_cfg80211_set_ftm_burst_period(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	INT burst_period)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	if (burst_period < 0 || burst_period > 65535) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"value is invalid\n");
		return;
	}

	pPeerInfo->burst_period = burst_period;
}

VOID mtk_cfg80211_set_ftm_target_mac(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UINT8 *macAddr)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;

	if (wdev->FtmCtrl.FTMRole != 2)
		wdev->FtmCtrl.FTMRole = 2;

	peer_num = 0;
	pPeerInfo = &pAd->ftm_peer[peer_num];

	RTMPMoveMemory(pPeerInfo->Responder, macAddr, MAC_ADDR_LEN);
}


VOID mtk_cfg80211_set_ftm_debug(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UCHAR *macAddr)
{
	struct _FTM_PEER_INFO *pPeerInfo;
	UINT8 peer_num = 0;
	int matches;
	UINT8 band_idx;

	if (wdev->FtmCtrl.FTMRole == 1) {
		peer_num = wdev->FtmCtrl.LastAssignedRSTA;
		pPeerInfo = &wdev->FtmCtrl.rSTA_pinfo[peer_num];
	} else if (wdev->FtmCtrl.FTMRole == 2) {
		peer_num = wdev->FtmCtrl.LastAssignedISTA;
		pPeerInfo = &pAd->ftm_peer[peer_num];
	} else {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"FTMRole=%hhu is invalid\n", wdev->FtmCtrl.FTMRole);
		return;
	}


	matches = sscanf(macAddr, "%u:%u:%u:%u:%u:%u",
		&pPeerInfo->debug[0], &pPeerInfo->debug[1], &pPeerInfo->debug[2],
		&pPeerInfo->debug[3], &pPeerInfo->debug[4], &pPeerInfo->debug[5]);

	if (matches != 6) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"Format Error! Please enter in the following format\n"
			"mwctl dev <inf> set ftm debug=u:u:u:u:u:u\n");
		return;
	}

	MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_INFO,
			"debug[0]=%u, debug[1]=%u, debug[2]=%u, debug[3]=%u, debug[4]=%u, debug[5]=%u\n",
			pPeerInfo->debug[0], pPeerInfo->debug[1], pPeerInfo->debug[2],
			pPeerInfo->debug[3], pPeerInfo->debug[4], pPeerInfo->debug[5]);

	band_idx = hc_get_hw_band_idx(pAd);
	UniCmdFTM(pAd, wdev, UNI_CMD_LOC_TAG_DEBUG, 1, band_idx, NULL);
}


int cfg80211_drv_vndr_get_ftm_stat(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	int ret = 0, msg_len = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_START_UP)) {
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_WARN,
			"Interface not start up. return!!\n");
		return -RTMP_IO_ENETDOWN;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_FTM_STAT_STR) {
		if (ctrl_data->u8_val == 1 || ctrl_data->u8_val == 2) {
			msg_len = mtk_cfg80211_vndr_get_ftm_stat_iSTA(
				mac_ad, wdev,
				ctrl_data->rsp_data, ctrl_data->rsp_data_len,
				ctrl_data->u8_val);
			ctrl_data->rsp_data_len = msg_len;
		}
	} else {
		MTWF_DBG(mac_ad, DBG_CAT_MLO, CATMLO_CFG, DBG_LVL_ERROR,
				"no MTK_NL80211_VENDOR_ATTR_FTM_STAT_STR attr\n");
		return -RTMP_IO_EINVAL;
	}

	if (msg_len < 0)
		ret = -RTMP_IO_EFAULT;
	else if (msg_len == 0) {
		ret = snprintf(ctrl_data->rsp_data, ctrl_data->rsp_data_len,
			"===>Error Statistics string!");
		if (os_snprintf_error(ctrl_data->rsp_data_len, ret)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"msg snprintf error!\n");
			ret = -RTMP_IO_EINVAL;
		}
		msg_len += ret;
	}
	return ret;
}

int mtk_cfg80211_vndr_get_ftm_stat_iSTA(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	RTMP_STRING *msg,
	int msg_buf_len,
	int value)
{
	INT msg_len = msg_buf_len;
	INT ret = 0;

	UINT8 *pAddr;
	UINT8 ind = 0;
	struct _FTM_PEER_INFO *pPeerInfo;
	void *pPeerInfoArray;

	ret = snprintf(msg, msg_len, "\n");
	if (os_snprintf_error(msg_len, ret)) {
		MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
			"snprintf error!\n");
		return 0;
	}

	if (value == 1) {
		ret = snprintf(msg + strlen(msg), msg_len - strlen(msg),
				"======================= as rSTA =======================\n\n"
				"target MAC address\tBandwidth\tDistance\n");
		if (os_snprintf_error(msg_len - strlen(msg), ret)) {
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"snprintf error!\n");
			return 0;
		}

		OS_SEM_LOCK(&pAd->ftm_lock);
		pPeerInfoArray = &wdev->FtmCtrl.rSTA_pinfo;
		for (ind = 0; ind < wdev->FtmCtrl.LastAssignedRSTA+1; ind++) {
			pPeerInfo = (struct _FTM_PEER_INFO *)pPeerInfoArray + ind;
			pAddr = pPeerInfo->Responder;
			ret = snprintf(msg + strlen(msg), msg_len - strlen(msg),
				"%02x:%02x:%02x:%02x:%02x:%02x\t%d\t\t%d\n",
				pAddr[0], pAddr[1], pAddr[2], pAddr[3], pAddr[4], pAddr[5],
				pPeerInfo->fmt_and_bw, 0);
			if (os_snprintf_error(msg_len - strlen(msg), ret)) {
				MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"snprintf error!\n");
				goto err;
			}
		}
		OS_SEM_UNLOCK(&pAd->ftm_lock);
	} else if (value == 2) {
		ret = snprintf(msg + strlen(msg), msg_len - strlen(msg),
				"======================= as iSTA =======================\n\n"
				"target MAC address\tChannel\tBandwidth\tDistance\n");
		if (os_snprintf_error(msg_len - strlen(msg), ret)) {
			MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
				"snprintf error!\n");
			return 0;
		}

		OS_SEM_LOCK(&pAd->ftm_lock);
		pPeerInfoArray = &pAd->ftm_peer;
		for (ind = 0; ind < wdev->FtmCtrl.LastAssignedISTA+1; ind++) {
			pPeerInfo = (struct _FTM_PEER_INFO *)pPeerInfoArray + ind;
			pAddr = pPeerInfo->Responder;
			ret = snprintf(msg + strlen(msg), msg_len - strlen(msg),
				"%02x:%02x:%02x:%02x:%02x:%02x\t%d\t%d\t\t%d\n",
				pAddr[0], pAddr[1], pAddr[2], pAddr[3], pAddr[4], pAddr[5],
				wdev->channel, pPeerInfo->fmt_and_bw, pPeerInfo->distanceMM);
			if (os_snprintf_error(msg_len - strlen(msg), ret)) {
				MTWF_DBG(pAd, DBG_CAT_PROTO, CATPROTO_FTM, DBG_LVL_ERROR,
					"snprintf error!\n");
				goto err;
			}
		}
		OS_SEM_UNLOCK(&pAd->ftm_lock);
	}
	return strlen(msg);

err:
	OS_SEM_UNLOCK(&pAd->ftm_lock);
	return 0;
}

#endif /* FTM_SUPPORT */

#endif /* RT_CFG80211_SUPPORT */

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

#ifdef WIFI_DIAG
#include "rt_config.h"
#include "rtmp.h"
#include "rtmp_comm.h"
#include "dot11_base.h"
#include "diag.h"

void diag_common_deauth_code_trans(USHORT reason,
	enum MT_ENUM_DIAG_CONN_ERROR_CODE *Code)
{
	switch (reason) {
	case REASON_4_WAY_TIMEOUT:
	case REASON_GROUP_KEY_HS_TIMEOUT:
	case REASON_MIC_FAILURE:
	case REASON_STA_REQ_ASSOC_NOT_AUTH:
	case REASON_8021X_AUTH_FAIL:
	case REASON_IE_DIFFERENT:
	case REASON_MCIPHER_NOT_VALID:
	case REASON_UCIPHER_NOT_VALID:
	case REASON_AKMP_NOT_VALID:
	case REASON_UNSUPPORT_RSNE_VER:
	case REASON_CIPHER_SUITE_REJECTED:
		*Code = MT_DIAG_CONN_AUTH_FAIL;
		break;
	case REASON_PREV_AUTH_INVALID:
		*Code = MT_DIAG_CONN_FRAME_LOST;
		break;
	case REASON_INVALID_IE:
	case REASON_INVALID_RSNE_CAP:
		*Code = MT_DIAG_CONN_CAP_ERROR;
		break;
	case REASON_DECLINED:
		*Code = MT_DIAG_CONN_ACL_BLK;
		break;
	default:
		*Code = MT_DIAG_CONN_DEAUTH;
		break;
	}
}

void diag_ctrl_alloc(PRTMP_ADAPTER pAd)
{
	struct _DIAG_CTRL *pCtrl = NULL;

	os_alloc_mem(NULL, (UCHAR **)&pCtrl, sizeof(struct _DIAG_CTRL));
	if (pCtrl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR,
			"fail to allocate pDiagCtrl memory\n");
		return;
	}
	os_zero_mem(pCtrl, sizeof(struct _DIAG_CTRL));
	pCtrl->init_flag = FALSE;
	pCtrl->diag_enable = FALSE;
	pAd->pDiagCtrl = pCtrl;
}

void diag_ctrl_free(PRTMP_ADAPTER pAd)
{
	if (!pAd)
		return;
	if (pAd->pDiagCtrl)
		os_free_mem(pAd->pDiagCtrl);
	pAd->pDiagCtrl = NULL;
}

void diag_conn_error(PRTMP_ADAPTER pAd, UCHAR apidx, UCHAR *addr,
	enum MT_ENUM_DIAG_CONN_ERROR_CODE Code, UINT32 Reason)
{
	enum MT_ENUM_DIAG_CONN_ERROR_CODE DiagCode = 0;
	struct _DIAG_CTRL *pCtrl = NULL;
	struct mt_diag_ops *ops;

	if (!addr)
		return;
	if (!pAd)
		return;
	if (!((apidx < MAX_BEACON_NUM) && (apidx < pAd->ApCfg.BssidNum)))
		return;

	pCtrl = pAd->pDiagCtrl;
	if (pCtrl && pCtrl->init_flag) {
		ops = &pCtrl->ops;
		if (!ops->diag_conn_error) {
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR,
				"No function hool\n");
			return;
		}

		if (Code == MT_DIAG_CONN_DEAUTH_COM)
			diag_common_deauth_code_trans(Reason, &DiagCode);
		else if (Code < MT_DIAG_CONN_ERROR_MAX)
			DiagCode = Code;

		ops->diag_conn_error(
			PD_GET_DEVICE_IDX(pAd->physical_dev),
			pAd->band_idx,
			pAd->ApCfg.MBSSID[apidx].Ssid,
			pAd->ApCfg.MBSSID[apidx].SsidLen,
			addr,
			DiagCode,
			Reason);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR, "pCtrl not init\n");
}

void diag_conn_error_write(PRTMP_ADAPTER pAd)
{
	struct _DIAG_CTRL *pCtrl = NULL;
	struct mt_diag_ops *ops;

	pCtrl = pAd->pDiagCtrl;
	if (pCtrl && pCtrl->init_flag) {
		ops = &pCtrl->ops;
		if (ops->diag_conn_error_write)
			ops->diag_conn_error_write(
				PD_GET_DEVICE_IDX(pAd->physical_dev), pAd->band_idx);
		else
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
				"[Band%d]No functio hook\n", pAd->band_idx);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
			"diag not init band_idx=%d\n", pAd->band_idx);
}

void diag_log_file_write(PRTMP_ADAPTER pAd)
{
	struct _DIAG_CTRL *pCtrl = NULL;
	struct mt_diag_ops *ops;

	pCtrl = pAd->pDiagCtrl;
	if (pCtrl && pCtrl->init_flag) {
		ops = &pCtrl->ops;
		if (ops->diag_log_file_write)
			ops->diag_log_file_write(
				PD_GET_DEVICE_IDX(pAd->physical_dev), pAd->band_idx);
		else
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
				"[Band%d]No function hook\n", pAd->band_idx);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
			"diag not init band_idx=%d\n", pAd->band_idx);
}

void diag_miniport_mm_request(PRTMP_ADAPTER pAd, UCHAR *pData, UINT Length)
{
	PHEADER_802_11 pHdr = NULL;
	BSS_STRUCT *pMbssEntry = NULL;
	MAC_TABLE_ENTRY *pMacTblEntry = NULL;
	struct mt_diag_frame_info diag_info;
	struct _DIAG_CTRL *pCtrl = NULL;
	UCHAR band_idx = 0;
	enum MT_ENUM_DIAG_BAND diag_band;
	struct mt_diag_ops *ops;

	if (!pAd || !pData || (Length == 0))
		return;

	band_idx = hc_get_hw_band_idx(pAd);
	if (band_idx >= MT_DIAG_BAND_MAX)
		return;
	memset(&diag_info, 0, sizeof(diag_info));
	pHdr = (PHEADER_802_11) pData;
	pMacTblEntry = MacTableLookup(pAd, pHdr->Addr1);

	if (IS_VALID_ENTRY(pMacTblEntry) && IS_ENTRY_CLIENT(pMacTblEntry)
		&& (pMacTblEntry->func_tb_idx < MAX_BEACON_NUM)
		 && (pMacTblEntry->func_tb_idx < pAd->ApCfg.BssidNum))
		pMbssEntry = &pAd->ApCfg.MBSSID[pMacTblEntry->func_tb_idx];
	else {
		UCHAR apidx, first_up_idx = 0xFF;

		for (apidx = 0; apidx < pAd->ApCfg.BssidNum; apidx++) {
			if ((pAd->ApCfg.MBSSID[apidx].wdev.if_dev != NULL) &&
				(RTMP_OS_NETDEV_STATE_RUNNING(pAd->ApCfg.MBSSID[apidx].wdev.if_dev))) {
				if (RTMPEqualMemory(pAd->ApCfg.MBSSID[apidx].wdev.bssid,
					pHdr->Addr2, MAC_ADDR_LEN)) {
					pMbssEntry = &pAd->ApCfg.MBSSID[apidx];
					break;
				}
				if (first_up_idx == 0xFF)
					first_up_idx = apidx;
			}
		}
		if ((pMbssEntry == NULL) && (first_up_idx != 0xFF))
			pMbssEntry = &pAd->ApCfg.MBSSID[first_up_idx];
	}
	if (pMbssEntry) {
		if (WMODE_CAP_6G(pMbssEntry->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_6G;
		else if (WMODE_CAP_5G(pMbssEntry->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_5G;
		else
			diag_band = MT_DIAG_BAND_2G;

		pCtrl = pAd->pDiagCtrl;
		if (pCtrl && pCtrl->init_flag) {
			ops = &pCtrl->ops;
			if (pCtrl->diag_enable && ops->diag_frame_cache) {
				diag_info.ssid = pMbssEntry->Ssid;
				diag_info.ssid_len = pMbssEntry->SsidLen;
				diag_info.isTX = 1;
				diag_info.band = diag_band;
				diag_info.pData = pData;
				diag_info.dataLen = Length;
				ops->diag_frame_cache(
					PD_GET_DEVICE_IDX(pAd->physical_dev), band_idx, &diag_info);
			}
		} else
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
				"pCtrl not init, band_idx=%d\n", band_idx);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_INFO,
			"Error, no UP ap interface!!!\n");
}

void diag_dev_rx_mgmt_frm(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk)
{
	struct mt_diag_frame_info diag_info;
	MAC_TABLE_ENTRY *pEntry = NULL;
	BSS_STRUCT *pMbss = NULL;
	FRAME_CONTROL *FC = NULL;
	struct _DIAG_CTRL *pCtrl = NULL;
	UCHAR band_idx = 0;
	enum MT_ENUM_DIAG_BAND diag_band;
	struct mt_diag_ops *ops;

	if (!pRxBlk || !pAd)
		return;
	band_idx = hc_get_hw_band_idx(pAd);
	if (band_idx >= MT_DIAG_BAND_MAX)
		return;
	/* frame type & SubType check */
	FC = (FRAME_CONTROL *)pRxBlk->FC;
	if ((FC->Type != FC_TYPE_MGMT) || (
		(FC->SubType != SUBTYPE_BEACON) &&
		(FC->SubType != SUBTYPE_PROBE_REQ) &&
		(FC->SubType != SUBTYPE_PROBE_RSP) &&
		(FC->SubType != SUBTYPE_ATIM) &&
		(FC->SubType != SUBTYPE_DISASSOC) &&
		(FC->SubType != SUBTYPE_DEAUTH) &&
		(FC->SubType != SUBTYPE_ASSOC_REQ) &&
		(FC->SubType != SUBTYPE_ASSOC_RSP) &&
		(FC->SubType != SUBTYPE_REASSOC_REQ) &&
		(FC->SubType != SUBTYPE_REASSOC_RSP) &&
		(FC->SubType != SUBTYPE_AUTH))) {
		return;
	}

	if (IS_WCID_VALID(pAd, pRxBlk->wcid))
		pEntry = entry_get(pAd, pRxBlk->wcid);
	else
		pEntry = MacTableLookup(pAd, pRxBlk->Addr2);

	if ((IS_VALID_ENTRY(pEntry)) && (IS_ENTRY_CLIENT(pEntry))
		&& (pEntry->func_tb_idx < MAX_BEACON_NUM)
		&& (pEntry->func_tb_idx < pAd->ApCfg.BssidNum))
		pMbss = &pAd->ApCfg.MBSSID[pEntry->func_tb_idx];
	else {
		UCHAR apidx, first_up_idx = 0xFF;

		for (apidx = 0; apidx < pAd->ApCfg.BssidNum; apidx++) {
			if ((pAd->ApCfg.MBSSID[apidx].wdev.if_dev != NULL) &&
				(RTMP_OS_NETDEV_STATE_RUNNING(pAd->ApCfg.MBSSID[apidx].wdev.if_dev))) {
				if (RTMPEqualMemory(pAd->ApCfg.MBSSID[apidx].wdev.bssid,
					pRxBlk->Addr1, MAC_ADDR_LEN)) {
					pMbss = &pAd->ApCfg.MBSSID[apidx];
					break;
				}
				if (first_up_idx == 0xFF)
					first_up_idx = apidx;
			}
		}
		if ((pMbss == NULL) && (first_up_idx != 0xFF))
			pMbss = &pAd->ApCfg.MBSSID[first_up_idx];
	}
	if (pMbss) {
		if (WMODE_CAP_6G(pMbss->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_6G;
		else if (WMODE_CAP_5G(pMbss->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_5G;
		else
			diag_band = MT_DIAG_BAND_2G;

		pCtrl = pAd->pDiagCtrl;
		if (pCtrl && pCtrl->init_flag) {
			ops = &pCtrl->ops;
			if (!pCtrl->diag_enable || !ops->diag_frame_cache) {
				/*MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_WARN,
					"diag_enable is 0 or no diag_frame_cache hook\n");*/
				return;
			}

			memset(&diag_info, 0, sizeof(diag_info));
			diag_info.isTX = 0;
			diag_info.band = diag_band;
			diag_info.pData = pRxBlk->pData;
			diag_info.dataLen = pRxBlk->DataSize;

			diag_info.ssid = pMbss->Ssid;
			diag_info.ssid_len = pMbss->SsidLen;
			ops->diag_frame_cache(
				PD_GET_DEVICE_IDX(pAd->physical_dev), band_idx, &diag_info);
		} else
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
				"pCtrl not init, band_idx=%d\n", band_idx);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
				"Error, no UP ap interface!!!\n");
}

void diag_dev_rx_cntl_frm(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk)
{
	struct mt_diag_frame_info diag_info;
	MAC_TABLE_ENTRY *pEntry = NULL;
	BSS_STRUCT *pMbss = NULL;
	FRAME_CONTROL *FC = NULL;
	struct _DIAG_CTRL *pCtrl = NULL;
	UCHAR band_idx = 0;
	enum MT_ENUM_DIAG_BAND diag_band;
	struct mt_diag_ops *ops;

	if (!pAd || !pRxBlk)
		return;
	band_idx = hc_get_hw_band_idx(pAd);
	if (band_idx >= MT_DIAG_BAND_MAX)
		return;
	FC = (FRAME_CONTROL *)pRxBlk->FC;
	/*only BAR*/

	if (IS_WCID_VALID(pAd, pRxBlk->wcid))
		pEntry = entry_get(pAd, pRxBlk->wcid);
	else
		pEntry = MacTableLookup(pAd, pRxBlk->Addr2);

	if ((IS_VALID_ENTRY(pEntry)) && (IS_ENTRY_CLIENT(pEntry))
		&& (pEntry->func_tb_idx < MAX_BEACON_NUM)
		&& (pEntry->func_tb_idx < pAd->ApCfg.BssidNum))
		pMbss = &pAd->ApCfg.MBSSID[pEntry->func_tb_idx];
	else {
		UCHAR apidx, first_up_idx = 0xFF;

		for (apidx = 0; apidx < pAd->ApCfg.BssidNum; apidx++) {
			if ((pAd->ApCfg.MBSSID[apidx].wdev.if_dev != NULL) &&
				(RTMP_OS_NETDEV_STATE_RUNNING(pAd->ApCfg.MBSSID[apidx].wdev.if_dev))) {
				if (RTMPEqualMemory(pAd->ApCfg.MBSSID[apidx].wdev.bssid,
					pRxBlk->Addr1, MAC_ADDR_LEN)) {
					pMbss = &pAd->ApCfg.MBSSID[apidx];
					break;
				}
				if (first_up_idx == 0xFF)
					first_up_idx = apidx;
			}
		}
		if ((pMbss == NULL) && (first_up_idx != 0xFF))
			pMbss = &pAd->ApCfg.MBSSID[first_up_idx];
	}

	if (pMbss) {
		if (WMODE_CAP_6G(pMbss->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_6G;
		else if (WMODE_CAP_5G(pMbss->wdev.PhyMode))
			diag_band = MT_DIAG_BAND_5G;
		else
			diag_band = MT_DIAG_BAND_2G;

		pCtrl = pAd->pDiagCtrl;
		if (pCtrl && pCtrl->init_flag) {
			ops = &pCtrl->ops;
			if (!pCtrl->diag_enable || !ops->diag_frame_cache) {
				/*MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_WARN,
					"diag_enable is 0 or no diag_frame_cache hook\n");*/
				return;
			}

			memset(&diag_info, 0, sizeof(diag_info));
			diag_info.isTX = 0;
			diag_info.band = diag_band;
			diag_info.pData = pRxBlk->pData;
			diag_info.dataLen = pRxBlk->DataSize;

			diag_info.ssid = pMbss->Ssid;
			diag_info.ssid_len = pMbss->SsidLen;
			ops->diag_frame_cache(
				PD_GET_DEVICE_IDX(pAd->physical_dev), band_idx, &diag_info);
		} else
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
				"pCtrl not init, band_idx=%d\n", band_idx);
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
			"Error, no UP ap interface!!!\n");
}


void diag_bcn_tx(RTMP_ADAPTER *pAd, BSS_STRUCT *pMbss, UCHAR *pBeaconFrame, ULONG FrameLen)
{
	struct mt_diag_frame_info diag_info;
	struct _DIAG_CTRL *pCtrl = NULL;
	enum MT_ENUM_DIAG_BAND diag_band;
	struct mt_diag_ops *ops;

	if (!pAd || !pMbss || !pBeaconFrame || (FrameLen == 0))
		return;
	if (WMODE_CAP_6G(pMbss->wdev.PhyMode))
		diag_band = MT_DIAG_BAND_6G;
	else if (WMODE_CAP_5G(pMbss->wdev.PhyMode))
		diag_band = MT_DIAG_BAND_5G;
	else if (WMODE_CAP_2G(pMbss->wdev.PhyMode))
		diag_band = MT_DIAG_BAND_2G;
	else
		diag_band = MT_DIAG_BAND_MAX;
	if (diag_band >= MT_DIAG_BAND_MAX)
		return;
	pCtrl = pAd->pDiagCtrl;
	if (pCtrl && pCtrl->init_flag) {
		ops = &pCtrl->ops;
		if (pCtrl->diag_enable && ops->diag_frame_cache) {
			memset(&diag_info, 0, sizeof(diag_info));
			diag_info.isTX = 1;
			diag_info.band = diag_band;
			diag_info.ssid = pMbss->Ssid;
			diag_info.ssid_len = pMbss->SsidLen;
			diag_info.pData = pBeaconFrame;
			diag_info.dataLen = FrameLen;
			ops->diag_frame_cache(PD_GET_DEVICE_IDX(pAd->physical_dev),
					hc_get_hw_band_idx(pAd), &diag_info);
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_INFO, "!\n");
		}
	} else
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
			"pCtrl not init, band_idx=%d\n", hc_get_hw_band_idx(pAd));
}

void diag_ap_mlme_one_sec_proc(PRTMP_ADAPTER pAd)
{
	struct _DIAG_CTRL *pCtrl = NULL;

	if (!pAd)
		return;
	pCtrl = pAd->pDiagCtrl;
	if (pCtrl && pCtrl->init_flag) {
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) {
			pCtrl->busy_time = 0;
			MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_WARN,
				"SCAN conflict with Diag channel Occupancy\n");
		} else
			pCtrl->busy_time =
				((UINT32)pAd->WlanCounters.CcaNavTxTime.QuadPart) >> 10;

		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_INFO,
			"update band_idx:%d busy_time:%d ms by OneSecMibBucket\n",
			pAd->band_idx, pCtrl->busy_time);
		if (pCtrl->diag_enable == 0)
			return;
		/* For diag_log/association_error file write in MLME task */
		MlmeEnqueue(pAd, WIFI_DAIG_STATE_MACHINE, 0, 0, NULL, 0);
	}
}

BOOLEAN diag_proc_init(PRTMP_ADAPTER pAd)
{
	struct _RTMP_CHIP_CAP *chip_cap;
	struct _DIAG_CTRL *pCtrl = NULL;
	u8 max_band_num = MT_DIAG_BAND_MAX;

	if (!pAd) {
		MTWF_DBG(NULL, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR, "NULL parameters\n");
		return FALSE;
	}

	pCtrl = pAd->pDiagCtrl;
	if (!pCtrl) {
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR,
			"pDiagCtrl not allocate band_idx=%d\n", pAd->band_idx);
		return FALSE;
	}
	if (pCtrl->init_flag) {
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
			"pDiagCtrl already init band_idx=%d\n", pAd->band_idx);
		return FALSE;
	}
	chip_cap = PD_GET_CHIP_CAP_PTR(pAd->physical_dev);
	if (chip_cap)
		max_band_num = chip_cap->hw_band_num;
	MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
			"Init diag system, band_idx=%d\n", pAd->band_idx);
	mt_diag_proc_init(
		PD_GET_DEVICE_IDX(pAd->physical_dev),
		pAd->band_idx,
		max_band_num,
		pAd->net_dev,
		&pCtrl->ops);
	pCtrl->init_flag = TRUE;
	return TRUE;
}

BOOLEAN diag_proc_exit(PRTMP_ADAPTER pAd)
{
	struct _DIAG_CTRL *pCtrl = NULL;
	struct mt_diag_ops *ops = NULL;
	u8 last = FALSE;

	if (!pAd)
		return FALSE;

	pCtrl = pAd->pDiagCtrl;
	if (pCtrl) {
		pCtrl->init_flag = FALSE;
		ops = &pCtrl->ops;
		if (ops) {
			ops->diag_frame_cache = NULL;
			ops->diag_conn_error = NULL;
			ops->diag_conn_error_write = NULL;
			ops->diag_log_file_write = NULL;
		}
		MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
			"pAd:%p, hwifi_get_inf_num = %d\n", pAd, hwifi_get_inf_num(pAd));
		if (hwifi_get_inf_num(pAd) == 0)
			last = TRUE;
		mt_diag_proc_exit(PD_GET_DEVICE_IDX(pAd->physical_dev), pAd->band_idx, last);
	}
	return TRUE;
}

/**********************************************************************************/

struct diag_cmd_handlers {
	enum mt_diag_cmd_attr_type cmd_type;
	int (*diag_cmd_handler)(
		struct _RTMP_ADAPTER *mac_ad,
		struct wifi_dev *wdev,
		void *data, unsigned int data_len);
};

static int diag_get_statistic(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *data,
	unsigned int data_len)
{
	struct _RTMP_CHIP_CAP *cap = NULL;
	struct mt_diag_statistic_info *info = data;
	UINT8 band_idx = 0;

	if (!data || (data_len != sizeof(struct mt_diag_statistic_info)))
		return -RTMP_IO_EFAULT;

	cap = hc_get_chip_cap(mac_ad->hdev_ctrl);
	band_idx = hc_get_hw_band_idx(mac_ad);
	MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE, "band_idx=%d\n", band_idx);

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
	if (cap->fgRateAdaptFWOffload == TRUE) {
		EXT_EVENT_TX_STATISTIC_RESULT_T rTxStatResult;

		memset(&rTxStatResult, 0, sizeof(rTxStatResult));
		MtCmdGetTxStatistic(mac_ad, GET_TX_STAT_TOTAL_TX_CNT, band_idx, 0, &rTxStatResult);
		mac_ad->WlanCounters.TransmittedFragmentCount.u.LowPart +=
			(rTxStatResult.u4TotalTxCount -	rTxStatResult.u4TotalTxFailCount);
		mac_ad->WlanCounters.FailedCount.u.LowPart += rTxStatResult.u4TotalTxFailCount;
		mac_ad->WlanCounters.CurrentBwTxCount.u.LowPart += rTxStatResult.u4CurrBwTxCnt;
		mac_ad->WlanCounters.OtherBwTxCount.u.LowPart += rTxStatResult.u4OtherBwTxCnt;
	}
#endif /* RACTRL_FW_OFFLOAD_SUPPORT */

	info->rx_count = (ULONG)mac_ad->WlanCounters.RxMpduCount.QuadPart;
	info->tx_count = mac_ad->WlanCounters.TransmittedFragmentCount.u.LowPart;
	info->tx_fails = mac_ad->WlanCounters.FailedCount.u.LowPart;
	info->rx_fcs_error_count = mac_ad->WlanCounters.RxFcsErrorCount.u.LowPart;
	return 0;
}

static int diag_set_drop_action(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *data,
	unsigned int data_len)
{
	u8 ucAction = 0;

	if (!data || (data_len != sizeof(u8)))
		return -RTMP_IO_EFAULT;

	os_move_mem(&ucAction, data, data_len);
	MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE, "ucAction=%d\n", ucAction);
	HW_RX_FILTER_CTRL(mac_ad, &ucAction);
	return 0;
}

static int diag_get_ch_occ(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *data,
	unsigned int data_len)
{
	struct mt_diag_ch_occ_info *ch_occ = data;
	struct _DIAG_CTRL *pCtrl = mac_ad->pDiagCtrl;

	if (!data || (data_len != sizeof(struct mt_diag_ch_occ_info)))
		return -RTMP_IO_EFAULT;

	ch_occ->channel = HcGetRadioChannel(mac_ad);
	if (pCtrl)
		ch_occ->ch_occ = (100 * pCtrl->busy_time)/1000;
	else {
		ch_occ->ch_occ = 0;
		MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE, "No pDiagCtrl\n");
	}
	MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE, "channel=%d, ch_occ=%d\n",
		ch_occ->channel, ch_occ->ch_occ);
	return 0;
}

static int diag_set_enable(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *data,
	unsigned int data_len)
{
	struct _DIAG_CTRL *pCtrl = mac_ad->pDiagCtrl;

	if (!pCtrl)
		return -RTMP_IO_EFAULT;

	if (!data || (data_len != sizeof(pCtrl->diag_enable)))
		return -RTMP_IO_EFAULT;

	os_move_mem(&pCtrl->diag_enable, data, sizeof(pCtrl->diag_enable));
	MTWF_DBG(mac_ad, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_NOTICE,
		"diag_enable=%d\n", pCtrl->diag_enable);
	return 0;
}

static int diag_get_entry_stat_info(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *data,
	unsigned int data_len)
{
	struct mt_diag_entry_stat_info *info = data;
	UINT8 band_idx = mac_ad->band_idx;
	PMAC_TABLE_ENTRY pEntry = NULL;
	UINT16 wtbl_max_num = 0;
	struct _RTMP_ADAPTER *entry_ad;
	EXT_EVENT_TX_STATISTIC_RESULT_T rTxStatResult;

	if (!data || (data_len != sizeof(struct mt_diag_entry_stat_info)))
		return -RTMP_IO_EFAULT;

	wtbl_max_num = WTBL_MAX_NUM(mac_ad);
	if (info->entry_idx < wtbl_max_num) {
		pEntry = entry_get(mac_ad, info->entry_idx);
		if (pEntry->EntryType != ENTRY_CLIENT) {
			info->entry_status = MT_DIAG_ENTRY_SKIP;
			return 0;
		}
		entry_ad = pEntry->pAd;
		if (!entry_ad) {
			info->entry_status = MT_DIAG_ENTRY_SKIP;
			return 0;
		}
		if (entry_ad->band_idx != band_idx) {
			info->entry_status = MT_DIAG_ENTRY_SKIP;
			return 0;
		}
		os_move_mem(&info->sta_addr[0], &pEntry->Addr[0], MAC_ADDR_LEN);
		HW_GET_STA_SNR(mac_ad, pEntry->wcid, &info->snr);
		memset(&rTxStatResult, 0, sizeof(EXT_EVENT_TX_STATISTIC_RESULT_T));
		MtCmdGetTxStatistic(
			mac_ad, GET_TX_STAT_TOTAL_TX_CNT, band_idx, pEntry->wcid, &rTxStatResult);
		info->total_tx_count = rTxStatResult.u4TotalTxCount;
		info->total_tx_fail_count = rTxStatResult.u4TotalTxFailCount;
		info->entry_status = MT_DIAG_ENTRY_NORMAL;
	} else
		info->entry_status = MT_DIAG_ENTRY_INVALID;
	return 0;
}


static struct diag_cmd_handlers diag_handlers[] = {
	{mt_diag_get_statistic, diag_get_statistic},
	{mt_diag_set_drop_action, diag_set_drop_action},
	{mt_diag_get_ch_occ, diag_get_ch_occ},
	{mt_diag_set_enable, diag_set_enable},
	{mt_diag_get_entry_stat, diag_get_entry_stat_info},
	{0, }
};

int diag_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	int ret = -RTMP_IO_EOPNOTSUPP;
	int sub_cmd = ioctl_cfg->subcmd;
	struct diag_cmd_handlers *cmd_handler;

	MTWF_DBG(NULL, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_DEBUG,
		"mac_ad: %p, cmd_attr_id = %d\n", mac_ad, sub_cmd);
	for (cmd_handler = diag_handlers; (cmd_handler->cmd_type != 0); cmd_handler++) {
		if (cmd_handler->cmd_type == sub_cmd)
			ret = cmd_handler->diag_cmd_handler(
				mac_ad, wdev, ioctl_cfg->data, ioctl_cfg->data_len);
	}
	return ret;
}
#endif



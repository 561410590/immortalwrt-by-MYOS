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
/*
 ***************************************************************************
 ***************************************************************************

    Module Name:
    rt_proc.c

    Abstract:
    Create and register proc file system for ralink device

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "rt_config.h"

#ifdef CFG_SUPPORT_CSI

#define CSI_PROC_ROOT			"wlan"
#define PROC_CSI_DATA_NAME		"csi_data"
#define PROC_CSI_DBG_NAME		"csi_debug"
#define CSI_DATA_DIR_PATH		"/proc/net/wlan"

static ssize_t procCSIDataRead(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_);
static ssize_t procCSIDataPrepare(UINT_8 *buf,	struct CSI_INFO_T *prCSIInfo,	struct CSI_DATA_T *prCSIData);
static ssize_t procCSIDataOpen(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_);
static ssize_t procCSIDataRelease(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_);
static ssize_t procCSIDbgInfoShow(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_);

struct mt_proc_cmd_handler csi_cmd_handler[] = {
	{proc_file_open, procCSIDataOpen},
	{proc_file_read, procCSIDataRead},
	{proc_file_release, procCSIDataRelease},
	{proc_file_show, procCSIDbgInfoShow},
	{0, }
};
#endif

struct mt_proc_cmd_handler *ProcCmdTable[] = {
#ifdef CFG_SUPPORT_CSI
	csi_cmd_handler,
#endif
	NULL
};

int mtk_proc_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	int ret = -RTMP_IO_EOPNOTSUPP;
	unsigned short cmd_attr_id = ioctl_cfg->subcmd;
	unsigned short cmd_sub_id = ioctl_cfg->subid;
	struct mt_proc_cmd_handler *handler;

	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_DEBUG,
		"mac_ad: %p, cmd_attr_id = %d subid = %d\n", mac_ad, cmd_attr_id, cmd_sub_id);

	if (cmd_sub_id < PROC_ATTR_MAX)
		for (handler = ProcCmdTable[cmd_sub_id]; (handler && (handler->attr_type != 0)); handler++) {
			if (handler->attr_type == cmd_attr_id)
				ret = handler->cmd_handler(mac_ad, wdev, ioctl_cfg);
		}

	return ret;
}

#ifdef CFG_SUPPORT_CSI
void *csi_proc_dir;
static INT8 csi_if_num;	/*csi proc interface counter*/

static ssize_t procCSIDbgInfoShow(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_)
{
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg = (struct mt_os_wifi_ioctl_cmd_cfg *)ctrl_data_;
	MT_OS_SEQFD seq = (MT_OS_SEQFD)ioctl_cfg->data;
	UCHAR csi_mode;
	struct wifi_dev *pwdev;
	struct CSI_INFO_T *prCSIInfo = NULL;
	UCHAR band_idx = 0;
	PCSI_STA pCSISta = NULL;
	UCHAR csi_sta_num = 0;
	u8 idx;
	MAC_TABLE_ENTRY *pEntry = NULL;

	prCSIInfo = &pAd->rCSIInfo;
	csi_mode = prCSIInfo->CSI_report_mode;

	if (!csi_mode) {
		mt_proc_show(seq, "report_mode: %s\n", "DISABLE");
		return 0;
	}

	mt_proc_show(seq, "report_mode: %s\n", (csi_mode == CSI_PROC)?"PROC" : "NETLINK");

	mt_proc_show(seq, "csi_fw_version: %03d\n", prCSIInfo->FWVer);

	mt_proc_show(seq, "csi_buffer_cnt: used(%d)-total(%d)\n", prCSIInfo->u4CSIBufferUsed, CSI_RING_SIZE);

	/*show sta list*/
	NdisAcquireSpinLock(&prCSIInfo->CSIStaListLock);
	mt_proc_show(seq, "###dump csi sta mac list###\n");
	DlListForEach(pCSISta, &prCSIInfo->CSIStaList, CSI_STA, List) {
		mt_proc_show(seq, "STA%02d-band%d-%02x:%02x:%02x:%02x:%02x:%02x\n",
			csi_sta_num, pCSISta->band_idx, PRINT_MAC(pCSISta->Addr));
		csi_sta_num++;
	}
	NdisReleaseSpinLock(&prCSIInfo->CSIStaListLock);

	mt_proc_show(seq, "###CSI active mode station list###\n");
	for (idx = 1; idx <= MAX_CSI_ACTIVE_STA_NUM; idx++) {
		if (BIT(idx) & prCSIInfo->wcid_bitmap) {
			pEntry = entry_get(pAd, idx);
			mt_proc_show(seq, "STA#%d: %pM - wcid(%d)\n", idx, pEntry->Addr, pEntry->wcid);
		}
	}

	pwdev = prCSIInfo->csi_wdev;

	mt_proc_show(seq, "band%d_status: %s\n", band_idx, (pwdev)?"ON" : "OFF");

	if (pwdev) {

		band_idx = HcGetBandByWdev(pwdev);

		mt_proc_show(seq, "band%d_channel: %d\n", band_idx, pwdev->channel);
		mt_proc_show(seq, "band%d_MAC: "MACSTR"\n", band_idx, MAC2STR(pwdev->bssid));

		if (prCSIInfo->ucValue2[CSI_CONFIG_FRAME_TYPE])
			mt_proc_show(seq, "band%d_frame_type(Subtype+Type): %d\n", band_idx, prCSIInfo->ucValue2[CSI_CONFIG_FRAME_TYPE]);

		if (prCSIInfo->ucValue1[CSI_CONFIG_OUTPUT_FORMAT])
			mt_proc_show(seq, "band%d_tone_process: %s\n", band_idx,
			(prCSIInfo->ucValue1[CSI_CONFIG_OUTPUT_FORMAT] == CSI_OUTPUT_TONE_MASKED)?"tone_mask" : "tone_mask_reorder");
		else
			mt_proc_show(seq, "band%d_tone_process: %s\n", band_idx, "raw_tone");

		mt_proc_show(seq, "band%d_protocol: %03d\n", band_idx, prCSIInfo->protocol_filter);
	}

	return 0;
}


/*for debug proc*/

static ssize_t procCSIDataOpen(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_)
{
	struct CSI_INFO_T *prCSIInfo = NULL;

	if (!pAd) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"proc pointer get fail!!!\n");
		return -1;
	}

	prCSIInfo = &pAd->rCSIInfo;
	prCSIInfo->bIncomplete = FALSE;		/*no remain pkt bytes left*/
	return 0;
}

static ssize_t procCSIDataRelease(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_)
{
	struct CSI_INFO_T *prCSIInfo = NULL;

	if (!pAd) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"proc pointer get fail!!!\n");
		return -1;
	}

	prCSIInfo = &pAd->rCSIInfo;
	prCSIInfo->bIncomplete = FALSE;		/*no remain pkt bytes left*/
	return 0;
}
/*
*this function prepare a pkt for APP to use.
 *the format of pkt depends on some specific rules as follows.
*/
static ssize_t procCSIDataPrepare(
	UINT_8 *buf,
	struct CSI_INFO_T *prCSIInfo,
	struct CSI_DATA_T *prCSIData)
{
	UINT_8 ucValue1 = prCSIInfo->ucValue1[CSI_CONFIG_INFO] & CSI_INFO_RSVD1;
	UINT_8 ucValue2 = prCSIInfo->ucValue1[CSI_CONFIG_INFO] & CSI_INFO_RSVD2;

	return mt_procCSIDataPrepare(buf, prCSIData, ucValue1, ucValue2);
}

static ssize_t procCSIDataRead(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, void *ctrl_data_)
{

	UINT_8 *temp = NULL;
	struct CSI_DATA_T *rTmpCSIData = NULL;
	UINT_32 u4CopySize = 0;
	UINT_32 u4StartIdx = 0;
	INT_32 i4Pos = 0;
	struct CSI_INFO_T *prCSIInfo = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg = (struct mt_os_wifi_ioctl_cmd_cfg *)ctrl_data_;
	size_t count = (size_t)ioctl_cfg->data_len;
	char *buf = (char *)ioctl_cfg->data;
	int ret;


	if (!pAd) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"proc pointer get fail!!!\n");
		return -1;
	}

	prCSIInfo = &pAd->rCSIInfo;
	temp = prCSIInfo->byte_stream;

	if (!temp) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"temp NULL pointer!!!\n");
		return -1;
	}
	/* alloc mem for CSIData */
	os_alloc_mem_suspend(NULL, (UCHAR **)&rTmpCSIData, sizeof(struct CSI_DATA_T));

	if (!rTmpCSIData) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"rTmpCSIData fail to alloc mem!\n");
		return -1;
	}

	os_zero_mem(rTmpCSIData, sizeof(struct CSI_DATA_T));

	if (prCSIInfo->bIncomplete == FALSE) {

		RtmpOSTaskWaitEvent(pAd, &prCSIInfo->waitq, prCSIInfo->u4CSIBufferUsed > 0, &ret);
		if (ret) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR, "RtmpOSTaskWaitEvent, ret=%d\n",	ret);
			goto out;
		}
		/*
		*No older CSI data in buffer waiting for reading out, so prepare a new one
		*for reading.
		*/
		os_zero_mem(temp, Max_Stream_Bytes);
		if (wlanPopCSIData(pAd, rTmpCSIData)) {
			i4Pos = procCSIDataPrepare(temp,
				prCSIInfo, rTmpCSIData);
		}

		/*reading the CSI data from 0 byte*/
		u4StartIdx = 0;
		if (i4Pos > count) {
			u4CopySize = count;
			prCSIInfo->u4RemainingDataSize = i4Pos - count;
			prCSIInfo->u4CopiedDataSize = count;
			prCSIInfo->bIncomplete = TRUE;
		} else {
			u4CopySize = i4Pos;
		}
	} else {
		/*if there is one pkt left some bytes to read */
		/* Reading the remaining CSI data in the buffer*/
		u4StartIdx = prCSIInfo->u4CopiedDataSize;
		if (prCSIInfo->u4RemainingDataSize > count) {
			u4CopySize = count;
			prCSIInfo->u4RemainingDataSize -= count;
			prCSIInfo->u4CopiedDataSize += count;
		} else {
			u4CopySize = prCSIInfo->u4RemainingDataSize;
			prCSIInfo->u4CopiedDataSize = 0;
			prCSIInfo->u4RemainingDataSize = 0;
			prCSIInfo->bIncomplete = FALSE;
		}
	}

	if (copy_to_user(buf, &temp[u4StartIdx], u4CopySize)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR, "copy to user failed\n");
		goto out;
	}

out:
	if (rTmpCSIData) {
		if (rTmpCSIData->ac2IData)
			os_free_mem(rTmpCSIData->ac2IData);
		if (rTmpCSIData->ac2QData)
			os_free_mem(rTmpCSIData->ac2QData);
		os_free_mem(rTmpCSIData);
	}

	return (ssize_t)u4CopySize;

}

int csi_proc_init(RTMP_ADAPTER *pAd)
{
	char csi_proc_name[64] = {0};
	u8 band_idx;
	int ret;

	band_idx = hc_get_hw_band_idx(pAd);

	if (!csi_proc_dir) {
		csi_proc_dir = mt_proc_mkdir(CSI_PROC_ROOT); /* proc /net /wlan */

		if (!csi_proc_dir) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"csi_proc_dir get error!\n");
			return -1;
		}
	}

	pAd->rCSIInfo.net_dev = pAd->net_dev;
	pAd->rCSIInfo.subid = CSI;
	/*for debug proc*/
	if (!pAd->rCSIInfo.debug_proc) {
		ret = snprintf(csi_proc_name, sizeof(csi_proc_name), "%s_%d", PROC_CSI_DBG_NAME, band_idx);

		if (os_snprintf_error(sizeof(csi_proc_name), ret)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"final_name snprintf error!\n");
			return -1;
		}

		/* proc /net /wlan /csi_debug_card0 */
		pAd->rCSIInfo.debug_proc = mt_csi_proc_create(csi_proc_dir, csi_proc_name, (void *)&pAd->rCSIInfo, CSI_PROC_DEBUG);

		if (!pAd->rCSIInfo.debug_proc) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"prEntry is NULL! csi debug proc create fail.\n");
			return -1;
		}

		csi_if_num++;

		os_zero_mem(csi_proc_name, sizeof(csi_proc_name));
	}

	/*for data proc*/
	if (!pAd->rCSIInfo.data_proc) {
		ret = snprintf(csi_proc_name, sizeof(csi_proc_name), "%s_%d", PROC_CSI_DATA_NAME, band_idx);

		if (os_snprintf_error(sizeof(csi_proc_name), ret)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"final_name snprintf error!\n");
			return -1;
		}

		/* proc /net /wlan /csi_data_0 */
		pAd->rCSIInfo.data_proc = mt_csi_proc_create(csi_proc_dir, csi_proc_name, (void *)&pAd->rCSIInfo, CSI_PROC_DATA);

		if (!pAd->rCSIInfo.data_proc) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"prEntry is NULL! csi data proc create fail.\n");
			return -1;
		}

		csi_if_num++;
	}

	return 0;
}

int csi_proc_deinit(RTMP_ADAPTER *pAd)
{
	if (!csi_proc_dir) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR, "csi_proc_dir is NULL!\n");
		return -1;
	}

	/*delect data proc entry*/
	if (mt_proc_remove(pAd->rCSIInfo.data_proc)) {
		pAd->rCSIInfo.data_proc = NULL;
		csi_if_num--;
	}

	/*delect debug proc entry*/
	if (mt_proc_remove(pAd->rCSIInfo.debug_proc)) {
		pAd->rCSIInfo.debug_proc = NULL;
		csi_if_num--;
	}

	if (csi_if_num == 0) {
		mt_proc_remove(csi_proc_dir);	/*delect dir: proc -net -wlan */
		csi_proc_dir = NULL;
	} else if (csi_if_num < 0)
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"csi_if_num is error!(%d)\n", (INT32)csi_if_num);

	return 0;
}
#endif

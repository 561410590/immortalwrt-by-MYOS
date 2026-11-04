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

#ifndef _DIAG_H_
#define _DIAG_H_

#ifdef WIFI_DIAG

#ifndef NONE
/*build error while define NONE onpened because of redefinition
of struct timeval & struct itimerspec*/
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#endif
#include "rtmp_comm.h"
#include "rtmp_type.h"
#include "rtmp_os.h"
#include "rtmp.h"
#include "mt_osal_diag.h"

struct _DIAG_CTRL {
	UINT32	diag_enable;
	UINT32	busy_time;
	struct  mt_diag_ops ops;
	UINT8	init_flag;
};
void diag_conn_error(PRTMP_ADAPTER pAd, UCHAR apidx, UCHAR* addr,
	enum MT_ENUM_DIAG_CONN_ERROR_CODE Code, UINT32 Reason);
void diag_conn_error_write(PRTMP_ADAPTER pAd);
void diag_miniport_mm_request(PRTMP_ADAPTER pAd, UCHAR *pData, UINT Length);
void diag_bcn_tx(RTMP_ADAPTER *pAd, BSS_STRUCT *pMbss, UCHAR *pBeaconFrame,ULONG FrameLen );
void diag_log_file_write(PRTMP_ADAPTER pAd);
void diag_dev_rx_mgmt_frm(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk);
void diag_dev_rx_cntl_frm(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk);
void diag_ap_mlme_one_sec_proc(PRTMP_ADAPTER pAd);
void diag_ctrl_alloc(PRTMP_ADAPTER pAd);
void diag_ctrl_free(PRTMP_ADAPTER pAd);

BOOLEAN diag_proc_init(PRTMP_ADAPTER pAd);
BOOLEAN diag_proc_exit(PRTMP_ADAPTER pAd);

int diag_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg);

#endif
#endif /* #ifndef _DIAG_H_ */


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
    lp.h

    Abstract:
    Support AP Low Power function.

*/
#ifndef __LP_H__
#define __LP_H__

#ifdef LP_SUPPORT
void lp_init_nic(struct physical_device *ph_dev);
void lp_exit_nic(struct physical_device *ph_dev);
void lp_cfg_nic(struct _RTMP_ADAPTER *ad);

void lp_init_band(struct _RTMP_ADAPTER *ad);
void lp_cfg_band(struct _RTMP_ADAPTER *ad);

/* per band PST */
void set_pst_cfg_band(struct _RTMP_ADAPTER *ad);
/* per band PB-TPO & LP-TPO */
void set_tpo_cfg_band(struct _RTMP_ADAPTER *ad);
/* per band 1RPD cfg */
void set_1rpd_cfg_band(struct _RTMP_ADAPTER *ad, u8 on);

int set_lp_option(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
int set_1rppd_option(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
int set_pci_l1ss(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
int set_tpt_option(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
int show_lp_info_nic(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
int show_1rpd_info_band(struct _RTMP_ADAPTER *ad, RTMP_STRING *arg);
/* int show_tpt_option_nic(RTMP_ADAPTER *ad, RTMP_STRING *arg); Query WM TBD */
#endif /* LP_SUPPORT */

#endif /* __LP_H__ */


/*
 * Copyright (c) [2024], MediaTek Inc. All rights reserved.
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
	rro_cmd.h
*/

#ifndef __CMM_RRO_CMD_H__
#define __CMM_RRO_CMD_H__

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/
#include "rt_config.h"
/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/

/*******************************************************************************
 *    MACRO
 ******************************************************************************/

/*******************************************************************************
 *    TYPES
 ******************************************************************************/

/*******************************************************************************
 *    GLOBAL VARIABLES
 ******************************************************************************/

/*******************************************************************************
 *    FUNCTION PROTOTYPES
 ******************************************************************************/
INT32 uni_cmd_rro_rx_path(
	struct _RTMP_ADAPTER *ad,
	UINT8 rx_path_type);

INT32 uni_cmd_release_rro_setbl(
	struct _RTMP_ADAPTER *ad,
	UINT16 seid);

INT32 uni_cmd_rro_bypass_mode(
	struct _RTMP_ADAPTER *ad,
	UINT8 rro_bypass_type);

INT32 uni_cmd_rro_tx_free_done_path(
	struct _RTMP_ADAPTER *ad,
	UINT8 tx_free_done_path);

INT32 uni_cmd_rro_flush_timeout(
	struct _RTMP_ADAPTER *ad,
	UINT16 step_one_timeout,
	UINT16 flush_all_timeout);

INT32 uni_cmd_rro_rss_map(
	struct _RTMP_ADAPTER *ad,
	UINT32 rss_map0,
	UINT32 rss_map1);

INT set_rro_debug_proc(
	struct _RTMP_ADAPTER *pAd,
	RTMP_STRING *arg);

#endif				/* __CMM_RRO_CMD_H__ */

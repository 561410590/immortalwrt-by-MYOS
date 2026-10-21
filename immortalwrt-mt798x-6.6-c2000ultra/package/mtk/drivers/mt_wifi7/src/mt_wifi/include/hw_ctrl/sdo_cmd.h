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
	sdo_cmd.h
*/

#ifndef __CMM_SDO_CMD_H__
#define __CMM_SDO_CMD_H__

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/
#include "rt_config.h"
#include "os/hwifi_main.h"

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
INT32 UniCmdGetSdo(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2,
	struct _CR4_QUERY_STRUC *cr4_query_list);

INT32 UniCmdSetSdo(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2,
	IN UINT32 u4ExtSize,
	IN PUINT8 pExtData);

INT32 UniCmdSetSdoAutoBA(
	IN struct _RTMP_ADAPTER *pAd,
	IN BOOLEAN Enable,
	IN UINT32 Timeout);

INT32 UniCmdSetSdoDscpPri(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT8 inf_idx,
	IN INT if_type);

#if defined(CONFIG_HOTSPOT_R2) || defined(CONFIG_PROXY_ARP)
INT32 UniCmdUpdateSdoHotspotInfo(
	IN struct _RTMP_ADAPTER *pAd,
	IN struct _MT_HOTSPOT_INFO_UPDATE *InfoUpdateT);
#endif /* defined(CONFIG_HOTSPOT_R2) || defined(CONFIG_PROXY_ARP) */

INT32 UniCmdSetSdoCPSEnable(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT8 Mode);

#ifdef CONFIG_RA_PHY_RATE_SUPPORT
UINT32 UniCmdSetHighpriFixedrate(IN struct _RTMP_ADAPTER *pAd,
	struct SDO_Fixed_Rate *sdo_fixed_rate,
	IN UINT32 count);
#endif

#ifdef CFG_RED_SUPPORT
INT32 UniCmdSetSdoRedSetting(
	IN struct _RTMP_ADAPTER *pAd,
	IN PUINT_8 prParam);
#endif /* CFG_RED_SUPPORT */


INT32 UniCmdSetSdoPktBudgetCtrl(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT_8 bss_idx,
	IN UINT_16 wcid,
	IN UCHAR type
	);

INT32 UniCmdGetSdoBssAcqPktNum(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT_32 *au4BssMap
	);

INT32 UniCmdSetSdoOverride(
	IN struct _RTMP_ADAPTER *pAd,
	IN struct mtk_mac_sdo_override *override
	);

VOID UniEventSdoHandler(
	VOID *physical_dev,
	UINT8 *pData,
	UINT32 Length,
	EVENT_RXD *event_rxd);

#endif				/* __CMM_SDO_CMD_H__ */

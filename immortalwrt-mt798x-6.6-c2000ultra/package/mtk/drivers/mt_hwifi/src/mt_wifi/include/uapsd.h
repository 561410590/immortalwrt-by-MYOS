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

    Abstract:

	All related WMM UAPSD definitions & function prototype.

***************************************************************************/


/* only for UAPSD_TIMING_RECORD */

#ifndef __UAPSD_H__
#define __UAPSD_H__

/* uapsd initialization */
#define UAPSD_INFO_INIT(__pInfo)				\
	{												\
		(__pInfo)->bAPSDCapable = FALSE;				\
	}

/*
	Enable or disable UAPSD flag in WMM element in beacon frame;
	purpose: set UAPSD enable/disable bit
*/
#define UAPSD_MR_IE_FILL(__QosCtrlField, __pUapsdInfo)						\
	((__QosCtrlField) |= ((__pUapsdInfo)->bAPSDCapable) ? 0x80 : 0x00)

/*
========================================================================
Routine Description:
	Parse the UAPSD field in WMM element in (re)association request frame.

Arguments:
	pAd				Pointer to our adapter
	*pEntry			STATION
	*pElm			QoS information field
	FlgApsdCapable	TRUE: Support UAPSD

Return Value:
	None

Note:
	No protection is needed.

	1. Association -> TSPEC:
		use static UAPSD settings in Association
		update UAPSD settings in TSPEC

	2. Association -> TSPEC(11r) -> Reassociation:
		update UAPSD settings in TSPEC
		backup static UAPSD settings in Reassociation

	3. Association -> Reassociation:
		update UAPSD settings in TSPEC
		backup static UAPSD settings in Reassociation
========================================================================
*/
VOID UAPSD_AssocParse(
	IN	PRTMP_ADAPTER		pAd,
	IN	MAC_TABLE_ENTRY		*pEntry,
	IN	UCHAR * pElm,
	IN	BOOLEAN				FlgApsdCapable);

/* End of ap_uapsd.h */
#endif /*__UAPSD_H__*/

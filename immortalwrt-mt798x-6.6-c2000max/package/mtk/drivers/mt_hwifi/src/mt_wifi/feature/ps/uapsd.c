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


#include "rt_config.h"

#ifdef UAPSD_SUPPORT
#include "uapsd.h"


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
	IN RTMP_ADAPTER *pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN UCHAR *pElm,
	IN BOOLEAN FlgApsdCapable)
{
	PQBSS_STA_INFO_PARM  pQosInfo;
	UCHAR UAPSD[4];
	UINT32 IdApsd;

	/* check if the station enables UAPSD function */
	if ((pEntry) && (FlgApsdCapable == TRUE)) {
		/* backup its UAPSD parameters */
		pQosInfo = (PQBSS_STA_INFO_PARM) pElm;
		UAPSD[QID_AC_BE] = pQosInfo->UAPSD_AC_BE;
		UAPSD[QID_AC_BK] = pQosInfo->UAPSD_AC_BK;
		UAPSD[QID_AC_VI] = pQosInfo->UAPSD_AC_VI;
		UAPSD[QID_AC_VO] = pQosInfo->UAPSD_AC_VO;
		pEntry->MaxSPLength = pQosInfo->MaxSPLength;
		MTWF_DBG(NULL, DBG_CAT_PS, CATPS_UAPSD, DBG_LVL_INFO, "apsd> UAPSD %d %d %d %d!\n",
				 pQosInfo->UAPSD_AC_BE, pQosInfo->UAPSD_AC_BK,
				 pQosInfo->UAPSD_AC_VI, pQosInfo->UAPSD_AC_VO);
		MTWF_DBG(NULL, DBG_CAT_PS, CATPS_UAPSD, DBG_LVL_INFO, "apsd> MaxSPLength = %d\n",
				 pEntry->MaxSPLength);

		/* use static UAPSD setting of association request frame */
		for (IdApsd = 0; IdApsd < 4; IdApsd++) {
			pEntry->bAPSDCapablePerAC[IdApsd] = UAPSD[IdApsd];
			pEntry->bAPSDDeliverEnabledPerAC[IdApsd] = UAPSD[IdApsd];
		}

		if ((pEntry->bAPSDCapablePerAC[QID_AC_BE] == 0) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_BK] == 0) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_VI] == 0) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_VO] == 0))
			CLIENT_STATUS_CLEAR_FLAG(pEntry, fCLIENT_STATUS_APSD_CAPABLE);
		else
			CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_APSD_CAPABLE);

		if ((pEntry->bAPSDCapablePerAC[QID_AC_BE] == 1) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_BK] == 1) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_VI] == 1) &&
			(pEntry->bAPSDCapablePerAC[QID_AC_VO] == 1)) {
			/* all AC are U-APSD */
			MTWF_DBG(NULL, DBG_CAT_PS, CATPS_UAPSD, DBG_LVL_INFO, "apsd> all AC are UAPSD\n");
		} else {
			/* at least one AC is not U-APSD */
			MTWF_DBG(NULL, DBG_CAT_PS, CATPS_UAPSD, DBG_LVL_INFO, "apsd> at least one AC is not UAPSD %d %d %d %d\n",
					 pEntry->bAPSDCapablePerAC[QID_AC_BE],
					 pEntry->bAPSDCapablePerAC[QID_AC_BK],
					 pEntry->bAPSDCapablePerAC[QID_AC_VI],
					 pEntry->bAPSDCapablePerAC[QID_AC_VO]);
		}
		MTWF_DBG(NULL, DBG_CAT_PS, CATPS_UAPSD, DBG_LVL_INFO, "apsd> MaxSPLength = %d\n", pEntry->MaxSPLength);
	}
}

#endif /* UAPSD_SUPPORT */


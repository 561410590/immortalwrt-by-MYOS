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
    sta_cfg.h

    Abstract:


    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------

*/

#ifndef __STA_CFG_H__
#define __STA_CFG_H__

INT RTMPSTAPrivIoctlSet(
	IN RTMP_ADAPTER * pAd,
	IN RTMP_STRING * SetProcName,
	IN RTMP_STRING * ProcArg);

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) || defined(MT_WOW_SUPPORT)
/* set WOW enable */
INT Set_WOW_Enable(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set GPIO pin for wake-up signal */
INT Set_WOW_GPIO(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set delay time for WOW really enable */
INT Set_WOW_Delay(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set wake up hold time */
INT Set_WOW_Hold(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set wakeup signal type */
INT Set_WOW_InBand(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set wakeup interface */
INT Set_WOW_Interface(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set wow if down support */
INT Set_WOW_IfDown_Support(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set IPAdress for ARP response */
INT Set_WOW_IPAddress(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
/* set wakeup GPIO High Low */
INT Set_WOW_GPIOHighLow(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) || defined(MT_WOW_SUPPORT) */


#ifndef MTK_CMD_STA_IOCTL_PMA_SA_FLUSH
#define MTK_CMD_STA_IOCTL_PMA_SA_FLUSH		0x01
#define MTK_CMD_STA_IOCTL_PMA_SA_REMOVE		0x02
#define MTK_CMD_STA_IOCTL_PMA_SA_ADD		0x03
struct mt_priv_pmksa_info {
	UINT32 Cmd;
	UCHAR *pBssid;
	UCHAR *pPmkid;
	VOID *pNetDev;
};
#endif

INT RtmpIoctl_rt_ioctl_siwpmksa(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_pmksa_info *pIoctlPmaSa);

#endif /* __STA_CFG_H__ */


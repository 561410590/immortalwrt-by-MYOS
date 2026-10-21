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
	rtmp_timer.h

    Abstract:
	Ralink Wireless Driver timer related data structures and delcarations

    Revision History:
	Who           When                What
	--------    ----------      ----------------------------------------------
	Name          Date                 Modification logs
	Shiang Tu    Aug-28-2008	init version

*/

#ifndef __RTMP_TIMER_H__
#define  __RTMP_TIMER_H__

#include "rtmp_os.h"

struct _RTMP_ADAPTER;

#ifdef LINUX
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
#define DECLARE_TIMER_FUNCTION(_func)			\
	void rtmp_timer_##_func(struct timer_list *_timer)
#else
#define DECLARE_TIMER_FUNCTION(_func)			\
	void rtmp_timer_##_func(unsigned long data)
#endif
#else /* LINUX */
#define DECLARE_TIMER_FUNCTION(_func)			\
	void rtmp_timer_##_func(unsigned long data)
#endif /* NONE */

#define GET_TIMER_FUNCTION(_func)				\
	(PVOID)rtmp_timer_##_func

/* ----------------- Timer Related MARCO ---------------*/
/* In some os or chipset, we have a lot of timer functions and will read/write register, */
/*   it's not allowed in Linux USB sub-system to do it ( because of sleep issue when */
/*  submit to ctrl pipe). So we need a wrapper function to take care it. */

typedef struct _RALINK_TIMER_STRUCT {
	RTMP_OS_TIMER TimerObj;	/* Ndis Timer object */
	BOOLEAN Valid;		/* Set to True when call RTMPInitTimer */
	BOOLEAN State;		/* True if timer cancelled */
	BOOLEAN PeriodicType;	/* True if timer is periodic timer */
	BOOLEAN Repeat;		/* True if periodic timer */
	ULONG TimerValue;	/* Timer value in milliseconds */
	ULONG cookie;		/* os specific object */
	void *pAd;
	void *ph_dev;
	NDIS_SPIN_LOCK *timer_lock;
	VOID *pCaller;
} RALINK_TIMER_STRUCT, *PRALINK_TIMER_STRUCT;

typedef struct _TIMER_FUNC_CONTEXT {
	struct _RTMP_ADAPTER *pAd;
	struct wifi_dev *wdev;
	UCHAR BandIdx;
} TIMER_FUNC_CONTEXT, *PTIMER_FUNC_CONTEXT;

DECLARE_TIMER_FUNCTION(MlmePeriodicExecTimer);
DECLARE_TIMER_FUNCTION(MlmeRssiReportExec);
DECLARE_TIMER_FUNCTION(AsicRxAntEvalTimeout);

#ifdef DOT11W_PMF_SUPPORT
DECLARE_TIMER_FUNCTION(PMF_SAQueryTimeOut);
DECLARE_TIMER_FUNCTION(PMF_SAQueryConfirmTimeOut);
#endif /* DOT11W_PMF_SUPPORT */


#ifdef CONFIG_AP_SUPPORT
DECLARE_TIMER_FUNCTION(APDetectOverlappingExec);

#ifdef DOT11N_DRAFT3
DECLARE_TIMER_FUNCTION(Bss2040CoexistTimeOut);
#endif /* DOT11N_DRAFT3 */

DECLARE_TIMER_FUNCTION(CMTimerExec);
DECLARE_TIMER_FUNCTION(APQuickResponeForRateUpExec);

#ifdef IDS_SUPPORT
DECLARE_TIMER_FUNCTION(RTMPIdsPeriodicExec);
#endif /* IDS_SUPPORT */

#ifdef DOT11R_FT_SUPPORT
DECLARE_TIMER_FUNCTION(FT_KDP_InfoBroadcast);
#endif /* DOT11R_FT_SUPPORT */
#ifdef ZERO_PKT_LOSS_SUPPORT
DECLARE_TIMER_FUNCTION(CSALastBcnTxEventTimeout);
DECLARE_TIMER_FUNCTION(ChnlSwitchStaNullAckWaitTimeout);
DECLARE_TIMER_FUNCTION(ChnlSwitchStaNullDataTxTimeout);
#endif /*ZERO_PKT_LOSS_SUPPORT*/
#endif /* CONFIG_AP_SUPPORT */
DECLARE_TIMER_FUNCTION(CSAEventTimeout);
DECLARE_TIMER_FUNCTION(PPCSAEventTimeout);

#ifdef CONFIG_STA_SUPPORT
DECLARE_TIMER_FUNCTION(StaQuickResponeForRateUpExec);
DECLARE_TIMER_FUNCTION(WpaDisassocApAndBlockAssoc);

#ifdef RTMP_PCI_SUPPORT
DECLARE_TIMER_FUNCTION(PsPollWakeExec);
DECLARE_TIMER_FUNCTION(RadioOnExec);
#endif /* RTMP_PCI_SUPPORT */

#ifdef SWACI_MECHANISM
DECLARE_TIMER_FUNCTION(SetRcpiTestMode);
#endif

#ifdef DOT11R_FT_SUPPORT
DECLARE_TIMER_FUNCTION(FT_OTA_AuthTimeout);
DECLARE_TIMER_FUNCTION(FT_OTD_TimeoutAction);
#endif /* DOT11R_FT_SUPPORT */


#endif /* CONFIG_STA_SUPPORT */

#ifdef WSC_INCLUDED
DECLARE_TIMER_FUNCTION(WscEAPOLTimeOutAction);
DECLARE_TIMER_FUNCTION(Wsc2MinsTimeOutAction);
DECLARE_TIMER_FUNCTION(WscUPnPMsgTimeOutAction);
DECLARE_TIMER_FUNCTION(WscM2DTimeOutAction);
DECLARE_TIMER_FUNCTION(WscPBCTimeOutAction);
#ifdef CON_WPS
DECLARE_TIMER_FUNCTION(WscScanDoneCheckTimeOutAction);
#endif /*CON_WPS*/
#ifdef WSC_STA_SUPPORT
DECLARE_TIMER_FUNCTION(WscPINTimeOutAction);
#endif
DECLARE_TIMER_FUNCTION(WscScanTimeOutAction);
DECLARE_TIMER_FUNCTION(WscProfileRetryTimeout);
#ifdef WSC_LED_SUPPORT
DECLARE_TIMER_FUNCTION(WscLEDTimer);
DECLARE_TIMER_FUNCTION(WscSkipTurnOffLEDTimer);
#endif /* WSC_LED_SUPPORT */
#ifdef LED_CONTROL_SUPPORT
DECLARE_TIMER_FUNCTION(LEDControlTimer);
#endif
#ifdef CONFIG_AP_SUPPORT
DECLARE_TIMER_FUNCTION(WscUpdatePortCfgTimeout);
#ifdef WSC_V2_SUPPORT
DECLARE_TIMER_FUNCTION(WscSetupLockTimeout);
#endif /* WSC_V2_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
#endif /* WSC_INCLUDED */

#ifdef CONFIG_HOTSPOT
#ifdef CONFIG_STA_SUPPORT
DECLARE_TIMER_FUNCTION(GASResponseTimeout);
DECLARE_TIMER_FUNCTION(GASCBDelayTimeout);
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
DECLARE_TIMER_FUNCTION(PostReplyTimeout);
#endif /* CONFIG_AP_SUPPORT */
#endif /* CONFIG_HOTSPOT */


#ifdef CONFIG_ATE
DECLARE_TIMER_FUNCTION(ATEPeriodicExec);
#endif /* CONFIG_ATE */

#ifdef BACKGROUND_SCAN_SUPPORT
DECLARE_TIMER_FUNCTION(BackgroundScanTimeout);
DECLARE_TIMER_FUNCTION(dedicated_rx_hist_scan_timeout);
#ifdef IPI_MAINRADIO_SCAN_SUPPORT
DECLARE_TIMER_FUNCTION(mainRadio_rx_hist_scan_timeout);
#endif
/*DECLARE_TIMER_FUNCTION(DfsZeroWaitTimeout);*/
DECLARE_TIMER_FUNCTION(dfs_zero_wait_ch_init_timeout);
#endif /* BACKGROUND_SCAN_SUPPORT */
#ifdef CONFIG_AP_SUPPORT
DECLARE_TIMER_FUNCTION(AutoChSelScanTimeout);
DECLARE_TIMER_FUNCTION(AutoChSelRestoreTimeout);
#endif/* CONFIG_AP_SUPPORT */

#ifdef CHANNEL_SWITCH_MONITOR_CONFIG
DECLARE_TIMER_FUNCTION(ch_switch_monitor_timeout);
#endif
DECLARE_TIMER_FUNCTION(ChOpTimeout);

#ifdef DOT11_HE_AX
#ifdef CONFIG_AP_SUPPORT
DECLARE_TIMER_FUNCTION(trigger_timer_callback);
#endif
#ifdef CONFIG_STA_SUPPORT
DECLARE_TIMER_FUNCTION(notify_timer_callback);
#endif

#ifdef ERR_RECOVERY
DECLARE_TIMER_FUNCTION(ser_l1_timeout);
#endif

#endif

#ifdef CFG_SUPPORT_CSI
DECLARE_TIMER_FUNCTION(csi_report_timer_handle);
#endif

#ifdef RT_CFG80211_SUPPORT
#endif


#endif /* __RTMP_TIMER_H__ */


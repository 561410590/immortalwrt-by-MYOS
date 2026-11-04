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
    rtmp_timer.c

    Abstract:
    task for timer handling

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name          Date            Modification logs
    Shiang Tu	08-28-2008   init version

*/

#include "rt_config.h"


BUILD_TIMER_FUNCTION(MlmePeriodicExecTimer);
/*BUILD_TIMER_FUNCTION(MlmeRssiReportExec);*/
BUILD_TIMER_FUNCTION(AsicRxAntEvalTimeout);

#ifdef DOT11W_PMF_SUPPORT
BUILD_TIMER_FUNCTION(PMF_SAQueryTimeOut);
BUILD_TIMER_FUNCTION(PMF_SAQueryConfirmTimeOut);
#endif /* DOT11W_PMF_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
extern VOID APDetectOverlappingExec(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3);

BUILD_TIMER_FUNCTION(APDetectOverlappingExec);

#ifdef DOT11N_DRAFT3
BUILD_TIMER_FUNCTION(Bss2040CoexistTimeOut);
#endif /* DOT11N_DRAFT3 */

BUILD_TIMER_FUNCTION(CMTimerExec);
BUILD_TIMER_FUNCTION(APQuickResponeForRateUpExec);
#ifdef IDS_SUPPORT
BUILD_TIMER_FUNCTION(RTMPIdsPeriodicExec);
#endif /* IDS_SUPPORT */

#ifdef DOT11R_FT_SUPPORT
BUILD_TIMER_FUNCTION(FT_KDP_InfoBroadcast);
#endif /* DOT11R_FT_SUPPORT */

BUILD_TIMER_FUNCTION(CSAEventTimeout);
BUILD_TIMER_FUNCTION(PPCSAEventTimeout);

#ifdef ZERO_PKT_LOSS_SUPPORT
BUILD_TIMER_FUNCTION(CSALastBcnTxEventTimeout);
BUILD_TIMER_FUNCTION(ChnlSwitchStaNullAckWaitTimeout);
BUILD_TIMER_FUNCTION(ChnlSwitchStaNullDataTxTimeout);
#endif /*ZERO_PKT_LOSS_SUPPORT*/
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
BUILD_TIMER_FUNCTION(StaQuickResponeForRateUpExec);
BUILD_TIMER_FUNCTION(WpaDisassocApAndBlockAssoc);



#endif /* CONFIG_STA_SUPPORT */

#ifdef WSC_INCLUDED
BUILD_TIMER_FUNCTION(WscEAPOLTimeOutAction);
BUILD_TIMER_FUNCTION(Wsc2MinsTimeOutAction);
BUILD_TIMER_FUNCTION(WscUPnPMsgTimeOutAction);
BUILD_TIMER_FUNCTION(WscM2DTimeOutAction);

BUILD_TIMER_FUNCTION(WscPBCTimeOutAction);
#ifdef CON_WPS
BUILD_TIMER_FUNCTION(WscScanDoneCheckTimeOutAction);
#endif /*CPN_WPS*/
#ifdef WSC_STA_SUPPORT
BUILD_TIMER_FUNCTION(WscPINTimeOutAction);
#endif
BUILD_TIMER_FUNCTION(WscScanTimeOutAction);
BUILD_TIMER_FUNCTION(WscProfileRetryTimeout);
#ifdef WSC_LED_SUPPORT
BUILD_TIMER_FUNCTION(WscLEDTimer);
BUILD_TIMER_FUNCTION(WscSkipTurnOffLEDTimer);
#endif /* WSC_LED_SUPPORT */
#ifdef LED_CONTROL_SUPPORT
BUILD_TIMER_FUNCTION(LEDControlTimer);
#endif
#ifdef CONFIG_AP_SUPPORT
BUILD_TIMER_FUNCTION(WscUpdatePortCfgTimeout);
#ifdef WSC_V2_SUPPORT
BUILD_TIMER_FUNCTION(WscSetupLockTimeout);
#endif /* WSC_V2_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

#endif /* WSC_INCLUDED */


#ifdef BACKGROUND_SCAN_SUPPORT
BUILD_TIMER_FUNCTION(BackgroundScanTimeout);
BUILD_TIMER_FUNCTION(dedicated_rx_hist_scan_timeout);
#ifdef IPI_MAINRADIO_SCAN_SUPPORT
BUILD_TIMER_FUNCTION(mainRadio_rx_hist_scan_timeout);
#endif
/*BUILD_TIMER_FUNCTION(DfsZeroWaitTimeout);*/
#if (DFS_ZEROWAIT_DEFAULT_FLOW == 1)
BUILD_TIMER_FUNCTION(dfs_zero_wait_ch_init_timeout);
#endif /* DFS_ZEROWAIT_DEFAULT_FLOW */
#endif /* BACKGROUND_SCAN_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
BUILD_TIMER_FUNCTION(AutoChSelScanTimeout);
BUILD_TIMER_FUNCTION(AutoChSelRestoreTimeout);
#endif/* CONFIG_AP_SUPPORT */

#ifdef ERR_RECOVERY
BUILD_TIMER_FUNCTION(ser_l1_timeout);
#endif

#ifdef CHANNEL_SWITCH_MONITOR_CONFIG
extern VOID ch_switch_monitor_timeout(IN PVOID system_specific1, IN PVOID function_context,
			IN PVOID system_specific2, IN PVOID system_specific3);
BUILD_TIMER_FUNCTION(ch_switch_monitor_timeout);
#endif
BUILD_TIMER_FUNCTION(ChOpTimeout);

#ifdef DOT11_HE_AX
#ifdef CONFIG_AP_SUPPORT
BUILD_TIMER_FUNCTION(trigger_timer_callback);
#endif
#ifdef CONFIG_STA_SUPPORT
BUILD_TIMER_FUNCTION(notify_timer_callback);
#endif
#endif

#ifdef CFG_SUPPORT_CSI
BUILD_TIMER_FUNCTION(csi_report_timer_handle);
#endif

#ifdef RT_CFG80211_SUPPORT
#endif



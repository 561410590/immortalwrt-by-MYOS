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

	All CFG80211 Function Prototype.

***************************************************************************/

#ifndef __CFG80211EXTR_H__
#define __CFG80211EXTR_H__

#ifdef RT_CFG80211_SUPPORT

#define CFG80211CB				    (pAd->pCfg80211_CB)

/* GPL Refine Begin */
void cfg80211_drv_start(
	struct _RTMP_ADAPTER *mac_ad);

int cfg80211_drv_vndr_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg);

/* GPL Refine End */

/* CFG_TODO */
#include "wfa_p2p.h"

#define RT_CFG80211_BEACON_CR_PARSE(__pAd, __pVIE, __LenVIE)				\
	CFG80211_BeaconCountryRegionParse((VOID *)__pAd, __pVIE, __LenVIE);

#define RT_CFG80211_BEACON_TIM_UPDATE(__pAd)                                \
	CFG80211_UpdateBeacon((VOID *)__pAd, NULL, 0, NULL, 0, FALSE);

#define RT_CFG80211_CRDA_REG_HINT(__pAd, __pCountryIe, __CountryIeLen)		\
	CFG80211_RegHint((VOID *)__pAd, __pCountryIe, __CountryIeLen);

#define RT_CFG80211_CRDA_REG_HINT11D(__pAd, __pCountryIe, __CountryIeLen)	\
	CFG80211_RegHint11D((VOID *)__pAd, __pCountryIe, __CountryIeLen);

#define RT_CFG80211_CRDA_REG_RULE_APPLY(__pAd)								\
	CFG80211_RegRuleApply((VOID *)__pAd, NULL, __pAd->cfg80211_ctrl.Cfg80211_Alpha2);

#define CFG80211_CONN_RESULT_INFORM(__pAd, __pBSSID, __ifIndex, __pReqIe,	\
			__ReqIeLen, __pRspIe, __RspIeLen, __FlgIsSuccess)	\
	CFG80211_ConnectResultInform((VOID *)__pAd, __pBSSID, __ifIndex,	\
			__pReqIe, __ReqIeLen, __pRspIe, __RspIeLen, __FlgIsSuccess)

#define RT_CFG80211_SCANNING_INFORM(__pAd, __BssIdx, __ChanId, __pFrame,	\
									__FrameLen, __RSSI, __RawChannel)									\
CFG80211_Scaning((VOID *)__pAd, __BssIdx, __ChanId, __pFrame,			\
				 __FrameLen, __RSSI, __RawChannel);

#define RT_CFG80211_SCAN_END(__pAd, __FlgIsAborted)							\
	CFG80211_ScanEnd((VOID *)__pAd, __FlgIsAborted);
#if defined(CONFIG_STA_SUPPORT) || defined(APCLI_CFG80211_SUPPORT)
#define RT_CFG80211_LOST_AP_INFORM(__pAd)									\
	CFG80211_LostApInform((VOID *)__pAd);
#endif /*CONFIG_STA_SUPPORT*/

#define RT_CFG80211_RFKILL_STATUS_UPDATE(_pAd, _active)					\
	CFG80211_RFKillStatusUpdate(_pAd, _active);

#ifdef SUPP_SAE_SUPPORT
int mtk_cfg80211_event_connect_params(
	struct _RTMP_ADAPTER *pAd, UCHAR *pmk, int pmk_len);
int mtk_cfg80211_event_pmksa(
	struct _RTMP_ADAPTER *pAd, UCHAR *pmk, int pmk_len, UCHAR *pmkid,
	UINT32 akmp, UINT8 *aa);
#endif

#define RT_CFG80211_P2P_CLI_SEND_NULL_FRAME(_pAd, _PwrMgmt)					\
	CFG80211_P2pClientSendNullFrame(_pAd, _PwrMgmt);

#define RT_CFG80211_JOIN_IBSS(_pAd, _pBssid) \
	CFG80211_JoinIBSS(_pAd, _pBssid);

#ifdef ANTENNA_CONTROL_SUPPORT
#define CFG80211_FILL_TXRX_STREAM(__pAd, __wdev, __pBandInfo) \
	{\
		do {\
			(__pBandInfo)->TxStream = !__pAd->TxStream ? __pAd->mcs_nss.max_nss : __pAd->TxStream;	\
			(__pBandInfo)->RxStream = !__pAd->RxStream ? __pAd->mcs_nss.max_nss : __pAd->RxStream;	\
		} while (0);\
	}
#else
#define CFG80211_FILL_TXRX_STREAM(__pAd, __wdev, __pBandInfo) \
	{\
		do {\
			(__pBandInfo)->TxStream = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? __pAd->mcs_nss.max_nss : wlan_operate_get_tx_stream(__wdev);	\
			(__pBandInfo)->RxStream = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? __pAd->mcs_nss.max_nss : wlan_operate_get_rx_stream(__wdev);	\
		} while (0);\
	}
#endif

#ifdef SINGLE_SKU
#define CFG80211_BANDINFO_FILL(__pAd, __wdev, __pBandInfo)\
	{\
		do {\
			(__pBandInfo)->RFICType = HcGetRadioRfIC(__pAd);\
			(__pBandInfo)->MpduDensity = __pAd->CommonCfg.BACapability.field.MpduDensity;\
			(__pBandInfo)->max_nss = __pAd->mcs_nss.max_nss;\
			CFG80211_FILL_TXRX_STREAM(__pAd, __wdev, __pBandInfo);\
			(__pBandInfo)->MaxTxPwr = __pAd->CommonCfg.DefineMaxTxPwr;\
			if (WMODE_EQUAL(HcGetRadioPhyMode(__pAd), WMODE_B))\
				(__pBandInfo)->FlgIsBMode = TRUE;\
			else\
				(__pBandInfo)->FlgIsBMode = FALSE;\
			(__pBandInfo)->MaxBssTable = MAX_LEN_OF_BSS_TABLE;\
			(__pBandInfo)->RtsThreshold = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? DEFAULT_RTS_LEN_THLD : wlan_operate_get_rts_len_thld(__wdev);\
			(__pBandInfo)->FragmentThreshold = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? DEFAULT_FRAG_THLD : wlan_operate_get_frag_thld(__wdev);\
			(__pBandInfo)->RetryMaxCnt = 0;	\
			if (__pAd->CommonCfg.bCountryFlag == TRUE)\
				NdisMoveMemory((__pBandInfo)->CountryCode, __pAd->CommonCfg.CountryCode, 2);\
		} while (0);\
	}
#else
#define CFG80211_BANDINFO_FILL(__pAd, __wdev, __pBandInfo)\
	{\
		do {\
			(__pBandInfo)->RFICType = HcGetRadioRfIC(__pAd);\
			(__pBandInfo)->MpduDensity = __pAd->CommonCfg.BACapability.field.MpduDensity;\
			(__pBandInfo)->max_nss = __pAd->mcs_nss.max_nss;\
			CFG80211_FILL_TXRX_STREAM(__pAd, __wdev, __pBandInfo);\
			(__pBandInfo)->MaxTxPwr = 0;\
			if (WMODE_EQUAL(HcGetRadioPhyMode(__pAd), WMODE_B))\
				(__pBandInfo)->FlgIsBMode = TRUE;\
			else\
				(__pBandInfo)->FlgIsBMode = FALSE;\
			(__pBandInfo)->MaxBssTable = MAX_LEN_OF_BSS_TABLE;\
			(__pBandInfo)->RtsThreshold = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? DEFAULT_RTS_LEN_THLD : wlan_operate_get_rts_len_thld(__wdev);\
			(__pBandInfo)->FragmentThreshold = ((__wdev == NULL) || (__wdev->wpf_op == NULL)) ? DEFAULT_FRAG_THLD : wlan_operate_get_frag_thld(__wdev);\
			(__pBandInfo)->RetryMaxCnt = 0; \
			if (__pAd->CommonCfg.bCountryFlag == TRUE)\
				NdisMoveMemory((__pBandInfo)->CountryCode, __pAd->CommonCfg.CountryCode, 2);\
		} while (0); \
	}
#endif /* SINGLE_SKU */

/* NoA Command Parm */
#define P2P_NOA_DISABLED 0x00
#define P2P_NOA_TX_ON    0x01
#define P2P_NOA_RX_ON    0x02

#ifdef SUPP_SAE_SUPPORT
#define WLAN_AKM_SUITE_SAE_EXT	0x000FAC18
#endif

#define WDEV_NOT_FOUND				-1


/* Scan Releated */
#if defined(CONFIG_STA_SUPPORT) || defined(APCLI_CFG80211_SUPPORT)
BOOLEAN CFG80211DRV_OpsScanRunning(VOID *pAdOrg);
#endif /*CONFIG_STA_SUPPORT*/

BOOLEAN CFG80211DRV_OpsScanSetSpecifyChannel(
	VOID *pAdOrg, VOID *pData, UINT8 dataLen);

BOOLEAN CFG80211DRV_OpsScanCheckStatus(
	VOID *pAdOrg, UINT8	IfType);

BOOLEAN CFG80211DRV_OpsScanExtraIesSet(VOID *pAdOrg);

VOID CFG80211DRV_OpsScanInLinkDownAction(VOID *pAdOrg);

#ifdef APCLI_CFG80211_SUPPORT
VOID CFG80211DRV_ApcliSiteSurvey(VOID *pAdOrg, VOID *pData);

VOID CFG80211DRV_SetApCliAssocIe(VOID *pAdOrg, PNET_DEV pNetDev, VOID *pData, UINT ie_len);

VOID CFG80211DRV_ApClientKeyAdd(VOID *pAdOrg, VOID *pData);

#endif /* APCLI_CFG80211_SUPPORT */

VOID CFG80211DRV_ApSiteSurvey(VOID *pAdOrg, VOID *pData);

INT CFG80211DRV_OpsScanGetNextChannel(VOID *pAdOrg);

VOID CFG80211_ScanStatusLockInit(VOID *pAdCB, UINT init);

VOID CFG80211_Scaning(VOID *pAdCB, UINT32 BssIdx, UINT32 ChanId,
					  UCHAR *pFrame, UINT32 FrameLen, INT32 RSSI, UINT16 RawChannel);

VOID CFG80211_ScanEnd(VOID *pAdCB, BOOLEAN FlgIsAborted);

/* Connect Releated */
BOOLEAN CFG80211DRV_OpsJoinIbss(VOID *pAdOrg, VOID *pData);
BOOLEAN CFG80211DRV_OpsLeave(VOID *pAdOrg, PNET_DEV pNetDev);

VOID CFG80211_ConnectResultInform(
	IN VOID *pAdCB, IN UCHAR * pBSSID, IN UCHAR ifIndex, IN UCHAR * pReqIe, IN UINT32 ReqIeLen,
	IN UCHAR * pRspIe, IN UINT32 RspIeLen, IN UCHAR FlgIsSuccess);

VOID CFG80211DRV_PmkidConfig(VOID *pAdOrg, VOID *pData);
VOID CFG80211_LostApInform(VOID *pAdCB);

INT CFG80211_StaPortSecured(
	VOID                         *pAdCB,
	UCHAR                        *pMac,
	UINT						flag);

/* AP Related*/
INT CFG80211_ApStaDel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *pwdev,
	struct mt_priv_ops_sta_del_info *pApStaDelInfo,
	UINT reason);

#ifdef HOSTAPD_PMKID_IN_DRIVER_SUPPORT
INT CFG80211_ApUpdateStaPmkid(VOID *pAdCB, VOID *pData);
#endif /*HOSTAPD_PMKID_IN_DRIVER_SUPPORT*/

VOID CFG80211_UpdateBeacon(
	VOID                           *pAdOrg,
	UCHAR                          *beacon_head_buf,
	UINT32                          beacon_head_len,
	UCHAR                          *beacon_tail_buf,
	UINT32                          beacon_tail_len,
	BOOLEAN                         isAllUpdate,
	UINT32							apidx);

#ifdef CUSTOMISED_MGMT_TRANS_SUPPORT
INT CFG80211_APStaDelSendVendorEvent(PRTMP_ADAPTER pAd, VOID *pEntry, VOID *data, INT len);
#endif
INT CFG80211_ApStaDelSendEvent(PRTMP_ADAPTER pAd, const PUCHAR mac_addr, IN PNET_DEV pNetDevIn);
INT CFG80211_FindMbssApIdxByNetDevice(RTMP_ADAPTER *pAd, PNET_DEV pNetDev);
INT CFG80211_FindApcliIdxByNetDevice(RTMP_ADAPTER *pAd, PNET_DEV pNetDev);

#ifdef APCLI_CFG80211_SUPPORT
INT CFG80211_FindStaIdxByNetDevice(RTMP_ADAPTER *pAd, PNET_DEV pNetDev);
#endif

/* Information Releated */
VOID CFG80211DRV_SurveyGet(
	VOID						*pAdOrg,
	VOID						*pData);

INT CFG80211_reSetToDefault(
	VOID						*pAdCB);


/* Key Releated */
BOOLEAN CFG80211DRV_StaKeyAdd(
	VOID						*pAdOrg,
	VOID						*pData);

BOOLEAN CFG80211DRV_ApKeyAdd(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *pWdev,
	struct __CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo);

VOID CFG80211DRV_RtsThresholdAdd(
	VOID                                            *pAdOrg,
	struct wifi_dev	*wdev,
	UINT                                            threshold);

VOID CFG80211DRV_FragThresholdAdd(
	VOID                                            *pAdOrg,
	struct wifi_dev	*wdev,
	UINT                                            threshold);

#ifdef ACK_CTS_TIMEOUT_SUPPORT
BOOLEAN CFG80211DRV_AckThresholdAdd(
	VOID * pAdOrg,
	struct wifi_dev	*wdev,
	UINT threshold);
#endif/*ACK_CTS_TIMEOUT_SUPPORT*/
BOOLEAN CFG80211DRV_ApKeyDel(
	VOID						*pAdOrg,
	VOID						*pData);

INT CFG80211_setApDefaultKey(
	IN VOID				* pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT					Data);
#ifdef BCN_PROTECTION_SUPPORT
INT CFG80211_setApBeaconDefaultKey(
	IN VOID   * pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT					Data);
#endif /*BCN_PROTECTION_SUPPORT*/

#ifdef DOT11W_PMF_SUPPORT
INT CFG80211_setApDefaultMgmtKey(
	IN VOID				* pAdCB,
	IN struct net_device		*pNetdev,
	IN UINT			Data);
#endif /*DOT11W_PMF_SUPPORT*/


#ifdef CONFIG_STA_SUPPORT
INT CFG80211_setStaDefaultKey(
	VOID                        *pAdCB,
	UINT                         Data);
#endif /*CONFIG_STA_SUPPORT*/
INT CFG80211_setPowerMgmt(
	VOID                     *pAdCB,
	UINT			Enable);

VOID CFG80211DRV_OpsMcastRateParm(VOID *pAdOrg, VOID *pData, INT apidx);

VOID CFG80211DRV_OpsBssColorParm(VOID *pAdOrg, VOID *pData, INT apidx);

VOID CFG80211DRV_OpsFtIesParm(VOID *pAdOrg, VOID *pData, INT apidx);

VOID CFG80211_UnRegister(VOID *pAdOrg,	VOID *pNetDev);

INT CFG80211DRV_IoctlHandle(
	VOID						*pAdSrc,
	RTMP_IOCTL_INPUT_STRUCT		*wrq,
	INT							cmd,
	USHORT						subcmd,
	VOID						*pData,
	ULONG						Data);

UCHAR CFG80211_getCenCh(RTMP_ADAPTER *pAd, UCHAR prim_ch);

/* CRDA Releatd */
VOID CFG80211DRV_RegNotify(
	VOID						*pAdOrg,
	VOID						*pData);

VOID CFG80211_RegHint(
	VOID						*pAdCB,
	UCHAR						*pCountryIe,
	ULONG						CountryIeLen);

VOID CFG80211_RegHint11D(
	VOID						*pAdCB,
	UCHAR						*pCountryIe,
	ULONG						CountryIeLen);

VOID CFG80211_RegRuleApply(
	VOID						*pAdCB,
	VOID						*pWiphy,
	UCHAR						*pAlpha2);

BOOLEAN CFG80211_SupBandReInit(
	struct _RTMP_ADAPTER *mac_ad);

#ifdef RFKILL_HW_SUPPORT
VOID CFG80211_RFKillStatusUpdate(
	struct _RTMP_ADAPTER *pAd,
	BOOLEAN active);
#endif /* RFKILL_HW_SUPPORT */

/* P2P Related */
VOID CFG80211DRV_SetP2pCliAssocIe(
	VOID						*pAdOrg,
	VOID						*pData,
	UINT                         ie_len);

VOID CFG80211DRV_P2pClientKeyAdd(
	VOID						*pAdOrg,
	VOID						*pData);

BOOLEAN CFG80211DRV_P2pClientConnect(
	VOID						*pAdOrg,
	VOID						*pData);

BOOLEAN CFG80211_checkScanTable(
	IN VOID                      *pAdCB);

VOID CFG80211_P2pClientSendNullFrame(
	VOID						*pAdCB,
	INT							 PwrMgmt);

VOID CFG80211RemainOnChannelTimeout(
	PVOID						SystemSpecific1,
	PVOID						FunctionContext,
	PVOID						SystemSpecific2,
	PVOID						SystemSpecific3);

BOOLEAN CFG80211DRV_OpsRemainOnChannel(
	VOID						*pAdOrg,
	VOID						*pData,
	UINT32						duration);

BOOLEAN CFG80211DRV_OpsCancelRemainOnChannel(
	VOID                                            *pAdOrg,
	UINT32                                          cookie);

BOOLEAN CFG80211_CheckActionFrameType(
	IN  RTMP_ADAPTER								 *pAd,
	IN	PUCHAR										 preStr,
	IN	PUCHAR										 pData,
	IN	UINT32										length);


#if defined(MTK_HOSTAPD_SUPPORT) && defined(CONFIG_MAP_SUPPORT) && defined(MAP_R3)
BOOLEAN CFG80211_CheckActionFrameTypeDpp(
	IN  RTMP_ADAPTER				* pAd,
	IN	PUCHAR					preStr,
	IN	PUCHAR					pData,
	IN	UINT32					length);
#endif /* MTK_HOSTAPD_SUPPORT && CONFIG_MAP_SUPPORT && MAP_R3 */

BOOLEAN CFG80211_SyncPacketWmmIe(RTMP_ADAPTER *pAd, VOID *pData, ULONG dataLen);
BOOLEAN CFG80211_isCfg80211FrameType(RTMP_ADAPTER *pAd, UINT16 frameSubType);
BOOLEAN CFG80211_HandleMgmtFrame(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk, UCHAR OpMode);
INT CFG80211_SendMgmtFrame(RTMP_ADAPTER *pAd, VOID *pData, ULONG Data_len);
BOOLEAN CFG80211_SyncBssWmmCap(
	struct _RTMP_ADAPTER *pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon);
VOID CFG80211_SyncBssUAPSDCap(
	struct _RTMP_ADAPTER *pAd,
	UINT mbss_idx,
	struct mt_priv_ops_beacon *pBeacon);

/* -------------------------------- */
VOID CFG80211_Announce802_3Packet(RTMP_ADAPTER *pAd, struct _RX_BLK *pRxBlk, UCHAR FromWhichBSSID);
#ifdef CONFIG_AP_SUPPORT
VOID CFG80211_ParseBeaconIE(
	RTMP_ADAPTER *pAd, struct _BSS_STRUCT *pMbss, struct wifi_dev *wdev,
	VOID *cfg, UCHAR *wpa_ie, UCHAR *rsn_ie);
BOOLEAN cfg80211_ap_parse_rsn_ie(
	struct _RTMP_ADAPTER *mac_ad,
	struct _SECURITY_CONFIG *sec_cfg,
	PUCHAR rsn_ie,
	BOOLEAN reset_setting,
	BOOLEAN rsn_override);
#endif /*CONFIG_AP_SUPPORT*/
VOID CFG80211_SwitchTxChannel(RTMP_ADAPTER *pAd, ULONG Data);

BOOLEAN CFG80211DRV_OpsBeaconSet(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pBcnCfg);

BOOLEAN CFG80211DRV_OpsBeaconAdd(
	struct _RTMP_ADAPTER *pAd,
	struct mt_priv_ops_beacon *pBeacon,
	struct mt_priv_ops_beacon_cfg *pBcnCfg);

#ifdef HOSTAPD_HS_R2_SUPPORT
BOOLEAN CFG80211DRV_SetQosParam(
		VOID 					*pAdOrg,
		VOID 					*pData,
		INT 					apindex);
#endif

INT CFG80211_setStaDefaultKey(
	IN VOID                                         *pAdCB,
	IN UINT                                         Data);

#if defined(IWCOMMAND_CFG80211_SUPPORT) || defined(HOSTAPD_MBSS_SUPPORT)
int CFG80211DRV_OpsVifAdd(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_);
int CFG80211DRV_OpsVifDel(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	void *ctrl_data_);
#endif /* IWCOMMAND_CFG80211_SUPPORT || HOSTAPD_MBSS_SUPPORT */

int mtk_cfg80211_event_bss_ml_info(PNET_DEV pNetDev, struct ml_info_event ml_event);
int mtk_cfg80211_event_reconf_sm(PNET_DEV pNetDev, UINT32 reconfig_state);
#ifdef ZERO_HANDOFF_ROAMING
int mtk_nl80211_zr_hdo_rm_event(PNET_DEV pNetDev,
					void *data, int len, u32 event_attr);

#endif

#ifdef CFG_RSNO_SUPPORT
BOOLEAN CFG80211_handle_rsne_override(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	unsigned char *ie,
	unsigned int ie_len);
BOOLEAN CFG80211_handle_rsne_override_2(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	unsigned char *ie,
	unsigned int ie_len);
void CFG80211_handle_rsnxe_override(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	UINT8 h2e_only,
	unsigned char *ie,
	unsigned int ie_len);
#endif /* CFG_RSNO_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */

#ifndef RT_CFG80211_SUPPORT
#define CFG80211DRV_OpsScanRunning(pAd) (FALSE)
#endif

VOID CFG80211_JoinIBSS(
	IN VOID						*pAdCB,
	IN UCHAR					*pBSSID);

#ifdef MT_MAC
VOID CFG80211_InitTxSCallBack(RTMP_ADAPTER *pAd);
#endif /* MT_MAC */
#ifdef CONFIG_VLAN_GTK_SUPPORT
struct vlan_gtk_info {
	PNET_DEV vlan_dev;           /* struct net_dev for vlan device */
	UINT16 vlan_id;              /* parse from interface name */
	UINT16 vlan_bmc_idx;         /* should be identical to vlan_tr_tb_idx */
	UINT16 vlan_tr_tb_idx;       /* should be identical to vlan_bmc_idx */
	UCHAR vlan_gtk[LEN_MAX_GTK]; /* only for debug print */
	UINT8 gtk_len;
	struct list_head list;
};

struct wifi_dev *CFG80211_GetWdevByVlandev(PRTMP_ADAPTER pAd, PNET_DEV net_dev);
BOOLEAN CFG80211_MatchVlandev(struct wifi_dev *wdev, PNET_DEV net_dev);
struct vlan_gtk_info *CFG80211_GetVlanInfoByVlandev(struct wifi_dev *wdev, PNET_DEV net_dev);
struct vlan_gtk_info *CFG80211_GetVlanInfoByVlanid(struct wifi_dev *wdev, UINT16 vlan_id);
INT16 CFG80211_IsVlanPkt(PNDIS_PACKET pkt);
#endif /* CONFIG_VLAN_GTK_SUPPORT */

void cfg80211_drv_send_acs_complete_event(
	struct wifi_dev *wdev);
int cfg80211_drv_init_cmd_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg);
int cfg80211_drv_ops_handle(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg);

#endif /* __CFG80211EXTR_H__ */


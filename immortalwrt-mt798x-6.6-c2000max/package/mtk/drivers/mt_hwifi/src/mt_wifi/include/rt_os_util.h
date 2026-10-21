/****************************************************************************

    Module Name:
	rt_os_util.h

	Abstract:
	All function prototypes are provided from UTIL modules.

	Note:
	But can not use any OS key word and compile option here.
	All functions are provided from UTIL modules.

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------

***************************************************************************/

#ifndef __RT_OS_UTIL_H__
#define __RT_OS_UTIL_H__

#include "common/mt_os_util.h"
#include "rt_config.h"
/* ============================ rt_linux.c ================================== */
/* General */
UCHAR MtRandom8(VOID);
UINT32 MtRandom32(VOID);

/* OS Time */
VOID RtmpusecDelay(ULONG usec);
VOID RtmpOsMsDelay(ULONG msec);

void RTMP_GetCurrentSystemTime(LARGE_INTEGER *time);

ULONG RTMPMsecsToJiffies(UINT msec);
void RTMP_GetCurrentSystemTick(ULONG *pNow);

VOID RtmpOsWait(UINT32 Time);
UINT32 RtmpOsTimerAfter(ULONG a, ULONG b);
UINT32 RtmpOsTimerBefore(ULONG a, ULONG b);
VOID RtmpOsGetSystemUpTime(ULONG *pTime);
UINT32 RtmpOsTickUnitGet(VOID);


/*
	OS Memory
*/
/*
	Function:
		allocate memory
	Parameters:

	Return:

		mem shall be set as NULL if allocation failed
*/
NDIS_STATUS os_alloc_mem_suspend(VOID *pReserved, UCHAR **mem, ULONG size);


NDIS_STATUS AdapterBlockAllocateMemory(
	IN	VOID *handle,
	OUT	VOID **ppAd,
	IN	UINT32 SizeOfpAd);

VOID *RtmpOsVmalloc(ULONG Size);
VOID RtmpOsVfree(VOID *pMem);

ULONG RtmpOsCopyFromUser(VOID *to, const void *from, ULONG n);
ULONG RtmpOsCopyToUser(VOID *to, const void *from, ULONG n);

BOOLEAN RtmpOsStatsAlloc(VOID **ppIwStats);

/* OS Packet */
NDIS_STATUS RTMPAllocateNdisPacket(
	IN	VOID					*pReserved,
	OUT PNDIS_PACKET			*ppPacket,
	IN	PUCHAR					pHeader,
	IN	UINT					HeaderLen,
	IN	PUCHAR					pData,
	IN	UINT					DataLen);

VOID RTMPFreeNdisPacket(VOID *pReserved, PNDIS_PACKET pPacket);
VOID RTMPFreeNdisPacketIRQ(VOID *pReserved, PNDIS_PACKET pPacket);

PNDIS_PACKET ClonePacket(BOOLEAN MonitorOn, PNET_DEV ndev, PNDIS_PACKET pkt, UCHAR *buf, ULONG sz);
PNDIS_PACKET DuplicatePacket(PNET_DEV pNetDev, PNDIS_PACKET pPacket);

PNDIS_PACKET duplicate_pkt_with_TKIP_MIC(
	IN VOID *pReserved,
	IN PNDIS_PACKET pOldPkt);

PNDIS_PACKET duplicate_pkt_vlan(
	IN	PNET_DEV				pNetDev,
	IN	USHORT					VLAN_VID,
	IN	USHORT					VLAN_Priority,
	IN	PUCHAR					pHeader802_3,
	IN  UINT					HdrLen,
	IN	PUCHAR					pData,
	IN	ULONG					DataSize,
	IN	UCHAR					*TPID);

typedef void (*RTMP_CB_8023_PACKET_ANNOUNCE)(
	IN VOID *pCtrlBkPtr,
	IN PNDIS_PACKET pPacket,
	IN UCHAR OpMode);

BOOLEAN RTMPL2FrameTxAction(
	IN  VOID					*pCtrlBkPtr,
	IN	RTMP_CB_8023_PACKET_ANNOUNCE _announce_802_3_packet,
	IN	UCHAR					apidx,
	IN	PUCHAR					pData,
	IN	UINT32					data_len,
	IN	UCHAR			OpMode);


void wlan_802_11_to_802_3_packet(
	IN	PNET_DEV				pNetDev,
	IN	UCHAR					OpMode,
	IN	USHORT					VLAN_VID,
	IN	USHORT					VLAN_Priority,
	IN	PNDIS_PACKET			pRxPacket,
	IN	UCHAR					*pData,
	IN	ULONG					DataSize,
	IN	PUCHAR					pHeader802_3,
	IN	UCHAR					*TPID);

UCHAR VLAN_8023_Header_Copy(
	IN	USHORT					VLAN_VID,
	IN	USHORT					VLAN_Priority,
	IN	PUCHAR					pHeader802_3,
	IN	UINT					HdrLen,
	OUT PUCHAR					pData,
	IN	UCHAR					*TPID);


#ifdef VLAN_SUPPORT
VOID *RtmpOsVLANInsertTag(PNDIS_PACKET pPacket, UINT16 tci);
#endif /*VLAN_SUPPORT*/

VOID RtmpOsPktBodyCopy(
	IN	PNET_DEV				pNetDev,
	IN	PNDIS_PACKET			pNetPkt,
	IN	ULONG					ThisFrameLen,
	IN	PUCHAR					pData);

INT RtmpOsIsPktCloned(PNDIS_PACKET pNetPkt);
PNDIS_PACKET RtmpOsPktCopy(PNDIS_PACKET pNetPkt);
PNDIS_PACKET RtmpOsPktClone(PNDIS_PACKET pNetPkt);

VOID RtmpOsPktDataPtrAssign(PNDIS_PACKET pNetPkt, UCHAR *pData);

VOID RtmpOsPktLenAssign(PNDIS_PACKET pNetPkt, LONG Len);
VOID RtmpOsPktTailAdjust(PNDIS_PACKET pNetPkt, UINT removedTagLen);

PUCHAR RtmpOsPktTailBufExtend(PNDIS_PACKET pNetPkt, UINT len);
PUCHAR RtmpOsPktHeadBufExtend(PNDIS_PACKET pNetPkt, UINT len);
VOID RtmpOsPktReserve(PNDIS_PACKET pNetPkt, UINT len);

VOID RtmpOsPktInfPpaSend(PNDIS_PACKET pNetPkt);
VOID RtmpOsPktRcvHandle(PNDIS_PACKET pNetPkt, VOID *napi);
#ifdef CONFIG_FAST_NAT_SUPPORT
VOID RtmpOsPktNatMagicTag(PNDIS_PACKET pNetPkt);
#endif /*CONFIG_FAST_NAT_SUPPORT*/
VOID RtmpOsPktInit(PNDIS_PACKET pNetPkt, PNET_DEV pNetDev, UCHAR *buf, USHORT len);

PNDIS_PACKET RtmpOsPktIappMakeUp(PNET_DEV pNetDev, UINT8 *pMac);

BOOLEAN RtmpOsPktOffsetInit(VOID);

UINT16 RtmpOsNtohs(UINT16 Value);
UINT16 RtmpOsHtons(UINT16 Value);
UINT32 RtmpOsNtohl(UINT32 Value);
UINT32 RtmpOsHtonl(UINT32 Value);

/* OS Network Interface */
int RtmpOSNetDevAddrSet(
	IN UCHAR OpMode,
	IN VOID *pNetDev,
	IN PUCHAR pMacAddr,
	IN PUCHAR dev_name);

void RtmpOSNetDevClose(PNET_DEV pNetDev);


#ifdef CONFIG_STA_SUPPORT
INT RtmpOSNotifyRawData(PNET_DEV pNetDev, UCHAR *buf, INT len, ULONG type, USHORT proto, VOID *napi);
#endif /* CONFIG_STA_SUPPORT */

INT RtmpOSNetDevDestory(VOID *pReserved, PNET_DEV pNetDev);
void RtmpOSNetDevDetach(void *pNetDev);
unsigned char *RtmpOsNetDevGetPhyAddr(VOID *pDev);

VOID RtmpOsNetQueueStart(PNET_DEV pDev);
VOID RtmpOsNetQueueStop(PNET_DEV pDev);
VOID RtmpOsNetQueueWake(PNET_DEV pDev);

VOID RtmpOsSetPktNetDev(VOID *pPkt, VOID *pDev);
PNET_DEV RtmpOsPktNetDevGet(VOID *pPkt);

char *RtmpOsGetNetDevName(VOID *pDev);

UINT32 RtmpOsGetNetIfIndex(IN VOID *pDev);
void RtmpOSNetDevFree(VOID *pNetDev);

VOID RtmpOsSetNetDevType(VOID *pDev, USHORT Type);
VOID RtmpOsSetNetDevTypeMonitor(VOID *pDev);

/* OS Semaphore */
VOID RtmpOsCmdUp(RTMP_OS_TASK *pCmdQTask);
BOOLEAN RtmpOsSemaInitLocked(RTMP_OS_SEM *pSemOrg, LIST_HEADER *pSemList);
BOOLEAN RtmpOsSemaInit(RTMP_OS_SEM *pSemOrg, LIST_HEADER *pSemList);
BOOLEAN RtmpOsSemaDestory(RTMP_OS_SEM *pSemOrg);
INT RtmpOsSemaWaitInterruptible(RTMP_OS_SEM *pSemOrg);
VOID RtmpOsSemaWakeUp(RTMP_OS_SEM *pSemOrg);
VOID RtmpOsMlmeUp(RTMP_OS_TASK *pMlmeQTask);

VOID RtmpOsInitCompletion(RTMP_OS_COMPLETION *pCompletion);
VOID RtmpOsReinitCompletion(RTMP_OS_COMPLETION *pCompletion);
VOID RtmpOsExitCompletion(RTMP_OS_COMPLETION *pCompletion);
VOID RtmpOsComplete(RTMP_OS_COMPLETION *pCompletion);
ULONG RtmpOsWaitForCompletionTimeout(RTMP_OS_COMPLETION *pCompletion, ULONG Timeout);

/* OS Task */
BOOLEAN RtmpOsTaskletSche(RTMP_NET_TASK_STRUCT *pTasklet);

BOOLEAN RtmpOsTaskletInit(
	RTMP_NET_TASK_STRUCT *pTasklet,
	VOID (*pFunc)(unsigned long data),
	ULONG Data,
	LIST_HEADER *pTaskletList);

BOOLEAN RtmpOsTaskletKill(RTMP_NET_TASK_STRUCT *pTasklet);
VOID RtmpOsTaskletDataAssign(RTMP_NET_TASK_STRUCT *pTasklet, ULONG Data);

VOID RtmpOsTaskWakeUp(RTMP_OS_TASK *pTaskOrg);
INT32 RtmpOsTaskIsKilled(RTMP_OS_TASK *pTaskOrg);

BOOLEAN RtmpOsCheckTaskLegality(RTMP_OS_TASK *pTaskOrg);

NDIS_STATUS RtmpOSTaskKill(RTMP_OS_TASK *pTaskOrg);

NDIS_STATUS RtmpOSTaskAttach(
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	RTMP_OS_TASK_CALLBACK	fn,
	IN	ULONG arg);

NDIS_STATUS RtmpOSTaskInit(
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	RTMP_STRING *pTaskName,
	IN	VOID *pPriv,
	IN	LIST_HEADER *pTaskList,
	IN	LIST_HEADER *pSemList);

BOOLEAN RtmpOSTaskWait(
	IN	VOID *pReserved,
	IN	RTMP_OS_TASK *pTaskOrg,
	IN	INT32 *pStatus);

BOOLEAN RtmpOSTaskWaitCond(
	IN	VOID *pReserved,
	IN	RTMP_OS_TASK *pTaskOrg,
	IN  UINT32 u4Cond,
	IN	INT32 *pStatus);

BOOLEAN RtmpOSTaskWaitEvent(VOID *pReserved, MT_OS_TASK *pTask, UINT32 u4Cond, INT32 *pStatus);

VOID RtmpOSTaskWakeupCond(MT_OS_TASK *pTask, UINT32 u4Cond);

INT32 RtmpThreadPidKill(RTMP_OS_PID	 PID);

/* OS I/O */
VOID RTMP_PCI_Writel(ULONG Value, VOID *pAddr);
VOID RTMP_PCI_Writew(ULONG Value, VOID *pAddr);
VOID RTMP_PCI_Writeb(ULONG Value, VOID *pAddr);
ULONG RTMP_PCI_Readl(VOID *pAddr);
ULONG RTMP_PCI_Readw(VOID *pAddr);
ULONG RTMP_PCI_Readb(VOID *pAddr);

int RtmpOsPciConfigReadWord(
	IN	VOID					*pDev,
	IN	UINT32					Offset,
	OUT UINT16					*pValue);

int RtmpOsPciConfigWriteWord(VOID *pDev, UINT32 Offset, UINT16 Value);
int RtmpOsPciConfigReadDWord(VOID *pDev, UINT32 Offset, UINT32 *pValue);
int RtmpOsPciConfigWriteDWord(VOID *pDev, UINT32 Offset, UINT32 Value);

int RtmpOsPciFindCapability(VOID *pDev, INT Cap);

VOID *RTMPFindHostPCIDev(VOID *pPciDevSrc);

/* OS Wireless */
ULONG RtmpOsMaxScanDataGet(VOID);

/* OS Interrutp */
INT32 RtmpOsIsInInterrupt(VOID);

/* OS USB */
VOID *RtmpOsUsbUrbDataGet(VOID *pUrb);
NTSTATUS RtmpOsUsbUrbStatusGet(VOID *pUrb);
ULONG RtmpOsUsbUrbLenGet(VOID *pUrb);

/* OS Atomic */
BOOLEAN RtmpOsAtomicInit(RTMP_OS_ATOMIC *pAtomic, LIST_HEADER *pAtomicList);
VOID RtmpOsAtomicDestroy(RTMP_OS_ATOMIC *pAtomic);
LONG RtmpOsAtomicRead(RTMP_OS_ATOMIC *pAtomic);
VOID RtmpOsAtomicDec(RTMP_OS_ATOMIC *pAtomic);
VOID RtmpOsAtomicInterlockedExchange(RTMP_OS_ATOMIC *pAtomicSrc, LONG Value);

/* OS Utility */
void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen);

typedef VOID (*RTMP_OS_SEND_WLAN_EVENT)(
	IN	VOID					*pAdSrc,
	IN	USHORT					Event_flag,
	IN	PUCHAR					pAddr,
	IN  UCHAR					BssIdx,
	IN	CHAR					Rssi);

VOID RtmpOsSendWirelessEvent(
	IN	VOID			*pAd,
	IN	USHORT			Event_flag,
	IN	PUCHAR			pAddr,
	IN	UCHAR			BssIdx,
	IN	CHAR			Rssi,
	IN	RTMP_OS_SEND_WLAN_EVENT pFunc);

#ifdef CONFIG_AP_SUPPORT
void SendSignalToDaemon(
	IN	INT sig,
	IN	RTMP_OS_PID	 pid,
	IN	unsigned long pid_no);
#endif /* CONFIG_AP_SUPPORT */

int RtmpOSWrielessEventSend(
	IN	PNET_DEV				pNetDev,
	IN	UINT32					eventType,
	IN	INT						flags,
	IN	PUCHAR					pSrcMac,
	IN	PUCHAR					pData,
	IN	UINT32					dataLen);

VOID RtmpOsWlanEventSet(
	IN	VOID					*pReserved,
	IN	BOOLEAN					*pCfgWEnt,
	IN	BOOLEAN					FlgIsWEntSup);

UINT16 RtmpOsGetUnaligned(UINT16 *pWord);

UINT32 RtmpOsGetUnaligned32(UINT32 *pWord);

ULONG RtmpOsGetUnalignedlong(ULONG *pWord);

long RtmpOsSimpleStrtol(
	IN	const char				*cp,
	IN	char					**endp,
	IN	unsigned int			base);

VOID RtmpOsOpsInit(RTMP_OS_ABL_OPS *pOps);

/* ============================ rt_os_util.c ================================ */
VOID RtmpDrvMaxRateGet(
	IN VOID *pReserved,
	IN UINT8 MODE,
	IN UINT8 ShortGI,
	IN UINT8 BW,
	IN UINT8 MCS,
	IN UINT8 Antenna,
	OUT UINT64 *pRate);

char *rtstrchr(char *s, int c);

RTMP_STRING *WscGetAuthTypeStr(USHORT authFlag);

RTMP_STRING *WscGetEncryTypeStr(USHORT encryFlag);

USHORT WscGetAuthTypeFromStr(RTMP_STRING *arg);

USHORT WscGetEncrypTypeFromStr(RTMP_STRING *arg);

VOID RtmpMeshDown(
	IN VOID *pDrvCtrlBK,
	IN BOOLEAN WaitFlag,
	IN BOOLEAN (*RtmpMeshLinkCheck)(IN VOID *pAd));

USHORT RtmpOsNetPrivGet(PNET_DEV pDev);

BOOLEAN RtmpOsCmdDisplayLenCheck(
	IN	UINT32					LenSrc,
	IN	UINT32					Offset);

VOID WpaSendMicFailureToWpaSupplicant(
	IN PNET_DEV pNetDev,
	IN const PUCHAR src_addr,
	IN BOOLEAN bUnicast,
	IN INT key_id,
	IN const PUCHAR tsc);

int wext_notify_event_assoc(
	IN	PNET_DEV				pNetDev,
	IN	UCHAR					*ReqVarIEs,
	IN	UINT32					ReqVarIELen);

VOID    SendAssocIEsToWpaSupplicant(
	IN	PNET_DEV				pNetDev,
	IN	UCHAR					*ReqVarIEs,
	IN	UINT32					ReqVarIELen);

#if defined(RTMP_RBUS_SUPPORT) || defined(RTMP_FLASH_SUPPORT)
void RtmpFlashRead(
	void *hdev_ctrl,
	UCHAR *p,
	ULONG a,
	ULONG b);

void RtmpFlashWrite(
	void *hdev_ctrl,
	UCHAR *p,
	ULONG a,
	ULONG b);
#endif /* defined(RTMP_RBUS_SUPPORT) || defined(RTMP_FLASH_SUPPORT) */

UINT32 RtmpOsGetUsbDevVendorID(
	IN VOID *pUsbDev);

UINT32 RtmpOsGetUsbDevProductID(
	IN VOID *pUsbDev);

/* CFG80211 */
#ifdef RT_CFG80211_SUPPORT

VOID CFG80211OS_RegHint(
	IN VOID						*pCB,
	IN UCHAR					*pCountryIe,
	IN ULONG					CountryIeLen);

VOID CFG80211OS_RegHint11D(
	IN VOID						*pCB,
	IN UCHAR					*pCountryIe,
	IN ULONG					CountryIeLen);

BOOLEAN CFG80211OS_BandInfoGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	OUT VOID					**ppBand24,
	OUT VOID					**ppBand5,
	OUT VOID					**ppBand6);

UINT32 CFG80211OS_ChanNumGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN UINT32					IdBand);

BOOLEAN CFG80211OS_ChanInfoGet(
	IN VOID						*pCB,
	IN VOID						*pWiphyOrg,
	IN UINT32					IdBand,
	IN UINT32					IdChan,
	OUT UINT32					*pChanId,
	OUT UINT32					*pPower,
	OUT BOOLEAN					*pFlgIsRadar);

BOOLEAN CFG80211OS_ChanInfoInit(
	IN VOID						*pCB,
	IN UINT32					InfoIndex,
	IN UCHAR					ChanId,
	IN UCHAR					MaxTxPwr,
	IN BOOLEAN					FlgIsNMode,
	IN BOOLEAN					FlgIsBW20M);

VOID CFG80211OS_Scaning(
	IN VOID						*pCB,
	IN UINT32					ChanId,
	IN UCHAR					*pFrame,
	IN UINT32					FrameLen,
	IN INT32					RSSI,
	IN BOOLEAN					FlgIsNMode,
	IN UINT16					RawChannel,
	IN UINT8					BW);

VOID CFG80211OS_ScanEnd(
	IN struct _RTMP_ADAPTER *pAd,
	IN BOOLEAN					FlgIsAborted);

void CFG80211OS_ConnectResultInform(
	IN PNET_DEV					pNetDev,
	IN UCHAR					*pBSSID,
	IN UCHAR					*pReqIe,
	IN UINT32					ReqIeLen,
	IN UCHAR					*pRspIe,
	IN UINT32					RspIeLen,
	IN UCHAR					FlgIsSuccess);

BOOLEAN CFG80211OS_RxMgmt(IN PNET_DEV pNetDev, IN INT32 freq, IN PUCHAR frame, IN UINT32 len, IN CHAR rssi);
VOID CFG80211OS_TxStatus(IN PNET_DEV pNetDev, IN INT32 cookie,	IN PUCHAR frame, IN UINT32 len, IN BOOLEAN ack);
VOID CFG80211OS_NewSta(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr, IN const PUCHAR assoc_frame, IN UINT32 assoc_len,
	IN BOOLEAN isReassoc, IN const PUCHAR assoc_resp_frame, IN UINT32 assoc_resp_len,
	IN BOOLEAN mlo_params_valid, IN UINT8 mlo_link_id, IN const PUCHAR mld_addr);
VOID CFG80211OS_DelSta(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr);
#ifdef CUSTOMISED_MGMT_TRANS_SUPPORT
VOID CFG80211OS_Conn_Failed(IN PNET_DEV pNetDev, IN const PUCHAR mac_addr, IN UCHAR fail_reason);
#endif
VOID CFG80211OS_MICFailReport(IN PNET_DEV pNetDev, IN const PUCHAR src_addr, IN BOOLEAN unicast, IN INT key_id, IN const PUCHAR tsc);
VOID CFG80211OS_Roamed(
	PNET_DEV pNetDev, IN UCHAR *pBSSID,
	IN UCHAR *pReqIe, IN UINT32 ReqIeLen,
	IN UCHAR *pRspIe, IN UINT32 RspIeLen);
VOID CFG80211OS_JoinIBSS(IN PNET_DEV pNetDev, IN const PUCHAR pBssid);
VOID CFG80211OS_Disconnected(IN PNET_DEV pNetDev);
VOID CFG80211OS_PutBss(IN VOID *pWiphyOrg, IN VOID *pCfg80211Bss);
#endif /* RT_CFG80211_SUPPORT */

#define RTMP_OS_NETDEV_GET_DEVNAME	mt_os_net_dev_get_name

/* ================================ MACRO =================================== */
#define RTMP_UTIL_DCACHE_FLUSH(__AddrStart, __Size)
#define RTMP_GET_PKT_SRC_VA(pPacket) GET_OS_PKT_DATAPTR(pPacket)
#define RTMP_GET_PKT_LEN(pPacket) GET_OS_PKT_LEN(pPacket)

/* ================================ EXTERN ================================== */
extern UCHAR SNAP_802_1H[6];
extern UCHAR SNAP_BRIDGE_TUNNEL[6];
extern UCHAR EAPOL[2];
extern UCHAR TPID[];
extern UCHAR IPX[2];
extern UCHAR APPLE_TALK[2];
extern UCHAR NUM_BIT8[8];
extern ULONG RTPktOffsetData, RTPktOffsetLen, RTPktOffsetCB;

extern ULONG OS_NumOfMemAlloc, OS_NumOfMemFree;
extern ULONG OS_NumOfVMemAlloc, OS_NumOfVMemFree;

extern INT32 ralinkrate[];
extern UINT32 RT_RateSize;

#ifdef PLATFORM_UBM_IPX8
#include "vrut_ubm.h"
#endif /* PLATFORM_UBM_IPX8 */

INT32  RtPrivIoctlSetVal(VOID);

void OS_SPIN_LOCK(NDIS_SPIN_LOCK *lock);
void OS_SPIN_UNLOCK(NDIS_SPIN_LOCK *lock);
void OS_SPIN_LOCK_BH(NDIS_SPIN_LOCK *lock);
void OS_SPIN_UNLOCK_BH(NDIS_SPIN_LOCK *lock);
void OS_SPIN_LOCK_IRQSAVE(NDIS_SPIN_LOCK *lock, unsigned long *flags);
void OS_SPIN_UNLOCK_IRQRESTORE(NDIS_SPIN_LOCK *lock, unsigned long *flags);
void OS_SPIN_LOCK_IRQ(NDIS_SPIN_LOCK *lock);
void OS_SPIN_UNLOCK_IRQ(NDIS_SPIN_LOCK *lock);
void RtmpOsSpinLockIrqSave(NDIS_SPIN_LOCK *lock, unsigned long *flags);
void RtmpOsSpinUnlockIrqRestore(NDIS_SPIN_LOCK *lock, unsigned long *flags);
void RtmpOsSpinLockIrq(NDIS_SPIN_LOCK *lock);
void RtmpOsSpinUnlockIrq(NDIS_SPIN_LOCK *lock);
int OS_TEST_BIT(int bit, ULONG *flags);
void OS_SET_BIT(unsigned int bit, ULONG *flags);
void OS_CLEAR_BIT(unsigned int bit, ULONG *flags);
#ifdef MEM_ALLOC_INFO_SUPPORT
void MemInfoListInital(void);
void MemInfoListExit(void);
UINT32 ShowMemAllocInfo(UINT show, UINT64 pCaller);
UINT32 ShowPktAllocInfo(UINT show, UINT64 pCaller);
#endif /* MEM_ALLOC_INFO_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
INT set_qiscdump_proc(struct _RTMP_ADAPTER *pAd, RTMP_STRING *arg);
#endif
VOID wifi_dump_info(VOID);

#ifdef RTMP_WLAN_HOOK_SUPPORT
#include <os/rt_linux_txrx_hook.h>
/*define os layer hook function implementation*/
#define WLAN_HOOK_CALL(hook, ad, priv) mt_wlan_hook_call(hook, ad, priv)
#define WLAN_HOOK_INIT() mt_wlan_hook_init()

#else
#define WLAN_HOOK_CALL(hook, ad, priv)
#define WLAN_HOOK_INIT()
#endif

#ifdef RT_CFG80211_SUPPORT
INT32 CFG80211OS_UpdateRegRuleByRegionIdx(IN VOID * pCB, IN VOID * pChDesc2G, IN VOID * pChDesc5G, IN VOID * pChDesc6G);
UCHAR CFG80211OS_SyncChCtrlFromIEEE80211Channels(RTMP_ADAPTER *pAd, CHANNEL_CTRL *pChCtrl, UCHAR BandIdx);
#endif

#ifdef APCLI_CFG80211_SUPPORT
int CFG80211OS_ExternalAuthRequest(PSTA_ADMIN_CONFIG pStaCfg);
#ifdef DOT11_EHT_BE
int CFG80211OS_MloExternalAuthRequest(struct _RTMP_ADAPTER *pAd, PSTA_ADMIN_CONFIG pStaCfg);
#endif
#endif

void *os_sysinfo_root_dir_get(void);
int os_sysinfo_init(void);
void os_sysinfo_deinit(void);

void *os_lockless_ctx_get(void);
int os_lockless_init(void);
void os_lockless_deinit(void);

#endif /* __RT_OS_UTIL_H__ */


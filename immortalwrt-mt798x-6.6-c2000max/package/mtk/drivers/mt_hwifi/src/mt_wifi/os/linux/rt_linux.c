/****************************************************************************
 *
 *   Module Name:
 *    rt_linux.c
 *
 *    Abstract:
 *	All functions provided from OS module are put here.
 *
 *	Revision History:
 *	Who        When          What
 *	---------  ----------    -------------------------------------------
 *
 ***************************************************************************/

#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "security/dot11i_wpa.h"
#include <linux/rtnetlink.h>
#include <linux/notifier.h>
#include <linux/oom.h>
#include <linux/netdevice.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#include <net/sch_generic.h>
#include "rt_os_net.h"
#include "rt_config.h"

#ifdef CONFIG_FAST_NAT_SUPPORT
#include <net/ra_nat.h>
#endif /*CONFIG_FAST_NAT_SUPPORT*/
#include "multi_hif.h"

#ifdef VLAN_SUPPORT
#include <linux/if_vlan.h>
#endif /*VLAN_SUPPORT*/
#include "wnm.h"

/* TODO */
#undef RT_CONFIG_IF_OPMODE_ON_AP
#undef RT_CONFIG_IF_OPMODE_ON_STA

#if defined(CONFIG_AP_SUPPORT) && defined(CONFIG_STA_SUPPORT)
#define RT_CONFIG_IF_OPMODE_ON_AP(__OpMode)	if (__OpMode == OPMODE_AP)
#define RT_CONFIG_IF_OPMODE_ON_STA(__OpMode)	if (__OpMode == OPMODE_STA)
#else
#define RT_CONFIG_IF_OPMODE_ON_AP(__OpMode)
#define RT_CONFIG_IF_OPMODE_ON_STA(__OpMode)
#endif

#ifdef CONFIG_CONNINFRA_SUPPORT
enum consys_drv_type {
	CONNDRV_TYPE_BT = 0,
	CONNDRV_TYPE_FM = 1,
	CONNDRV_TYPE_GPS = 2,
	CONNDRV_TYPE_WIFI = 3,
	CONNDRV_TYPE_CONNINFRA = 4,
	CONNDRV_TYPE_MAX
};
int conninfra_pwr_on(enum consys_drv_type drv_type);
int conninfra_pwr_off(enum consys_drv_type drv_type);
#endif /* CONFIG_CONNINFRA_SUPPORT */

int DebugLevel = DBG_LVL_NOTICE;

UINT32 DebugCategory = DBG_CAT_EN_ALL_MASK
				& ~(0x1 << DBG_CAT_TX)
				& ~(0x1 << DBG_CAT_PS);

UINT32 DebugSubCategory[DBG_LVL_MAX + 1][32] = {
	{
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_0, for DBG_CAT_MISC */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_1, for DBG_CAT_INIT */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_2, for DBG_CAT_HW */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_3, for DBG_CAT_FW */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_4, for DBG_CAT_HIF */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_5, for DBG_CAT_FPGA */
		DBG_SUBCAT_DIS_ALL_MASK,		/* bit_6, for DBG_CAT_TEST */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_7, for DBG_CAT_RA */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_8, for DBG_CAT_AP */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_9, for DBG_CAT_CLIENT */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_10, for DBG_CAT_TX */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_11, for DBG_CAT_RX */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_12, for DBG_CAT_CFG */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_13, for DBG_CAT_MLME */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_14, for DBG_CAT_PROTO */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_15, for DBG_CAT_SEC */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_16, for DBG_CAT_PS */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_17, for DBG_CAT_POWER */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_18, for DBG_CAT_COEX */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_19, for DBG_CAT_P2P */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_20, for DBG_CAT_TOKEN */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_21, for DBG_CAT_CMW */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_22, for DBG_CAT_BF */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_23 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_24 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_25 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_26 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_27 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_28 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_29 */
		DBG_SUBCAT_EN_ALL_MASK,			/* bit_30 */
		DBG_SUBCAT_EN_ALL_MASK			/* bit_31, for DBG_CAT_TDMA */
	},
	{
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_DIS_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK
	}, /* ERROR */
	{
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_DIS_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK
	}, /* WARN */
	{
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_DIS_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK,
		DBG_SUBCAT_EN_ALL_MASK
	}, /* NOTICE */
	{0}, {0}
};


/* Unify all delay routine by using udelay */
VOID RtmpusecDelay(ULONG usec)
{
	ULONG i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}


VOID RtmpOsMsDelay(ULONG msec)
{
	mdelay(msec);
}

void RTMP_GetCurrentSystemTime(LARGE_INTEGER *time)
{
	time->u.LowPart = jiffies;
}

void RTMP_GetCurrentSystemTick(ULONG *pNow)
{
	*pNow = jiffies;
}

ULONG RTMPMsecsToJiffies(UINT32 m)
{
	return mt_os_msecs_to_jiffies(m);
}

/*
 * The allocated NDIS PACKET must be freed via RTMPFreeNdisPacket()
 */
NDIS_STATUS RTMPAllocateNdisPacket(
	IN VOID *pReserved,
	OUT PNDIS_PACKET *ppPacket,
	IN UCHAR * pHeader,
	IN UINT HeaderLen,
	IN UCHAR * pData,
	IN UINT DataLen)
{
	struct sk_buff *pPacket;

	/* Add LEN_CCMP_HDR + LEN_CCMP_MIC for PMF */
	OS_DEV_ALLOC_SKB(
		pPacket,
		(HeaderLen + DataLen + RTMP_PKT_TAIL_PADDING + LEN_CCMP_HDR + LEN_CCMP_MIC));

	if (pPacket == NULL) {
		*ppPacket = NULL;
#ifdef DBG
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_TX, DBG_LVL_ERROR,
				 KERN_ERR "RTMPAllocateNdisPacket Fail\n\n");
#endif
		return NDIS_STATUS_FAILURE;
	}

	/* Clone the frame content and update the length of packet */
	if ((HeaderLen > 0) && (pHeader != NULL))
		os_move_mem(pPacket->data, pHeader, HeaderLen);

	if ((DataLen > 0) && (pData != NULL))
		os_move_mem(pPacket->data + HeaderLen, pData, DataLen);

	skb_put(pPacket, HeaderLen + DataLen);
	*ppPacket = (PNDIS_PACKET)pPacket;
	return NDIS_STATUS_SUCCESS;
}


/*
 * ========================================================================
 * Description:
 * This routine frees a miniport internally allocated NDIS_PACKET and its
 * corresponding NDIS_BUFFER and allocated memory.
 * ========================================================================
 */
VOID RTMPFreeNdisPacket(VOID *pReserved, PNDIS_PACKET pPacket)
{
	if (pPacket) {
		WLAN_HOOK_CALL(WLAN_HOOK_DRIVER_FREE_PKT, NULL, RTPKT_TO_OSPKT(pPacket));

		dev_kfree_skb_any(RTPKT_TO_OSPKT(pPacket));
		MEM_DBG_PKT_FREE_INC(pPacket);
	}
}


VOID RTMPFreeNdisPacketIRQ(VOID *pReserved, PNDIS_PACKET pPacket)
{
	if (pPacket) {
		dev_kfree_skb_irq(RTPKT_TO_OSPKT(pPacket));
		MEM_DBG_PKT_FREE_INC(pPacket);
	}
}



PNDIS_PACKET ClonePacket(BOOLEAN MonitorOn, PNET_DEV ndev, PNDIS_PACKET pkt, UCHAR *buf, ULONG sz)
{
	PNDIS_PACKET *pClonedPkt;

	pClonedPkt = mt_os_clone_pkt_by_data(0, pkt, buf, sz);

	if (pClonedPkt)
		MEM_DBG_PKT_ALLOC_INC(pClonedPkt);

	return pClonedPkt;
}


PNDIS_PACKET DuplicatePacket(PNET_DEV pNetDev, PNDIS_PACKET pPacket)
{
	PNDIS_PACKET *pClonedPkt;

	pClonedPkt = mt_os_clone_pkt(pNetDev, pPacket);

	if (pClonedPkt)
		MEM_DBG_PKT_ALLOC_INC(pClonedPkt);

	return pClonedPkt;
}


PNDIS_PACKET duplicate_pkt_vlan(
	IN PNET_DEV pNetDev,
	IN USHORT VLAN_VID,
	IN USHORT VLAN_Priority,
	IN PUCHAR pHeader802_3,
	IN UINT HdrLen,
	IN PUCHAR pData,
	IN ULONG DataSize,
	IN UCHAR *TPID)
{
	PNDIS_PACKET pkt = NULL;

#ifdef WIFI_VLAN_SUPPORT
	pkt = mt_os_duplicate_pkt_vlan(pNetDev, VLAN_VID, VLAN_Priority, pHeader802_3, HdrLen, pData, DataSize, TPID);
#else
	pkt = mt_os_duplicate_pkt_vlan(pNetDev, 0, VLAN_Priority, pHeader802_3, HdrLen, pData, DataSize, TPID);
#endif

	if (pkt)
		MEM_DBG_PKT_ALLOC_INC(pkt);

	return pkt;
}


#define TKIP_TX_MIC_SIZE		8
PNDIS_PACKET duplicate_pkt_with_TKIP_MIC(VOID *pReserved, PNDIS_PACKET pPacket)
{
	struct sk_buff *skb, *newskb;

	skb = RTPKT_TO_OSPKT(pPacket);

	if (skb_tailroom(skb) < TKIP_TX_MIC_SIZE) {
		/* alloc a new skb and copy the packet */
		newskb = skb_copy_expand(skb, skb_headroom(skb), TKIP_TX_MIC_SIZE, GFP_ATOMIC);
		dev_kfree_skb_any(skb);
		MEM_DBG_PKT_FREE_INC(skb);

		if (newskb == NULL) {
			MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_TX, DBG_LVL_ERROR,
				"Extend Tx.MIC for packet failed!, dropping packet!\n");
			return NULL;
		}

		skb = newskb;
		MEM_DBG_PKT_ALLOC_INC(skb);
	}

	return OSPKT_TO_RTPKT(skb);
}


/*
 * ========================================================================
 *
 * Routine Description:
 * Send a L2 frame to upper daemon to trigger state machine
 *
 * Arguments:
 * pAd - pointer to our pAdapter context
 *
 * Return Value:
 *
 * Note:
 *
 * ========================================================================
 */
BOOLEAN RTMPL2FrameTxAction(
	IN VOID *pCtrlBkPtr,
	IN RTMP_CB_8023_PACKET_ANNOUNCE _announce_802_3_packet,
	IN UCHAR apidx,
	IN UCHAR *pData,
	IN UINT32 data_len,
	IN UCHAR OpMode)
{
	struct sk_buff *skb = NULL;
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pCtrlBkPtr;
	PNET_DEV pNetDev = NULL;

	if (!pAd || apidx >= MAX_BEACON_NUM) {
		MTWF_DBG(pAd, DBG_CAT_TX, CATTX_DATA, DBG_LVL_ERROR,
				 "Error! pAd is NULL or apidx invalid, apidx=%d.\n", apidx);
		return FALSE;
	}

	OS_DEV_ALLOC_SKB(skb, (data_len + 2));

	if (!skb) {
		MTWF_DBG(NULL, DBG_CAT_TX, CATTX_DATA, DBG_LVL_ERROR,
				 "Error! Can't allocate a skb.\n");
		return FALSE;
	}

	pNetDev = get_netdev_from_bssid(pAd, pAd->ApCfg.MBSSID[apidx].wdev.wdev_idx),

	SET_OS_PKT_NETDEV(skb, pNetDev);
	/* 16 byte align the IP header */
	skb_reserve(skb, 2);
	/* Insert the frame content */
	os_move_mem(GET_OS_PKT_DATAPTR(skb), pData, data_len);
	/* End this frame */
	skb_put(GET_OS_PKT_TYPE(skb), data_len);
	MTWF_DBG(NULL, DBG_CAT_TX, CATTX_DATA, DBG_LVL_INFO, "done\n");
	_announce_802_3_packet(pCtrlBkPtr, skb, OpMode);
	return TRUE;
}


VOID RtmpOsPktInit(
	IN PNDIS_PACKET pNetPkt,
	IN PNET_DEV pNetDev,
	IN UCHAR *pData,
	IN USHORT DataSize)
{
	PNDIS_PACKET pRxPkt;

	pRxPkt = RTPKT_TO_OSPKT(pNetPkt);
	SET_OS_PKT_NETDEV(pRxPkt, pNetDev);
	SET_OS_PKT_DATAPTR(pRxPkt, pData);
	SET_OS_PKT_LEN(pRxPkt, DataSize);
	SET_OS_PKT_DATATAIL(pRxPkt, DataSize);
}


void wlan_802_11_to_802_3_packet(
	IN PNET_DEV pNetDev,
	IN UCHAR OpMode,
	IN USHORT VLAN_VID,
	IN USHORT VLAN_Priority,
	IN PNDIS_PACKET pRxPacket,
	IN UCHAR *pData,
	IN ULONG DataSize,
	IN PUCHAR pHeader802_3,
	IN UCHAR *TPID)
{
	if (pHeader802_3 == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_TX, DBG_LVL_ERROR,
			 "pHeader802_3 is NULL\n");
		ASSERT(pHeader802_3);
		return;
	}

	/* copy 802.3 header */
#ifdef CONFIG_AP_SUPPORT
	RT_CONFIG_IF_OPMODE_ON_AP(OpMode) {
		wlan_802_11_to_802_3_packet_ap(pNetDev, VLAN_VID, VLAN_Priority, pRxPacket, pHeader802_3, pData, DataSize, TPID);
	}
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	RT_CONFIG_IF_OPMODE_ON_STA(OpMode) {
		wlan_802_11_to_802_3_packet_sta(pNetDev, pRxPacket, pHeader802_3, pData, DataSize);
	}
#endif /* CONFIG_STA_SUPPORT */
}

#ifdef SYSTEM_LOG_SUPPORT
/*
 * ========================================================================
 *
 * Routine Description:
 * Send log message through wireless event
 *
 * Support standard iw_event with IWEVCUSTOM. It is used below.
 *
 * iwreq_data.data.flags is used to store event_flag that is
 * defined by user. iwreq_data.data.length is the length of the
 * event log.
 *
 * The format of the event log is composed of the entry's MAC
 * address and the desired log message (refer to
 * pWirelessEventText).
 *
 * ex: 11:22:33:44:55:66 has associated successfully
 *
 * p.s. The requirement of Wireless Extension is v15 or newer.
 *
 * ========================================================================
 */
VOID RtmpOsSendWirelessEvent(
	IN VOID *pAd,
	IN USHORT Event_flag,
	IN PUCHAR pAddr,
	IN UCHAR wdev_idx,
	IN CHAR Rssi,
	IN RTMP_OS_SEND_WLAN_EVENT pFunc)
{
#if WIRELESS_EXT >= 15
	pFunc(pAd, Event_flag, pAddr, wdev_idx, Rssi);
#else
	MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
			 "The Wireless Extension MUST be v15 or newer.\n");
#endif /* WIRELESS_EXT >= 15 */
}
#endif /* SYSTEM_LOG_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
VOID SendSignalToDaemon(
	IN INT sig,
	RTMP_OS_PID pid,
	unsigned long pid_no)
{
}
#endif /* CONFIG_AP_SUPPORT */

#if LINUX_VERSION_CODE <= 0x20402	/* Red Hat 7.1 */
struct net_device *alloc_netdev(
	int sizeof_priv,
	const char *mask,
	void (*setup)(struct net_device *))
{
	struct net_device *dev;
	INT alloc_size;
	/* ensure 32-byte alignment of the private area */
	alloc_size = sizeof(*dev) + sizeof_priv + 31;
	dev = kmalloc(alloc_size, GFP_KERNEL);

	if (dev == NULL) {
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR,
				 "alloc_netdev: Unable to allocate device memory.\n");
		return NULL;
	}

	memset(dev, 0, alloc_size);

	if (sizeof_priv)
		dev->priv = (void *)(((long)(dev + 1) + 31) & ~31);

	setup(dev);
	strlcpy(dev->name, mask, sizeof(dev->name));
	return dev;
}
#endif /* LINUX_VERSION_CODE */


static UINT32 RtmpOSWirelessEventTranslate(IN UINT32 eventType)
{
	switch (eventType) {
	case RT_WLAN_EVENT_CUSTOM:
		eventType = IWEVCUSTOM;
		break;

	case RT_WLAN_EVENT_CGIWAP:
		eventType = SIOCGIWAP;
		break;
#if WIRELESS_EXT > 17

	case RT_WLAN_EVENT_ASSOC_REQ_IE:
		eventType = IWEVASSOCREQIE;
		break;
#endif /* WIRELESS_EXT */
#if WIRELESS_EXT >= 14

	case RT_WLAN_EVENT_SCAN:
		eventType = SIOCGIWSCAN;
		break;
#endif /* WIRELESS_EXT */

	case RT_WLAN_EVENT_EXPIRED:
		eventType = IWEVEXPIRED;
		break;
	default:
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
				 "Unknown event: 0x%x\n", eventType);
		break;
	}

	return eventType;
}


int RtmpOSWrielessEventSend(
	IN PNET_DEV pNetDev,
	IN UINT32 eventType,
	IN INT flags,
	IN PUCHAR pSrcMac,
	IN PUCHAR pData,
	IN UINT32 dataLen)
{
	union iwreq_data wrqu;

	/* translate event type */
	eventType = RtmpOSWirelessEventTranslate(eventType);
	memset(&wrqu, 0, sizeof(wrqu));

	if (flags > -1)
		wrqu.data.flags = flags;

	if (pSrcMac)
		memcpy(wrqu.ap_addr.sa_data, pSrcMac, MAC_ADDR_LEN);

	if ((pData != NULL) && (dataLen > 0))
		wrqu.data.length = dataLen;
	else
		wrqu.data.length = 0;
#ifdef CONFIG_WEXT_CORE
	wireless_send_event(pNetDev, eventType, &wrqu, (char *)pData);
#endif /* CONFIG_WEXT_CORE */

	return 0;
}

int RtmpOSNetDevAddrSet(
	IN UCHAR OpMode,
	IN VOID *pNetDev,
	IN PUCHAR pMacAddr,
	IN PUCHAR dev_name)
{
#ifdef CONFIG_STA_SUPPORT
	/* work-around for the SuSE due to it has it's own interface name management system. */
	RT_CONFIG_IF_OPMODE_ON_STA(OpMode)
		mt_os_net_dev_set_addr(pNetDev, pMacAddr, dev_name);
	else
#endif /* CONFIG_STA_SUPPORT */
		mt_os_net_dev_set_addr(pNetDev, pMacAddr, NULL);
	return 0;
}

void RtmpOSNetDevClose(PNET_DEV pNetDev)
{
	dev_close(pNetDev);
}

#if (KERNEL_VERSION(2, 6, 31) <= LINUX_VERSION_CODE)
void RtmpOSNetDevOpsFree(PNET_DEV pNetDev)
{
	void *pNetDevOps = NULL;

	if (pNetDev == NULL) {
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR, "pNetDev is NULL!\n");
		ASSERT(pNetDev);
		return;
	}

	pNetDevOps = (void *)pNetDev->netdev_ops;
	if (pNetDevOps)
		vfree(pNetDevOps);
}
#endif


INT RtmpOSNetDevDestory(VOID *pReserved, PNET_DEV pNetDev)
{
	/* TODO: Need to fix this */
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR,
			"WARNING: This function not implement yet!!!\n");
	return 0;
}

/*
 * ========================================================================
 * Routine Description:
 *    Allocate memory for adapter control block.
 *
 * Arguments:
 *    pAd					Pointer to our adapter
 *
 * Return Value:
 *	NDIS_STATUS_SUCCESS
 *	NDIS_STATUS_FAILURE
 *	NDIS_STATUS_RESOURCES
 * ========================================================================
 */
NDIS_STATUS AdapterBlockAllocateMemory(VOID *handle, VOID **ppAd, UINT32 SizeOfpAd)
{
	os_alloc_vmem(NULL, (UCHAR **) ppAd, SizeOfpAd);

	if (*ppAd) {
		os_zero_mem(*ppAd, SizeOfpAd);
		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}


/* ========================================================================== */

#ifdef VLAN_SUPPORT
VOID* RtmpOsVLANInsertTag(PNDIS_PACKET pPacket, UINT16 tci)
{
	return (VOID *)vlan_insert_tag_set_proto(RTPKT_TO_OSPKT(pPacket), cpu2be16(ETH_TYPE_VLAN), tci);
}
#endif /*VLAN_SUPPORT*/

/*
 * ========================================================================
 * Routine Description:
 * Wake up the command thread.
 *
 * Arguments:
 * pAd                          - WLAN control block pointer
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
VOID RtmpOsCmdUp(RTMP_OS_TASK *pCmdQTask)
{
	MT_OS_TASK *pTask = RTMP_OS_TASK_GET(pCmdQTask);
#ifdef KTHREAD_SUPPORT
	pTask->kthread_running = TRUE;
	wake_up(&pTask->kthread_q);
#else
	if (CHECK_PID_LEGALITY(pTask->taskPID))
		RTMP_SEM_EVENT_UP(&(pTask->taskSema));
#endif /* KTHREAD_SUPPORT */
}


/*
 * ========================================================================
 * Routine Description:
 * Wake up USB Mlme thread.
 *
 * Arguments:
 * pAd                          - WLAN control block pointer
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
VOID RtmpOsMlmeUp(IN RTMP_OS_TASK *pMlmeQTask)
{
	MT_OS_TASK *pTask = RTMP_OS_TASK_GET(pMlmeQTask);
#ifdef KTHREAD_SUPPORT

	if ((pTask != NULL) && (pTask->kthread_task)) {
		pTask->kthread_running = TRUE;
		wake_up(&pTask->kthread_q);
	} else {
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_MLME, DBG_LVL_ERROR,
				 "invalid pTask 0x%p or kthread_task 0x%p\n",
				  pTask,
				  (pTask) ? pTask->kthread_task : NULL);
	}

#else

	if (pTask != NULL) {
		if (CHECK_PID_LEGALITY(pTask->taskPID))
			RTMP_SEM_EVENT_UP(&(pTask->taskSema));
	}

#endif /* KTHREAD_SUPPORT */
}

/*
 * ========================================================================
 * Routine Description:
 * Enable or disable wireless event sent.
 *
 * Arguments:
 * pReserved            - Reserved
 * FlgIsWEntSup - TRUE or FALSE
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
VOID RtmpOsWlanEventSet(
	IN VOID *pReserved,
	IN BOOLEAN *pCfgWEnt,
	IN BOOLEAN FlgIsWEntSup)
{
#if WIRELESS_EXT >= 15
	/*	pAd->CommonCfg.bWirelessEvent = FlgIsWEntSup; */
	*pCfgWEnt = FlgIsWEntSup;
#else
	*pCfgWEnt = 0;		/* disable */
#endif
}

/*
 * ========================================================================
 * Routine Description:
 * vmalloc
 *
 * Arguments:
 * Size                 - memory size
 *
 * Return Value:
 * the memory
 *
 * Note:
 * ========================================================================
 */
VOID *RtmpOsVmalloc(ULONG Size)
{
	return vmalloc(Size);
}

/*
 * ========================================================================
 * Routine Description:
 * vfree
 *
 * Arguments:
 * pMem                 - the memory
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
VOID RtmpOsVfree(VOID *pMem)
{
	if (pMem != NULL)
		vfree(pMem);
}

#ifdef CONFIG_SPECIAL_RPS_SUPPORT
unsigned int rps_cpu_idx[] = { 0x10000000, 0xF0000000, 0x80000000, 0x40000000};
unsigned int change_idx = 0, g_accnt = 0;

inline BOOLEAN is_udp(PNDIS_PACKET pPacket)
{
	PUCHAR pSrcBuf;

	pSrcBuf = GET_OS_PKT_DATAPTR(pPacket);

	if (pSrcBuf[0] == 0x45 && pSrcBuf[1] == 0x0) {
		if (pSrcBuf[9] == IP_PROTO_UDP)
			return TRUE;
	}
	return FALSE;
}
#endif /* CONFIG_SPECIAL_RPS_SUPPORT */
/*
 * ========================================================================
 * Routine Description:
 * Pass the received packet to OS.
 *
 * Arguments:
 * pPkt                 - the packet
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
VOID RtmpOsPktRcvHandle(PNDIS_PACKET pNetPkt, VOID *napi)
{
	PRTMP_ADAPTER pAd = NULL;
	struct tx_rx_ctl *tr_ctl;
	struct tr_counter *tr_cnt;
	int status;
	int len;

	GET_PAD_FROM_NET_DEV(pAd, GET_OS_PKT_NETDEV(pNetPkt));
	tr_ctl = &pAd->tr_ctl;
	tr_cnt = &tr_ctl->tr_cnt;

	WLAN_HOOK_CALL(WLAN_HOOK_RX_DRIVER_TO_OS, NULL, pNetPkt);

#ifdef CONFIG_CSO_SUPPORT

	mt_os_pkt_set_ip_summed(pNetPkt, RTMP_GET_TCP_CHKSUM_FAIL(pNetPkt));

#endif /* CONFIG_CSO_SUPPORT */

	tr_cnt->rx_to_os_cnt++;

#ifdef CONFIG_SPECIAL_RPS_SUPPORT
	if (pAd->rps_special_hash_en)
		if (!mt_os_pkt_set_rps_hash(pNetPkt, pAd->rps_per_round))
			goto done;
done:
#endif /* CONFIG_SPECIAL_RPS_SUPPORT */
	len = mt_os_get_pkt_len(pNetPkt);

	mt_os_pkt_send(pNetPkt, napi, &status);

#ifdef IXIA_C50_MODE
	if (status && IS_EXPECTED_LENGTH(pAd, len + 14))
		pAd->rx_cnt.rx_pkt_to_os[smp_processor_id()]++;
#endif
}

/*
 * ========================================================================
 * Routine Description:
 * Get the network interface for the packet.
 *
 * Arguments:
 * pPkt                 - the packet
 *
 * Return Value:
 * None
 *
 * Note:
 * ========================================================================
 */
PNET_DEV RtmpOsPktNetDevGet(VOID *pPkt)
{
	return GET_OS_PKT_NETDEV(pPkt);
}


#ifdef IAPP_SUPPORT
/* Layer 2 Update frame to switch/bridge */
/* For any Layer2 devices, e.g., bridges, switches and other APs, the frame
  * can update their forwarding tables with the correct port to reach the new
  * location of the STA
  */
typedef struct GNU_PACKED _RT_IAPP_L2_UPDATE_FRAME {

	UCHAR DA[ETH_ALEN];	/* broadcast MAC address */
	UCHAR SA[ETH_ALEN];	/* the MAC address of the STA that has just associated or reassociated */
	USHORT Len;		/* 8 octets */
	UCHAR DSAP;		/* null */
	UCHAR SSAP;		/* null */
	UCHAR Control;		/* reference to IEEE Std 802.2 */
	UCHAR XIDInfo[3];	/* reference to IEEE Std 802.2 */
} RT_IAPP_L2_UPDATE_FRAME, *PRT_IAPP_L2_UPDATE_FRAME;


PNDIS_PACKET RtmpOsPktIappMakeUp(
	IN PNET_DEV pNetDev,
	IN UINT8 * pMac)
{
	RT_IAPP_L2_UPDATE_FRAME frame_body;
	INT size = sizeof(RT_IAPP_L2_UPDATE_FRAME);
	PNDIS_PACKET pNetBuf;

	if (pNetDev == NULL)
		return NULL;

	pNetBuf = mt_os_net_pkt_alloc(NULL, size);

	if (!pNetBuf) {
		MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_FT, DBG_LVL_ERROR,
			"Error! Can't allocate a skb.\n");
		return NULL;
	}

	/* init the update frame body */
	os_zero_mem(&frame_body, size);
	memset(frame_body.DA, 0xFF, ETH_ALEN);
	memcpy(frame_body.SA, pMac, ETH_ALEN);
	frame_body.Len = OS_HTONS(ETH_ALEN);
	frame_body.DSAP = 0;
	frame_body.SSAP = 0x01;
	frame_body.Control = 0xAF;
	frame_body.XIDInfo[0] = 0x81;
	frame_body.XIDInfo[1] = 1;
	frame_body.XIDInfo[2] = 1 << 1;
	SET_OS_PKT_NETDEV(pNetBuf, pNetDev);
	skb_reserve(pNetBuf, 2);
	memcpy(skb_put(pNetBuf, size), &frame_body, size);
	return pNetBuf;
}
#endif /* IAPP_SUPPORT */

#ifdef CONFIG_FAST_NAT_SUPPORT
VOID RtmpOsPktNatMagicTag(PNDIS_PACKET pNetPkt)
{
	FOE_MAGIC_TAG(RTPKT_TO_OSPKT(pNetPkt)) = FOE_MAGIC_WLAN;
}
#endif /*CONFIG_FAST_NAT_SUPPORT*/

inline int OS_TEST_BIT(int bit, ULONG *flags)
{
	return test_bit(bit, flags);
}

inline void OS_SET_BIT(unsigned int bit, ULONG *flags)
{
	set_bit(bit, flags);
}

inline void OS_CLEAR_BIT(unsigned int bit, ULONG *flags)
{
	clear_bit(bit, flags);
}


VOID RtmpOsTaskWakeUp(RTMP_OS_TASK *pTask)
{
#ifdef KTHREAD_SUPPORT
	WAKE_UP(pTask);
#else
	RTMP_SEM_EVENT_UP(&pTask->taskSema);
#endif
}


void os_kref_init(os_kref *ref)
{
	kref_init(ref);
}

void os_kref_get(os_kref *ref)
{
	kref_get(ref);
}

int os_kref_put(os_kref *ref, void (*release)(os_kref *ref))
{
	return kref_put(ref, release);
}


#ifdef CONFIG_DBG_QDISC
/*
*
*/
void os_system_tx_queue_dump(PNET_DEV dev)
{
	struct netdev_queue *txq;
	struct Qdisc *q;
	struct sk_buff *skb;
	int i = 0, j = 0, k = 0;
	struct page *p;
	size_t page_size;
	int len;

	for (i = 0; i < dev->num_tx_queues; i++) {
		txq = netdev_get_tx_queue(dev, i);
		q = mt_os_lockless_access_pointer(os_lockless_ctx_get(), txq->qdisc);
		len = qdisc_qlen(q);
		MTWF_PRINT("devname:%s, q=%p, qlen=%d, page size:%lu\n",
			dev->name, q, len, PAGE_SIZE);
		skb = q->ops->peek(q);
		/* If we run with a TX queue, check if the queue is too long*/
		for (j = 0; j < len; j++) {
			MTWF_PRINT("skb=%p,len:%d, skb->head: %p, %p, dev:%s\n",
				skb, skb->len, skb->data, skb->data, skb->dev->name);
			if (skb->head) {
				p = virt_to_head_page(skb->head);
				page_size = PAGE_SIZE << compound_order(p);
				MTWF_PRINT("index:%d, page:%p ,rfcnt:%d, page size:%zu",
					k++, p, OS_PAGE_REF(p), page_size);
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
				MTWF_PRINT("order:%u, dtor:%u\n",
					(unsigned int) compound_order(p), 0);
#else
				MTWF_PRINT("order:%u, dtor:%u\n",
					(unsigned int) compound_order(p),
					(unsigned int) p->compound_dtor);
#endif
			}
			skb = skb->next;
		}
		MTWF_PRINT("qlen: %d, backlog: %d\n", q->qstats.qlen, q->qstats.backlog);
		MTWF_PRINT("drops: %d, requeues: %d, overlimits:%d\n",
			q->qstats.drops, q->qstats.requeues, q->qstats.overlimits);
	}
}
#endif /*CONFIG_DBG_QDISC*/

#ifdef CFG_SUPPORT_CSI

static PNDIS_PACKET make_csi_nlmsg_fragment(RTMP_ADAPTER *pAd, void *info, int reqtype)
{
	struct CSI_DATA_T *tmp_csi_data = NULL;
	PNDIS_PACKET nl_skb = NULL;
	struct csi_msg_Info msginfo = {0};
	struct CSI_INFO_T *prCSIInfo = &pAd->rCSIInfo;

	/* alloc mem for CSIData */
	os_alloc_mem(NULL, (UCHAR **)&tmp_csi_data, sizeof(struct CSI_DATA_T));

	if (!tmp_csi_data) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"tmp_csi_data fail to alloc mem!\n");
		goto out;
	}

	os_zero_mem(tmp_csi_data, sizeof(struct CSI_DATA_T));

	if (wlanPopCSIData(pAd, tmp_csi_data) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"pop csi_data fail!\n");
		goto out;
	}

	msginfo.channel = prCSIInfo->csi_wdev->channel;
	msginfo.bssid = prCSIInfo->csi_wdev->bssid;
	msginfo.reqtype = reqtype;
	nl_skb = mt_make_csi_nlmsg_fragment(tmp_csi_data, info, &msginfo);
out:
	if (tmp_csi_data) {
		if (tmp_csi_data->ac2IData)
			os_free_mem(tmp_csi_data->ac2IData);
		if (tmp_csi_data->ac2QData)
			os_free_mem(tmp_csi_data->ac2QData);
		os_free_mem(tmp_csi_data);
	}

	return nl_skb;
}

VOID csi_report_timer_handle(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)FunctionContext;
	struct CSI_INFO_T *prCSIInfo;
	PNDIS_PACKET nl_skb = NULL;


	prCSIInfo = &pAd->rCSIInfo;

	nl_skb = make_csi_nlmsg_fragment(pAd, &prCSIInfo->peer_info, FALSE);

	if (!nl_skb) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"make nl msg fail!!!\n");
		return;
	}

}

int csi_genl_recv_doit(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	char *req_type = (char *)ioctl_cfg->name; /*chain or pkt*/
	PNET_DEV dev = ioctl_cfg->net_dev;
	void *info = ioctl_cfg->data;
	UINT32 recvd_dump_num = 0;
	PRTMP_ADAPTER pAd = NULL;
	char cmd_msg[32] = {0};
	char dev_string[16] = {0};
	struct CSI_INFO_T *prCSIInfo;
	UINT32 usr_loop_cnt = 0;
	char *temp_str = NULL;
	u32 report_interval = 0;
	PNDIS_PACKET nl_skb = NULL;
	int ret = 0;

	req_type = rstrtok(req_type, "-");
	if (!req_type)
		return -1;

	if (!strcmp(req_type, "chain") || !strcmp(req_type, "pkt")) {
		temp_str = rstrtok(NULL, "-");

		if (temp_str)
			ret = kstrtou32(temp_str, 10, &recvd_dump_num);

		if (ret) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
			"CSI kstrtou32 fail.\n");
			return ret;
		}

		/*at least send out one pkt*/
		if (!strcmp(req_type, "pkt") && !temp_str)
			recvd_dump_num = 1;
	} else if (!strcmp(req_type, "auto")) {
		temp_str = rstrtok(NULL, "-");

		if (temp_str)
			ret = kstrtou32(temp_str, 10, &report_interval);

		if (ret) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
			"CSI kstrtou32 fail.\n");
			return ret;
		}
	}

	/*get our pAd*/
	GET_PAD_FROM_NET_DEV(pAd, dev);

	if (!pAd) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
		"CSI genl_rev, get null pAd.\n");
		return -EFAULT;
	}

	prCSIInfo = &pAd->rCSIInfo;

	/*step2: send our status: used buffer*/
	/*step2:  Or start reporting csi data*/
	/*TBD: put make_csi_nlmsg_complete(pAd) as 3rd op*/
	if ((!strcmp(req_type, "chain")) && (recvd_dump_num == 0)) {
		os_zero_mem(cmd_msg, sizeof(cmd_msg));
		ret = snprintf(cmd_msg, sizeof(cmd_msg), "%s-%d", dev_string, prCSIInfo->u4CSIBufferUsed);

		if (os_snprintf_error(sizeof(cmd_msg), ret)) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
			"final_name snprintf error!\n");
			return -EFAULT;
		}

		mt_send_msg_reply(info, CSI_ATTR_REPORT_MSG, cmd_msg);
	} else if ((!strcmp(req_type, "chain")) && (recvd_dump_num != 0)) {
		/*user request/ pkt total chain num/ left chains after fix*/
		/*select the less num to send pkt.*/
		/*TBD: need to check if fw will fix the total num with chain filter */
		usr_loop_cnt = recvd_dump_num;

		while (usr_loop_cnt > 0) {
			nl_skb = make_csi_nlmsg_fragment(pAd, info, FALSE);

			if (!nl_skb) {
				MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"make nl msg fail!!!\n");
				break;
			}

			usr_loop_cnt--;
		}
	} else if (!strcmp(req_type, "pkt")) {
		usr_loop_cnt = recvd_dump_num;

		while (usr_loop_cnt > 0) {
			nl_skb = make_csi_nlmsg_fragment(pAd, info, TRUE);

			if (!nl_skb) {
				MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_ERROR,
				"make nl msg fail!!!\n");
				break;
			}
			usr_loop_cnt--;
		}
	} else if (!strcmp(req_type, "auto")) {
		BOOLEAN Cancelled;

		prCSIInfo->report_interval = report_interval;
		os_move_mem(&prCSIInfo->peer_info, info, ioctl_cfg->data_len);

		if (!report_interval) {
			RTMPCancelTimer(&prCSIInfo->csi_report_timer, &Cancelled);
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_NOTICE,
			"csi genl timer stop!\n");
		} else if (!prCSIInfo->report_interval) {
			RTMPSetTimer(&prCSIInfo->csi_report_timer, report_interval);
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_NOTICE,
			"csi genl timer start, interval(%d ms)!\n", prCSIInfo->report_interval);
		} else {
			RTMPModTimer(&prCSIInfo->csi_report_timer, report_interval);
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CSI, DBG_LVL_NOTICE,
			"csi genl timer changed, interval(%d ms)!\n", prCSIInfo->report_interval);
		}
	}

	return 0;
}

#endif


/*initial global data structure for single driver*/
VOID os_module_init(VOID)
{
#ifndef CONFIG_PROFILE_OFF
#ifdef DBG
	RTMP_STRING dbg_level[32] = {0};
	RTMP_STRING dbg_option[32] = {0};
	NDIS_STATUS retval = NDIS_STATUS_SUCCESS;
#endif /* DBG */
#endif /* !CONFIG_PROFILE_OFF */
	os_lockless_init();

	os_sysinfo_init();

	// register module debug info to muldf.

	rt_profile_init();

#ifndef CONFIG_PROFILE_OFF
#ifdef DBG
	retval = get_dbg_setting_by_profile(dbg_level, dbg_option);
	if (retval == NDIS_STATUS_SUCCESS) {
		if (strlen(dbg_level) != 0)
			Set_Debug_Proc(NULL, dbg_level);

		if (strlen(dbg_option) != 0)
			Set_DebugOption_Proc(NULL, dbg_option);
	}
#endif /* DBG */
#endif /* !CONFIG_PROFILE_OFF */

	physical_device_init();
#ifdef CONFIG_CONNINFRA_SUPPORT
	conninfra_pwr_on(CONNDRV_TYPE_WIFI);
#endif /* CONFIG_CONNINFRA_SUPPORT */
#ifdef MEM_ALLOC_INFO_SUPPORT
	MemInfoListInital();
#endif /* MEM_ALLOC_INFO_SUPPORT */

	multi_hif_init();

	/* init bss manager */
	bss_mngr_init();
	/* init ser mngr */
	ser_mngr_init();

	/* init sta mld link manager*/
#ifdef DOT11_EHT_BE
	init_sta_mld_link_mgr();
	/* mld entry init */
	mld_entry_init();
#endif

#ifdef CFG_SUPPORT_CSI
	mtk_csi_genl_register();
#endif
	/* EMI init */
	if (!mt_os_wm_emi_init(1)) {
		unsigned long phys_addr = 0;
		u8 *vir_addr = 0;
		size_t len = 0;

		if (!mt_os_wm_emi_get_resource(&phys_addr, (void *)&vir_addr, &len))
			MTWF_DBG(NULL, DBG_CAT_FW, CATFW_MCU, DBG_LVL_INFO,
			"%s: phy addr: 0x%lx, size: 0x%zx, vir_addr: 0x%p",
			WMCPU_EMI_DRIVER_NAME, phys_addr, len, vir_addr);
	}
}

/*exit global data structrue for single driver*/
VOID os_module_exit(VOID)
{
	os_sysinfo_deinit();
	/* EMI de-init */
	mt_os_wm_emi_deinit();
#ifdef CFG_SUPPORT_CSI
	mtk_csi_genl_unregister();
#endif
#ifdef DOT11_EHT_BE
	/* mld entry exit */
	mld_entry_exit();
#endif
	ser_mngr_exit();
	/* de-init bss manager */
	bss_mngr_deinit();

	multi_hif_exit();
#ifdef MEM_ALLOC_INFO_SUPPORT
	MemInfoListExit();
#endif /*MEM_ALLOC_INFO_SUPPORT*/
#ifdef CONFIG_CONNINFRA_SUPPORT
	conninfra_pwr_off(CONNDRV_TYPE_WIFI);
#endif /* CONFIG_CONNINFRA_SUPPORT */
	physical_device_exit();

	rt_profile_deinit();

	os_lockless_deinit();
}

void net_interface_down(IN struct net_device *net_dev)
{
	rtnl_lock();
	call_netdevice_notifiers(NETDEV_GOING_DOWN, net_dev);
	rtnl_unlock();
	rtnl_lock();
	dev_change_flags(net_dev, (net_dev->flags &= ~IFF_UP), NULL);
	rtnl_unlock();
	rtnl_lock();
	call_netdevice_notifiers(NETDEV_CHANGE, net_dev);
	rtnl_unlock();
}

void net_interface_up(IN struct net_device *net_dev)
{
	rtnl_lock();
	dev_change_flags(net_dev, (net_dev->flags |= IFF_UP), NULL);
	rtnl_unlock();
	rtnl_lock();
	call_netdevice_notifiers(NETDEV_CHANGE, net_dev);
	rtnl_unlock();
}

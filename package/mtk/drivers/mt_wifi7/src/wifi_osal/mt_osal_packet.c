/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#if (KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE)
#include <net/netdev_rx_queue.h>
#endif

#define SUBMODULE "mt_osal_packet"
#include <mt_osal_debug.h>
#include <mt_osal_packet.h>
#include <mt_osal_net.h>
#include <mt_osal_module.h>


static inline unsigned int mt_os_net_pkt_csum_add(
	unsigned int csum, unsigned int addend)
{
	unsigned int res = csum;

	res += addend;
	return res + (res < addend);
}

PNDIS_PACKET mt_os_net_pkt_alloc(void *dummy, int size)
{
	struct sk_buff *skb;
	/* Add 2 more bytes for ip header alignment */
	OS_DEV_ALLOC_SKB(skb, (size + 2));
	return (PNDIS_PACKET) skb;
}
EXPORT_SYMBOL(mt_os_net_pkt_alloc);

void mt_kfree_skb(PNDIS_PACKET skb)
{
	if (skb)
		kfree_skb((struct sk_buff *)skb);
}
EXPORT_SYMBOL(mt_kfree_skb);

PNDIS_PACKET mt_os_alloc_frag_pkt_buf(void *dummy, unsigned long len)
{
	struct sk_buff *pkt;

	OS_DEV_ALLOC_SKB(pkt, len);

	if (pkt == NULL) {
		MT_DEBUG_TRACE("can't allocate frag rx %ld size packet\n", len);
	}

	return (PNDIS_PACKET) pkt;
}
EXPORT_SYMBOL(mt_os_alloc_frag_pkt_buf);

void mt_os_pkt_protocol_assign(void *net_pkt)
{
	struct sk_buff *rx_pkt = (struct sk_buff *)net_pkt;

	rx_pkt->protocol = eth_type_trans(rx_pkt, rx_pkt->dev);
}
EXPORT_SYMBOL(mt_os_pkt_protocol_assign);

void mt_os_pkt_pull_rcsum(void *skb_, unsigned int len)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;

	if (len > skb->len)
		return;

	skb_pull(skb, len);
	if (skb->ip_summed == CHECKSUM_COMPLETE)
		skb->csum = mt_os_net_pkt_csum_add(skb->csum, ~csum_partial(skb->data, len, 0));
	else if (skb->ip_summed == CHECKSUM_PARTIAL &&
		 (skb->csum_start - (skb->data - skb->head)) < 0)
		skb->ip_summed = CHECKSUM_NONE;
}
EXPORT_SYMBOL(mt_os_pkt_pull_rcsum);

void mt_os_pkt_reset(void *skb_, int reset_mac_hdr)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;

	if (reset_mac_hdr)
		skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb_reset_mac_len(skb);
}
EXPORT_SYMBOL(mt_os_pkt_reset);

PNDIS_PACKET mt_os_clone_pkt(void *pNetDev, void *pPacket)
{
	struct sk_buff *skb;

	skb = skb_clone((struct sk_buff *)pPacket, GFP_ATOMIC);

	if (skb)
		skb->dev = pNetDev;

	return (PNDIS_PACKET)skb;
}
EXPORT_SYMBOL(mt_os_clone_pkt);

PNDIS_PACKET mt_os_copy_pkt(void *pNetDev, void *pPacket)
{
	struct sk_buff *skb = NULL;

	skb = skb_copy((struct sk_buff *)pPacket, GFP_ATOMIC);

	if (skb)
		skb->dev = pNetDev;

	return (PNDIS_PACKET)skb;
}
EXPORT_SYMBOL(mt_os_copy_pkt);

PNDIS_PACKET mt_os_clone_pkt_by_data(unsigned char MonitorOn, void *pkt, unsigned char *buf, unsigned long sz)
{
	struct sk_buff *pClonedPkt, *pkt_ = (struct sk_buff *)pkt;

	if (pkt == NULL) {
		MT_DEBUG_ERROR("pkt is NULL\n");
		return NULL;
	}

	if (!MonitorOn && sz >= MAX_RX_DATA_BUFFER_SIZE ) {
		MT_DEBUG_TRACE("pkt sz is too large %d\n", sz);
		return NULL;
	}


	/* clone the packet */
	pClonedPkt = skb_clone((struct sk_buff *)pkt, GFP_ATOMIC);

	if (pClonedPkt) {
		/* set the correct dataptr and data len */
		pClonedPkt->dev = pkt_->dev;
		pClonedPkt->data = buf;
		pClonedPkt->len = sz;
		SET_OS_PKT_DATA_TAIL(pClonedPkt, pClonedPkt->len);
	}

	return (PNDIS_PACKET)pClonedPkt;
}
EXPORT_SYMBOL(mt_os_clone_pkt_by_data);


unsigned char mt_os_vlan_8023_header_copy(
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	unsigned char *pHeader802_3,
	unsigned int HdrLen,
	unsigned char *pData,
	unsigned char *TPID)
{
	unsigned int TCI;
	unsigned char VLAN_Size = 0;

	if (VLAN_VID != 0) {
		/* need to insert VLAN tag */
		VLAN_Size = OS_LENGTH_802_1Q;
		/* make up TCI field */
		TCI = (VLAN_VID & 0x0fff) | ((VLAN_Priority & 0x7) << 13);
		TCI = cpu_to_le16(TCI);
		/* copy dst + src MAC (12B) */
		memcpy(pData, pHeader802_3, OS_LENGTH_802_3_NO_TYPE);
		/* copy VLAN tag (4B) */
		/* do NOT use memcpy to speed up */
		*(unsigned short *) (pData + OS_LENGTH_802_3_NO_TYPE) = *(unsigned short *) TPID;
		*(unsigned short *) (pData + OS_LENGTH_802_3_NO_TYPE + 2) = TCI;
		/* copy type/len (2B) */
		*(unsigned short *) (pData + OS_LENGTH_802_3_NO_TYPE + OS_LENGTH_802_1Q) =
			*(unsigned short *) &pHeader802_3[OS_LENGTH_802_3 - OS_LENGTH_802_3_TYPE];

		/* copy tail if exist */
		if (HdrLen > OS_LENGTH_802_3)
			memcpy(pData + OS_LENGTH_802_3 + OS_LENGTH_802_1Q, pHeader802_3 + OS_LENGTH_802_3, HdrLen - OS_LENGTH_802_3);
	} else {
		/* no VLAN tag is needed to insert */
		memcpy(pData, pHeader802_3, HdrLen);
	}

	return VLAN_Size;
}
EXPORT_SYMBOL(mt_os_vlan_8023_header_copy);

PNDIS_PACKET mt_os_duplicate_pkt_vlan(
	void *pNetDev,
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	unsigned char *pHeader802_3,
	unsigned int HdrLen,
	unsigned char *pData,
	unsigned long DataSize,
	unsigned char *TPID)
{
	struct sk_buff *skb;
	unsigned short VLAN_Size = 0;
	int skb_len = HdrLen + DataSize + 2;

	if (VLAN_VID != 0)
		skb_len += OS_LENGTH_802_1Q;

	OS_DEV_ALLOC_SKB(skb, skb_len);
	if (skb != NULL) {
		skb_reserve(skb, 2);

		/* copy header (maybe with VLAN tag) */
		VLAN_Size = mt_os_vlan_8023_header_copy(VLAN_VID, VLAN_Priority,
										  pHeader802_3, HdrLen,
										  GET_OS_PKT_DATA_TAIL(skb),
										  TPID);
		skb_put(skb, HdrLen + VLAN_Size);
		/* copy data body */
		memcpy(GET_OS_PKT_DATA_TAIL(skb), pData, DataSize);

		skb_put(skb, DataSize);
		skb->dev = pNetDev;
	}

	return (PNDIS_PACKET)skb;
}
EXPORT_SYMBOL(mt_os_duplicate_pkt_vlan);


void wlan_802_11_to_802_3_packet_ap(
	void *pNetDev,
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	void *pRxPacket,
	unsigned char *pHeader802_3,
	unsigned char *pData,
	unsigned long DataSize,
	unsigned char *TPID)
{
	struct sk_buff *pOSPkt = (struct sk_buff *)pRxPacket;
	unsigned char VLAN_Size = 0;
	unsigned char *data_p;

	pOSPkt->dev = pNetDev;
	pOSPkt->data = pData;
	pOSPkt->len = DataSize;
	SET_OS_PKT_DATA_TAIL(pOSPkt, pOSPkt->len);

	if (VLAN_VID != 0)
		VLAN_Size = OS_LENGTH_802_1Q;

	data_p = skb_push(pOSPkt, OS_LENGTH_802_3 + VLAN_Size);
	mt_os_vlan_8023_header_copy(VLAN_VID, VLAN_Priority,
							pHeader802_3, OS_LENGTH_802_3,
							data_p, TPID);
}
EXPORT_SYMBOL(wlan_802_11_to_802_3_packet_ap);


void wlan_802_11_to_802_3_packet_sta(
		void *pNetDev,
		void *pRxPacket,
		unsigned char *pHeader802_3,
		unsigned char *pData,
		unsigned long DataSize)
{
	struct sk_buff *pOSPkt = (struct sk_buff *)pRxPacket;

	pOSPkt->dev = pNetDev;
	pOSPkt->data = pData;
	pOSPkt->len = DataSize;
	SET_OS_PKT_DATA_TAIL(pOSPkt, pOSPkt->len);
	memcpy(skb_push(pOSPkt, OS_LENGTH_802_3), pHeader802_3, OS_LENGTH_802_3);
}
EXPORT_SYMBOL(wlan_802_11_to_802_3_packet_sta);


PNDIS_PACKET mt_os_expand_pkt(
	PNDIS_PACKET pPacket,
	unsigned int ext_head_len,
	unsigned int ext_tail_len)
{
	struct sk_buff *skb, *newskb;

	skb = (struct sk_buff *)pPacket;

	if (skb_cloned(skb) ||
		(skb_headroom(skb) < ext_head_len) ||
		(skb_tailroom(skb) < ext_tail_len)) {
		unsigned int head_len =
			(skb_headroom(skb) < ext_head_len) ? ext_head_len : skb_headroom(skb);
		unsigned int tail_len =
			(skb_tailroom(skb) < ext_tail_len) ? ext_tail_len : skb_tailroom(skb);
		/* alloc a new skb and copy the packet */
		newskb = skb_copy_expand(skb, head_len, tail_len, GFP_ATOMIC);
		dev_kfree_skb_any(skb);
		// MEM_DBG_PKT_FREE_INC(skb);

		if (newskb == NULL) {
			MT_DEBUG_TRACE("Extend Tx buffer for WPI failed!, dropping packet!\n");
			return NULL;
		}

		skb = newskb;
		// MEM_DBG_PKT_ALLOC_INC(skb);
	}

	return (PNDIS_PACKET)skb;
}
EXPORT_SYMBOL(mt_os_expand_pkt);

void mt_os_pkt_set_dest_type(void *skb_)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;
	struct net_device *net_dev = skb->dev;
	struct ethhdr *eth = (struct ethhdr *)skb->data;

	if (unlikely(eth->h_dest[0] & 0x1)) {
		if (ether_addr_equal(eth->h_dest, net_dev->broadcast))
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	}
}
EXPORT_SYMBOL(mt_os_pkt_set_dest_type);

void mt_os_pkt_set_protocol(void *skb_, u8 *header, u32 header_len)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;
	struct ethhdr *eth = (struct ethhdr *)header;

	if (!skb || !eth || header_len < ETH_HLEN)
		return;

	skb->protocol = eth->h_proto;
}
EXPORT_SYMBOL(mt_os_pkt_set_protocol);

void mt_os_pkt_set_ip_summed(void *skb_, int ip_summed)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;
	struct net_device *net_dev = skb->dev;

	if (net_dev->features & NETIF_F_HW_CSUM) {
		if (ip_summed)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb->ip_summed = CHECKSUM_NONE;
	}
}
EXPORT_SYMBOL(mt_os_pkt_set_ip_summed);

unsigned int cpu_idx[] = { 0x10000000, 0xF0000000, 0x80000000, 0x40000000};

bool mt_os_pkt_set_rps_hash(void *skb_, int rps_per_round)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;
	struct net_device *net_dev = skb->dev;
	struct netdev_rx_queue *rxqueue = net_dev->_rx;
	struct rps_map *map;
	static int g_accnt = 0, change_idx = 0;
	u8 *pSrcBuf = skb->data;

	map = rcu_dereference(rxqueue->rps_map);
	if (!map || map->len >= 4 || map->len == 0)
		return false;
	if (map->len == 1) {
		skb->sw_hash = 1;
		skb->hash = cpu_idx[0];
	} else if (pSrcBuf[0] == 0x45 && pSrcBuf[1] == 0x0 && pSrcBuf[9] == 0x11) {
		if (g_accnt++ > rps_per_round) {
			g_accnt = 0;
			if (change_idx++ >= map->len)
				change_idx = 0;
		}
		skb->sw_hash = 1;
		skb->hash = cpu_idx[change_idx];
	}

	return true;
}
EXPORT_SYMBOL(mt_os_pkt_set_rps_hash);

void mt_os_pkt_clear_info(void *skb_)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;

	memset(&skb->cb[CB_OFF], 0, CB_LEN);
}
EXPORT_SYMBOL(mt_os_pkt_clear_info);

void mt_os_pkt_send(void *skb_, void *napi, int *pSend)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;
	int send;

	if (napi && in_serving_softirq()) {
		napi_gro_receive((struct napi_struct *)napi, skb);
		send = 0;
	} else {
#ifdef CONFIG_NETIF_RX_DISPATCH
		if (in_interrupt())
			netif_rx(skb);
		else
			netif_rx_ni(skb);
#else
		netif_rx(skb);
#endif /* CONFIG_NETIF_RX_DISPATCH */
		send = 1;
	}

	if(pSend)
		*pSend = send;
}
EXPORT_SYMBOL(mt_os_pkt_send);

void mt_os_802_11_radiotap_packet(
	void *skb_, void *netdev_)
{
	struct sk_buff *pOSPkt = (struct sk_buff *)skb_;

	if (netdev_ == NULL) {
		dev_kfree_skb(pOSPkt);
		return;
	}
	skb_reset_mac_header(pOSPkt);
	pOSPkt->protocol = htons(ETH_P_802_2);

	pOSPkt->dev = netdev_;
	pOSPkt->pkt_type = PACKET_OTHERHOST;
	pOSPkt->ip_summed = CHECKSUM_NONE;

#if (KERNEL_VERSION(5, 18, 0) <= LINUX_VERSION_CODE)
	netif_receive_skb(pOSPkt);
#else
	netif_rx_ni(pOSPkt);
#endif
}
EXPORT_SYMBOL(mt_os_802_11_radiotap_packet);

int mt_os_get_pkt_len(void *skb_)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;

	return skb->len;
}
EXPORT_SYMBOL(mt_os_get_pkt_len);

void *mt_os_pkt_get_wdev(void *skb_)
{
	struct sk_buff *skb = (struct sk_buff *)skb_;

	return mt_os_net_dev_get_wdev(skb->dev);
}
EXPORT_SYMBOL(mt_os_pkt_get_wdev);

DECLARE_SUB_MODULE_INIT(mt_osal_packet_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_packet_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_packet_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_packet_exit);

/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_PACKET_H_
#define __MT_OSAL_PACKET_H_

#include <linux/kernel.h>
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
#include <linux/version.h>
#include <linux/byteorder/generic.h>


typedef void *PNDIS_PACKET;
/* ======================== Packet ========================================== */
#define MAX_RX_DATA_BUFFER_SIZE     1530
#define OS_LENGTH_802_11			24
#define OS_LENGTH_802_11_AND_H		30
#define OS_LENGTH_802_11_CRC_H		34
#define OS_LENGTH_802_11_CRC		28
#define OS_LENGTH_802_11_WITH_ADDR4	30
#define OS_LENGTH_802_3			14
#define OS_LENGTH_802_3_TYPE		2
#define OS_LENGTH_802_1_H			8
#define OS_LENGTH_EAPOL_H			4
#define OS_LENGTH_WMMQOS_H			2
#define OS_LENGTH_CRC			4
#define OS_MAX_SEQ_NUMBER			0x0fff
#define OS_LENGTH_802_3_NO_TYPE		12
#define OS_LENGTH_802_1Q			4 /* VLAN related */
#define OS_LENGTH_802_11_QOS_FIELD		2
#define OS_LENGTH_802_11_HTC		4
#define OS_LENGTH_802_11_CAP_INFO		2
#define OS_LENGTH_802_11_LISTEN_INTERVAL	2
#define OS_LENGTH_802_11_STATUS_CODE	2
#define OS_LENGTH_802_11_AID	2

#if (KERNEL_VERSION(2, 6, 22) <= LINUX_VERSION_CODE)
#define GET_OS_PKT_DATA_TAIL(_pkt) \
	skb_tail_pointer((struct sk_buff *)(_pkt))
#define SET_OS_PKT_DATA_TAIL(_pkt, _len)	\
	skb_set_tail_pointer((struct sk_buff *)(_pkt), _len)
#else
#define GET_OS_PKT_DATA_TAIL(_pkt) \
	((struct sk_buff *)(_pkt)->tail)
#define SET_OS_PKT_DATA_TAIL(_pkt, _len)	\
	((((struct sk_buff *)(_pkt))->tail) = (unsigned char *)((((struct sk_buff *)(_pkt))->data) + (_len)))
#endif

#define OS_DEV_ALLOC_SKB(_Pkt, _length) \
	do { \
		_Pkt = dev_alloc_skb(_length); \
	} while (0)

#define CB_OFF  10
#define CB_LEN 37

PNDIS_PACKET mt_os_net_pkt_alloc(void *dummy, int size);
PNDIS_PACKET mt_os_alloc_frag_pkt_buf(void *dummy, unsigned long len);
PNDIS_PACKET mt_os_copy_pkt(void *pNetDev, void *pPacket);
PNDIS_PACKET mt_os_clone_pkt(void *pNetDev, void *pPacket);
PNDIS_PACKET mt_os_clone_pkt_by_data(unsigned char MonitorOn, void *pkt, unsigned char *buf, unsigned long sz);
PNDIS_PACKET mt_os_duplicate_pkt_vlan(
	void *pNetDev,
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	unsigned char *pHeader802_3,
	unsigned int HdrLen,
	unsigned char *pData,
	unsigned long DataSize,
	unsigned char *TPID);
void wlan_802_11_to_802_3_packet_ap(
	void *pNetDev,
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	void *pRxPacket,
	unsigned char *pHeader802_3,
	unsigned char *pData,
	unsigned long DataSize,
	unsigned char *TPID);
void wlan_802_11_to_802_3_packet_sta(
		void *pNetDev,
		void *pRxPacket,
		unsigned char *pHeader802_3,
		unsigned char *pData,
		unsigned long DataSize);
PNDIS_PACKET mt_os_expand_pkt(
	PNDIS_PACKET pPacket,
	unsigned int ext_head_len,
	unsigned int ext_tail_len);
unsigned char mt_os_vlan_8023_header_copy(
	unsigned short VLAN_VID,
	unsigned short VLAN_Priority,
	unsigned char *pHeader802_3,
	unsigned int HdrLen,
	unsigned char *pData,
	unsigned char *TPID);

void mt_os_pkt_protocol_assign(void *net_pkt);
void mt_os_pkt_pull_rcsum(void *skb_, unsigned int len);
void mt_os_pkt_reset(void *skb_, int reset_mac_hdr);
void mt_kfree_skb(PNDIS_PACKET skb);
void mt_os_pkt_set_dest_type(void *skb_);
void mt_os_pkt_set_protocol(void *skb_, u8 *header, u32 header_len);
void mt_os_pkt_set_ip_summed(void *skb_, int ip_summed);
bool mt_os_pkt_set_rps_hash(void *skb_, int rps_per_round);

void mt_os_pkt_clear_info(void *skb_);
void mt_os_pkt_send(void *skb_, void *napi, int *pSend);
void *mt_os_pkt_get_wdev(void *skb_);
int mt_os_get_pkt_len(void *skb_);
void mt_os_802_11_radiotap_packet(
	void *skb_, void *netdev_);

#endif /* __MT_OSAL_PACKET_H_ */

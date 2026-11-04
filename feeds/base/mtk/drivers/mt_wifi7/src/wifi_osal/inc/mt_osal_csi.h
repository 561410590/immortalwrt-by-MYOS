/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_CSI_H_
#define __MT_OSAL_CSI_H_
//#ifdef CFG_SUPPORT_CSI

#include <linux/netlink.h>
#include <net/genetlink.h>


enum CSI_NL_ATTR {
	CSI_ATTR_UNSPEC = 0,
/*for nest header*/
	CSI_ATTR_DATA_HEADER,
	CSI_ATTR_CHAIN_HEADER,
/*for msg reply*/
	CSI_ATTR_REPORT_MSG,
/*for comcon info*/
	CSI_ATTR_TS = 4,
	CSI_ATTR_RA,
	CSI_ATTR_TA,
	CSI_ATTR_BAND_IDX,
	CSI_ATTR_CBW,
	CSI_ATTR_DBW,
	CSI_ATTR_FRAME_MODE,
	CSI_ATTR_FRAME_TYPE,
	CSI_ATTR_CHAIN_NUM, /*total chain num for this pkt*/
	CSI_ATTR_CH_IDX,
	CSI_ATTR_ERR,
	CSI_ATTR_MCS_RATE,
	CSI_ATTR_EXTRA_INFO,
	CSI_ATTR_CHANNEL,
	CSI_ATTR_CSI_LEN,	/*subcarrier num*/
	CSI_ATTR_PKT_IDX = 19,
/*for chain info*/
	CSI_ATTR_CHAIN_IDX,
	CSI_ATTR_RSSI,
	CSI_ATTR_SNR,
	CSI_ATTR_NOISE,
	CSI_ATTR_AGC_CDOE,
	CSI_ATTR_FREQ_OFFSET,
	CSI_ATTR_TX_IDX,
	CSI_ATTR_RX_IDX,
	CSI_ATTR_I,
	CSI_ATTR_Q,
	__CSI_ATTR_MAX,
};

enum CSI_DATA_TLV_TAG {
	CSI_DATA_VER,
	CSI_DATA_TYPE,
	CSI_DATA_TS,
	CSI_DATA_RSSI,
	CSI_DATA_SNR,
	CSI_DATA_DBW,
	CSI_DATA_CH_IDX,
	CSI_DATA_TA,
	CSI_DATA_I,
	CSI_DATA_Q,
	CSI_DATA_EXTRA_INFO,
	CSI_DATA_RSVD1,
	CSI_DATA_RSVD2,
	CSI_DATA_RSVD3,
	CSI_DATA_RSVD4,
	CSI_DATA_TX_IDX,
	CSI_DATA_RX_IDX,
	CSI_DATA_FRAME_MODE,
	CSI_DATA_H_IDX,
	CSI_DATA_RX_RATE,
	CSI_DATA_PKT_SN,
	CSI_DATA_TR_STREAM,
	CSI_DATA_TLV_TAG_NUM,
};

enum {
    CSI_OPS_UNSPEC,
 	CSI_OPS_REPORT,
    CSI_OPS_MAX,
};

enum {
    CSI_PROC_DATA,
 	CSI_PROC_DEBUG,
    CSI_PROC_MAX,
};

#define CSI_ATTR_MAX (__CSI_ATTR_MAX - 1)
#define CB_CHAIN_NUM	0
#define CB_CHAIN_IDX	1
#define CSI_MAX_RSVD1_COUNT 10
#define CSI_MAX_RSVD2_COUNT 10

/*csi netlink related*/
#define CSI_GENL_NAME "csi_genl"

#define CB_OFF  10
#define CB_LEN 37
#define SET_CSI_INFO_TO_SKB(p, offset, num)		((p)->cb[CB_OFF + offset] = (num))
#define GET_CSI_INFO_FROM_SKB(p, offset)		((p)->cb[CB_OFF + offset])
/*antenna item parse*/
#define BITS(m, n)                       (~(BIT(m)-1) & ((BIT(n) - 1) | BIT(n)))
#define PARSE_CHAIN_IDX(item) (item & BITS(0, 3))
#define PARSE_MAX_CHAIN_NUM(item) ((item & BITS(4, 7)) >> 4)
#define PARSE_CSI_SEQ_NUM(item) ((item & BITS(16, 31)) >> 16)
#define PARSE_CHAIN_END_FLAG(item) (item & BIT(15))
/*get tx or rx idx*/
#define GET_CSI_RX_IDX(TRX_IDX) (TRX_IDX & BITS(0, 15))
#define GET_CSI_TX_IDX(TRX_IDX) ((TRX_IDX & BITS(16, 31)) >> 16)

#define MAC_ADDR_LEN 6
#define FC_TYPE_DATA                2
#define SUBTYPE_QDATA               8

struct csi_msg_Info {
	u16 channel;
	u8 *bssid;
	u8 reqtype;
};

/*
 * CSI_DATA_T is used for representing
 * the CSI and other useful * information
 * for application usage
 */
struct CSI_DATA_T {
	u8 ucBw;
	u8 FWVer;
	u16 u2DataCount;
	u8 ucDbdcIdx;
	s8 cRssi;
	u8 ucSNR;
	u32 u4TimeStamp;
	u8 ucDataBw;
	u8 ucPrimaryChIdx;
	u8 aucTA[MAC_ADDR_LEN];
	u32 u4ExtraInfo;
	u16 ucRxMode;
	u16 rx_rate;		/*rx mcs*/
	s32 ai4Rsvd1[CSI_MAX_RSVD1_COUNT];
	s32 au4Rsvd2[CSI_MAX_RSVD2_COUNT];
	u8 ucRsvd1Cnt;
	u8 ucRsvd2Cnt;
	s32 i4Rsvd3;
	u8 ucRsvd4;
	u32 chain_info;	/*show chain idx, total chain fot this pkt, pkt counter*/
	u32 Tx_Rx_Idx;		/*show tx stream and rx stream idx of the packet*/
	u32 u4SegmentNum;	/*idx for BW, for BW >= 160, it need be divided into N * BW80*/
	u8 ucRemainLast;	/*set to 1: there is more BW80 transmiting */
	u16 pkt_sn;		/*for debug, sniffer check*/
	u8 tr_stream;	/*for debug, tx rx stream nuber, [0:3]-tx [4:7]-rx */
	s16 *ac2IData;	/*pointer to CSI I DATA*/
	s16 *ac2QData;	/*pointer to CSI Q DATA*/
};

typedef void *PNDIS_PACKET;

int mt_csi_genl_recv_doit(struct sk_buff *skb_temp, struct genl_info *info);
PNDIS_PACKET mt_make_csi_nlmsg_fragment(struct CSI_DATA_T *tmp_csi_data, void *info, void *msginfo);
int mt_send_msg_reply(void *info, int attrtype, char *cmd_msg);
int mtk_csi_genl_register(void);
int mtk_csi_genl_unregister(void);
void *mt_csi_proc_create(void *csi_proc_dir, char *data_name, void *data, int type);
u32 mt_procCSIDataPrepare(u8 *buf, struct CSI_DATA_T *prCSIData, u8 ucValue1, u8 ucValue2);
//#endif /* CFG_SUPPORT_CSI*/


#endif /* __MT_OSAL_CSI_H_ */

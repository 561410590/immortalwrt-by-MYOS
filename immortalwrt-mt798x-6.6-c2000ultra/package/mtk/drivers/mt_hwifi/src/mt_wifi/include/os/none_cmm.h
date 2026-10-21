/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attempt
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************

    Module Name:
	none_cmm.h

    Abstract:
	Common OS structure/definition in NONE OS whatever OS ABL.

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------
 */



#ifndef __NONE_CMM_H__
#define __NONE_CMM_H__

/* declare new chipset function here */
#define RTMP_DRV_NAME   "mt_drv"

/*****************************************************************************
 *	OS task related data structure and definitions
 ******************************************************************************/
typedef struct _MT_OS_TASK_ {
	char taskName[16];
	bool kthread_running;
	void *priv;
	unsigned char task_killed;
	void *kthread_task;
} MT_OS_TASK;

#define RTMP_OS_TASK				MT_OS_TASK

#define RTMP_OS_TASK_GET(__pTask)					\
	(__pTask)

#define RTMP_OS_TASK_DATA_GET(__pTask)					\
	((__pTask)->priv)

#define RTMP_OS_TASK_IS_KILLED(__pTask)					\
	((__pTask)->task_killed)

#define RTMP_OS_TASK_WAKE_UP(__pTask)					\
	WAKE_UP(pTask)

#define RTMP_OS_TASK_LEGALITY(__pTask)					\
	((__pTask)->kthread_task != NULL)

/*****************************************************************************
 * Timer related definitions and data structures.
 ******************************************************************************/
#define RTMP_OS_TIMER				OS_TIMER

/*****************************************************************************
 *	OS file operation related data structure definitions
 ******************************************************************************/
/* if you add any new type, please also modify os_file_open() */
#define MT_FILE_RDONLY      0x0F01
#define MT_FILE_WRONLY      0x0F02
#define MT_FILE_CREAT       0x0F03
#define MT_FILE_TRUNC       0x0F04
#define MT_FILE_APPEND      0x0F05

/*****************************************************************************
 *	OS semaphore related data structure and definitions
 ******************************************************************************/

/*****************************************************************************
 *	OS task related data structure and definitions
 ******************************************************************************/

/* rt_linux.h */
#define RTMP_NET_TASK_STRUCT		OS_NET_TASK_STRUCT
#define PRTMP_NET_TASK_STRUCT		POS_NET_TASK_STRUCT

#define RTMP_NAPI_STRUCT		OS_NAPI_STRUCT
#define RTMP_NET_STRUCT			OS_NET_STRUCT


typedef struct none_completion RTMP_OS_COMPLETION;


#define RTMP_OS_INIT_COMPLETION(__pCompletion)	\
	RtmpOsInitCompletion(__pCompletion)

#define RTMP_OS_REINIT_COMPLETION(__pCompletion)	\
	RtmpOsReinitCompletion(__pCompletion)

#define RTMP_OS_EXIT_COMPLETION(__pCompletion)	\
	RtmpOsExitCompletion(__pCompletion)

#define RTMP_OS_COMPLETE(__pCompletion)	\
	RtmpOsComplete(__pCompletion)

#define RTMP_OS_COMPLETION_DONE(__pCompletion)	\
	RtmpOsCompletionDone(__pCompletion)

#define RTMP_OS_WAIT_FOR_COMPLETION_TIMEOUT(__pCompletion, __Timeout)	\
	RtmpOsWaitForCompletionTimeout(__pCompletion, __Timeout)

#define RTMP_OS_TASKLET_SCHE(__pTasklet)			\
	RtmpOsTaskletSche(__pTasklet)

#define RTMP_OS_TASKLET_INIT(__pAd, __pTasklet, __pFunc, __Data)	\
	RtmpOsTaskletInit(__pTasklet, __pFunc, __Data, NULL)

#define RTMP_OS_TASKLET_KILL(__pTasklet)			\
	RtmpOsTaskletKill(__pTasklet)

#define RTMP_NET_TASK_DATA_ASSIGN(__pTasklet, __Data)		\
	RtmpOsTaskletDataAssign(__pTasklet, __Data)

#define RTMP_OS_NAPI_SCHE(__pNapi)	RtmpOsTaskletSche(__pNapi)

/*****************************************************************************
 *	OS definition related data structure and definitions
 ******************************************************************************/


#define RTMP_OS_PCI_VENDOR_ID			PCI_VENDOR_ID
#define RTMP_OS_PCI_DEVICE_ID			PCI_DEVICE_ID

typedef unsigned long            ra_dma_addr_t;

/******************************************************************************
 *	Others
 ******************************************************************************/
#define APCLI_IF_UP_CHECK(pAd, ifidx)	\
	(mt_os_net_dev_is_up((pAd)->StaCfg[(ifidx)].wdev.if_dev) == TRUE)

#ifdef RTMP_MAC_PCI
#ifdef MEMORY_OPTIMIZATION
#define MGMT_RING_SIZE          64/*32*/
#else
#ifdef WIFI_EAP_FEATURE
#define MGMT_RING_SIZE          512
#else /* WIFI_EAP_FEATURE */
#define MGMT_RING_SIZE          128
#endif /* !WIFI_EAP_FEATURE */
#endif
#endif

#define MAX_TX_PROCESS 512

struct work_struct {
	void			*funcPtr;
	unsigned long		data;
	unsigned long		taskStatus;
	char			taskName[8];
};

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct rcu_head {
	struct rcu_head *next;
	void *funcPtr;
};

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = NULL;
	n->pprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline void hlist_del_init(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

static inline void hlist_add_before(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

#define hlist_first_rcu(head)	(*((struct hlist_node **)(&(head)->first)))
#define hlist_next_rcu(node)	(*((struct hlist_node **)(&(node)->next)))
#define hlist_pprev_rcu(node)	(*((struct hlist_node **)((node)->pprev)))

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define hlist_entry_safe(ptr, type, member) \
			({ typeof(ptr) ____ptr = (ptr); \
			   ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
			})

#define hlist_for_each_entry_rcu(pos, head, member, cond...) \
		for (pos = hlist_entry_safe(hlist_first_rcu(head), \
				typeof(*(pos)), member); \
			pos; \
			pos = hlist_entry_safe(hlist_next_rcu( \
				&(pos)->member), typeof(*(pos)), member))

#define mt_os_lockless_hlist_for_each_entry(pos, head, member, cond...) \
	hlist_for_each_entry_rcu(pos, head, member, ##cond)

#define hlist_for_each_entry_safe(pos, n, head, member)	  \
	for (pos = hlist_entry_safe((head)->first, typeof(*pos), member);\
	     pos && ({ n = pos->member.next; 1; });		  \
	     pos = hlist_entry_safe(n, typeof(*pos), member))


#define RTMP_HLIST_HEAD struct hlist_head
#define RTMP_HLIST_NODE struct hlist_node
#define RtmpHashHeadInit(h) INIT_HLIST_HEAD(h)
#define RtmpHashNodeInit(n) INIT_HLIST_NODE(n)
#define RtmpHashListDel(n) hlist_del(n)
#define RtmpHashListDelInit(n) hlist_del_init(n)
#define RtmpHashListAddHead(n, h) hlist_add_head(n, h)
#define RtmpHashListAddBefore(n, h) hlist_add_before(n, h)
#define RtmpHashListForEach(p, h, m) hlist_for_each_entry(p, h, m)
#define RtmpHashListForEachSafe(p, n, h, m) hlist_for_each_entry_safe(p, n, h, m)

static inline int list_empty(const struct list_head *head)
{
	return (head->next == head);
}

typedef struct os_lock  {
	unsigned long	lock;
	unsigned long	flags;
} spinlock_t;
typedef spinlock_t OS_LOCK;

struct mt_net_dev_priv {
	int priv_flags;
	void *sys_handle;
	void *wifi_dev;
	unsigned char sniffer_mode;
};

struct mt_os_net_dev_op_hook {
	void *open;
	void *stop;
	void *xmit;
	void *ioctl;
	void *priv;
	void *wdev;
	void *set_mac_addr;
	void *init;
	void *uninit;
	void *wiress_dev;
	int priv_flags;
	unsigned int max_mtu;
	unsigned char devAddr[6];
	unsigned char devName[16];
	unsigned char needProtcted;
	u8 band_idx;
	u8 device_idx; /* physical device index */
};

enum mt_io_cmd {
	/* GET CMD */
	MT_IO_GET_NAME,
	MT_IO_GET_IF_HW_ADDR,
	MT_IO_GET_ESSID,
	MT_IO_GET_FREQ,
	MT_IO_GET_RATE,
	MT_IO_GET_AP,
	MT_IO_GET_TXPOW,
	MT_IO_GET_IW_STATS,
	MT_IO_GET_ETHER_STATS,
	MT_IO_GET_END, /* Please add new get cmd above MT_IO_GET_END. */
	/* SET CMD */
	MT_IO_SET_PRIV,
	MT_IO_PROC,
};

struct mt_os_wifi_ioctl_cmd_cfg {
	void *net_dev;
	void *data;
	unsigned int data_len;
	int priv_flags;
	enum mt_io_cmd cmd;
	unsigned short subcmd;
	unsigned short subid;
	char *name;
	int status;
	signed int s_int_val;
};

enum mt_proc_cmd_attr_type {
	proc_file_open = 1,
	proc_file_read,
	proc_file_release,
	proc_file_show,
};

struct mt_proc_priv {
	struct net_device *dev;
	unsigned short subid;
};

struct mt_os_net_dev_stats {
	void *stats; /* point to mac_ad->stats */

	unsigned long rx_packets; /* total packets received */
	unsigned long tx_packets; /* total packets transmitted */
	unsigned long rx_bytes;	/* total bytes received */
	unsigned long tx_bytes;	/* total bytes transmitted */
	unsigned long rx_errors; /* bad packets received */
	unsigned long tx_errors; /* packet transmit problems */
	unsigned long multicast; /* multicast packets received */
	unsigned long collisions;
	unsigned long rx_drop_count;
	unsigned long tx_drop_count;

	unsigned long rx_over_errors; /* receiver ring buff overflow */
	unsigned long rx_crc_errors; /* recved pkt with crc error */
	unsigned long rx_frame_errors; /* recv'd frame alignment error */
	unsigned long rx_fifo_errors; /* recv'r fifo overrun */
};

#ifdef CFG_SUPPORT_CSI
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

#define CSI_ATTR_MAX (__CSI_ATTR_MAX - 1)
#define CB_CHAIN_NUM	0
#define SET_CSI_INFO_TO_SKB(p, offset, num)		((p)->cb[CB_OFF + offset] = (num))
#define GET_CSI_INFO_FROM_SKB(p, offset)		((p)->cb[CB_OFF + offset])
#define CSI_MAX_RSVD1_COUNT 10
#define CSI_MAX_RSVD2_COUNT 10
/*get tx or rx idx*/
#define GET_CSI_RX_IDX(TRX_IDX) (TRX_IDX & BITS(0, 15))
#define GET_CSI_TX_IDX(TRX_IDX) ((TRX_IDX & BITS(16, 31)) >> 16)

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
#endif /* CFG_SUPPORT_CSI */

#endif /* __NONE_CMM_H__ */



/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_NET_H_
#define __MT_OSAL_NET_H_

struct mt_net_dev_priv {
	int priv_flags;
	void *sys_handle;
	void *wifi_dev;
	int (*ioctl)(void *net_dev, void *ioctl_cfg_);
	int (*init)(void *net_dev);
	int (*uninit)(void *net_dev);
	unsigned char sniffer_mode;
	unsigned char register_mode;
};

enum net_dev_mode {
	NET_DEV_MODE_NORMAL,
	NET_DEV_MODE_CFG80211,
};

enum mt_dev_features {
	MT_FEATURE_SEL_DUAL_BAND                = 0x00000001,
	MT_FEATURE_VLAN_GTK_INTF                = 0x00000002,
	MT_FEATURE_RFKILL_HW_SUPPORT            = 0x00000004,
	MT_FEATURE_MLO                          = 0x00000008,
	MT_FEATURE_11V_MBSS                     = 0x00000010,
	MT_FEATURE_4ADDR_STATION                = 0x00000020,
};

struct mt_os_net_dev_op_hook {
	void *open;
	void *stop;
	void *xmit;
	void *ioctl;
	void *get_stats;
	void *priv;
	void *wdev;
	void *set_mac_addr;
	void *init;
	void *uninit;
	void *mtk_cfg80211_ops;
	void *dev;
	int priv_flags;
	int dev_idx; /* mac_ad index */
	unsigned int max_mtu;
	unsigned int mtk_dev_features;
	unsigned int mtk_cfg80211_features;
	unsigned int mtk_phy_caps;
	u8 devAddr[ETH_ALEN];
	u8 devName[16];
	u8 permanent_addr[ETH_ALEN];	/* Factory default MAC address */
	u8 needProtcted;
	u8 reg_cfg80211;
	u8 band_idx;
	u8 device_idx; /* physical device index */
};

struct mt_os_net_dev_stats {
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

typedef int (*MT_OS_HARD_START_XMIT_FN)(struct sk_buff *skb, struct net_device *net_dev);

unsigned char mt_os_net_dev_is_up(void *net_dev);
void mt_os_net_dev_set_priv(void *net_dev, void *priv);
void mt_os_net_dev_set_wdev(void *net_dev, void *wdev);
void mt_os_net_dev_set_priv_flags(void *net_dev, int PrivFlags);
void mt_os_net_dev_set_sniffer_mode(void *net_dev, unsigned char sniffer_mode);
void mt_os_net_dev_set_fn_handle(
	void *net_dev, struct mt_os_net_dev_op_hook *net_dev_op_hook);
void mt_os_net_dev_set_addr(void *net_dev_, u8 *pMacAddr, char *dev_name);
int mt_os_net_dev_prepare_mac_change(void *net_dev_, void *addr, void *addr_str);
int mt_os_net_dev_commit_mac_change(void *net_dev_, void *addr, void *addr1, void *addr2);
void *mt_os_net_dev_get_priv(void *net_dev);
void *mt_os_net_dev_get_wdev(void *net_dev);
int mt_os_net_dev_get_priv_flags(void *net_dev);
unsigned char mt_os_net_dev_get_sniffer_mode(void *net_dev);
unsigned long mt_os_net_dev_get_state(void *net_dev);
unsigned int mt_os_net_dev_get_flags(void *net_dev);
unsigned int mt_os_net_dev_get_txq_num(void *net_dev);
unsigned long mt_os_net_dev_get_qstate(void *net_dev, unsigned int q_idx);
char *mt_os_net_dev_get_name(void *net_dev);
int mt_os_net_dev_get_if_idx(void *net_dev);
void mt_os_net_dev_free(void *net_dev);
void *mt_os_net_dev_eth_convert_search(void *net_dev_, unsigned char *data);
void *mt_os_net_dev_create(
	int mc_row_id,
	unsigned int *ioctl_if,
	int dev_type,
	int dev_num,
	int priv_mem_size,
	char *name_prefix,
	unsigned char add_suffix,
	unsigned char auto_suffix);
void mt_os_net_dev_detach(void *net_dev_, enum net_dev_mode unused);
void mt_os_net_dev_protect(bool lock_it);
int mt_os_net_dev_register(
	void *net_dev_,
	struct mt_os_net_dev_op_hook *net_dev_op_hook,
	unsigned char backport);
unsigned int mt_os_net_dev_get_iw_stats_size(void);

int mt_init_dummy_netdev(struct net_device *dev);

#endif /* __MT_OSAL_NET_H_ */

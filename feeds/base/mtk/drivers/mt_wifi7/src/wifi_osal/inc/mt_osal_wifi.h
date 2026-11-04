/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_WIFI_H_
#define __MT_OSAL_WIFI_H_

#define MAX_CMD_DATA_LEN 2048

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
	MT_IO_GET_STATISTICS,
	MT_IO_GET_SITESURVEY,
	MT_IO_GET_MAC_TABLE,
	MT_IO_GET_END, /* Please add new get cmd above MT_IO_GET_END. */
	/* SET CMD */
	MT_IO_SET_PRIV,
	/* CFG80211 */
	MT_IO_CFG80211_INIT,
	MT_IO_CFG80211_VNDR,
	MT_IO_CSI_DOIT,
	MT_IO_PROC,
	MT_IO_CFG80211_OPS,
	MT_IO_DIAG
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
	unsigned long u_long_val;
};

int mt_os_wifi_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd);
int mt_os_wifi_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			      void __user *data, int cmd);

#endif /* __MT_OSAL_WIFI_H_ */

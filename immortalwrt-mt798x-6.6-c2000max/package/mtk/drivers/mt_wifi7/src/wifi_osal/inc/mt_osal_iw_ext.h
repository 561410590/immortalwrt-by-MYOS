/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_WIRELESS_EXT_H_
#define __MT_OSAL_WIRELESS_EXT_H_

#include <linux/version.h>

#if (KERNEL_VERSION(6, 5, 0) > LINUX_VERSION_CODE)
#define MT_PRIV_IOCTL_SET		(SIOCIWFIRSTPRIV + 0x02)
#define MT_PRIV_IOCTL_STATISTICS	(SIOCIWFIRSTPRIV + 0x09)
#define MT_PRIV_IOCTL_GSITESURVEY	(SIOCIWFIRSTPRIV + 0x0D)
#define MT_PRIV_IOCTL_QUERY_BATABLE	(SIOCIWFIRSTPRIV + 0x16)
#define MT_PRIV_IOCTL_RX_STATISTICS	(SIOCIWFIRSTPRIV + 0x1B)
#define MT_PRIV_IOCTL_GET_DRIVER_INFO	(SIOCIWFIRSTPRIV + 0x1D)
#define MT_PRIV_IOCTL_PHY_STATE		(SIOCIWFIRSTPRIV + 0x21)

#else
enum {
	MT_PRIV_IOCTL_SET = SIOCDEVPRIVATE,
	MT_PRIV_IOCTL_PHY_STATE,
	MT_PRIV_IOCTL_GSITESURVEY,
	MT_PRIV_IOCTL_GET_DRIVER_INFO,
	MT_PRIV_IOCTL_QUERY_BATABLE,
	MT_PRIV_IOCTL_STATISTICS,
	MT_PRIV_IOCTL_RX_STATISTICS,
};

#endif

struct mt_os_iw_stats {
	void *stats;		/* point to pAd->iw_stats */
	unsigned char qual;
	unsigned char level;
	unsigned char noise;
	unsigned char updated;
};

void mt_os_iw_reg_handlers(struct net_device *net_dev);
int mt_os_get_priv_tab(void *wrqin_);

#endif /* __MT_OSAL_WIRELESS_EXT_H_ */

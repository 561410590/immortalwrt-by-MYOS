/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


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
#include <linux/wireless.h>
#include <net/iw_handler.h>

#define SUBMODULE "mt_osal_wireless_ext"
#include <mt_osal_debug.h>

#include <mt_osal_wifi.h>
#include <mt_osal_net.h>
#include <mt_osal_module.h>

#ifdef CONFIG_WIRELESS_EXT
#include <mt_osal_iw_ext.h>
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
static const struct iw_ioctl_description mt_iw_standard_ioctl[] = {
	[IW_IOCTL_IDX(SIOCSIWCOMMIT)] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[IW_IOCTL_IDX(SIOCGIWNAME)] = {
		.header_type	= IW_HEADER_TYPE_CHAR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWNWID)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[IW_IOCTL_IDX(SIOCGIWNWID)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWFREQ)] = {
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[IW_IOCTL_IDX(SIOCGIWFREQ)] = {
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWMODE)] = {
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[IW_IOCTL_IDX(SIOCGIWMODE)] = {
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWSENS)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWSENS)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWRANGE)] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[IW_IOCTL_IDX(SIOCGIWRANGE)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_range),
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWPRIV)] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[IW_IOCTL_IDX(SIOCGIWPRIV)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct iw_priv_args),
		.max_tokens	= 16,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[IW_IOCTL_IDX(SIOCSIWSTATS)] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[IW_IOCTL_IDX(SIOCGIWSTATS)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_statistics),
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWSPY)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr),
		.max_tokens	= IW_MAX_SPY,
	},
	[IW_IOCTL_IDX(SIOCGIWSPY)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) +
				  sizeof(struct iw_quality),
		.max_tokens	= IW_MAX_SPY,
	},
	[IW_IOCTL_IDX(SIOCSIWTHRSPY)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct iw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[IW_IOCTL_IDX(SIOCGIWTHRSPY)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct iw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[IW_IOCTL_IDX(SIOCSIWAP)] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[IW_IOCTL_IDX(SIOCGIWAP)] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWMLME)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_mlme),
		.max_tokens	= sizeof(struct iw_mlme),
	},
	[IW_IOCTL_IDX(SIOCGIWAPLIST)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) +
				  sizeof(struct iw_quality),
		.max_tokens	= IW_MAX_AP,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[IW_IOCTL_IDX(SIOCSIWSCAN)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= 0,
		.max_tokens	= sizeof(struct iw_scan_req),
	},
	[IW_IOCTL_IDX(SIOCGIWSCAN)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_SCAN_MAX_DATA,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[IW_IOCTL_IDX(SIOCSIWESSID)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[IW_IOCTL_IDX(SIOCGIWESSID)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[IW_IOCTL_IDX(SIOCSIWRATE)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWRATE)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWRTS)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWRTS)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWFRAG)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWFRAG)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWTXPOW)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWTXPOW)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWRETRY)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWRETRY)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWENCODE)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_EVENT | IW_DESCR_FLAG_RESTRICT,
	},
	[IW_IOCTL_IDX(SIOCGIWENCODE)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT,
	},
	[IW_IOCTL_IDX(SIOCSIWPOWER)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWPOWER)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWGENIE)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IW_IOCTL_IDX(SIOCGIWGENIE)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IW_IOCTL_IDX(SIOCSIWAUTH)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCGIWAUTH)] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[IW_IOCTL_IDX(SIOCSIWENCODEEXT)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  IW_ENCODING_TOKEN_MAX,
	},
	[IW_IOCTL_IDX(SIOCGIWENCODEEXT)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  IW_ENCODING_TOKEN_MAX,
	},
	[IW_IOCTL_IDX(SIOCSIWPMKSA)] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_pmksa),
		.max_tokens	= sizeof(struct iw_pmksa),
	},
};
static const unsigned int mt_iw_standard_ioctl_num = ARRAY_SIZE(mt_iw_standard_ioctl);

static int mt_iw_handler(struct net_device *dev, struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra);
static iw_handler mt_iw_handlers[] = {
	mt_iw_handler,	/* SIOCSIWCOMMIT */
	mt_iw_handler,	/* SIOCGIWNAME */
	mt_iw_handler,	/* SIOCSIWNWID */
	mt_iw_handler,	/* SIOCGIWNWID */
	mt_iw_handler,	/* SIOCSIWFREQ */
	mt_iw_handler,	/* SIOCGIWFREQ */
	mt_iw_handler,	/* SIOCSIWMODE */
	mt_iw_handler,	/* SIOCGIWMODE */
	mt_iw_handler,	/* SIOCSIWSENS */
	mt_iw_handler,	/* SIOCGIWSENS */
	mt_iw_handler,	/* SIOCSIWRANGE */
	mt_iw_handler,	/* SIOCGIWRANGE */
	mt_iw_handler,	/* SIOCSIWPRIV */
	mt_iw_handler,	/* SIOCGIWPRIV */
	mt_iw_handler,	/* SIOCSIWSTATS */
	mt_iw_handler,	/* SIOCGIWSTATS */
	mt_iw_handler,	/* SIOCSIWSPY */
	mt_iw_handler,	/* SIOCGIWSPY */
	mt_iw_handler,	/* SIOCSIWTHRSPY */
	mt_iw_handler,	/* SIOCGIWTHRSPY */
	mt_iw_handler,	/* SIOCSIWAP */
	mt_iw_handler,	/* SIOCGIWAP */
	mt_iw_handler,	/* SIOCSIWMLME */
	mt_iw_handler,	/* SIOCGIWAPLIST */
	mt_iw_handler,	/* SIOCSIWSCAN */
	mt_iw_handler,	/* SIOCGIWSCAN */
	mt_iw_handler,	/* SIOCSIWESSID */
	mt_iw_handler,	/* SIOCGIWESSID */
	mt_iw_handler,	/* SIOCSIWNICKN */
	mt_iw_handler,	/* SIOCGIWNICKN */
	mt_iw_handler,	/* 0x8B1E */
	mt_iw_handler,	/* 0x8B1F */
	mt_iw_handler,	/* SIOCSIWRATE */
	mt_iw_handler,	/* SIOCGIWRATE */
	mt_iw_handler,	/* SIOCSIWRTS */
	mt_iw_handler,	/* SIOCGIWRTS */
	mt_iw_handler,	/* SIOCSIWFRAG */
	mt_iw_handler,	/* SIOCGIWFRAG */
	mt_iw_handler,	/* SIOCSIWTXPOW */
	mt_iw_handler,	/* SIOCGIWTXPOW */
	mt_iw_handler,	/* SIOCSIWRETRY */
	mt_iw_handler,	/* SIOCGIWRETRY */
	mt_iw_handler,	/* SIOCSIWENCODE */
	mt_iw_handler,	/* SIOCGIWENCODE */
	mt_iw_handler,	/* SIOCSIWPOWER */
	mt_iw_handler,	/* SIOCGIWPOWER */
	mt_iw_handler,	/* SIOCSIWMODUL */
	mt_iw_handler,	/* SIOCGIWMODUL */
	mt_iw_handler,	/* SIOCSIWGENIE */
	mt_iw_handler,	/* SIOCGIWGENIE */
	mt_iw_handler,	/* SIOCSIWAUTH */
	mt_iw_handler,	/* SIOCGIWAUTH */
	mt_iw_handler,	/* SIOCSIWENCODEEXT */
	mt_iw_handler,	/* SIOCGIWENCODEEXT */
	mt_iw_handler,	/* SIOCSIWPMKSA */
};

#ifdef CONFIG_WEXT_PRIV
static iw_handler mt_iw_priv_handler[] = {
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x00 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x01 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x02 */
	NULL,           /* SIOCIWFIRSTPRIV + 0x03 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x04 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x05 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x06 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x07 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x08 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x09 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0a */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0b */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0c */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0d */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0e */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x0f */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x10 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x11 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x12 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x13 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x14 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x15 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x16 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x17 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x18 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x19 */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x1a */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x1b */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x1c */
	mt_iw_handler,  /* SIOCIWFIRSTPRIV + 0x1d */
	NULL,           /* SIOCIWFIRSTPRIV + 0x1e */
	mt_iw_handler	/* SIOCIWFIRSTPRIV + 0x1f */
};
#endif /* CONFIG_WEXT_PRIV */
#endif /* (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE) */

#ifdef CONFIG_WEXT_PRIV
struct iw_priv_args mt_iw_priv_tab[] = {
	{
		MT_PRIV_IOCTL_SET,
		/* 1024 --> 1024 + 512 */
		/* larger size specific to allow 64 ACL MAC addresses to be set up all at once. */
		IW_PRIV_TYPE_CHAR | 1536, 0,
		"set"
	},
	{
		MT_PRIV_IOCTL_PHY_STATE,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"phystate"
	},
	{
		MT_PRIV_IOCTL_GSITESURVEY,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"get_site_survey"
	},
	{
		MT_PRIV_IOCTL_GET_DRIVER_INFO,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"get_driverinfo"
	},
	{
		MT_PRIV_IOCTL_QUERY_BATABLE,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"get_ba_table"
	},
	{
		MT_PRIV_IOCTL_STATISTICS,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"stat"
	},
	{
		MT_PRIV_IOCTL_RX_STATISTICS,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
		"rx"
	},
};
#endif /* CONFIG_WEXT_PRIV */

#if WIRELESS_EXT >= 12
static struct iw_statistics *mt_os_iw_ext_get_wireless_stats(
	struct net_device *net_dev);
#endif /* WIRELESS_EXT >= 12 */

const struct iw_handler_def mt_iw_handler_def = {
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	.standard = mt_iw_handlers,
	.num_standard = ARRAY_SIZE(mt_iw_handlers),
#endif

#ifdef CONFIG_WEXT_PRIV
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	.private = mt_iw_priv_handler,
	.num_private = ARRAY_SIZE(mt_iw_priv_handler),
#endif
	.private_args = (struct iw_priv_args *) mt_iw_priv_tab,
	.num_private_args = ARRAY_SIZE(mt_iw_priv_tab),
#endif /* CONFIG_WEXT_PRIV */
#if IW_HANDLER_VERSION >= 7
	.get_wireless_stats = mt_os_iw_ext_get_wireless_stats,
#endif
};

#ifdef CONFIG_WEXT_PRIV
/* size (in bytes) of the various private data types */
static const char mt_iw_priv_type_size[] = {
	0, /* IW_PRIV_TYPE_NONE */
	1, /* IW_PRIV_TYPE_BYTE */
	1, /* IW_PRIV_TYPE_CHAR */
	0, /* Not defined */
	sizeof(__u32), /* IW_PRIV_TYPE_INT */
	sizeof(struct iw_freq),  /* IW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr), /* IW_PRIV_TYPE_ADDR */
	0, /* Not defined */
};

static int mt_os_iw_ext_get_priv_size(__u16 args)
{
	int	num = args & IW_PRIV_SIZE_MASK;
	int	type = (args & IW_PRIV_TYPE_MASK) >> 12;

	return num * mt_iw_priv_type_size[type];
}

static int mt_os_iw_ext_adjust_priv_size(
	__u16 args, struct iw_point *iwp)
{
	int	num = iwp->length;
	int	max = args & IW_PRIV_SIZE_MASK;
	int	type = (args & IW_PRIV_TYPE_MASK) >> 12;

	if (max < num)
		num = max;

	return num * mt_iw_priv_type_size[type];
}

static int mt_os_iw_ext_priv_descr_and_size(
	struct net_device *dev,
	unsigned int cmd,
	const struct iw_priv_args **descrp)
{
	const struct iw_priv_args *descr;
	int i, extra_size;

	descr = NULL;
	for (i = 0; i < dev->wireless_handlers->num_private_args; i++) {
		if (cmd == dev->wireless_handlers->private_args[i].cmd) {
			descr = &dev->wireless_handlers->private_args[i];
			break;
		}
	}

	extra_size = 0;
	if (descr) {
		if (IW_IS_GET(cmd)) {
			/* size of get arguments */
			extra_size = mt_os_iw_ext_get_priv_size(descr->get_args);

			if ((descr->get_args & IW_PRIV_SIZE_FIXED) &&
			   (extra_size <= IFNAMSIZ))
				extra_size = 0;
		}
	}
	*descrp = descr;
	return extra_size;
}
#endif /* CONFIG_WEXT_PRIV */

static int mt_iw_handler(
	struct net_device *dev,
	struct iw_request_info *info,
	union iwreq_data *wrqu,
	char *extra)
{
	int ret;
	struct iwreq *wreq = container_of(wrqu, struct iwreq, u);
	const struct iw_ioctl_description *standard_descr;
	unsigned short data_len = 0;

	if (dev->netdev_ops->ndo_do_ioctl) {
		ret = dev->netdev_ops->ndo_do_ioctl(dev, (struct ifreq *)wreq, info->cmd);
	} else {
		return -EOPNOTSUPP;
	}

	if (ret || !IW_IS_GET(info->cmd) || extra == NULL)
		return ret;

	/* fill info to extra for get command */
	if (info->cmd < SIOCIWFIRSTPRIV) {
		if (IW_IOCTL_IDX(info->cmd) >= mt_iw_standard_ioctl_num)
			return -EOPNOTSUPP;
		standard_descr = &(mt_iw_standard_ioctl[IW_IOCTL_IDX(info->cmd)]);
		data_len = (unsigned short)(wrqu->data.length * standard_descr->token_size);
		if (standard_descr->header_type == IW_HEADER_TYPE_POINT && data_len != 0)
			ret = copy_from_user(extra, wrqu->data.pointer, data_len);
	} else {
#ifdef CONFIG_WEXT_PRIV
		int extra_size = 0;
		const struct iw_priv_args *private_descr;

		extra_size = mt_os_iw_ext_priv_descr_and_size(dev, info->cmd, &private_descr);
		if (extra_size != 0) {
			if (!(private_descr->get_args & IW_PRIV_SIZE_FIXED))
				extra_size = mt_os_iw_ext_adjust_priv_size(
					private_descr->get_args, &wrqu->data);
			ret = copy_from_user(extra, wrqu->data.pointer, extra_size);
		}
#endif /* CONFIG_WEXT_PRIV */
	}

	return ret;
}

#if WIRELESS_EXT >= 12
/* This function will be called when query /proc */
static struct iw_statistics *mt_os_iw_ext_get_wireless_stats(
	struct net_device *net_dev)
{
	struct iw_statistics *pStats;
	struct mt_net_dev_priv *priv_info = NULL;
	struct mt_os_iw_stats DrvIwStats, *pDrvIwStats = &DrvIwStats;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!priv_info)
		return NULL;
	if (!priv_info->ioctl)
		return NULL;

	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->data = (void *)pDrvIwStats;
	pIoctlConfig->data_len = sizeof(*pDrvIwStats);
	memset(pDrvIwStats, 0, pIoctlConfig->data_len);
	pIoctlConfig->cmd = MT_IO_GET_IW_STATS;
	if (priv_info->ioctl((void *)net_dev, pIoctlConfig))
		return NULL;

	pStats = (struct iw_statistics *)(pDrvIwStats->stats);
	if (pStats) {
		pStats->status = 0; /* Status - device dependent for now */
		pStats->qual.updated = 1;     /* Flags to know if updated */
#ifdef IW_QUAL_DBM
		pStats->qual.updated |= IW_QUAL_DBM;	/* Level + Noise are dBm */
#endif /* IW_QUAL_DBM */
		pStats->qual.qual = pDrvIwStats->qual;
		pStats->qual.level = pDrvIwStats->level;
		pStats->qual.noise = pDrvIwStats->noise;
		pStats->discard.nwid = 0;     /* Rx : Wrong nwid/essid */
		pStats->miss.beacon = 0;      /* Missed beacons/superframe */
	}
	return pStats;
}
#endif /* WIRELESS_EXT */

void mt_os_iw_reg_handlers(
	struct net_device *net_dev)
{
	net_dev->wireless_handlers = &mt_iw_handler_def;
#if (WIRELESS_EXT < 21) && (WIRELESS_EXT >= 12)
	net_dev->get_wireless_stats = mt_os_iw_ext_get_wireless_stats;
#endif

}
EXPORT_SYMBOL(mt_os_iw_reg_handlers);

int mt_os_get_priv_tab(void *wrqin_)
{
	struct iwreq *wrqin = (struct iwreq *)wrqin_;
	int status = 0;

	if (wrqin->u.data.pointer) {
#if (KERNEL_VERSION(5, 0, 0) > LINUX_VERSION_CODE)
		if (access_ok(VERIFY_WRITE, wrqin->u.data.pointer, sizeof(mt_iw_priv_tab)) != true)
#else
		if (access_ok(wrqin->u.data.pointer, sizeof(mt_iw_priv_tab)) != true)
#endif
			return -EFAULT;

		if ((ARRAY_SIZE(mt_iw_priv_tab)) <= wrqin->u.data.length) {
			wrqin->u.data.length = ARRAY_SIZE(mt_iw_priv_tab);

			if (copy_to_user(
				wrqin->u.data.pointer, mt_iw_priv_tab, sizeof(mt_iw_priv_tab)))
				status = -EFAULT;
		} else
			status = -E2BIG;
	} else
		status = -EFAULT;
	return status;
}
#else
int mt_os_get_priv_tab(void *wrqin_)
{
	return -EFAULT;
}
#endif /* CONFIG_WIRELESS_EXT */

DECLARE_SUB_MODULE_INIT(mt_osal_iw_ext_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_iw_ext_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_iw_ext_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_iw_ext_exit);

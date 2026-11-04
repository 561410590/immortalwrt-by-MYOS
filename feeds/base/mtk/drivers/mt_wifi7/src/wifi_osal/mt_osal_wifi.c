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
#include <linux/wireless.h>
#include <linux/uaccess.h>

#define SUBMODULE "mt_osal_wifi"
#include <mt_osal_debug.h>
#include <mt_osal_wifi.h>
#include <mt_osal_net.h>
#include <mt_osal_iw_ext.h>
#include <mt_osal_module.h>

#define MT_INT_APCLI	0x0400
#define MT_PRIV_IOCTL_GET_MAC_TABLE_STRUCT	(SIOCIWFIRSTPRIV + 0x1f)

int mt_os_wifi_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd)
{
	struct iwreq *wrqin = (struct iwreq *)rq;
	int status = 0;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	struct mt_net_dev_priv *priv_info = NULL;
	struct iw_point *erq;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!priv_info)
		return -ENETDOWN;
	if (!priv_info->ioctl)
		return -ENETDOWN;
	memset(&IoctlConfig, 0, sizeof(IoctlConfig));
	pIoctlConfig->data = NULL;
	pIoctlConfig->status = 0;
	pIoctlConfig->net_dev = net_dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->cmd = cmd;
	pIoctlConfig->subcmd = 0;
	pIoctlConfig->name = net_dev->name;
	pIoctlConfig->s_int_val = 0;
	switch(cmd) {
	case SIOCGIWNAME:
		pIoctlConfig->data = wrqin->u.name;
		pIoctlConfig->data_len = sizeof(wrqin->u.name);
		pIoctlConfig->cmd = MT_IO_GET_NAME;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		break;
	case SIOCGIFHWADDR:
		pIoctlConfig->data = wrqin->u.name;
		pIoctlConfig->data_len = sizeof(wrqin->u.name);
		pIoctlConfig->cmd = MT_IO_GET_IF_HW_ADDR;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		break;
	case SIOCGIWESSID:
		erq = &wrqin->u.essid;
		pIoctlConfig->cmd = MT_IO_GET_ESSID;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		if (status)
			break;
		erq->length = pIoctlConfig->data_len;
		erq->flags = 1;
		if (erq->pointer && erq->length) {
			if (copy_to_user(
				erq->pointer, pIoctlConfig->data, erq->length))
				status = -EFAULT;
		}
		break;
	case SIOCGIWFREQ:
		pIoctlConfig->cmd = MT_IO_GET_FREQ;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		if (status)
			break;
		wrqin->u.freq.m = pIoctlConfig->s_int_val;
		wrqin->u.freq.e = 0;
		wrqin->u.freq.i = 0;
		break;
	case SIOCGIWRATE:
		pIoctlConfig->cmd = MT_IO_GET_RATE;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		if (status)
			break;
		wrqin->u.bitrate.value = pIoctlConfig->u_long_val;
		wrqin->u.bitrate.disabled = 0;
		break;
	case SIOCGIWAP:
		pIoctlConfig->cmd = MT_IO_GET_AP;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		if (status)
			break;
		wrqin->u.ap_addr.sa_family = ARPHRD_ETHER;
		if (pIoctlConfig->data)
			memcpy(wrqin->u.ap_addr.sa_data, pIoctlConfig->data, ETH_ALEN);
		else
			memset(wrqin->u.ap_addr.sa_data, 0, ETH_ALEN);
		break;
	case SIOCGIWMODE:
		if (mt_os_net_dev_get_priv_flags(net_dev) == MT_INT_APCLI)
			wrqin->u.mode = IW_MODE_INFRA;		/* ApCli Mode. */
		else
			wrqin->u.mode = IW_MODE_MASTER;		/* AP Mode. */
		break;
	case SIOCGIWTXPOW:
		pIoctlConfig->cmd = MT_IO_GET_TXPOW;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		if (status)
			break;
		wrqin->u.txpower.value = pIoctlConfig->s_int_val;
		wrqin->u.txpower.disabled = 0;/* Disable the feature */
		wrqin->u.txpower.flags = 0;/* dBm */
		wrqin->u.txpower.fixed = 0;/* Hardware should not use auto select */
		break;
	case SIOCGIWPRIV:
		status = mt_os_get_priv_tab(wrqin);
		break;
	case MT_PRIV_IOCTL_SET:
		pIoctlConfig->cmd = MT_IO_SET_PRIV;
		pIoctlConfig->subcmd = wrqin->u.data.flags;
#if (KERNEL_VERSION(5, 0, 0) > LINUX_VERSION_CODE)
		if (access_ok(VERIFY_READ, wrqin->u.data.pointer, wrqin->u.data.length) == true)
#else
		if (access_ok(wrqin->u.data.pointer, wrqin->u.data.length) == true)
#endif
		{
			pIoctlConfig->data_len = wrqin->u.data.length;
			if (wrqin->u.data.length) {
				pIoctlConfig->data = kmalloc(wrqin->u.data.length + 1, GFP_ATOMIC);
				if (pIoctlConfig->data) {
					if (copy_from_user(
						pIoctlConfig->data,
						wrqin->u.data.pointer,
						wrqin->u.data.length)) {
						kfree(pIoctlConfig->data);
						return -EFAULT;
					}
				} else
					return -ENOMEM;
			}
			else
				pIoctlConfig->data = NULL;
			status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		}
		if (status) {
			kfree(pIoctlConfig->data);
			break;
		}
		if (wrqin->u.data.length != pIoctlConfig->data_len)
			wrqin->u.data.length = pIoctlConfig->data_len;
		kfree(pIoctlConfig->data);
		break;
	case MT_PRIV_IOCTL_STATISTICS:
		pIoctlConfig->cmd = MT_IO_GET_STATISTICS;
		pIoctlConfig->data = wrqin;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		break;
	case MT_PRIV_IOCTL_GSITESURVEY:
		pIoctlConfig->cmd = MT_IO_GET_SITESURVEY;
		pIoctlConfig->data = wrqin;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		break;
	case MT_PRIV_IOCTL_GET_MAC_TABLE_STRUCT:
		pIoctlConfig->cmd = MT_IO_GET_MAC_TABLE;
		pIoctlConfig->data = wrqin;
		status = priv_info->ioctl((void *)net_dev, pIoctlConfig);
		break;
	default:
		status = -EOPNOTSUPP;
		break;
	}

	return status;
}
EXPORT_SYMBOL(mt_os_wifi_ioctl);

#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
int mt_os_wifi_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			      void __user *data, int cmd)
{
	return mt_os_wifi_ioctl(dev, ifr, cmd);
}
EXPORT_SYMBOL(mt_os_wifi_siocdevprivate);
#endif

DECLARE_SUB_MODULE_INIT(mt_osal_wifi_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_wifi_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_wifi_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_wifi_exit);

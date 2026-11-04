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

#define SUBMODULE "mt_osal_net"
#include <mt_osal_debug.h>
#include <mt_osal_net.h>
#include <mt_osal_iw_ext.h>
#include <mt_osal_wifi.h>
#include <mt_osal_cfg80211.h>
#include <mt_osal_module.h>

#define MT_MOD_INC_USE_COUNT() \
	do {\
		if (!try_module_get(THIS_MODULE)) { \
			MT_DEBUG_TRACE("cannot reserve module\n"); \
			return -1; \
		}	\
	} while (0)

#define MT_MOD_DEC_USE_COUNT() module_put(THIS_MODULE);

static inline int mt_os_snprintf_error(size_t size, int res)
{
	return res < 0 || (unsigned int) res >= size;
}

static void mt_os_net_dev_alloc(
	struct net_device **new_dev_p,
	unsigned int priv_data_size)
{
	struct net_device *new_dev = NULL;

	MT_DEBUG_TRACE("Allocate a net device with private data size=%d!\n", priv_data_size);
#if LINUX_VERSION_CODE <= 0x20402	/* Red Hat 7.1 */
	new_dev = alloc_netdev(priv_data_size, "eth%d", ether_setup);
#else
	new_dev = alloc_etherdev(priv_data_size);
#endif /* LINUX_VERSION_CODE */

	if (new_dev)
		*new_dev_p = new_dev;
	else
		*new_dev_p = NULL;
}

static void mt_os_net_dev_ops_alloc(void **net_dev_ops)
{
	*net_dev_ops = (void *)vmalloc(sizeof(struct net_device_ops));

	if (*net_dev_ops)
		memset(*net_dev_ops, 0, sizeof(struct net_device_ops));
}

static bool mt_os_net_dev_request_name(
	int mc_row_id,
	unsigned int *ioctl_if,
	struct net_device *net_dev,
	char *pPrefixStr,
	int devIdx,
	bool auto_suffix)
{
	struct net_device *existNetDev;
	char suffixName[IFNAMSIZ];
	char desiredName[IFNAMSIZ];
	int ifNameIdx, prefixLen, slotNameLen;
	int ret;

	prefixLen = strlen(pPrefixStr);
	if ((prefixLen >= IFNAMSIZ))
		return false;

	for (ifNameIdx = devIdx; ifNameIdx < 32; ifNameIdx++) {
		ret = snprintf(suffixName, sizeof(suffixName), "%d", ifNameIdx);
		if (mt_os_snprintf_error(sizeof(suffixName), ret) != 0) {
			MT_DEBUG_TRACE("%s(%d): snprintf error!\n", __func__, __LINE__);
			return false;
		}

		slotNameLen = strlen(suffixName);
		if ((slotNameLen + prefixLen) >= IFNAMSIZ)
			return false;

		if (auto_suffix)
			ret = snprintf(desiredName, sizeof(desiredName),
				"%s%s", pPrefixStr, suffixName);
		else
			ret = snprintf(desiredName, sizeof(desiredName), "%s", pPrefixStr);

		if (mt_os_snprintf_error(sizeof(desiredName), ret) != 0) {
			MT_DEBUG_TRACE("%s(%d): snprintf error!\n", __func__, __LINE__);
			return false;
		}

		existNetDev = dev_get_by_name(dev_net(net_dev), &desiredName[0]);

		if (existNetDev == NULL)
			break;
		else if (auto_suffix == false && existNetDev) {
			ifNameIdx = 32;	/* Tend to leave loop then return failure */
			MT_DEBUG_TRACE("Cannot request DevName with string(%s) from OS!\n",
				pPrefixStr);
		}

		/*
		 *  every time dev_get_by_name is called, and it has returned a valid struct
		 *  net_device*, dev_put should be called afterwards, because otherwise the
		 *  machine hangs when the device is unregistered (since dev->refcnt > 1).
		 */
		if (existNetDev)
			dev_put(existNetDev);
	}

	if (ifNameIdx < 32) {
		*ioctl_if = ifNameIdx;
		strlcpy(&net_dev->name[0], &desiredName[0], sizeof(net_dev->name));
		return true;
	} else {
		MT_DEBUG_TRACE(
		"Cannot request DevName with preifx(%s) and in range(0~32) as suffix from OS!\n",
		pPrefixStr);
		return false;
	}
}

#ifdef BACKPORT_NOSTDINC
static void mt_os_net_dev_ops_free(struct net_device *net_dev)
{
	void *net_dev_ops = NULL;

	if (net_dev == NULL) {
		MT_DEBUG_TRACE("pNetDev is NULL!\n");
		return;
	}

	net_dev_ops = (void *)net_dev->netdev_ops;
	if (net_dev_ops)
		vfree(net_dev_ops);
}
#endif /* BACKPORT_NOSTDINC */

static int mt_os_net_dev_main_if_open(
	struct net_device *net_dev)
{
	struct mt_net_dev_priv *priv_info = NULL;
	int ret;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!priv_info)
		return -1;
	if (!priv_info->init)
		return -1;
	ret = priv_info->init(net_dev);
	if (ret)
		return -1;

	MT_MOD_INC_USE_COUNT();
	netif_start_queue(net_dev);
	netif_carrier_on(net_dev);
	netif_wake_queue(net_dev);
	return 0;
}

static int mt_os_net_dev_main_if_stop(
	struct net_device *net_dev)
{
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	netif_carrier_off(net_dev);
	netif_stop_queue(net_dev);

	if (priv_info->uninit)
		priv_info->uninit(net_dev);

	MT_MOD_DEC_USE_COUNT();
	return 0; /* close ok */
}

static struct net_device_stats *mt_os_net_dev_get_ether_stats(
	struct net_device *net_dev)
{
	struct net_device_stats *pStats = &net_dev->stats;
	struct mt_net_dev_priv *priv_info = NULL;
	struct mt_os_net_dev_stats DrvStats, *pDrvStats = &DrvStats;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!priv_info)
		return pStats;
	if (!priv_info->ioctl)
		return pStats;

	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->data = (void *)pDrvStats;
	pIoctlConfig->data_len = sizeof(*pDrvStats);
	memset(pDrvStats, 0, pIoctlConfig->data_len);
	pIoctlConfig->cmd = MT_IO_GET_ETHER_STATS;
	if (priv_info->ioctl((void *)net_dev, pIoctlConfig))
		return pStats;
	pStats->rx_packets = pDrvStats->rx_packets;
	pStats->tx_packets = pDrvStats->tx_packets;
	pStats->rx_bytes = pDrvStats->rx_bytes;
	pStats->tx_bytes = pDrvStats->tx_bytes;
	pStats->rx_errors = pDrvStats->rx_errors;
	pStats->tx_errors = pDrvStats->tx_errors;
	pStats->rx_dropped = pDrvStats->rx_drop_count;
	pStats->tx_dropped = pDrvStats->tx_drop_count;
	pStats->multicast = pDrvStats->multicast;
	pStats->collisions = pDrvStats->collisions;
	pStats->rx_length_errors = 0;
	pStats->rx_over_errors = pDrvStats->rx_over_errors;
	pStats->rx_crc_errors = 0;
	pStats->rx_frame_errors = pDrvStats->rx_frame_errors;
	pStats->rx_fifo_errors = pDrvStats->rx_fifo_errors;
	pStats->rx_missed_errors = 0;
	/* detailed tx_errors */
	pStats->tx_aborted_errors = 0;
	pStats->tx_carrier_errors = 0;
	pStats->tx_fifo_errors = 0;
	pStats->tx_heartbeat_errors = 0;
	pStats->tx_window_errors = 0;
	/* for cslip etc */
	pStats->rx_compressed = 0;
	pStats->tx_compressed = 0;
	return pStats;
}

unsigned char mt_os_net_dev_is_up(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;

	if ((dev == NULL) || !(dev->flags & IFF_UP))
		return false;

	return true;
}
EXPORT_SYMBOL(mt_os_net_dev_is_up);

void mt_os_net_dev_set_priv(void *net_dev, void *priv)
{
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->sys_handle = (void *)priv;
	priv_info->priv_flags = 0;
}
EXPORT_SYMBOL(mt_os_net_dev_set_priv);

void mt_os_net_dev_set_wdev(void *net_dev, void *wdev)
{
	struct mt_net_dev_priv *priv_info;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->wifi_dev = wdev;
}
EXPORT_SYMBOL(mt_os_net_dev_set_wdev);

void mt_os_net_dev_set_priv_flags(void *net_dev, int PrivFlags)
{
	struct mt_net_dev_priv *priv_info;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->priv_flags = PrivFlags;
}
EXPORT_SYMBOL(mt_os_net_dev_set_priv_flags);

void mt_os_net_dev_set_sniffer_mode(void *net_dev, unsigned char sniffer_mode)
{
	struct mt_net_dev_priv *priv_info;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->sniffer_mode = sniffer_mode;
}
EXPORT_SYMBOL(mt_os_net_dev_set_sniffer_mode);

void mt_os_net_dev_set_fn_handle(
	void *net_dev, struct mt_os_net_dev_op_hook *net_dev_op_hook)
{
	struct mt_net_dev_priv *priv_info;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->ioctl = net_dev_op_hook->ioctl;
	priv_info->init = net_dev_op_hook->init;
	priv_info->uninit = net_dev_op_hook->uninit;
}
EXPORT_SYMBOL(mt_os_net_dev_set_fn_handle);

void mt_os_net_dev_set_addr(void *net_dev_, u8 *pMacAddr, char *dev_name)
{
	struct net_device *net_dev = (struct net_device *)net_dev_;

	if (dev_name != NULL) {
		memset(dev_name, 0, 16);
		memmove(dev_name, net_dev->name, strlen(net_dev->name));
	}

#if (KERNEL_VERSION(5, 4, 251) <= LINUX_VERSION_CODE)
	dev_addr_set(net_dev, pMacAddr);
#else
	memmove(net_dev->dev_addr, pMacAddr, 6);
#endif
}
EXPORT_SYMBOL(mt_os_net_dev_set_addr);

int mt_os_net_dev_prepare_mac_change(void *net_dev_, void *addr, void *addr_str)
{
	struct net_device *dev = (struct net_device *)net_dev_;
	struct sockaddr *sa = addr;
	int ret = 0;

	if (addr_str)
		ret = snprintf(addr_str, 18, "%pM", sa->sa_data);
	if (ret != 17)
		return -17;

	ret = eth_prepare_mac_addr_change(dev, addr);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(mt_os_net_dev_prepare_mac_change);

int mt_os_net_dev_commit_mac_change(void *net_dev_, void *addr, void *addr1, void *addr2)
{
	struct net_device *dev = (struct net_device *)net_dev_;
	struct sockaddr *sa = addr;

	memcpy(addr1, sa->sa_data, 6);
	memcpy(addr2, sa->sa_data, 6);

	eth_commit_mac_addr_change(dev, addr);

	return 0;
}
EXPORT_SYMBOL(mt_os_net_dev_commit_mac_change);

void *mt_os_net_dev_get_priv(void *net_dev)
{
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	return priv_info->sys_handle;
}
EXPORT_SYMBOL(mt_os_net_dev_get_priv);

void *mt_os_net_dev_get_wdev(void *net_dev)
{
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	return priv_info->wifi_dev;
}
EXPORT_SYMBOL(mt_os_net_dev_get_wdev);

int mt_os_net_dev_get_priv_flags(void *net_dev)
{
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	return priv_info->priv_flags;
}
EXPORT_SYMBOL(mt_os_net_dev_get_priv_flags);

unsigned char mt_os_net_dev_get_sniffer_mode(void *net_dev)
{
	struct mt_net_dev_priv *priv_info;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	return priv_info->sniffer_mode;
}
EXPORT_SYMBOL(mt_os_net_dev_get_sniffer_mode);

unsigned long mt_os_net_dev_get_state(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;

	return dev->state;
}
EXPORT_SYMBOL(mt_os_net_dev_get_state);

unsigned int mt_os_net_dev_get_flags(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;

	return dev->flags;
}
EXPORT_SYMBOL(mt_os_net_dev_get_flags);

unsigned int mt_os_net_dev_get_txq_num(void *net_dev)
{
#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
	struct net_device *dev = (struct net_device *)net_dev;

	return dev->num_tx_queues;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mt_os_net_dev_get_txq_num);

unsigned long mt_os_net_dev_get_qstate(void *net_dev, unsigned int q_idx)
{
#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
	struct netdev_queue *que = netdev_get_tx_queue((struct net_device *)net_dev, q_idx);

	if (que)
		return que->state;
	else
		return ~0x0;

#else
	return ~0x0;
#endif
}
EXPORT_SYMBOL(mt_os_net_dev_get_qstate);

char *mt_os_net_dev_get_name(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;

	return dev->name;
}
EXPORT_SYMBOL(mt_os_net_dev_get_name);

int mt_os_net_dev_get_if_idx(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;

	return dev->ifindex;
}
EXPORT_SYMBOL(mt_os_net_dev_get_if_idx);

void mt_os_net_dev_free(void *net_dev)
{
	struct net_device *dev = (struct net_device *)net_dev;
	void *tmp_ptr;

	if (net_dev == NULL) {
		MT_DEBUG_TRACE("net_dev is NULL!\n");
		return;
	}

	tmp_ptr = (void *)dev->netdev_ops;
	free_netdev(dev);
	if (tmp_ptr)
		vfree(tmp_ptr);
}
EXPORT_SYMBOL(mt_os_net_dev_free);

void *mt_os_net_dev_eth_convert_search(void *net_dev_, unsigned char *data)
{
	struct net_device *net_dev = (struct net_device *)net_dev_;
	struct net *net;

	net = dev_net(net_dev);
	BUG_ON(!net);
	for_each_netdev(net, net_dev) {
		if ((net_dev->type == ARPHRD_ETHER)
			&& (!memcmp(net_dev->dev_addr, &data[6], net_dev->addr_len)))
			break;
	}

	return (void *)net_dev;
}
EXPORT_SYMBOL(mt_os_net_dev_eth_convert_search);

void *mt_os_net_dev_create(
	int mc_row_id,
	unsigned int *ioctl_if,
	int dev_type,
	int dev_num,
	int priv_mem_size,
	char *name_prefix,
	unsigned char add_suffix,
	unsigned char auto_suffix)
{
	struct net_device_ops *net_dev_ops = NULL;
	struct net_device *net_dev = NULL;
	bool status;

	/* allocate a new network device */
	mt_os_net_dev_alloc(&net_dev, priv_mem_size);
	if (!net_dev) {
		MT_DEBUG_TRACE("Allocate network device fail (%s)...\n", name_prefix);
		return NULL;
	}
	mt_os_net_dev_ops_alloc((void **)&net_dev_ops);

	if (!net_dev_ops) {
		MT_DEBUG_TRACE("Allocate net device ops fail!\n");
		mt_os_net_dev_free(net_dev);
		return NULL;
	}
	MT_DEBUG_TRACE("Allocate net device ops success!\n");
	net_dev->netdev_ops = net_dev_ops;

	/* find a available interface name, max 32 interfaces */
	if (!add_suffix) {
		memset(&net_dev->name[0], 0, IFNAMSIZ);
		strlcpy(&net_dev->name[0], name_prefix, sizeof(net_dev->name));
	} else {
		status = mt_os_net_dev_request_name(
			mc_row_id, ioctl_if, net_dev, name_prefix, dev_num, auto_suffix);

		if (!status) {
			/* error! no any available ra name can be used! */
			MT_DEBUG_TRACE("Assign inf name (%s with suffix 0~32) failed\n",
				name_prefix);
			mt_os_net_dev_free(net_dev);
			return NULL;
		}
	}
	MT_DEBUG_TRACE("The name of the new %s interface is %s\n",
			 name_prefix, net_dev->name);

	return net_dev;

}
EXPORT_SYMBOL(mt_os_net_dev_create);

void mt_os_net_dev_detach(void *net_dev_, enum net_dev_mode unused)
{
	struct net_device *net_dev = (struct net_device *)net_dev_;
	struct mt_net_dev_priv *priv_info = NULL;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);

	MT_DEBUG_TRACE("register mode(=%d)\n", priv_info->register_mode);
	switch(priv_info->register_mode) {
		case NET_DEV_MODE_NORMAL:
			unregister_netdevice(net_dev);
			break;
#ifdef BACKPORT_NOSTDINC
		case NET_DEV_MODE_CFG80211:
			cfg80211_unregister_netdevice(net_dev);
			break;
#endif /* BACKPORT_NOSTDINC */
		default:
			break;
	}
}
EXPORT_SYMBOL(mt_os_net_dev_detach);

void mt_os_net_dev_protect(bool lock_it)
{
	if (lock_it)
		rtnl_lock();
	else
		rtnl_unlock();
}
EXPORT_SYMBOL(mt_os_net_dev_protect);

int mt_os_net_dev_register(
	void *net_dev_,
	struct mt_os_net_dev_op_hook *net_dev_op_hook,
	unsigned char backport)
{
	struct net_device *net_dev = (struct net_device *)net_dev_;
	struct net_device_ops *net_dev_ops = (struct net_device_ops *)net_dev->netdev_ops;
	int ret = 0, rtnl_locked = false;
	struct mt_net_dev_priv *priv_info = NULL;
	struct wireless_dev *wdev;

	priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	priv_info->register_mode = backport;

	MT_DEBUG_TRACE("backport=%d\n", backport);
	/* If we need hook some callback function to the net device structrue, now do it. */
	if (net_dev_op_hook) {
		mt_os_net_dev_set_priv_flags(net_dev, net_dev_op_hook->priv_flags);
		mt_os_net_dev_set_fn_handle(net_dev, net_dev_op_hook);
		if (net_dev_op_hook->open)
			net_dev_ops->ndo_open = net_dev_op_hook->open;
		else
			net_dev_ops->ndo_open = mt_os_net_dev_main_if_open;
		if (net_dev_op_hook->stop)
			net_dev_ops->ndo_stop = net_dev_op_hook->stop;
		else
			net_dev_ops->ndo_stop = mt_os_net_dev_main_if_stop;
		net_dev_ops->ndo_start_xmit =
			(MT_OS_HARD_START_XMIT_FN)(net_dev_op_hook->xmit);
		net_dev_ops->ndo_do_ioctl = mt_os_wifi_ioctl;
#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
		/* 5.15+ routes SIOCDEVPRIVATE..+15 here; iwpriv uses 0x89F0+ on 6.6. */
		net_dev_ops->ndo_siocdevprivate = mt_os_wifi_siocdevprivate;
#endif
		if (net_dev_op_hook->set_mac_addr)
			net_dev_ops->ndo_set_mac_address = net_dev_op_hook->set_mac_addr;
		net_dev_ops->ndo_get_stats = mt_os_net_dev_get_ether_stats;
		/* copy the net device mac address to the net_device structure. */
#if (KERNEL_VERSION(5, 4, 251) <= LINUX_VERSION_CODE)
		dev_addr_set(net_dev, net_dev_op_hook->devAddr);
#else
		memmove(net_dev->dev_addr, &net_dev_op_hook->devAddr[0], 6);
#endif
		rtnl_locked = net_dev_op_hook->needProtcted;
		if (net_dev_op_hook->max_mtu)
			net_dev->max_mtu = net_dev_op_hook->max_mtu;
#ifdef CONFIG_WIRELESS_EXT
		mt_os_iw_reg_handlers(net_dev);
#endif /* CONFIG_WIRELESS_EXT */
#ifdef CFG80211_SUPPORT
		if (net_dev_op_hook->reg_cfg80211)
			mt_cfg80211_register(net_dev_op_hook->dev, net_dev, net_dev_op_hook);
#endif /* CFG80211_SUPPORT */
	}

	net_dev_ops->ndo_validate_addr = NULL;

	if (backport) {
		wdev = net_dev->ieee80211_ptr;   
        if (!wdev)
      		return -EPERM;
#ifdef BACKPORT_NOSTDINC
		net_dev->needs_free_netdev = true;
		net_dev->priv_destructor = mt_os_net_dev_ops_free;
		if (rtnl_locked) {
			ret = cfg80211_register_netdevice(net_dev);
		} else {
			if (rtnl_lock_killable())
				return -EINTR;
			wiphy_lock(wdev->wiphy);
			ret = cfg80211_register_netdevice(net_dev);
			wiphy_unlock(wdev->wiphy);
			rtnl_unlock();
		}
#else
		MT_DEBUG_TRACE("backport shall not be true.\n");
#endif
	} else {
		if (rtnl_locked)
			ret = register_netdevice(net_dev);
		else
			ret = register_netdev(net_dev);
	}

	netif_stop_queue(net_dev);
	MT_DEBUG_TRACE("ret=%d\n", ret);

	return ret;
}
EXPORT_SYMBOL(mt_os_net_dev_register);

unsigned int mt_os_net_dev_get_iw_stats_size(void)
{
	return sizeof(struct iw_statistics);
}
EXPORT_SYMBOL(mt_os_net_dev_get_iw_stats_size);


int mt_init_dummy_netdev(struct net_device *dev)
{
	return init_dummy_netdev(dev);
}
EXPORT_SYMBOL(mt_init_dummy_netdev);

DECLARE_SUB_MODULE_INIT(mt_osal_net_init)
{
	MT_DEBUG_TRACE("loaded");
	return 0;
}
sub_module_init(mt_osal_net_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_net_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_net_exit);

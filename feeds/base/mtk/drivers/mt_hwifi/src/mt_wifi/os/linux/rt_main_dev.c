/*
 * Copyright (c) [2020], MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws.
 * The information contained herein is confidential and proprietary to
 * MediaTek Inc. and/or its licensors.
 * Except as otherwise provided in the applicable licensing terms with
 * MediaTek Inc. and/or its licensors, any reproduction, modification, use or
 * disclosure of MediaTek Software, and information contained herein, in whole
 * or in part, shall be strictly prohibited.
*/
/****************************************************************************
 ***************************************************************************

    Module Name:
    rt_main_dev.c

    Abstract:
    Create and register network interface.

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
*/

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"
#include <linux/ethtool.h>
/*---------------------------------------------------------------------*/
/* Private Variables Used                                              */
/*---------------------------------------------------------------------*/

RTMP_STRING *mac = "";		/* default 00:00:00:00:00:00 */

module_param(mac, charp, 0);
MODULE_PARM_DESC(mac, "mt_wifi: wireless mac addr");


#ifndef __DEFINE_LICENSE__
MODULE_LICENSE("Proprietary");
#endif

/*---------------------------------------------------------------------*/
/* Prototypes of Functions Used                                        */
/*---------------------------------------------------------------------*/

/* public function prototype */
int mt_wifi_close(VOID *net_dev);
int mt_wifi_open(VOID *net_dev);
int virtual_if_up_handler(VOID *dev);
int virtual_if_down_handler(VOID *dev);

/* private function prototype */

/*
 * ========================================================================
 * Routine Description:
 *    Close raxx interface.
 *
 * Arguments:
 *	*net_dev			the raxx interface pointer
 *
 * Return Value:
 *    0					Open OK
 *	otherwise			Open Fail
 *
 * Note:
 *	1. if open fail, kernel will not call the close function.
 *	2. Free memory for
 *		(1) Mlme Memory Handler:		MlmeHalt()
 *		(2) TX & RX:					RTMPFreeTxRxRingMemory()
 *		(3) BA Reordering:				ba_reordering_resource_release()
 * ========================================================================
 */
int main_virtual_if_close(IN struct net_device *net_dev)
{
	VOID *pAd = NULL;

	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO, "===> %s\n",
		RTMP_OS_NETDEV_GET_DEVNAME(net_dev));

	pAd = mt_os_net_dev_get_priv(net_dev);

	if (pAd == NULL)
		return 0;

	RTMP_OS_NETDEV_CARRIER_OFF(net_dev);
	RTMP_OS_NETDEV_STOP_QUEUE(net_dev);

#ifdef IFUP_IN_PROBE
#else
	VIRTUAL_IF_DOWN(pAd, net_dev);
#endif /* IFUP_IN_PROBE */

	VIRTUAL_IF_DEINIT(pAd, net_dev);

	RT_MOD_HNAT_DEREG(net_dev);
	RT_MOD_DEC_USE_COUNT();
	return 0; /* close ok */
}

/*
 * ========================================================================
 * Routine Description:
 *   Open raxx interface.
 *
 * Arguments:
 *	*net_dev			the raxx interface pointer
 *
 * Return Value:
 *   0					Open OK
 *	otherwise			Open Fail
 *
 * Note:
 *	1. if open fail, kernel will not call the close function.
 *	2. Free memory for
 *		(1) Mlme Memory Handler:		MlmeHalt()
 *		(2) TX & RX:					RTMPFreeTxRxRingMemory()
 *		(3) BA Reordering:				ba_reordering_resource_release()
 * ========================================================================
 */
int main_virtual_if_open(struct net_device *net_dev)
{
	VOID *pAd = NULL;
	struct wifi_dev *wdev = NULL;

	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO, "===> %s\n",
		RTMP_OS_NETDEV_GET_DEVNAME(net_dev));

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL)
		return 0;

#ifdef IFUP_IN_PROBE

	while (RTMP_DRIVER_IOCTL_SANITY_CHECK(pAd, NULL) != NDIS_STATUS_SUCCESS) {
		OS_WAIT(10);
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO,
			"Card not ready, NDIS_STATUS_SUCCESS!\n");
	}

#else

	if (VIRTUAL_IF_INIT(pAd, net_dev) != 0)
		return -1;

	if (VIRTUAL_IF_UP(pAd, net_dev) != 0)
		return -1;

#endif /* IFUP_IN_PROBE */
	wdev = wdev_search_by_netdev(pAd, net_dev);
	if (wdev == NULL) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR, "wdev fail!!!\n");
		return -1;
	}

	MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO,
		"dev=%s addr_assign_type=%d. if_addr=%pM\n", net_dev->name,
		net_dev->addr_assign_type, wdev->if_addr);

	/* Fix issue of "ethtool -P "command cannot get main interface's macaddr
	 * due to not set dev perm address in probe process */
	COPY_MAC_ADDR(net_dev->perm_addr, wdev->if_addr);

	RT_MOD_INC_USE_COUNT();
	RT_MOD_HNAT_REG(net_dev);
	netif_start_queue(net_dev);
	netif_carrier_on(net_dev);
	netif_wake_queue(net_dev);
	return 0;
}


/*
 * ========================================================================
 * Routine Description:
 *   Close raxx interface.
 *
 * Arguments:
 *	*net_dev			the raxx interface pointer
 *
 * Return Value:
 *    0					Open OK
 *	otherwise			Open Fail
 *
 * Note:
 *	1. if open fail, kernel will not call the close function.
 *	2. Free memory for
 *		(1) Mlme Memory Handler:		MlmeHalt()
 *		(2) TX & RX:					RTMPFreeTxRxRingMemory()
 *		(3) BA Reordering:				ba_reordering_resource_release()
 * ========================================================================
 */
int mt_wifi_close(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	VOID	*pAd = NULL;

	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO, "===>\n");
	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL)
		return 0;

#ifdef CFG_SUPPORT_CSI
	csi_support_deinit(pAd);
#endif


	RTMPDrvClose(pAd, net_dev);

#ifdef CONFIG_WLAN_SERVICE
	mt_service_close(pAd);
#endif /* CONFIG_WLAN_SERVICE */

#ifdef WIFI_MD_COEX_SUPPORT
	deinit_wifi_md_coex(pAd);
#endif /* WIFI_MD_COEX_SUPPORT */

	/*system down hook point*/
	WLAN_HOOK_CALL(WLAN_HOOK_SYS_DOWN, pAd, NULL);

	MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO, "<===\n");
	return 0;
}

/*
 * ========================================================================
 * Routine Description:
 *   Open raxx interface.
 *
 * Arguments:
 *	*net_dev			the raxx interface pointer
 *
 * Return Value:
 *    0					Open OK
 *	otherwise			Open Fail
 * ========================================================================
 */
int mt_wifi_open(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	VOID *pAd = NULL;
	int retval = 0;
	UINT32 OpMode;
#ifdef WARP_512_SUPPORT
	UINT8 Enable;
#endif

	if (sizeof(ra_dma_addr_t) < sizeof(dma_addr_t))
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR,
			"Fatal error for DMA address size!!!\n");

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL) {
		/* if 1st open fail, pAd will be free; */
		/*   So the net_dev->priv will be NULL in 2rd open */
		return -1;
	}

	RTMP_DRIVER_MCU_SLEEP_CLEAR(pAd);
	RTMP_DRIVER_OP_MODE_GET(pAd, &OpMode);
	/*system up hook point should before interrupt register*/
#ifndef CONFIG_PROFILE_OFF
#if defined(WHNAT_SUPPORT) || defined(MT7986) || defined(MT7916) || defined(MT7981)
	RTMPPreReadProfile(pAd);
#endif /* defined(WHNAT_SUPPORT) || defined(MT7986) || defined(MT7916) || defined(MT7981) */
#endif /* !CONFIG_PROFILE_OFF */
	WLAN_HOOK_CALL(WLAN_HOOK_SYS_UP, pAd, NULL);

#ifdef CONFIG_WLAN_SERVICE
	mt_service_open(pAd);
#endif /* CONFIG_WLAN_SERVICE */

#ifdef WARP_512_SUPPORT
	Enable = TRUE;
	WLAN_HOOK_CALL(WLAN_HOOK_WARP_512_SUPPORT, pAd, &Enable);
	if (((RTMP_ADAPTER *)pAd)->physical_dev.whnat_en) {
		if (Enable)
			((RTMP_ADAPTER *)pAd)->Warp512Support = TRUE;
		else
			((RTMP_ADAPTER *)pAd)->Warp512Support = FALSE;
	}
#endif

	/* Chip & other init */
	if (mt_wifi_init(pAd) == FALSE)
		goto err;
#ifdef MBSS_SUPPORT
	/*
	 *	the function can not be moved to RT2860_probe() even register_netdev()
	 *	is changed as register_netdevice().
	 *	Or in some PC, kernel will panic (Fedora 4)
	 */
	RT_CONFIG_IF_OPMODE_ON_AP(GET_OPMODE_FROM_PAD(pAd))
	{
	}
#endif /* MBSS_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	{
#ifndef IWCOMMAND_CFG80211_SUPPORT
		RT28xx_MSTA_Init(pAd, net_dev);
#endif /* IWCOMMAND_CFG80211_SUPPORT */
	}
#endif /* CONFIG_STA_SUPPORT */
#ifdef ACTION_MONITOR_SUPPORT
	RT28xx_Action_Mon_Init(pAd, net_dev);
#endif

#ifdef RT_CFG80211_SUPPORT
	cfg80211_drv_start(pAd);
#endif /* RT_CFG80211_SUPPORT */

#ifdef CONFIG_WLAN_SERVICE
	mt_service_init(pAd);
#endif /* CONFIG_WLAN_SERVICE */

	RTMPDrvOpen(pAd);
#ifdef VENDOR_FEATURE2_SUPPORT
	MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_DEBUG,
			 "Number of Packet Allocated in open = %lu\n", OS_NumOfPktAlloc);
	MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_DEBUG,
			 "Number of Packet Freed in open = %lu\n", OS_NumOfPktFree);
#endif /* VENDOR_FEATURE2_SUPPORT */


#ifdef WIFI_MD_COEX_SUPPORT
	init_wifi_md_coex(pAd);
#endif /* WIFI_MD_COEX_SUPPORT */

	return retval;
err:
	return -1;
}

int virtual_if_up_handler(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	struct _RTMP_ADAPTER *pAd = NULL;
	int retval = 0;
	struct wifi_dev *wdev = NULL;

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL) {
		retval = -1;
		return retval;
	}

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INIT_FAIL)) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR, "init fail!!!\n");
		retval = -1;
		return retval;
	}

	if (physical_device_get_band_en(pAd->physical_dev, hc_get_hw_band_idx(pAd)) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR, "disabled interface\n");
		retval = -1;
		return retval;
	}

	wdev = wdev_search_by_netdev(pAd, net_dev);
	if (wdev == NULL) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR, "wdev fail!!!\n");
		retval = -1;
		return retval;
	}

	wdev_if_up_down(pAd, wdev, TRUE);

#ifdef GREENAP_SUPPORT
		/* This function will check and update allow status */
		if (greenap_check_when_if_down_up(pAd) == FALSE)
			return retval;
#endif /* GREENAP_SUPPORT */

	if (VIRTUAL_IF_NUM(pAd) != 0) {
		if (wdev_do_open(wdev) != TRUE) {
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_START_UP);
			wdev_if_up_down(pAd, wdev, FALSE);
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR,
				"inf_up (idx %d) fail!!!\n",
				wdev->wdev_idx);
			retval = -1;
			return retval;
		}
	}

#ifdef WDS_SUPPORT
	if ((wdev->wdev_type == WDEV_TYPE_AP) && (wdev->func_idx == 0)) {
		RT28xx_WDS_Init(pAd, hc_get_hw_band_idx(pAd), dev);
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_DEBUG,
			"wds_if bound to main dev!\n");
	}
#endif	/* WDS_SUPPORT */

#ifdef CONFIG_6G_AFC_SUPPORT
		if (is_afc_stop() == TRUE)
			afc_initiate_request_to_daemon(pAd, wdev);
#endif /*CONFIG_6G_AFC_SUPPORT*/


	return retval;
}

int virtual_if_down_handler(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	struct _RTMP_ADAPTER *pAd = NULL;
	int retval = 0;
	struct wifi_dev *wdev = NULL;

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL) {
		retval = -1;
		return retval;
	}

	wdev = wdev_search_by_netdev(pAd, net_dev);
	if (wdev == NULL) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR, "wdev fail!!!\n");
		retval = -1;
		return retval;
	}

/*wdev->if_up_down_state should be mark false after wdev_do_close*/
	if (wdev_do_close(wdev) != TRUE) {
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO,
			"inf_down (idx %d) fail!!!\n",
			wdev->wdev_idx);
	}
	wdev_if_up_down(pAd, wdev, FALSE);

#ifdef GREENAP_SUPPORT
	greenap_check_when_if_down_up(pAd);
#endif /* GREENAP_SUPPORT */

#ifdef RT_CFG80211_SUPPORT
	pAd->cfg80211_ctrl.beaconIsSetFromHostapd = FALSE;
#endif

#ifdef LED_CONTROL_SUPPORT
	RTMPSetLED(pAd, LED_RADIO_OFF, HcGetBandByWdev(wdev));
#endif /* LED_CONTROL_SUPPORT */

	return retval;
}


INT virtual_if_init_handler(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	struct _RTMP_ADAPTER *pAd = NULL;
	int retval = 0;

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL) {
		retval = -1;
		return retval;
	}

	if (VIRTUAL_IF_NUM(pAd) == 0) {
		VIRTUAL_IF_INC(pAd);

#ifdef MT_DFS_SUPPORT
		/* Update bInitMbssZeroWait  */
		UPDATE_MT_INIT_ZEROWAIT_MBSS(pAd, TRUE);
#endif /* MT_DFS_SUPPORT */

		/* must use main net_dev */
		if (mt_wifi_open(pAd->net_dev) != 0) {
			VIRTUAL_IF_DEC(pAd);
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO,
					 "mt_wifi_open return fail!\n");
			return NDIS_STATUS_FAILURE;
		}
	} else
		VIRTUAL_IF_INC(pAd);

	return retval;
}

INT virtual_if_deinit_handler(VOID *dev)
{
	struct net_device *net_dev = (struct net_device *)dev;
	struct _RTMP_ADAPTER *pAd = NULL;
	int retval = 0;

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	if (pAd == NULL) {
		retval = -1;
		return retval;
	}

	if (VIRTUAL_IF_NUM(pAd) == 0) {
		mt_wifi_close(pAd->net_dev);
	}

	return retval;
}

/*
 * ========================================================================
 * Routine Description:
 *   The entry point for Linux kernel sent packet to our driver.
 *
 * Arguments:
 *   sk_buff *skb	the pointer refer to a sk_buffer.
 *
 * Return Value:
 *    0
 *
 * Note:
 *	This function is the entry point of Tx Path for OS delivery packet to
 *	our driver. You only can put OS-depened & STA/AP common handle procedures
 *	in here.
 * ========================================================================
 */
int rt28xx_packet_xmit(void *pkt)
{
	struct wifi_dev *wdev;
	int status = 0;

	wdev = mt_os_pkt_get_wdev(pkt);
	if (wdev == NULL) {
		MTWF_DBG(NULL, DBG_CAT_TX, CATTX_DATA, DBG_LVL_ERROR, "wdev is NULL!\n");
		ASSERT(wdev);
		return status;
	}


	status = RTMPSendPackets((NDIS_HANDLE)wdev, (PPNDIS_PACKET) & pkt, 1,
						   mt_os_get_pkt_len(pkt), mt_os_net_dev_eth_convert_search);
	return status;
}


/*
 * ========================================================================
 * Routine Description:
 *    Send a packet to WLAN.
 *
 * Arguments:
 *    skb_p           points to our adapter
 *    dev_p           which WLAN network interface
 *
 * Return Value:
 *   0: transmit successfully
 *   otherwise: transmit fail
 *
 * ========================================================================
 */
int rt28xx_send_packets(PNDIS_PACKET pkt, void *ndev)
{
	mt_os_pkt_clear_info(pkt);
	WLAN_HOOK_CALL(WLAN_HOOK_TX_OS_TO_DRIVER, NULL, pkt);

	if (!(RTMP_OS_NETDEV_STATE_RUNNING(ndev))) {
		RELEASE_NDIS_PACKET(NULL, pkt, NDIS_STATUS_FAILURE);
		return 0;
	}

	MEM_DBG_PKT_ALLOC_INC(pkt);
	return rt28xx_packet_xmit(pkt);
}

/*
 * ========================================================================
 *
 *  Routine Description:
 *	set mac address
 *
 *  Arguments:
 *	net_dev		Pointer to net_device
 *	addr		the mac addr need to set
 *  Return Value:
 *	0: success 1: failed
 * ========================================================================
 */
#ifdef HOSTAPD_MBSS_SUPPORT
int rt28xx_change_mac(void *dev, void *addr)
{
	VOID *pAd = NULL;
	struct wifi_dev *wdev = NULL;
	int ret;
	CHAR addr_str[18];

	if (!dev)
		return -EINVAL;

	GET_PAD_FROM_NET_DEV(pAd, dev);
	if (!pAd)
		return -EINVAL;

	wdev = mt_os_net_dev_get_wdev(dev);
	if (!wdev)
		return -EINVAL;

	if (wdev->if_up_down_state == TRUE)
		return  -EBUSY;

	ret = mt_os_net_dev_prepare_mac_change(dev, addr, addr_str);
	if (ret)
		return -EINVAL;

	if (wdev->wdev_type == WDEV_TYPE_AP) {
		ret = RT_CfgSetMacAddress(pAd, addr_str, wdev->func_idx, OPMODE_AP);
		if (ret != TRUE) {
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
				"RT_CfgSetMacAddress failed!\n");
			return  -EADDRNOTAVAIL;
		}

	} else if (wdev->wdev_type == WDEV_TYPE_STA) {
		ret = RT_CfgSetMacAddress(pAd, addr_str, wdev->func_idx, OPMODE_STA);
		if (ret != TRUE) {
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
				"RT_CfgSetMacAddress failed!\n");
			return -EADDRNOTAVAIL;
		}
	}

	mt_os_net_dev_commit_mac_change(dev, addr, wdev->if_addr, wdev->bssid);

	return 0;
}
#endif /*HOSTAPD_MBSS_SUPPORT*/

BOOLEAN RtmpPhyNetDevExit(VOID *pAd, PNET_DEV net_dev)
{
	/*remove cfg */
	wpf_exit(pAd);
#ifdef CONFIG_AP_SUPPORT
#ifdef WDS_SUPPORT
	/* remove all WDS virtual interfaces. */
	RT28xx_WDS_Remove(pAd);
#endif /* WDS_SUPPORT */
#ifdef ACTION_MONITOR_SUPPORT
	RT28xx_Action_Mon_Remove(pAd);
#endif
#ifdef MBSS_SUPPORT
	RT28xx_MBSS_Remove(pAd);
#endif /* MBSS_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
	RT28xx_MSTA_Remove(pAd);
#endif
	/* Unregister network device */
	if (net_dev != NULL) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR,
				 "RtmpOSNetDevDetach(): RtmpOSNetDeviceDetach(), dev->name=%s!\n", net_dev->name);
		mt_os_net_dev_protect(1);
		RtmpOSNetDevDetach(net_dev);
		mt_os_net_dev_protect(0);
#ifdef RT_CFG80211_SUPPORT
		mt_cfg80211_unregister(net_dev);
#endif /* RT_CFG80211_SUPPORT */
	}

	return TRUE;
}


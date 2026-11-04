/****************************************************************************

    Module Name:
    rt_os_util.c

    Abstract:
	All functions provided from UTIL module are put here (OS independent).

    Revision History:
    Who        When          What
    ---------  ----------    ----------------------------------------------

***************************************************************************/

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "oid.h"
#include "oid_struct.h"
#include "rtmp_iface.h"


extern INT32 getLegacyOFDMMCSIndex(UINT8 MCS);

UINT32 RalinkRate[256] = {
	2,  4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108, 109, 110, 111, 112,/* CCK and OFDM */
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260,
	39, 78,  117, 156, 234, 312, 351, 390, /* BW 20, 800ns GI, MCS 0~23 */
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540,
	81, 162, 243, 324, 486, 648, 729, 810, /* BW 40, 800ns GI, MCS 0~23 */
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288,
	43, 87,  130, 173, 260, 317, 390, 433, /* BW 20, 400ns GI, MCS 0~23 */
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600,
	90, 180, 270, 360, 540, 720, 810, 900, /* BW 40, 400ns GI, MCS 0~23 */
	13, 26,   39,  52,  78, 104, 117, 130, 156, /* 11ac: 20Mhz, 800ns GI, MCS: 0~8 */
	27, 54,   81, 108, 162, 216, 243, 270, 324, 360, /*11ac: 40Mhz, 800ns GI, MCS: 0~9 */
	59, 117, 176, 234, 351, 468, 527, 585, 702, 780, /*11ac: 80Mhz, 800ns GI, MCS: 0~9 */
	14, 29,   43,  57,  87, 115, 130, 144, 173, /* 11ac: 20Mhz, 400ns GI, MCS: 0~8 */
	30, 60,   90, 120, 180, 240, 270, 300, 360, 400, /*11ac: 40Mhz, 400ns GI, MCS: 0~9 */
	65, 130, 195, 260, 390, 520, 585, 650, 780, 867, /*11ac: 80Mhz, 400ns GI, MCS: 0~9 */
	59 * 2, 117 * 2, 176 * 2, 234 * 2, 351 * 2, 468 * 2, 527 * 2, 585 * 2, 702 * 2, 780 * 2, /*11ac: 160Mhz, 800ns GI, MCS: 0~9 */
	65 * 2, 130 * 2, 195 * 2, 260 * 2, 390 * 2, 520 * 2, 585 * 2, 650 * 2, 780 * 2, 867 * 2, /*11ac: 160Mhz, 400ns GI, MCS: 0~9 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37
}; /* 3*3 */

char iee80211_mode_str[PHY_MODE_MAX][IFNAMSIZ] = {
"IEEE 802.11bg", "IEEE 802.11b", "IEEE 802.11a", "IEEE 802.11abg",
"IEEE 802.11g", "IEEE 802.11abgn", "IEEE 802.11n",  "IEEE 802.11gn",
"IEEE 802.11an","IEEE 802.11bgn", "IEEE 802.11agn", "IEEE 802.11n",
"IEEE 802.11nabg", "IEEE 802.11nag", "IEEE 802.11acan", "IEEE 802.11acan",
"IEEE 802.11ax", "IEEE 802.11ax", "IEEE 802.11ax", "IEEE 802.11ax",
"IEEE 802.11ax", "IEEE 802.11ax", "IEEE 802.11be", "IEEE 802.11be",
"IEEE 802.11be", "IEEE 802.11be", "IEEE 802.11be", "IEEE 802.11be"};

VOID RtmpDrvMaxRateGet(
	IN	VOID					*pReserved,
	IN	UINT8					MODE,
	IN	UINT8					ShortGI,
	IN	UINT8					BW,
	IN	UINT8					MCS,
	IN	UINT8					Antenna,
	OUT	UINT64					*pRate)
{
	int rate_index = 0;
#ifdef DOT11_VHT_AC

	if (MODE >= MODE_VHT) {
		if (BW == 0/*20Mhz*/) {
			rate_index = 112 +
						 ((UCHAR)ShortGI * 29) +
						 ((UCHAR)MCS);
		} else if (BW == 1/*40Mhz*/) {
			rate_index = 121 +
						 ((UCHAR)ShortGI * 29) +
						 ((UCHAR)MCS);
		} else if (BW == 2/*80Mhz*/) {
			rate_index = 131 +
						 ((UCHAR)ShortGI * 29) +
						 ((UCHAR)MCS);
		} else if (BW > 2/*160Mhz*/) {
			rate_index = (131 + 29 + 10) +
						 ((UCHAR)ShortGI * 10) +
						 ((UCHAR)MCS);
		}
	} else
#endif /* DOT11_VHT_AC */
#ifdef DOT11_N_SUPPORT
		if ((MODE >= MODE_HTMIX) && (MODE < MODE_VHT)) {
			/*           	rate_index = 16 + ((UCHAR)pHtPhyMode->field.BW *16) + ((UCHAR)pHtPhyMode->field.ShortGI *32) + ((UCHAR)pHtPhyMode->field.MCS);
			*/
			/* map back to 1SS MCS , multiply by antenna numbers later */
			if (MCS > 7)
				MCS %= 8;

			rate_index = 16 + ((UCHAR)BW * 24) + ((UCHAR)ShortGI * 48) + ((UCHAR)MCS);
		} else
#endif /* DOT11_N_SUPPORT */
			if (MODE == MODE_OFDM)
				rate_index = getLegacyOFDMMCSIndex(MCS) + 4;
			else
				rate_index = (UCHAR)(MCS);

	if (rate_index < 0)
		rate_index = 0;

	if (rate_index > 255)
		rate_index = 255;

	*pRate = (UINT64)RalinkRate[rate_index] * 500000;
#if defined(DOT11_VHT_AC) || defined(DOT11_N_SUPPORT)

	if (MODE >= MODE_HTMIX)
		*pRate *= Antenna;

#endif /* DOT11_VHT_AC */

#ifdef DOT11_HE_AX
	if (MODE >= MODE_HE) {
		/*ax tx */
		ULONG DataRate = 0;

		get_rate_he(MCS, BW, Antenna, 0, &DataRate);
		if (ShortGI == FIXED_GI_16_US)
			DataRate = (DataRate * 967) >> 10;
		if (ShortGI == FIXED_GI_32_US)
			DataRate = (DataRate * 870) >> 10;
		*pRate = DataRate;
		*pRate = *pRate * 1000000;
	}
#endif /*DOT11_HE_AX*/

#ifdef DOT11_EHT_BE
	if (MODE >= MODE_EHT) {
		/*be tx */
		ULONG DataRate = 0;

		get_rate_eht(MCS, BW, Antenna, 0, &DataRate);

		if (ShortGI == FIXED_GI_16_US)
			DataRate = (DataRate * 967) >> 10;
		if (ShortGI == FIXED_GI_32_US)
			DataRate = (DataRate * 870) >> 10;

		*pRate = DataRate;
		*pRate = *pRate * 1000000;
	}
#endif

	MTWF_DBG(NULL, DBG_CAT_MLME, CATMLME_WTBL, DBG_LVL_INFO,
		"MODE: %d shortGI: %d BW: %d MCS: %d Antenna num: %d  Rate = %llu\n",
		MODE, ShortGI, BW, MCS, Antenna, *pRate);
}


char *rtstrchr(char *s, int c)
{
	for (; *s != (char) c; ++s)
		if (*s == '\0')
			return NULL;

	return (char *) s;
}


VOID RtmpMeshDown(
	IN VOID *pDrvCtrlBK,
	IN BOOLEAN WaitFlag,
	IN BOOLEAN(*RtmpMeshLinkCheck)(IN VOID *pAd))
{
}


#ifdef ETH_CONVERT_SUPPORT
USHORT RtmpOsNetPrivGet(
	IN PNET_DEV pDev)
{
	return mt_os_net_dev_get_priv_flags(pDev);
}
#endif /* ETH_CONVERT_SUPPORT */


BOOLEAN RtmpOsCmdDisplayLenCheck(
	IN UINT32 LenSrc,
	IN UINT32 Offset)
{
	if (LenSrc > (IW_PRIV_SIZE_MASK - Offset))
		return FALSE;

	return TRUE;
}

char *RtmpOsGetNetDevName(VOID *pDev)
{
	return mt_os_net_dev_get_name(pDev);
}

UINT32 RtmpOsGetNetIfIndex(IN VOID *pDev)
{
	return mt_os_net_dev_get_if_idx(pDev);
}

void RtmpOSNetDevFree(VOID *pNetDev)
{
	mt_os_net_dev_free(pNetDev);
#ifdef VENDOR_FEATURE4_SUPPORT
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO,
			 "OS_NumOfMemAlloc = %ld, OS_NumOfMemFree = %ld\n",
			  OS_NumOfMemAlloc, OS_NumOfMemFree);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO,
			 "OS_NumOfVMemAlloc = %ld, OS_NumOfVMemFree = %ld\n",
			  OS_NumOfVMemAlloc, OS_NumOfVMemFree);
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef VENDOR_FEATURE2_SUPPORT
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_INFO,
			 "OS_NumOfPktAlloc = %ld, OS_NumOfPktFree = %ld\n",
			  OS_NumOfPktAlloc, OS_NumOfPktFree);
#endif /* VENDOR_FEATURE2_SUPPORT */
}

BOOLEAN RtmpOsStatsAlloc(
	IN VOID **ppIwStats)
{
#if WIRELESS_EXT >= 12
	unsigned int iw_stats_size = mt_os_net_dev_get_iw_stats_size();
	os_alloc_mem(NULL, (UCHAR **) ppIwStats, iw_stats_size);

	if ((*ppIwStats) == NULL) {
		return FALSE;
	}

	os_zero_mem((UCHAR *)*ppIwStats, iw_stats_size);
#endif
	return TRUE;
}

void RtmpOSNetDevDetach(void *pNetDev)
{
#if defined(CCN34_SPLIT_MAC_SUPPORT) && defined(ACTION_MONITOR_SUPPORT)
	/* ccn34 action monitor is not support cfg80211 interface for now */
	mt_os_net_dev_detach(pNetDev, NET_DEV_MODE_NORMAL);
#else /* defined(CCN34_SPLIT_MAC_SUPPORT) && defined(ACTION_MONITOR_SUPPORT) */
#if defined(RT_CFG80211_SUPPORT) && defined(BACKPORT_NOSTDINC)
	mt_os_net_dev_detach(pNetDev, NET_DEV_MODE_CFG80211);
#else
	mt_os_net_dev_detach(pNetDev, NET_DEV_MODE_NORMAL);
#endif
#endif /* !(defined(CCN34_SPLIT_MAC_SUPPORT) && defined(ACTION_MONITOR_SUPPORT)) */
}

/*******************************************************************************
 *	Task create/management/kill related functions.
 ******************************************************************************/
NDIS_STATUS RtmpOSTaskKill(MT_OS_TASK *pTask)
{
	if (!pTask) {
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_TASK, DBG_LVL_WARN,
			 "pTask is Null, return!!!\n");
		return NDIS_STATUS_FAILURE;
	}

	if (mt_os_task_kill(pTask))
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

NDIS_STATUS RtmpOSTaskAttach(
	MT_OS_TASK *pTask,
	RTMP_OS_TASK_CALLBACK fn,
	ULONG arg)
{
	if (mt_os_task_attach(pTask, fn, arg))
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

NDIS_STATUS RtmpOSTaskInit(
	RTMP_OS_TASK *pTask,
	RTMP_STRING *pTaskName,
	VOID *pPriv,
	LIST_HEADER *pTaskList,
	LIST_HEADER *pSemList)
{
	if (mt_os_task_init(pTask, pTaskName, pPriv))
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

BOOLEAN RtmpOSTaskWait(VOID *pReserved, MT_OS_TASK *pTask, INT32 *pStatus)
{
	return mt_os_task_wait(pReserved, pTask, pStatus);
}

BOOLEAN RtmpOSTaskWaitCond(
	VOID *pReserved, MT_OS_TASK *pTask, UINT32 u4Cond, INT32 *pStatus)
{
	return mt_os_task_wait_cond(pReserved, pTask, u4Cond, pStatus);
}

BOOLEAN RtmpOSTaskWaitEvent(VOID *pReserved, MT_OS_TASK *pTask, UINT32 u4Cond, INT32 *pStatus)
{
	return mt_os_task_wait_event(pReserved, pTask, u4Cond, pStatus);
}

VOID RtmpOSTaskWakeupCond(MT_OS_TASK *pTask, UINT32 u4Cond)
{
	mt_os_task_wake_up(pTask, u4Cond);
}

INT32  RtPrivIoctlSetVal(VOID)
{
	return (INT32)RTPRIV_IOCTL_SET;
}

UCHAR MtRandom8(VOID)
{
	ULONG i;
	UCHAR R, Result;
	static ULONG shift_reg;

	R = 0;

	if (shift_reg == 0)
		NdisGetSystemUpTime((ULONG *)&shift_reg);

	for (i = 0; i < 8; i++) {
		if (shift_reg & 0x00000001) {
			shift_reg = ((shift_reg ^ LFSR_MASK) >> 1) | 0x80000000;
			Result = 1;
		} else {
			shift_reg = shift_reg >> 1;
			Result = 0;
		}

		R = (R << 1) | Result;
	}

	return R;
}

UINT32 MtRandom32(VOID)
{
	UINT32 val = 0;

	mt_os_random_get(MT_OS_RANDOM_TYPE_U32, &val, sizeof(val));

	return val;
}

MT_OS_FD os_file_open(
	char *pPath, int flag, int mode)
{
	return mt_os_file_open(pPath, flag, mode);
}

INT os_file_close(
	MT_OS_FD osfd)
{
	return mt_os_file_close(osfd);
}

VOID os_file_seek(
	MT_OS_FD osfd, int offset)
{
	mt_os_file_seek(osfd, offset);
}

INT os_file_write(
	MT_OS_FD osfd, char *pDataPtr, int writeLen)
{
	return mt_os_file_write(osfd, pDataPtr, writeLen);
}

INT os_file_read(
	MT_OS_FD osfd, char *pDataPtr, int readLen)
{
	return mt_os_file_read(osfd, pDataPtr, readLen);
}

void os_fs_info_change(
	MT_OS_FS_INFO *pOSFSInfo, unsigned char bSet)
{
	mt_os_fs_info_change(pOSFSInfo, bSet);
}

VOID os_file_open_ext(
	CHAR * pPath,
	MT_OS_FD_EXT *fd,
	INT32  flag, /* CreateDisposition */
	INT32 file_mode)
{
	mt_os_file_open_ext(pPath, fd, flag, file_mode);
}

INT os_file_close_ext(
	MT_OS_FD_EXT osfd)
{
	INT ret;
	ret = mt_os_file_close(osfd.fsFd);

	if (!ret)
		mt_os_fs_info_change(&osfd.fsInfo, FALSE);

	return ret;
}

INT os_file_write_ext(
	MT_OS_FD_EXT osfd,
	CHAR *pDataPtr,
	INT32 writeLen)
{
	return mt_os_file_write(osfd.fsFd, pDataPtr, writeLen);
}

INT os_file_read_ext(
	MT_OS_FD_EXT osfd,
	CHAR *pDataPtr,
	INT32 readLen)
{
	return mt_os_file_read(osfd.fsFd, pDataPtr, readLen);
}

VOID os_msec_delay(UINT msec)
{
	RtmpOsMsDelay(msec);
}

VOID os_usec_delay(UINT usec)
{
	RtmpusecDelay(usec);
}

void os_load_code_from_bin(
	void *pAd, unsigned char **image, char *bin_name, UINT32 *code_len)
{
	RTMP_ADAPTER *mac_ad = (RTMP_ADAPTER *)pAd;
	MT_OS_DEV *dev = NULL;

	if (mac_ad) {
		POS_COOKIE obj = (POS_COOKIE)mac_ad->OS_Cookie;
		struct mtk_mac_hw *hw;

		if (obj->mac_dev) {
			hw = &obj->mac_dev->hw;
			dev = (MT_OS_DEV *)hw->pdev;
		}
	}
	mt_os_load_code_from_bin(dev, image, bin_name, code_len);
}

void *os_net_dev_create(
	int MC_RowID,
	unsigned int *pIoctlIF,
	int devType,
	int devNum,
	int privMemSize,
	char *pNamePrefix,
	BOOLEAN autoSuffix)
{
	unsigned char add_suffix = true;

	/* find a available interface name, max 32 interfaces */
	if (devType == INT_MBSSID)
		add_suffix = false;

	return mt_os_net_dev_create(
		MC_RowID,
		pIoctlIF,
		devType,
		devNum,
		privMemSize,
		pNamePrefix,
		add_suffix,
		autoSuffix);
}

extern struct wifi_dev_ops ap_wdev_ops;
extern struct wifi_dev_ops sta_wdev_ops;

static VOID rtmp_netdev_set(RTMP_ADAPTER *pAd, void *net_dev)
{
	struct wifi_dev *wdev = NULL;
	INT32 ret = 0;
	struct _RTMP_CHIP_CAP *cap = hc_get_chip_cap(pAd->hdev_ctrl);

	/* set main net_dev */
	pAd->net_dev = net_dev;
#ifdef CONFIG_AP_SUPPORT

	if (pAd->OpMode == OPMODE_AP) {
		BSS_STRUCT *pMbss;

		pMbss = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
		ASSERT(pMbss);
		wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;
		mt_os_net_dev_set_wdev(net_dev, wdev);
		ret = wdev_init(pAd, wdev, WDEV_TYPE_AP, net_dev, MAIN_MBSSID, (VOID *)pMbss, (VOID *)pAd);

		if (!ret) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_INTF, DBG_LVL_ERROR,
				"Assign wdev idx for %s failed, free net device!\n",
				RTMP_OS_NETDEV_GET_DEVNAME(pAd->net_dev));
			RtmpOSNetDevFree(pAd->net_dev);
			return;
		}

		ret = wdev_ops_register(wdev, WDEV_TYPE_AP, &ap_wdev_ops,
					cap->qos.wmm_detect_method);

		if (!ret) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_INTF, DBG_LVL_ERROR,
				 "register wdev_ops %s failed, free net device!\n",
				 RTMP_OS_NETDEV_GET_DEVNAME(pAd->net_dev));
			RtmpOSNetDevFree(pAd->net_dev);
			return;
		}
	}

#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT

	if (pAd->OpMode == OPMODE_STA) {
		wdev = &pAd->StaCfg[MAIN_MSTA_ID].wdev;
		mt_os_net_dev_set_wdev(net_dev, wdev);
		ret = wdev_init(pAd, wdev, WDEV_TYPE_STA, net_dev, MAIN_MSTA_ID, (VOID *)&pAd->StaCfg[MAIN_MSTA_ID], (VOID *)pAd);

		if (!ret) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_INTF, DBG_LVL_ERROR,
				"Assign wdev idx for %s failed, free net device!\n",
				RTMP_OS_NETDEV_GET_DEVNAME(pAd->net_dev));
			RtmpOSNetDevFree(pAd->net_dev);
			return;
		}

		ret = wdev_ops_register(wdev, WDEV_TYPE_STA, &sta_wdev_ops,
					cap->qos.wmm_detect_method);

		if (!ret) {
			MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_INTF, DBG_LVL_ERROR,
				 "register wdev_ops %s failed, free net device!\n",
				 RTMP_OS_NETDEV_GET_DEVNAME(pAd->net_dev));
			RtmpOSNetDevFree(pAd->net_dev);
			return;
		}
	}

#endif /* CONFIG_STA_SUPPORT */
}

void *os_net_dev_init(
	struct _RTMP_ADAPTER *pAd, struct mt_os_net_dev_op_hook *pNetDevHook)
{
	void *net_dev = NULL;
	ULONG InfId = INT_MAIN;
	POS_COOKIE pObj = (POS_COOKIE)pAd->OS_Cookie;
	struct mtk_mac_hw *hw = &pObj->mac_dev->hw;

	net_dev = RtmpPhyNetDevMainCreate(pAd);
	if (net_dev == NULL) {
		MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_ERROR,
				 "main physical net device creation failed!\n");
		return NULL;
	}

	os_zero_mem((unsigned char *)pNetDevHook, sizeof(struct mt_os_net_dev_op_hook));
	pNetDevHook->init = NULL;
	pNetDevHook->uninit = NULL;
	pNetDevHook->open = main_virtual_if_open;
	pNetDevHook->stop = main_virtual_if_close;
	pNetDevHook->xmit = rt28xx_send_packets;
	pNetDevHook->priv_flags = InfId; /*INT_MAIN; */
	pNetDevHook->needProtcted = FALSE;
#ifdef HOSTAPD_MBSS_SUPPORT
	pNetDevHook->set_mac_addr = rt28xx_change_mac;
#endif /*HOSTAPD_MBSS_SUPPORT*/
	/* put private data structure */
	mt_os_net_dev_set_priv(net_dev, pAd);
	pNetDevHook->dev_idx = pAd->dev_idx;
	pNetDevHook->dev = hw->pdev;

	/* double-check if pAd is associated with the net_dev */
	if (mt_os_net_dev_get_priv(net_dev) == NULL) {
		RtmpOSNetDevFree(net_dev);
		return NULL;
	}

	rtmp_netdev_set(pAd, net_dev);
	return net_dev;
}

int os_net_dev_attach(
	struct _RTMP_ADAPTER *mac_ad,
	void *net_dev,
	struct mt_os_net_dev_op_hook *pDevOpHook)
{
	int ret = NDIS_STATUS_FAILURE;
	unsigned char backport = FALSE;

	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO, "--->\n");

	if (mac_ad->net_dev == net_dev) {
		/* register mac address */
		mac_ad->bLocalAdminMAC = FALSE;
		if ((NdisEqualMemory(
			ZERO_MAC_ADDR,
			PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev), MAC_ADDR_LEN) == FALSE)
			&& (hc_get_hw_band_idx(mac_ad) == BAND0)) {
			COPY_MAC_ADDR(mac_ad->CurrentAddress,
				PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev));
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
		}
		if ((NdisEqualMemory(
			ZERO_MAC_ADDR,
			PD_GET_ADMIN_ADDRESS1(mac_ad->physical_dev), MAC_ADDR_LEN) == FALSE)
			&& (hc_get_hw_band_idx(mac_ad) == BAND1)) {
			COPY_MAC_ADDR(mac_ad->CurrentAddress,
				PD_GET_ADMIN_ADDRESS1(mac_ad->physical_dev));
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
		} else if ((NdisEqualMemory(
			ZERO_MAC_ADDR,
			PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev), MAC_ADDR_LEN) == FALSE)
			&& (hc_get_hw_band_idx(mac_ad) == BAND1)) {
			PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev)[0] |= (0x8);
			COPY_MAC_ADDR(mac_ad->CurrentAddress,
				PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev));
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
		}
		if ((NdisEqualMemory(
			ZERO_MAC_ADDR,
			PD_GET_ADMIN_ADDRESS2(mac_ad->physical_dev), MAC_ADDR_LEN) == FALSE)
			&& (hc_get_hw_band_idx(mac_ad) == BAND2)) {
			COPY_MAC_ADDR(mac_ad->CurrentAddress,
				PD_GET_ADMIN_ADDRESS2(mac_ad->physical_dev));
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
		} else if ((NdisEqualMemory(
			ZERO_MAC_ADDR,
			PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev), MAC_ADDR_LEN) == FALSE)
			&& (hc_get_hw_band_idx(mac_ad) == BAND2)) {
			PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev)[0] |= (0xc);
			COPY_MAC_ADDR(mac_ad->CurrentAddress,
				PD_GET_ADMIN_ADDRESS(mac_ad->physical_dev));
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
		}
	}

	/* If we need hook some callback function to the net device structrue, now do it. */
	if (pDevOpHook) {
#if defined(RT_CFG80211_SUPPORT) && defined(BACKPORT_NOSTDINC)
		backport = TRUE;
#endif /* defined(RT_CFG80211_SUPPORT) && defined(BACKPORT_NOSTDINC) */

#ifdef ACTION_MONITOR_SUPPORT
		if (backport && pDevOpHook->priv_flags == INT_ACT_MONITOR)
			backport = FALSE;
#endif /* ACTION_MONITOR_SUPPORT */

#ifdef CFG_JUMBO_SUPPORT
		pDevOpHook->max_mtu = MAX_MTU_SIZE;
#endif /* CFG_JUMBO_SUPPORT */

		pDevOpHook->ioctl = os_ioctl;
		pDevOpHook->reg_cfg80211 = 0;
#ifdef RT_CFG80211_SUPPORT
		if (mac_ad->net_dev == net_dev)
			cfg80211_drv_init(mac_ad, pDevOpHook);
#endif /* RT_CFG80211_SUPPORT */
		pDevOpHook->band_idx = mac_ad->band_idx;
		if (mac_ad->physical_dev)
			pDevOpHook->device_idx = PD_GET_DEVICE_IDX(mac_ad->physical_dev);
	}

	ret = mt_os_net_dev_register(net_dev, pDevOpHook, backport);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_UP, DBG_LVL_INFO,
		"<---ret=%d\n", ret);

	if (ret == 0)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

static int os_ioctl_prepare(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	int priv_flags)
{
	POS_COOKIE pObj;
	INT	Status = NDIS_STATUS_SUCCESS;

	pObj = (POS_COOKIE) mac_ad->OS_Cookie;
	pObj->pSecConfig = NULL;

	/* determine this ioctl command is coming from which interface. */
	pObj->ioctl_if = wdev->func_idx;
	pObj->pSecConfig = &wdev->SecConfig;
	pObj->ioctl_if_type = priv_flags;
	return Status;
}

static void os_ioctl_get_rate(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	union _HTTRANSMIT_SETTING HtPhyMode;
#ifdef DOT11_HE_AX
	HE_TRANSMIT_SETTING HePhyMode;
#endif /* DOT11_HE_AX */
	UINT8 BW;
	UINT8 Antenna = 0;
	USHORT MCS;
	ULONG getBitRate = 0;
	INT if_idx = wdev->func_idx;
	UINT64 rate_value = 0;
	struct mcs_nss_caps *mcs_nss = MCS_NSS_CAP(mac_ad);

	ioctl_cfg->u_long_val = 0;
	if (!wdev->DevInfo.WdevActive)
		return;

	HtPhyMode = wdev->HTPhyMode;
	HtPhyMode.word = 0;
	MCS = HtPhyMode.field.MCS;
#ifdef DOT11_N_SUPPORT
	if ((HtPhyMode.field.MODE == MODE_HTMIX)
	    || (HtPhyMode.field.MODE == MODE_HTGREENFIELD)) {
		Antenna = (HtPhyMode.field.MCS >> 3) + 1;
		MCS = MCS & 0xffff;
	}

#endif /* DOT11_N_SUPPORT */
#ifdef DOT11_VHT_AC
	if (HtPhyMode.field.MODE >= MODE_VHT)
		MCS = MCS & 0xf;

	if (HtPhyMode.field.MODE >= MODE_VHT) {
		BW = wlan_operate_get_vht_bw(wdev);
		if (wlan_operate_get_ht_bw(wdev)) {
			if (BW == 0) /* VHT40 */
				BW = 1;
			else if (BW == 1) /* VHT80 */
				BW = 2;
			else if (BW >= 2) /* VHT80-80,VHT160 */
				BW = 3;
		}
		/*for spec VHT mode (bw20M no 3*3) or (bw160M and 3*3) maxMCS is 8*/
		if (((wlan_operate_get_ht_bw(wdev) == 0)
			&& (wlan_config_get_tx_stream(wdev) != 3) && (BW == 0))
			|| ((wlan_config_get_tx_stream(wdev) == 3) && (BW == 3)))
			MCS = 8;
			/*bw40M or bw80M or (3*3 and bw20M) maxMCS is 9*/
		else
			MCS = 9;
	} else
#endif /*DOT11_VHT_AC*/
		if (HtPhyMode.field.MODE >= MODE_HTMIX)
			BW = wlan_operate_get_ht_bw(wdev);
		else
			BW = HtPhyMode.field.BW;
	if (Antenna == 0)
		Antenna = wlan_config_get_tx_stream(wdev);

	if (Antenna > mcs_nss->max_nss) {
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_WARN,
			"This config resoruce cannot be trusted.\n");
		return;
	}
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"HtPhyMode.field.MODE=%d\n\r", HtPhyMode.field.MODE);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"HtPhyMode.field.ShortGI=%d\n\r", HtPhyMode.field.ShortGI);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"HtPhyMode.field.BW=%d\n\r", HtPhyMode.field.BW);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"HtPhyMode.field.MCS=%d\n\r", MCS);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"BW=%d\n\r", BW);
	MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_INFO,
		"Antenna=%d\n\r", Antenna);
#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(wdev->PhyMode)) {
		if (if_idx >= MAX_BEACON_NUM) {
			MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
				"invalid mbss id(%d)\n", if_idx);
			return;
		}
		Antenna = wlan_config_get_eht_tx_nss(wdev);
		if (Antenna > mcs_nss->max_nss) {
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_WARN,
				"This config resoruce cannot be trusted.\n");
			return;
		}
		BW =  wlan_operate_get_eht_bw(wdev);
		MCS = 13;

		RtmpDrvMaxRateGet(mac_ad, MODE_EHT, FIXED_GI_08_US,
			  BW, MCS,
			  Antenna,
			  &rate_value);
		ioctl_cfg->u_long_val = rate_value;
		return;
	}
#endif /*DOT11_HE_AX*/

#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(wdev->PhyMode)) {
		if (VALID_MBSS(mac_ad, if_idx))
			HePhyMode = mac_ad->ApCfg.MBSSID[if_idx].wdev.HEPhyMode;
		else {
			MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
				"invalid mbss id(%d)\n", if_idx);
			return;
		}
		Antenna = wlan_config_get_he_tx_nss(wdev);
		if (Antenna > mcs_nss->max_nss) {
			MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_WARN,
				"This config resoruce cannot be trusted.\n");
			return;
		}
		BW =  wlan_operate_get_he_bw(wdev);
		MCS = HePhyMode.field.MCS;

		RtmpDrvMaxRateGet(mac_ad, HePhyMode.field.MODE, HePhyMode.field.ShortGI,
			  BW, MCS,
			  Antenna,
			  &rate_value);
		ioctl_cfg->u_long_val = rate_value;
		return;
	}
#endif /*DOT11_HE_AX*/

#ifdef DOT11_VHT_AC
	if ((mac_ad->CommonCfg.g_band_256_qam == 1) && (HtPhyMode.field.MODE == MODE_HTMIX) &&
	    (wdev->channel <= 14)) {
		if ((MCS == 7) || (MCS == 15) || (MCS == 23) || (MCS == 31)) {
			HtPhyMode.field.MODE = MODE_VHT;
			/*for spec VHT mode (bw20M no 3*3) or (bw160M and 3*3) maxMCS is 8*/
			if (((BW == 0) && (MCS != 23)) || ((BW == 3) && (MCS == 23)))
				MCS = 8;
			/*bw40M or bw80M or (3*3 and bw20M) maxMCS is 9*/
			else
				MCS = 9;
		}
	}
#endif /*DOT11_VHT_AC*/

	if (HtPhyMode.field.MODE == MODE_VHT) {
		HtPhyMode.field.MCS = ((Antenna - 1) & 0x3) << 4;
		HtPhyMode.field.MCS |= MCS;
	}
	HtPhyMode.field.BW = BW;
	getRate(HtPhyMode, &getBitRate);
	ioctl_cfg->u_long_val = getBitRate * 1000000;
}

#ifdef CONFIG_WIRELESS_EXT
static int os_ioctl_get_iw_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	PMAC_TABLE_ENTRY pMacEntry = NULL;
	struct mt_os_iw_stats *pStats;

	/*check if the interface is down*/
	if (!RTMP_TEST_FLAG(mac_ad, fRTMP_ADAPTER_INTERRUPT_REGISTER_TO_OS))
		return NDIS_STATUS_FAILURE;

	pStats = (struct mt_os_iw_stats *)ioctl_cfg->data;
	if (!pStats)
		return NDIS_STATUS_FAILURE;
	pStats->qual = 0;
	pStats->level = 0;
	pStats->noise = 0;
	pStats->stats = mac_ad->iw_stats;

	if (ioctl_cfg->priv_flags == INT_APCLI) {
		UINT ApCliIdx = wdev->func_idx;

		if ((ApCliIdx < MAX_MULTI_STA)
			&& IS_WCID_VALID(mac_ad, mac_ad->StaCfg[ApCliIdx].MacTabWCID))
			pMacEntry = entry_get(
				mac_ad, mac_ad->StaCfg[ApCliIdx].MacTabWCID);
	}

	if (pMacEntry)
		pStats->qual = ((pMacEntry->ChannelQuality * 12) / 10 + 10);
	else
		pStats->qual = ((mac_ad->Mlme.ChannelQuality * 12) / 10 + 10);

	if (pStats->qual > 100)
		pStats->qual = 100;


	if (pMacEntry != NULL)
		pStats->level =
			RTMPMaxRssi(mac_ad, pMacEntry->RssiSample.AvgRssi[0],
				    pMacEntry->RssiSample.AvgRssi[1],
				    pMacEntry->RssiSample.AvgRssi[2]);

	pStats->noise = RTMPMaxRssi(mac_ad, mac_ad->ApCfg.RssiSample.AvgRssi[0],
				    mac_ad->ApCfg.RssiSample.AvgRssi[1],
				    mac_ad->ApCfg.RssiSample.AvgRssi[2]) -
			RTMPMinSnr(mac_ad, mac_ad->ApCfg.RssiSample.AvgSnr[0],
				   mac_ad->ApCfg.RssiSample.AvgSnr[1]);

	return NDIS_STATUS_SUCCESS;
}
#endif /* CONFIG_WIRELESS_EXT */

static int os_ioctl_get_ether_stats(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg,
	unsigned int if_idx)
{
	struct mt_os_net_dev_stats *stats;

	stats = (struct mt_os_net_dev_stats *)ioctl_cfg->data;
	if (!stats)
		return NDIS_STATUS_FAILURE;

	{
		BOOLEAN not_support = TRUE;

		if (!wdev) {
			MTWF_DBG(mac_ad, DBG_CAT_INIT, CATINIT_INTF, DBG_LVL_ERROR,
				"can not find mbss I/F\n");
			return NDIS_STATUS_FAILURE;
		}
#ifdef CONFIG_AP_SUPPORT
		if (wdev->wdev_type == WDEV_TYPE_AP) {
			struct _BSS_STRUCT *mbss;

			if (if_idx >= MAX_BEACON_NUM)
				return NDIS_STATUS_FAILURE;
			mbss = &mac_ad->ApCfg.MBSSID[if_idx];
			stats->rx_packets = mbss->RxCount;
			stats->tx_packets = mbss->TxCount;
			stats->rx_bytes = mbss->ReceivedByteCount;
			stats->tx_bytes = mbss->TransmittedByteCount;
			stats->rx_errors = mbss->RxErrorCount;
			stats->tx_errors = mbss->TxErrorCount;
			stats->rx_drop_count = mbss->RxDropCount;
			stats->tx_drop_count = mbss->TxDropCount;
			stats->multicast = mbss->mcPktsRx; /* multicast packets received */
			stats->collisions = 0; /* Collision packets*/
			stats->rx_over_errors = 0; /* receiver ring buff overflow*/
			stats->rx_crc_errors = 0; /* recved pkt with crc error*/
			stats->rx_frame_errors = 0; /* recv'd frame alignment error*/
			stats->rx_fifo_errors = 0; /* recv'r fifo overrun*/
			not_support = FALSE;
		}
		if (wdev->wdev_type == WDEV_TYPE_WDS) {
			struct _WDS_COUNTER *wds_counters;

			if (if_idx >= MAX_WDS_ENTRY)
				return NDIS_STATUS_FAILURE;
			wds_counters = &mac_ad->WdsTab.WdsEntry[if_idx].WdsCounter;
			stats->rx_packets = wds_counters->ReceivedFragmentCount.QuadPart;
			stats->tx_packets = wds_counters->TransmittedFragmentCount.QuadPart;
			stats->rx_bytes = wds_counters->ReceivedByteCount;
			stats->tx_bytes = wds_counters->TransmittedByteCount;
			stats->rx_errors = wds_counters->RxErrorCount;
			stats->tx_errors = wds_counters->TxErrors;
			/* multicast packets received */
			stats->multicast = wds_counters->MulticastReceivedFrameCount.QuadPart;
			stats->collisions = 0; /* Collision packets */
			/* receiver ring buff overflow */
			stats->rx_over_errors = wds_counters->RxNoBuffer;
			stats->rx_crc_errors = 0; /* recved pkt with crc error */
			stats->rx_frame_errors = 0; /* recv'd frame alignment error */
			stats->rx_fifo_errors = wds_counters->RxNoBuffer; /* recv'r fifo overrun */
		}
#endif /* CONFIG_AP_SUPPORT */
#ifdef CONFIG_STA_SUPPORT
		if (wdev->wdev_type == WDEV_TYPE_STA) {
			struct _stat_counters *stat_counters;

			if (if_idx >= MAX_MULTI_STA)
				return NDIS_STATUS_FAILURE;
			stat_counters = &mac_ad->StaCfg[if_idx].StaStatistic;
			stats->rx_packets = stat_counters->RxCount;
			stats->tx_packets = stat_counters->TxCount;
			stats->rx_bytes = stat_counters->ReceivedByteCount;
			stats->tx_bytes = stat_counters->TransmittedByteCount;
			stats->rx_errors = stat_counters->RxErrorCount;
			stats->tx_errors = stat_counters->TxErrorCount;
			stats->rx_drop_count = stat_counters->RxDropCount;
			stats->tx_drop_count = stat_counters->TxDropCount;
			stats->multicast = stat_counters->mcPktsRx; /* multicast packets received */
			stats->collisions = 0; /* Collision packets*/
			stats->rx_over_errors = 0; /* receiver ring buff overflow*/
			stats->rx_crc_errors = 0;/* recved pkt with crc error*/
			stats->rx_frame_errors = 0; /* recv'd frame alignment error*/
			stats->rx_fifo_errors = 0; /* recv'r fifo overrun*/
			not_support = FALSE;
		}
#endif /* CONFIG_STA_SUPPORT */
		if (not_support)
			return NDIS_STATUS_FAILURE;
	}

	return NDIS_STATUS_SUCCESS;
}

static int os_priv_ioctl_sSet(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	RTMP_STRING *this_char, *value;
	INT Status = NDIS_STATUS_SUCCESS;
	UCHAR *tmp = NULL, *buf = ioctl_cfg->data;
	char *dev_name = ioctl_cfg->name;

	/* Play safe - take care of a situation in which user-space didn't NULL terminate */
	if (buf) {
		buf[ioctl_cfg->data_len] = 0;
		/* Use tmp to parse string, because strsep() would change it */
		tmp = buf;
	}

	while ((this_char = strsep((char **)&tmp, "\0")) != NULL) {
		if (!*this_char)
			continue;

		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
			"CFG: iwpriv set %s %s\n", dev_name? dev_name:"N/A", this_char);

		value = strchr(this_char, '=');
		if (value != NULL)
			*value++ = 0;
		if (!value
#ifdef WSC_AP_SUPPORT
			&& (
				(strcmp(this_char, "WscStop") != 0) &&
				(strcmp(this_char, "ser") != 0) &&
				(strcmp(this_char, "WscGenPinCode") != 0)
			)
#endif /* WSC_AP_SUPPORT */
		   )
			continue;

		for (PRTMP_PRIVATE_AP_SET_PROC = RTMP_PRIVATE_AP_SUPPORT_PROC;
			PRTMP_PRIVATE_AP_SET_PROC->name; PRTMP_PRIVATE_AP_SET_PROC++) {
			if (rtstrcasecmp(this_char, PRTMP_PRIVATE_AP_SET_PROC->name) == TRUE) {
				if (!PRTMP_PRIVATE_AP_SET_PROC->set_proc(mac_ad, value)) {
					/*FALSE:Set private failed then return Invalid argument */
					Status = -EINVAL;
				}

				break;	/*Exit for loop. */
			}
		}

		if (PRTMP_PRIVATE_AP_SET_PROC->name == NULL) {
			/*Not found argument */
			Status = -EINVAL;
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_ERROR,
				"IOCTL::(iwpriv) Command not Support [%s=%s]\n", this_char,
				value);
			break;
		}
	}

	return Status;
}

VOID os_free_mac_ad(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdSrc;
	POS_COOKIE os_cookie;
	struct _PCI_HIF_T *hif;
	struct physical_device *ph_dev = pAd->physical_dev;

	MlmeQueueDestroy(&pAd->Mlme);
	hif = hc_get_hif_ctrl(pAd->hdev_ctrl);
	os_cookie = (POS_COOKIE)pAd->OS_Cookie;
#ifdef RLM_CAL_CACHE_SUPPORT
	rlmCalCacheDeinit(&pAd->rlmCalCache);
#endif /* RLM_CAL_CACHE_SUPPORT */
#ifdef MULTIPLE_CARD_SUPPORT
#ifdef RTMP_FLASH_SUPPORT

	/* only if in MULTIPLE_CARD the eebuf be allocated not static */
	if (pAd->eebuf  /*&& (pAd->eebuf != pAd->chipCap.EEPROM_DEFAULT_BIN)*/) {
		os_free_mem(pAd->eebuf);
		pAd->eebuf = NULL;
	}

#endif /* RTMP_FLASH_SUPPORT */
#endif /* MULTIPLE_CARD_SUPPORT */

	NdisFreeSpinLock(&pAd->BssInfoIdxBitMapLock);
	NdisFreeSpinLock(&pAd->WdevListLock);

#ifdef GREENAP_SUPPORT
	NdisFreeSpinLock(&pAd->ApCfg.greenap.lock);
#endif /* GREENAP_SUPPORT */
#if defined(MTK_HOSTAPD_SUPPORT) && defined(CONFIG_MAP_SUPPORT) && defined(MAP_R3)
	NdisFreeSpinLock(&pAd->gas_frame_list_lock);
#endif /* MTK_HOSTAPD_SUPPORT && CONFIG_MAP_SUPPORT && MAP_R3 */

	if (pAd->iw_stats) {
		os_free_mem(pAd->iw_stats);
		pAd->iw_stats = NULL;
	}

#ifdef WIFI_DIAG
	diag_ctrl_free(pAd);
#endif
#ifdef CONFIG_AP_SUPPORT
	if (pAd->OpMode == OPMODE_AP) {
		BSS_STRUCT *pMbss;

		pMbss = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
		ASSERT(pMbss);

		if (pMbss)
			bcn_buf_deinit(pAd, &pMbss->wdev);
		else {
			MTWF_DBG(pAd, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR,
				"func_dev is NULL!\n");
			return;
		}
	}

#endif
	/*remove chipcap*/
	hif_core_ops_unregister(pAd->hdev_ctrl, pAd->infType);
	hdev_ctrl_exit(pAd);
	physical_device_remove_mac_adapter(ph_dev->hw_dev, pAd);
	RtmpOsVfree(pAd);

	if (os_cookie)
		os_free_mem(os_cookie);
}

union iwinfo_mtk_rate_setting {
	struct {
		USHORT mcs:6;
		USHORT ldpc:1;
		USHORT bw:2;
		USHORT short_gi:2;
		USHORT stbc:1;
		USHORT etxbf:1;
		USHORT itxbf:1;
		USHORT mode:4;
	} field;
	UINT32 word;
};

struct iwinfo_mtk_mac_entry {
	UCHAR ap_idx;
	UCHAR addr[MAC_ADDR_LEN];
	UINT16 aid;
	UCHAR psm;
	UCHAR mimo_ps;
	CHAR avg_rssi[4];
	UINT32 connected_time;
	union iwinfo_mtk_rate_setting tx_rate;
	union iwinfo_mtk_rate_setting rx_rate;
	SHORT stream_snr[3];
	SHORT sounding_snr[3];
	UINT32 encry_mode;
	UINT32 auth_mode;
};

struct iwinfo_mtk_mac_table {
	ULONG num;
	struct iwinfo_mtk_mac_entry entry[MAX_LEN_OF_MAC_TABLE];
};

static void os_iwinfo_pack_rate(union iwinfo_mtk_rate_setting *rate,
	UCHAR mode, UCHAR bw, UCHAR gi, UCHAR stbc, UCHAR ldpc,
	UCHAR mcs, UCHAR streams)
{
	if (mode == MODE_EHT)
		mode = MODE_EHT_ER_SU;

	rate->field.mode = mode;
	rate->field.bw = bw > BW_160 ? BW_160 : bw;
	rate->field.short_gi = gi;
	rate->field.stbc = stbc;
	rate->field.ldpc = !!ldpc;
	if (mode >= MODE_VHT) {
		if (!streams)
			streams = 1;
		rate->field.mcs = (((streams - 1) & 0x3) << 4) | (mcs & 0xf);
	} else {
		rate->field.mcs = mcs & 0x3f;
	}
}

static INT os_ioctl_get_mac_table(RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev, struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg)
{
	RTMP_IOCTL_INPUT_STRUCT *wrq = ioctl_cfg->data;
	struct iwinfo_mtk_mac_table *table = NULL;
	INT i, ret = 0;

	if (!wrq || !wrq->u.data.pointer)
		return -RTMP_IO_EFAULT;

	os_alloc_mem(NULL, (UCHAR **)&table, sizeof(*table));
	if (!table)
		return -RTMP_IO_ENOMEM;

	NdisZeroMemory(table, sizeof(*table));
	for (i = 1; VALID_UCAST_ENTRY_WCID(mac_ad, i); i++) {
		MAC_TABLE_ENTRY *entry = entry_get(mac_ad, i);
		struct iwinfo_mtk_mac_entry *dst;
#ifndef RACTRL_FW_OFFLOAD_SUPPORT
		union _HTTRANSMIT_SETTING tx_rate, rx_rate;
		UCHAR tx_streams, rx_streams;
#endif

		if (!IS_ENTRY_CLIENT(entry) || entry->Sst != SST_ASSOC ||
		    entry->wdev != wdev)
			continue;
		if (table->num >= MAX_LEN_OF_MAC_TABLE)
			break;

		dst = &table->entry[table->num++];
		dst->ap_idx = entry->func_tb_idx;
		/* Report the station's LAN identity so iwinfo and DHCP use the same key. */
#ifdef DOT11_EHT_BE
		if (entry->mlo.mlo_en)
			COPY_MAC_ADDR(dst->addr, entry->mlo.mld_addr);
		else
#endif
			COPY_MAC_ADDR(dst->addr, entry->Addr);
		dst->aid = entry->Aid;
		dst->psm = entry->PsMode;
#ifdef DOT11_N_SUPPORT
		dst->mimo_ps = entry->MmpsMode;
#endif
		dst->avg_rssi[0] = entry->RssiSample.AvgRssi[0];
		dst->avg_rssi[1] = entry->RssiSample.AvgRssi[1];
		dst->avg_rssi[2] = entry->RssiSample.AvgRssi[2];
		dst->avg_rssi[3] = entry->RssiSample.AvgRssi[3];
		dst->connected_time = entry->StaConnectTime;
		dst->encry_mode = entry->SecConfig.PairwiseCipher;
		dst->auth_mode = entry->SecConfig.AKMMap;

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
		{
			EXT_EVENT_TX_STATISTIC_RESULT_T tx_stat;
			EXT_EVENT_PHY_STATE_RX_RATE rx_stat;
			UCHAR band = HcGetBandByWdev(entry->wdev);

			NdisZeroMemory(&tx_stat, sizeof(tx_stat));
			NdisZeroMemory(&rx_stat, sizeof(rx_stat));
			MtCmdGetTxStatistic(mac_ad, GET_TX_STAT_ENTRY_TX_RATE, 0,
				entry->wcid, &tx_stat);
			MtCmdPhyGetRxRate(mac_ad, CMD_PHY_STATE_CONTENTION_RX_PHYRATE,
				band, entry->wcid, (UINT32 *)&rx_stat);

			os_iwinfo_pack_rate(&dst->tx_rate,
				tx_stat.rEntryTxRate.MODE,
				tx_stat.rEntryTxRate.BW,
				tx_stat.rEntryTxRate.ShortGI,
				tx_stat.rEntryTxRate.STBC,
				tx_stat.rEntryTxRate.ldpc,
				tx_stat.rEntryTxRate.MCS,
				tx_stat.rEntryTxRate.VhtNss);
			os_iwinfo_pack_rate(&dst->rx_rate,
				rx_stat.u1RxMode, rx_stat.u1BW, rx_stat.u1Gi,
				rx_stat.u1Stbc, rx_stat.u1Coding, rx_stat.u1RxRate,
				rx_stat.u1RxNsts + 1);
		}
#else
		tx_rate = entry->HTPhyMode;
		rx_rate.word = entry->LastRxRate;
		tx_streams = ((tx_rate.field.MCS >> 4) & 0x3) + 1;
		rx_streams = ((rx_rate.field.MCS >> 4) & 0x3) + 1;
		os_iwinfo_pack_rate(&dst->tx_rate, tx_rate.field.MODE,
			tx_rate.field.BW, tx_rate.field.ShortGI, tx_rate.field.STBC,
			tx_rate.field.ldpc, tx_rate.field.MCS, tx_streams);
		os_iwinfo_pack_rate(&dst->rx_rate, rx_rate.field.MODE,
			rx_rate.field.BW, rx_rate.field.ShortGI, rx_rate.field.STBC,
			rx_rate.field.ldpc, rx_rate.field.MCS, rx_streams);
#endif
	}

	wrq->u.data.length = sizeof(*table);
	if (copy_to_user(wrq->u.data.pointer, table, sizeof(*table)))
		ret = -RTMP_IO_EFAULT;
	os_free_mem(table);
	return ret;
}

INT os_ioctl(void *net_dev, void *ioctl_cfg_)
{
	struct mt_os_wifi_ioctl_cmd_cfg *ioctl_cfg = (struct mt_os_wifi_ioctl_cmd_cfg *)ioctl_cfg_;
	INT ret = 0;
	struct wifi_dev *wdev;
	struct _RTMP_ADAPTER *mac_ad = NULL;
	POS_COOKIE pObj = NULL;
	unsigned int if_idx;

	if (!ioctl_cfg)
		return -RTMP_IO_EFAULT;

	GET_PAD_FROM_NET_DEV(mac_ad, net_dev);
	if (mac_ad == NULL)
		return -RTMP_IO_ENETDOWN;

	GET_WDEV_FROM_NET_DEV(wdev, net_dev);
	if (wdev == NULL)
		return -RTMP_IO_ENETDOWN;

	pObj = (POS_COOKIE)mac_ad->OS_Cookie;
	if (os_ioctl_prepare(mac_ad, wdev, ioctl_cfg->priv_flags))
		return -RTMP_IO_ENETDOWN;

	if_idx = pObj->ioctl_if;
	switch (ioctl_cfg->cmd) {
	case MT_IO_GET_NAME:
		if (wdev && wmode_2_cfgmode(wdev->PhyMode) < PHY_MODE_MAX) {
			RT_802_11_PHY_MODE cfg_mode = wmode_2_cfgmode(wdev->PhyMode);

			ret = snprintf(
				ioctl_cfg->data, ioctl_cfg->data_len,
				"%s", iee80211_mode_str[cfg_mode]);
			if (os_snprintf_error(ioctl_cfg->data_len, ret)) {
				MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
					": snprintf error!\n");
				return -RTMP_IO_EINVAL;
			}
		}
		break;
	case MT_IO_GET_IF_HW_ADDR:
		if (ioctl_cfg->data_len >= MAC_ADDR_LEN)
			NdisCopyMemory(ioctl_cfg->data, wdev->bssid, MAC_ADDR_LEN);
		else
			ret = -RTMP_IO_EINVAL;
		break;
	case MT_IO_GET_ESSID:
#ifdef APCLI_SUPPORT
		if (ioctl_cfg->priv_flags == INT_APCLI) {
			if (if_idx >= MAX_MULTI_STA) {
				MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
					"error station index %d\n", if_idx);
				ret = -RTMP_IO_EINVAL;
				break;
			}

			if (mac_ad->StaCfg[if_idx].ApcliInfStat.Valid == TRUE) {
				ioctl_cfg->data_len = mac_ad->StaCfg[if_idx].SsidLen;
				ioctl_cfg->data = (char *)&mac_ad->StaCfg[if_idx].Ssid;
			} else {
				ioctl_cfg->data_len = 0;
				ioctl_cfg->data = NULL;
			}
		} else
#endif /* APCLI_SUPPORT */
		{
			if (if_idx >= MAX_MBSSID_NUM(mac_ad)) {
				MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
					"error AP index %d\n", if_idx);
				ret = -RTMP_IO_EINVAL;
				break;
			}

			ioctl_cfg->data_len = mac_ad->ApCfg.MBSSID[if_idx].SsidLen;
			ioctl_cfg->data = (char *)mac_ad->ApCfg.MBSSID[if_idx].Ssid;
		}
		break;
	case MT_IO_GET_FREQ:
		/* get channel number */
		ioctl_cfg->s_int_val = wdev->channel;
		break;
	case MT_IO_GET_RATE:
		os_ioctl_get_rate(mac_ad, wdev, ioctl_cfg);
		break;
	case MT_IO_GET_AP:
#ifdef APCLI_SUPPORT
		if (ioctl_cfg->priv_flags == INT_APCLI) {
			if (if_idx >= MAX_MULTI_STA) {
				MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
					"error AP index %d\n", if_idx);
				ret = -RTMP_IO_EINVAL;
				break;
			}

			if (mac_ad->StaCfg[if_idx].ApcliInfStat.Valid == TRUE)
				ioctl_cfg->data = (PCHAR)APCLI_ROOT_BSSID_GET(
					mac_ad, mac_ad->StaCfg[if_idx].MacTabWCID);
			else
				ioctl_cfg->data = NULL;
		} else
#endif /* APCLI_SUPPORT */
		{
			if (if_idx >= MAX_MBSSID_NUM(mac_ad)) {
				MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
					"error AP index %d\n", if_idx);
				ret = -RTMP_IO_EINVAL;
				break;
			}

			ioctl_cfg->data = (unsigned char *)&wdev->bssid[0];
		}
		break;
	case MT_IO_GET_TXPOW:
		ioctl_cfg->s_int_val = rtmp_get_macPower(mac_ad);
		break;
	case MT_IO_GET_IW_STATS:
		ret = -RTMP_IO_EOPNOTSUPP;
#ifdef CONFIG_WIRELESS_EXT
		ret = os_ioctl_get_iw_stats(mac_ad, wdev, ioctl_cfg);
#endif /* CONFIG_WIRELESS_EXT */
		break;
	case MT_IO_GET_ETHER_STATS:
		ret = os_ioctl_get_ether_stats(mac_ad, wdev, ioctl_cfg, if_idx);
		break;
	case MT_IO_GET_STATISTICS:
		if (ioctl_cfg->data)
			RTMPIoctlStatistics(mac_ad, ioctl_cfg->data);
		break;
#if defined(AP_SCAN_SUPPORT) || defined(CONFIG_STA_SUPPORT)
	case MT_IO_GET_SITESURVEY:
		if (ioctl_cfg->data)
			RTMPIoctlGetSiteSurvey(mac_ad, ioctl_cfg->data);
		break;
#endif
	case MT_IO_GET_MAC_TABLE:
		ret = os_ioctl_get_mac_table(mac_ad, wdev, ioctl_cfg);
		break;
	case MT_IO_SET_PRIV:
		if (wdev->if_up_down_state == FALSE) {
			MTWF_DBG(mac_ad, DBG_CAT_INTF, CATINTF_CTRL, DBG_LVL_ERROR,
				"interface is down, cmd [%x] return!!!\n", ioctl_cfg->cmd);
			ret = -RTMP_IO_ENETDOWN;
			break;
		}
		os_priv_ioctl_sSet(mac_ad, wdev, ioctl_cfg);
		break;
#ifdef RT_CFG80211_SUPPORT
	case MT_IO_CFG80211_INIT:
		cfg80211_drv_init_cmd_handle(mac_ad, wdev, ioctl_cfg);
		break;
	case MT_IO_CFG80211_VNDR:
		ret = cfg80211_drv_vndr_cmd_handle(mac_ad, wdev, ioctl_cfg);
		break;
	case MT_IO_CFG80211_OPS:
		ret = cfg80211_drv_ops_handle(mac_ad, wdev, ioctl_cfg);
		break;
#endif /* RT_CFG80211_SUPPORT */
#ifdef CFG_SUPPORT_CSI
	case MT_IO_CSI_DOIT:
		ret = csi_genl_recv_doit(mac_ad, wdev, ioctl_cfg);
		break;
#endif
	case MT_IO_PROC:
		ret = mtk_proc_handle(mac_ad, wdev, ioctl_cfg);
		break;
#ifdef WIFI_DIAG
	case MT_IO_DIAG:
		ret = diag_cmd_handle(mac_ad, wdev, ioctl_cfg);
		break;
#endif /* WIFI_DIAG */
	default:
		ret = -RTMP_IO_EOPNOTSUPP;
		break;
	}
	return ret;
}

#ifdef MEM_ALLOC_INFO_SUPPORT
MEM_INFO_LIST MemInfoList;
MEM_INFO_LIST PktInfoList;
#endif /* MEM_ALLOC_INFO_SUPPORT */
#ifdef VENDOR_FEATURE2_SUPPORT
ULONG OS_NumOfPktAlloc = 0, OS_NumOfPktFree = 0;
#endif /* VENDOR_FEATURE2_SUPPORT */
#ifdef VENDOR_FEATURE4_SUPPORT
ULONG OS_NumOfMemAlloc = 0, OS_NumOfMemFree = 0;
ULONG OS_NumOfVMemAlloc = 0, OS_NumOfVMemFree = 0;
#endif /* VENDOR_FEATURE4_SUPPORT */


NDIS_STATUS os_alloc_mem_suspend(
	VOID * pReserved,
	UCHAR **mem,
	ULONG size)
{
	int ret;

	ret = mt_os_mem_alloc((void **)mem, size, true);
	if (ret == 0) {
#ifdef VENDOR_FEATURE4_SUPPORT
		OS_NumOfMemAlloc++;
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef MEM_ALLOC_INFO_SUPPORT
		MIListAddHead(&MemInfoList, size, *mem, __func__, __LINE__, __builtin_return_address(0));
#endif /* MEM_ALLOC_INFO_SUPPORT */
		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}

INT os_alloc_mem(
	VOID * pAd,
	UCHAR **mem,
	ULONG size)
{
	int ret;

	ret = mt_os_mem_alloc((void **)mem, size, false);
	if (ret == 0) {
#ifdef VENDOR_FEATURE4_SUPPORT
		OS_NumOfMemAlloc++;
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef MEM_ALLOC_INFO_SUPPORT
		MIListAddHead(&MemInfoList, size, *mem, __func__, __LINE__, __builtin_return_address(0));
#endif /* MEM_ALLOC_INFO_SUPPORT */
		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}

VOID os_free_mem(
	PVOID mem)
{
#ifdef MEM_ALLOC_INFO_SUPPORT
	MEM_INFO_LIST_ENTRY * delEntry;

	delEntry = MIListRemove(&MemInfoList, mem, __builtin_return_address(0));

	if (delEntry == NULL) {
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_MLME, DBG_LVL_ERROR,
				 "the memory has not been allocated\n");
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_MLME, DBG_LVL_ERROR,
				 "mem addr = %p, caller is %pS\n", mem, __builtin_return_address(0));
		dump_stack();
	}
#endif /* MEM_ALLOC_INFO_SUPPORT */
	ASSERT(mem);
	mt_os_mem_free(mem);

#ifdef VENDOR_FEATURE4_SUPPORT
	OS_NumOfMemFree++;
#endif /* VENDOR_FEATURE4_SUPPORT */
}

INT os_alloc_vmem(
	VOID * pAd,
	UCHAR **vmem,
	ULONG size)
{
	int ret;

	ret = mt_os_vmem_alloc((void **)vmem, size);
	if (ret == 0) {
#ifdef VENDOR_FEATURE4_SUPPORT
		OS_NumOfVMemAlloc++;
#endif /* VENDOR_FEATURE4_SUPPORT */
#ifdef MEM_ALLOC_INFO_SUPPORT
		MIListAddHead(&MemInfoList, size, *vmem, __func__, __LINE__, __builtin_return_address(0));
#endif /* MEM_ALLOC_INFO_SUPPORT */
		return NDIS_STATUS_SUCCESS;
	} else
		return NDIS_STATUS_FAILURE;
}

VOID os_free_vmem(
	PVOID vmem)
{
#ifdef MEM_ALLOC_INFO_SUPPORT
	MEM_INFO_LIST_ENTRY * delEntry;

	delEntry = MIListRemove(&MemInfoList, vmem, __builtin_return_address(0));

	if (delEntry == NULL) {
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_MLME, DBG_LVL_ERROR,
				 "the memory has not been allocated\n");
		MTWF_DBG(NULL, DBG_CAT_INIT, CATINIT_MLME, DBG_LVL_ERROR,
				 "mem addr = %p, caller is %pS\n", vmem, __builtin_return_address(0));
		dump_stack();
	}
#endif /* MEM_ALLOC_INFO_SUPPORT */
	ASSERT(vmem);
	mt_os_vmem_free(vmem);

#ifdef VENDOR_FEATURE4_SUPPORT
	OS_NumOfVMemFree++;
#endif /* VENDOR_FEATURE4_SUPPORT */
}

#ifdef MEM_ALLOC_INFO_SUPPORT
VOID MemInfoListInital(VOID)
{
	MemInfoList.type = 0;
	PktInfoList.type = 1;
	MIListInit(&MemInfoList);
	MIListInit(&PktInfoList);
}

UINT32 ShowMemAllocInfo(UINT show, UINT64 pCaller)
{
	ShowMIList(&MemInfoList, show, pCaller);
	return MemInfoList.EntryNumber;
}


UINT32 ShowPktAllocInfo(UINT show, UINT64 pCaller)
{
	ShowMIList(&PktInfoList, show, pCaller);
	return PktInfoList.EntryNumber;
}

VOID MemInfoListExit(VOID)
{
	UINT32 memalctotal, pktalctotal;

	memalctotal = ShowMemAllocInfo(SHOW_ACTIVE_PCALLER_INFO, 0);
	pktalctotal = ShowPktAllocInfo(SHOW_ACTIVE_PCALLER_INFO, 0);

	if ((memalctotal != 0) || (pktalctotal != 0)) {
		MTWF_DBG(NULL, DBG_CAT_INTF, CATINTF_DOWN, DBG_LVL_ERROR,
				"Error: Memory leak  mem size=%d pkt num=%d!!\n", memalctotal, pktalctotal);
		ASSERT(0);
	}

	MIListExit(&MemInfoList);
	MIListExit(&PktInfoList);
}
#endif /* MEM_ALLOC_INFO_SUPPORT */

static void *os_sysinfo_root_dir;

void *os_sysinfo_root_dir_get(void)
{
	return os_sysinfo_root_dir;
}

int os_sysinfo_init(void)
{
	os_sysinfo_root_dir = mt_os_sysinfo_dir_create(NULL, "mtk_wifi", true);
	if (!os_sysinfo_root_dir)
		return -ENOENT;

	return 0;
}

void os_sysinfo_deinit(void)
{
	if (os_sysinfo_root_dir)
		mt_os_sysinfo_dir_destroy(os_sysinfo_root_dir);

	os_sysinfo_root_dir = NULL;
}


static void *global_lockless_ctx;

void *os_lockless_ctx_get(void)
{
	return global_lockless_ctx;
}

int os_lockless_init(void)
{
	return mt_os_lockless_ctx_init("mt_wifi", &global_lockless_ctx);
}

void os_lockless_deinit(void)
{
	mt_os_lockless_ctx_cleanup(&global_lockless_ctx);
}

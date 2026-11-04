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
#include <net/mac80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <net/arp.h>

#define SUBMODULE "mt_osal_wifi"
#include <mt_osal_debug.h>
#include <mt_osal_net.h>
#include <mt_osal_wifi.h>
#include <mt_osal_cfg80211.h>
#include <mt_osal_cfg80211_vndr.h>
#include <mt_osal_module.h>

#ifdef CFG80211_SUPPORT

#ifndef AP_SETTINGS_DOT11VMBSSID_SUPPORT
#define AP_SETTINGS_DOT11VMBSSID_SUPPORT   (0x1 << 16)
#endif

#define MSEC_TO_SEC(_msec)		((_msec) / MSEC_PER_SEC)

static u16 mtk_vht_high_rate_bw80[3][4] = {
	{292, 585, 877, 1170},
	{351, 702, 1053, 1404},
	{390, 780, 1170, 1560},
};

static const u8 mtk_cfg80211_chan[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,

	/* 802.11 UNI / HyperLan 2 */
	36, 40, 44, 48, 52, 56, 60, 64,

	/* 802.11 HyperLan 2 */
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136,

	/* 802.11 UNII */
	140, 144, 149, 153, 157, 161, 165, 169, 173, 177,

	1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41,

	45, 49, 53, 57, 61, 65,	69, 73, 77, 81,

	85, 89, 93, 97, 101, 105, 109, 113, 117,

	121, 125, 129, 133, 137, 141, 145, 149,

	153, 157, 161, 165, 169, 173, 177, 181,

	185, 189, 193, 197, 201, 205, 209, 213,

	217, 221, 225, 229, 233,
};

u8 mtk_cfg80211_radar_chan[] = {
	52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
};

/*
 *	Array of bitrates the hardware can operate with
 *	in this band. Must be sorted to give a valid "supported
 *	rates" IE, i.e. CCK rates first, then OFDM.
 *
 *	For HT, assign MCS in another structure, ieee80211_sta_ht_cap.
 */
const struct ieee80211_rate mtk_cfg80211_sup_rate[12] = {
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 10,
		.hw_value = 0,
		.hw_value_short = 0,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 20,
		.hw_value = 1,
		.hw_value_short = 1,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 55,
		.hw_value = 2,
		.hw_value_short = 2,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 110,
		.hw_value = 3,
		.hw_value_short = 3,
	},
	{
		.flags = 0,
		.bitrate = 60,
		.hw_value = 4,
		.hw_value_short = 4,
	},
	{
		.flags = 0,
		.bitrate = 90,
		.hw_value = 5,
		.hw_value_short = 5,
	},
	{
		.flags = 0,
		.bitrate = 120,
		.hw_value = 6,
		.hw_value_short = 6,
	},
	{
		.flags = 0,
		.bitrate = 180,
		.hw_value = 7,
		.hw_value_short = 7,
	},
	{
		.flags = 0,
		.bitrate = 240,
		.hw_value = 8,
		.hw_value_short = 8,
	},
	{
		.flags = 0,
		.bitrate = 360,
		.hw_value = 9,
		.hw_value_short = 9,
	},
	{
		.flags = 0,
		.bitrate = 480,
		.hw_value = 10,
		.hw_value_short = 10,
	},
	{
		.flags = 0,
		.bitrate = 540,
		.hw_value = 11,
		.hw_value_short = 11,
	},
};

const struct ieee80211_rate cfg80211_sup_rate[12] = {
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 10,
		.hw_value = 0,
		.hw_value_short = 0,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 20,
		.hw_value = 1,
		.hw_value_short = 1,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 55,
		.hw_value = 2,
		.hw_value_short = 2,
	},
	{
		.flags = IEEE80211_RATE_SHORT_PREAMBLE,
		.bitrate = 110,
		.hw_value = 3,
		.hw_value_short = 3,
	},
	{
		.flags = 0,
		.bitrate = 60,
		.hw_value = 4,
		.hw_value_short = 4,
	},
	{
		.flags = 0,
		.bitrate = 90,
		.hw_value = 5,
		.hw_value_short = 5,
	},
	{
		.flags = 0,
		.bitrate = 120,
		.hw_value = 6,
		.hw_value_short = 6,
	},
	{
		.flags = 0,
		.bitrate = 180,
		.hw_value = 7,
		.hw_value_short = 7,
	},
	{
		.flags = 0,
		.bitrate = 240,
		.hw_value = 8,
		.hw_value_short = 8,
	},
	{
		.flags = 0,
		.bitrate = 360,
		.hw_value = 9,
		.hw_value_short = 9,
	},
	{
		.flags = 0,
		.bitrate = 480,
		.hw_value = 10,
		.hw_value_short = 10,
	},
	{
		.flags = 0,
		.bitrate = 540,
		.hw_value = 11,
		.hw_value_short = 11,
	},
};

#if (KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
static const struct wiphy_iftype_ext_capab add_iftypes_ext_capa[] = {
	{
		.iftype = NL80211_IFTYPE_AP,
		.eml_capabilities = IEEE80211_EML_CAP_EMLSR_SUPP,
	},
	{
		.iftype = NL80211_IFTYPE_STATION,
		.eml_capabilities = IEEE80211_EML_CAP_EMLSR_SUPP,
	},
};
#endif

#if (KERNEL_VERSION(3, 0, 0) <= LINUX_VERSION_CODE)
static const struct ieee80211_iface_limit mtk_p2p_sta_go_limits[] = {
	{
		.max = 16,
		.types = BIT(NL80211_IFTYPE_STATION) |
		BIT(NL80211_IFTYPE_AP),
	},
};

static const struct ieee80211_iface_combination
	mtk_iface_combinations_p2p[] = {
	{
		.num_different_channels = 2,
		.max_interfaces = 8,
		/* .beacon_int_infra_match = true, */
		.limits = mtk_p2p_sta_go_limits,
		.n_limits = ARRAY_SIZE(mtk_p2p_sta_go_limits),
	},
};
#endif /* LINUX_VERSION_CODE: 3.8.0 */

static const struct ieee80211_txrx_stypes
	mtk_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_STATION] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) | BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) | BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4),
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4),
	},

};

static const u32 cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_AES_CMAC,
	WLAN_CIPHER_SUITE_BIP_CMAC_256,
	WLAN_CIPHER_SUITE_BIP_GMAC_128,
	WLAN_CIPHER_SUITE_BIP_GMAC_256,
	WLAN_CIPHER_SUITE_GCMP,
	WLAN_CIPHER_SUITE_CCMP_256,
	WLAN_CIPHER_SUITE_GCMP_256,
};

struct freqlist freq_range[] = {
	{2412, 2472},
	{4920, 5905},
	{5735, 7115},
	{0},
};
struct freqlist radar_freq[] = {
	{5250, 5350},
	{5470, 5725},
	{0}
};

static bool IsRadarFreq(u32 freq)
{
	struct freqlist *_radar_freq = radar_freq;

	while (_radar_freq->startFreq) {
		if (freq > _radar_freq->startFreq && freq < _radar_freq->endFreq)
			return true;

		_radar_freq++;
	}

	return false;
}

static bool is_radar_channel(u8 ch)
{
	u32 idx = 0;

	for (idx = 0; idx < sizeof(mtk_cfg80211_radar_chan); idx++) {
		if (mtk_cfg80211_radar_chan[idx] == ch)
			return true;
	}

	return false;
}

static int mt_cfg80211_init_ctrl(
	struct wiphy *pWiphy,
	enum mt_cfg80211_init_cmd cmd_id,
	void *data,
	unsigned int data_len)
{
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	int ret;
	struct mt_cfg80211_priv *mt_priv;
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct net_device *net_dev;

	mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(pWiphy));
	if (!mt_priv) {
		MT_DEBUG_TRACE("no mt priv");
		return -ENETDOWN;
	}
	net_dev = mt_priv->net_dev;
	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = net_dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->cmd = MT_IO_CFG80211_INIT;
	pIoctlConfig->subcmd = cmd_id;
	pIoctlConfig->data = data;
	pIoctlConfig->data_len = data_len;
	ret = net_priv_info->ioctl(net_dev, pIoctlConfig);
	return ret;
}

static int mt_cfg80211_ops_ctrl(
	struct wiphy *pWiphy,
	struct net_device *net_dev,
	enum mt_cfg80211_ops_cmd cmd_id,
	void *data,
	unsigned int data_len)
{
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	int ret;
	struct mt_cfg80211_priv *mt_priv = NULL;
	struct mt_net_dev_priv *net_priv_info = NULL;

	if (pWiphy)
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(pWiphy));
	if (!mt_priv) {
		MT_DEBUG_TRACE("no mt priv");
		return -ENETDOWN;
	}
	if (!net_dev)
		net_dev = mt_priv->net_dev;
	if (net_dev)
		net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = net_dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->cmd = MT_IO_CFG80211_OPS;
	pIoctlConfig->subcmd = cmd_id;
	pIoctlConfig->data = data;
	pIoctlConfig->data_len = data_len;
	ret = net_priv_info->ioctl(net_dev, pIoctlConfig);
	return ret;
}

void mt_cfg80211_scan_status_lock_init(
	struct wiphy *wiphy,
	bool init)
{
	struct mt_cfg80211_priv *mt_priv;

#ifdef __MOCK
	return;
#else

	if (wiphy) {
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));
		if (mt_priv && init)
			spin_lock_init(&mt_priv->scan_notify_lock);
	}
#endif /* !__MOCK */
}

static void mt_cfg80211_wiphy_feature_set(
	struct wiphy *prWiphy,
	unsigned int mtk_cfg8011_features)
{
	/* @NL80211_FEATURE_INACTIVITY_TIMER:
	 * This driver takes care of freeingup
	 * the connected inactive stations in AP mode.
	 */
	/*what if you get compile error for below flag, please add the patch into your kernel*/
	/* http://www.permalink.gmane.org/gmane.linux.kernel.wireless.general/86454 */
	MT_DEBUG_TRACE("mtk_cfg8011_features = 0x%x\n", mtk_cfg8011_features);
	prWiphy->features |= NL80211_FEATURE_INACTIVITY_TIMER;

	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE)
		prWiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;

	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_SAE)
		prWiphy->features |= NL80211_FEATURE_SAE;

	/* Enabling OCV to indicate Hostapd to support OCV Feature*/
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE || defined(BACKPORT_NOSTDINC)
	wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_OPERATING_CHANNEL_VALIDATION);
#endif
	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_MGMT_TX_RANDOM_TA)
		wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA);

#if KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE || defined(BACKPORT_NOSTDINC)
	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_BEACON_PROTECTION)
		wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_BEACON_PROTECTION);
#endif

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE || defined(BACKPORT_NOSTDINC)
	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_UNSOL_BCAST_PROBE_RESP)
		wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_UNSOL_BCAST_PROBE_RESP);
#endif

	wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_DFS_OFFLOAD);

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_RADAR_BACKGROUND)
		wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_RADAR_BACKGROUND);
#endif
	wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_BEACON_RATE_LEGACY);
	wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_ACK_SIGNAL_SUPPORT);
#if (KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	if (mtk_cfg8011_features & MT_CFG80211_FEATURE_VLAN_OFFLOAD)
		wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_VLAN_OFFLOAD);
#endif
}

static void mt_cfg80211_reg_notifier(
	struct wiphy *pWiphy,
	struct regulatory_request *pRequest)
{
	/* sanity check */
	if (pWiphy == NULL) {
		MT_DEBUG_TRACE("crda> reg notify but pWiphy = NULL!");
		return;
	}

	/*
	 *	Change the band settings (PASS scan, IBSS allow, or DFS) in mac80211
	 *	based on EEPROM.
	 *
	 *	IEEE80211_CHAN_DISABLED: This channel is disabled.
	 *	IEEE80211_CHAN_PASSIVE_SCAN: Only passive scanning is permitted
	 *				on this channel.
	 *	IEEE80211_CHAN_NO_IBSS: IBSS is not allowed on this channel.
	 *	IEEE80211_CHAN_RADAR: Radar detection is required on this channel.
	 *	IEEE80211_CHAN_NO_FAT_ABOVE: extension channel above this channel
	 *				is not permitted.
	 *	IEEE80211_CHAN_NO_FAT_BELOW: extension channel below this channel
	 *				is not permitted.
	 */

	switch (pRequest->initiator) {
	case NL80211_REGDOM_SET_BY_CORE:
		/* Core queried CRDA for a dynamic world regulatory domain. */
		MT_DEBUG_TRACE("crda> requlation requestion by core: ");
		break;

	case NL80211_REGDOM_SET_BY_USER:
		/*
		 *	User asked the wireless core to set the regulatory domain.
		 *	(when iw, network manager, wpa supplicant, etc.)
		 */
		MT_DEBUG_TRACE("crda> requlation requestion by user: ");
		break;

	case NL80211_REGDOM_SET_BY_DRIVER:
		/*
		 *	A wireless drivers has hinted to the wireless core it thinks
		 *	its knows the regulatory domain we should be in.
		 *	(when driver initialization, calling regulatory_hint)
		 */
		MT_DEBUG_TRACE("crda> requlation requestion by driver: ");
		break;

	case NL80211_REGDOM_SET_BY_COUNTRY_IE:
		/*
		 *	The wireless core has received an 802.11 country information
		 *	element with regulatory information it thinks we should consider.
		 *	(when beacon receive, calling regulatory_hint_11d)
		 */
		MT_DEBUG_TRACE("crda> requlation requestion by country IE: ");
		break;
	} /* End of switch */

	MT_DEBUG_TRACE("%c%c\n", pRequest->alpha2[0], pRequest->alpha2[1]);

	/* only follow rules from user */
	if (pRequest->initiator == NL80211_REGDOM_SET_BY_USER) {
		struct mt_cfg80211_io_reg_notify reg_info;

		reg_info.Alpha2[0] = pRequest->alpha2[0];
		reg_info.Alpha2[1] = pRequest->alpha2[1];
		reg_info.pWiphy = pWiphy;
		mt_cfg80211_init_ctrl(
			pWiphy,
			MT_80211_INIT_REG_NOTIFY_TO,
			&reg_info,
			sizeof(struct mt_cfg80211_io_reg_notify));
	}

	return;
}

u16 get_he_mcs_map(unsigned char nss, enum ieee80211_he_mcs_support sup)
{
	u16 he_mcs_map;
	int i = 0;

	for (i = 0, he_mcs_map = 0xFFFF; i < nss; i++)
		he_mcs_map = (he_mcs_map << 2) | sup;

	return cpu_to_le16(he_mcs_map);
}

static void CFG80211_sband_data_he_cap_init(
	struct mt_cfg80211_band *mt_band_info,
	enum nl80211_band band_idx,
	struct ieee80211_sband_iftype_data *data)
{
	struct ieee80211_sta_he_cap *he_cap = &data->he_cap;
	struct ieee80211_he_cap_elem *he_cap_elem;
	struct ieee80211_he_mcs_nss_supp *he_mcs_nss_supp;

	if (mt_band_info->phy_caps & MTK_PHY_CAP_HE) {
		he_cap->has_he = true;
		he_cap_elem = &he_cap->he_cap_elem;
		he_mcs_nss_supp = &he_cap->he_mcs_nss_supp;
		he_cap_elem->mac_cap_info[0] = IEEE80211_HE_MAC_CAP0_HTC_HE |
				IEEE80211_HE_MAC_CAP0_TWT_RES;
		he_cap_elem->mac_cap_info[1] =
				IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
				IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8;
		he_cap_elem->mac_cap_info[2] =
				IEEE80211_HE_MAC_CAP2_32BIT_BA_BITMAP |
				IEEE80211_HE_MAC_CAP2_ACK_EN |
				IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
				IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO;
#ifdef BACKPORT_NOSTDINC
		he_cap_elem->mac_cap_info[3] = IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3;
		he_cap_elem->mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU;
#else /* BACKPORT_NOSTDINC */
		he_cap_elem->mac_cap_info[3] = IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_VHT_2;
		he_cap_elem->mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMDSU_IN_AMPDU;
#endif /* !BACKPORT_NOSTDINC */
		if (band_idx == IEEE80211_BAND_2GHZ) {
			he_cap_elem->phy_cap_info[0] =
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G;
		} else {
			he_cap_elem->phy_cap_info[0] =
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
		}
		he_cap_elem->phy_cap_info[1] =
				IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A |
				IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
				IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS;
		he_cap_elem->phy_cap_info[2] =
				IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
				IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
				IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
				IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
				IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO;
		he_cap_elem->phy_cap_info[3] =
				IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_BPSK |
				IEEE80211_HE_PHY_CAP3_DCM_MAX_TX_NSS_1 |
				IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_BPSK |
				IEEE80211_HE_PHY_CAP3_DCM_MAX_RX_NSS_1 |
				IEEE80211_HE_PHY_CAP3_SU_BEAMFORMER;
		he_cap_elem->phy_cap_info[4] =
				IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE |
				IEEE80211_HE_PHY_CAP4_MU_BEAMFORMER |
				IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_ABOVE_80MHZ_8 |
				IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_8;
		he_cap_elem->phy_cap_info[5] =
				IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_UNDER_80MHZ_2 |
				IEEE80211_HE_PHY_CAP5_BEAMFORMEE_NUM_SND_DIM_ABOVE_80MHZ_2;
		he_cap_elem->phy_cap_info[6] =
				IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT |
				IEEE80211_HE_PHY_CAP6_PARTIAL_BANDWIDTH_DL_MUMIMO;
		he_cap_elem->phy_cap_info[7] =
#ifdef BACKPORT_NOSTDINC
				IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_SUPP |
#else
				IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_AR |
#endif
				IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI |
				IEEE80211_HE_PHY_CAP7_MAX_NC_7;
		he_cap_elem->phy_cap_info[8] =
				IEEE80211_HE_PHY_CAP8_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI |
				IEEE80211_HE_PHY_CAP8_20MHZ_IN_40MHZ_HE_PPDU_IN_2G |
				IEEE80211_HE_PHY_CAP8_20MHZ_IN_160MHZ_HE_PPDU |
				IEEE80211_HE_PHY_CAP8_80MHZ_IN_160MHZ_HE_PPDU;
		he_mcs_nss_supp->rx_mcs_80 =
			get_he_mcs_map(mt_band_info->RxStream, IEEE80211_HE_MCS_SUPPORT_0_11);
		he_mcs_nss_supp->tx_mcs_80 =
			get_he_mcs_map(mt_band_info->TxStream, IEEE80211_HE_MCS_SUPPORT_0_11);

		if (band_idx != IEEE80211_BAND_2GHZ) {
			he_mcs_nss_supp->rx_mcs_160 = he_mcs_nss_supp->rx_mcs_80;
			he_mcs_nss_supp->rx_mcs_80p80 = cpu_to_le16(0xffff);
			he_mcs_nss_supp->tx_mcs_160 = he_mcs_nss_supp->tx_mcs_80;
			he_mcs_nss_supp->tx_mcs_80p80 = cpu_to_le16(0xffff);
		}


		/*
		 * Set default PPE thresholds, with PPET16 set to 0, PPET8 set
		 * to 7
		 */
		he_cap->ppe_thres[0] = 0x61;
		he_cap->ppe_thres[1] = 0x1c;
		he_cap->ppe_thres[2] = 0xc7;
		he_cap->ppe_thres[3] = 0x71;
	}
#if (KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	if ((band_idx == IEEE80211_BAND_6GHZ)
		&& (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_6GHZ)) {
		struct ieee80211_he_6ghz_capa *he_6ghz_capa = &data->he_6ghz_capa;

		he_6ghz_capa->capa = IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN |
				IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP |
				IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS |
				IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS;
	}
#endif
}

static void CFG80211_sband_data_eht_cap_init(
	struct mt_cfg80211_band *mt_band_info,
	enum nl80211_band band_idx,
	struct ieee80211_sband_iftype_data *data)
{
#if (KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	enum mt_cfg80211_phy_cap phy_caps = mt_band_info->phy_caps;
	struct ieee80211_sta_eht_cap *eht_cap;
	struct ieee80211_eht_cap_elem_fixed *eht_cap_elem;
	struct ieee80211_eht_mcs_nss_supp *eht_mcs_nss_supp;
	u8 val;
	int nss = mt_band_info->max_nss;
	int sts = mt_band_info->max_nss;

#define SET_EHT_MAX_NSS(_bw, _val) do {			 \
		eht_mcs_nss_supp->bw._##_bw.rx_tx_mcs9_max_nss = _val;   \
		eht_mcs_nss_supp->bw._##_bw.rx_tx_mcs11_max_nss = _val;  \
		eht_mcs_nss_supp->bw._##_bw.rx_tx_mcs13_max_nss = _val;  \
	} while (0)

	eht_cap = &data->eht_cap;
	if (phy_caps & MTK_PHY_CAP_EHT) {
		eht_cap->has_eht = true;
		eht_cap_elem = &eht_cap->eht_cap_elem;
		eht_mcs_nss_supp = &eht_cap->eht_mcs_nss_supp;

		memset(eht_cap_elem, 0, sizeof(*eht_cap_elem));
		memset(eht_mcs_nss_supp, 0, sizeof(*eht_mcs_nss_supp));
		if ((band_idx == IEEE80211_BAND_2GHZ) &&
		    phy_caps & MTK_PHY_CAP_RFIC_24GHZ) {
			eht_cap_elem->mac_cap_info[0] =
				IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
				IEEE80211_EHT_MAC_CAP0_OM_CONTROL;
			eht_cap_elem->phy_cap_info[0] =
				IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE |
				u8_encode_bits(u8_get_bits(sts - 1, BIT(0)),
					IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK);
			eht_cap_elem->phy_cap_info[1] =
				u8_encode_bits(u8_get_bits(sts - 1, GENMASK(2, 1)),
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK);
			eht_cap_elem->phy_cap_info[2] =
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK);
			eht_cap_elem->phy_cap_info[3] =
				IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK;
			eht_cap_elem->phy_cap_info[4] =
				u8_encode_bits(min_t(int, sts - 1, 2),
					IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK);
			eht_cap_elem->phy_cap_info[5] =
				IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
				u8_encode_bits(IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_16US,
					IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK) |
				u8_encode_bits(u8_get_bits(0x11, GENMASK(1, 0)),
					IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK);

			val = 0x1;
			eht_cap_elem->phy_cap_info[6] =
				u8_encode_bits(u8_get_bits(0x11, GENMASK(4, 2)),
					       IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK) |
				u8_encode_bits(val, IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK);

			eht_cap_elem->phy_cap_info[7] = 0;

			val = u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_RX) |
			      u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_TX);
			SET_EHT_MAX_NSS(80, val);
		} else if ((band_idx == IEEE80211_BAND_5GHZ) &&
			   phy_caps & MTK_PHY_CAP_RFIC_5GHZ) {
			eht_cap_elem->mac_cap_info[0] =
				IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
				IEEE80211_EHT_MAC_CAP0_OM_CONTROL;
			eht_cap_elem->phy_cap_info[0] =
				IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE |
				u8_encode_bits(u8_get_bits(sts - 1, BIT(0)),
					IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK);
			eht_cap_elem->phy_cap_info[1] =
				u8_encode_bits(u8_get_bits(sts - 1, GENMASK(2, 1)),
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK) |
				u8_encode_bits(sts - 1,
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK);
			eht_cap_elem->phy_cap_info[2] =
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK) |
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK);
			eht_cap_elem->phy_cap_info[3] =
				IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK;
			eht_cap_elem->phy_cap_info[4] =
				u8_encode_bits(min_t(int, sts - 1, 2),
					IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK);
			eht_cap_elem->phy_cap_info[5] =
				IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
				u8_encode_bits(IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_16US,
					IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK) |
				u8_encode_bits(u8_get_bits(0x11, GENMASK(1, 0)),
					IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK);
			if (mt_band_info->phy_caps & MTK_PHY_CAP_BW160C_STD
				|| mt_band_info->phy_caps & MTK_PHY_CAP_BW160NC
				|| mt_band_info->phy_caps & MTK_PHY_CAP_BW160C)
				val = 0x7;
			else if (mt_band_info->phy_caps & MTK_PHY_CAP_BW80)
				val = 0x3;
			else
				val = 0x1;
			eht_cap_elem->phy_cap_info[6] =
				u8_encode_bits(u8_get_bits(0x11, GENMASK(4, 2)),
					       IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK) |
				u8_encode_bits(val, IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK);

			eht_cap_elem->phy_cap_info[7] =
				IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
				IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
				IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
				IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ;

			val = u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_RX) |
			      u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_TX);
			SET_EHT_MAX_NSS(80, val);
			SET_EHT_MAX_NSS(160, val);
		} else if ((band_idx == IEEE80211_BAND_6GHZ) &&
			   phy_caps & MTK_PHY_CAP_RFIC_6GHZ) {
			eht_cap_elem->mac_cap_info[0] =
				IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
				IEEE80211_EHT_MAC_CAP0_OM_CONTROL;
			eht_cap_elem->phy_cap_info[0] =
				IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
				IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE |
				u8_encode_bits(u8_get_bits(sts - 1, BIT(0)),
					IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK);
			eht_cap_elem->phy_cap_info[1] =
				u8_encode_bits(u8_get_bits(sts - 1, GENMASK(2, 1)),
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK) |
				u8_encode_bits(sts - 1,
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK) |
				u8_encode_bits(sts - 1,
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK);
			eht_cap_elem->phy_cap_info[2] =
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK) |
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK) |
				u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_320MHZ_MASK);
			eht_cap_elem->phy_cap_info[3] =
				IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
				IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
				IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK;
			eht_cap_elem->phy_cap_info[4] =
				u8_encode_bits(min_t(int, sts - 1, 2),
					IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK);
			eht_cap_elem->phy_cap_info[5] =
				IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
				u8_encode_bits(IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_16US,
					IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK) |
				u8_encode_bits(u8_get_bits(0x11, GENMASK(1, 0)),
					IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK);
			if (mt_band_info->phy_caps & MTK_PHY_CAP_BW320)
				val = 0xf;
			else if (mt_band_info->phy_caps & MTK_PHY_CAP_BW160C_STD
				|| mt_band_info->phy_caps & MTK_PHY_CAP_BW160NC
				|| mt_band_info->phy_caps & MTK_PHY_CAP_BW160C)
				val = 0x7;
			else if (mt_band_info->phy_caps & MTK_PHY_CAP_BW80)
				val = 0x3;
			else
				val = 0x1;

			eht_cap_elem->phy_cap_info[6] =
				u8_encode_bits(u8_get_bits(0x11, GENMASK(4, 2)),
					       IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK) |
				u8_encode_bits(val, IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK);

			eht_cap_elem->phy_cap_info[7] =
				IEEE80211_EHT_PHY_CAP7_20MHZ_STA_RX_NDP_WIDER_BW |
				IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
				IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
				IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
				IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ;

			val = u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_RX) |
			      u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_TX);

			SET_EHT_MAX_NSS(80, val);
			SET_EHT_MAX_NSS(160, val);

			if (mt_band_info->phy_caps & MTK_PHY_CAP_BW320) {
				eht_cap_elem->phy_cap_info[0] |=
					IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;
				eht_cap_elem->phy_cap_info[1] |=
					IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK;
				eht_cap_elem->phy_cap_info[2] |=
					IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_320MHZ_MASK;
				eht_cap_elem->phy_cap_info[7] |=
					IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_320MHZ |
					IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_320MHZ;
				SET_EHT_MAX_NSS(320, val);
			}
		}
	}
#undef SET_EHT_MAX_NSS
#endif /* (KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC) */
}

static void mt_cfg80211_sband_data_init(
	struct mt_cfg80211_priv *mt_priv,
	struct mt_cfg80211_band *mt_band_info,
	enum nl80211_band band_idx,
	struct ieee80211_supported_band *band_info,
	bool re_init)
{
	struct ieee80211_sband_iftype_data *data;

	if (!mt_band_info) {
		MT_DEBUG_TRACE("mt_band_info is null.\n");
		return;
	}
	if (!band_info) {
		MT_DEBUG_TRACE("band_info is null.\n");
		return;
	}

	if (!mt_priv
		|| (band_idx >= NUM_NL80211_BANDS)) {
		band_info->n_iftype_data = 0;
		MT_DEBUG_TRACE("band_idx = %d (NUM_NL80211_BANDS = %d)\n",
			band_idx, NUM_NL80211_BANDS);
		return;
	}
	data = &mt_priv->sband_data[band_idx];
	memset(data, 0, sizeof(struct ieee80211_sband_iftype_data));
	data->types_mask = BIT(NL80211_IFTYPE_AP);
	CFG80211_sband_data_he_cap_init(mt_band_info, band_idx, data);
	CFG80211_sband_data_eht_cap_init(mt_band_info, band_idx, data);
	band_info->n_iftype_data = 1;
	band_info->iftype_data = &mt_priv->sband_data[band_idx];

	/*
	 * ATE would call CFG80211OS_SupBandReInit directly
	 * (i.e. w/o calling CFG80211_SupBandInit)
	 */
	if (re_init) {
		data = (struct ieee80211_sband_iftype_data *)band_info->iftype_data;
		data->types_mask |= BIT(NL80211_IFTYPE_STATION);
	}
}

u16 get_vht_mcs_map(unsigned char nss, enum ieee80211_vht_mcs_support sup)
{
	u16 vht_mcs_map;
	int i = 0;

	for (i = 0, vht_mcs_map = 0xFFFF; i < nss; i++)
		vht_mcs_map = (vht_mcs_map << 2) | sup;

	return cpu_to_le16(vht_mcs_map);
}

static bool mt_cfg80211_sup_band_init(
	struct wiphy *wiphy,
	struct mt_cfg80211_band *mt_band_info,
	struct ieee80211_channel *pChannelsOrg,
	struct ieee80211_rate *pRatesOrg,
	u8 re_init)
{
	struct ieee80211_channel *pChannels = (struct ieee80211_channel *)pChannelsOrg;
	struct ieee80211_rate *pRates = (struct ieee80211_rate *)pRatesOrg;
	struct ieee80211_supported_band *pBand;
	u32 NumOfChan = 0, NumOfRate = 0;
	u32 IdLoop = 0;
	u32 CurTxPower;
	u16 vht_mcs;
	struct mt_cfg80211_priv *mt_priv;
	struct freqlist *range = freq_range;
	u32 freq = 0, band = 0, channel_num_2g = 0, channel_num_5g = 0, channel_num_6g = 0;

	mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));
	if (!mt_priv)
		return false;

	/* 1. Calcute the Channel Number */
	if (mt_band_info->phy_caps & MTK_PHY_CAP_5M_SHIFT) {
		channel_num_2g = MTK_CFG80211_NUM_OF_CHAN_2GHZ_5M_SHIFT;
		channel_num_5g = MTK_CFG80211_NUM_OF_CHAN_5GHZ_5M_SHIFT;
		channel_num_6g = MTK_CFG80211_NUM_OF_CHAN_6GHZ_5M_SHIFT;
	} else {
		channel_num_2g = MTK_CFG80211_NUM_OF_CHAN_2GHZ;
		channel_num_5g = MTK_CFG80211_NUM_OF_CHAN_5GHZ;
		channel_num_6g = MTK_CFG80211_NUM_OF_CHAN_6GHZ;
	}

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_24GHZ)
		NumOfChan += channel_num_2g;

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_5GHZ)
		NumOfChan += channel_num_5g;

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_6GHZ)
		NumOfChan += channel_num_6g;

	/*need to check why FlgIsBMode set to TRUE*/
	NumOfRate = 4 + 8;

	MT_DEBUG_TRACE("80211> mtk_phy_caps = 0x%x, NumOfChan= %d, re_init = %d\n",
		mt_band_info->phy_caps, NumOfChan, re_init);
	MT_DEBUG_TRACE("80211> Number of rate = %d\n", NumOfRate);

	/* 3. Allocate the Channel instance */
	if (pChannels == NULL && NumOfChan) {
		pChannels = kcalloc(NumOfChan, sizeof(*pChannels), GFP_KERNEL);

		if (!pChannels) {
			MT_DEBUG_TRACE("80211> ieee80211_channel allocation fail!\n");
			return false;
		}
	}

	/* 4. Allocate the Rate instance */
	if (pRates == NULL && NumOfRate) {
		pRates = kcalloc(NumOfRate, sizeof(*pRates), GFP_KERNEL);

		if (!pRates) {
			kfree(pChannels);
			MT_DEBUG_TRACE("80211> ieee80211_rate allocation fail!\n");
			return false;
		}
	}

	/* get TX power */
	CurTxPower = 0; /* unknown */
	MT_DEBUG_TRACE("80211> CurTxPower = %d dBm\n", CurTxPower);

	/* 5. init channel */
	if (!re_init) {
		if (mt_band_info->phy_caps & MTK_PHY_CAP_5M_SHIFT) {
			while (range->startFreq) {
				u32 end = range->endFreq;
				u32 start = range->startFreq;

				if (end < 4920)
					band = IEEE80211_BAND_2GHZ;
				else if (end < 5905)
					band = IEEE80211_BAND_5GHZ;
				else
					band = IEEE80211_BAND_6GHZ;

				for (freq = start; freq <= end; freq = freq + 5) {
					if (IdLoop >= NumOfChan)
						break;

					pChannels[IdLoop].band = band;
					pChannels[IdLoop].center_freq = freq;
					pChannels[IdLoop].hw_value = IdLoop;
					pChannels[IdLoop].max_power = CurTxPower;
					pChannels[IdLoop].max_antenna_gain = 0xff;
					pChannels[IdLoop].flags = 0;

					if (IsRadarFreq(freq)) {
						MT_DEBUG_TRACE("====> Radar Channel %d\n", freq);
						pChannels[IdLoop].flags |= IEEE80211_CHAN_RADAR;
						pChannels[IdLoop].dfs_state = NL80211_DFS_AVAILABLE;
					}

					IdLoop = IdLoop + 1;
				}
				range++;
			}
		} else {
			for (IdLoop = 0; IdLoop < NumOfChan; IdLoop++) {
				u32 center_freq;

				if (IdLoop >= (channel_num_5g + channel_num_2g)) {
					pChannels[IdLoop].band = IEEE80211_BAND_6GHZ;
					center_freq = ieee80211_channel_to_frequency(
						mtk_cfg80211_chan[IdLoop], IEEE80211_BAND_6GHZ);
				} else if (IdLoop >= channel_num_2g) {
					pChannels[IdLoop].band = IEEE80211_BAND_5GHZ;
					center_freq = ieee80211_channel_to_frequency(
						mtk_cfg80211_chan[IdLoop], IEEE80211_BAND_5GHZ);
				} else {
					pChannels[IdLoop].band = IEEE80211_BAND_2GHZ;
					center_freq = ieee80211_channel_to_frequency(
						mtk_cfg80211_chan[IdLoop], IEEE80211_BAND_2GHZ);
				}

				pChannels[IdLoop].center_freq = center_freq;
				pChannels[IdLoop].hw_value = IdLoop;
				pChannels[IdLoop].max_power = CurTxPower;

				pChannels[IdLoop].max_antenna_gain = 0xff;
				pChannels[IdLoop].flags = 0;

				/* if (RadarChannelCheck(pAd, Cfg80211_Chan[IdLoop])) */
				if (is_radar_channel(mtk_cfg80211_chan[IdLoop])) {
					MT_DEBUG_TRACE("====> Radar Channel %d\n",
						mtk_cfg80211_chan[IdLoop]);
					pChannels[IdLoop].flags |= IEEE80211_CHAN_RADAR;
					pChannels[IdLoop].dfs_state = NL80211_DFS_AVAILABLE;
				}
			}
		}

		/* 6. init rate */
		for (IdLoop = 0; IdLoop < NumOfRate; IdLoop++)
			memcpy(&pRates[IdLoop], &cfg80211_sup_rate[IdLoop], sizeof(*pRates));
	}

	/*		CFG_TODO:
	 *		enum ieee80211_rate_flags {
	 *			IEEE80211_RATE_SHORT_PREAMBLE	= 1<<0,
	 *			IEEE80211_RATE_MANDATORY_A	= 1<<1,
	 *			IEEE80211_RATE_MANDATORY_B	= 1<<2,
	 *			IEEE80211_RATE_MANDATORY_G	= 1<<3,
	 *			IEEE80211_RATE_ERP_G		= 1<<4,
	 *		};
	 */
	/* 7. Fill the Band 2.4GHz */
	pBand = &mt_priv->Cfg80211_bands[IEEE80211_BAND_2GHZ];

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_24GHZ) {
		pBand->n_channels = channel_num_2g;
		if (mt_band_info->FlgIsBMode)
			pBand->n_bitrates = 4;
		else
			pBand->n_bitrates = NumOfRate;

		pBand->channels = pChannels;
		pBand->bitrates = pRates;
		if (mt_band_info->phy_caps & MTK_PHY_CAP_HT) {
			/* for HT, assign pBand->ht_cap */
			pBand->ht_cap.ht_supported = true;
			pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
							IEEE80211_HT_CAP_SM_PS |
							IEEE80211_HT_CAP_SGI_40 |
							IEEE80211_HT_CAP_SGI_20 |
							IEEE80211_HT_CAP_TX_STBC |
							IEEE80211_HT_CAP_RX_STBC |
							IEEE80211_HT_CAP_DSSSCCK40 |
							IEEE80211_HT_CAP_LDPC_CODING |
							IEEE80211_HT_CAP_MAX_AMSDU |
							IEEE80211_HT_CAP_GRN_FLD;
			pBand->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K; /* 2 ^ 16 */
			pBand->ht_cap.ampdu_density = mt_band_info->MpduDensity;
			memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
			MT_DEBUG_TRACE("80211> TxStream = %d, RxStream = %d\n",
							mt_band_info->TxStream, mt_band_info->RxStream);
			switch (mt_band_info->RxStream) {
			case 1:
			default:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(150);
				break;

			case 2:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(300);
				break;

			case 3:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(450);
				break;

			case 4:
			    pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				pBand->ht_cap.mcs.rx_mask[3] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(600);
				break;
			}

			pBand->ht_cap.mcs.rx_mask[4] = 0x01; /* 40MHz*/
			pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
			if (mt_band_info->TxStream != mt_band_info->RxStream) {
				pBand->ht_cap.mcs.tx_params |= IEEE80211_HT_MCS_TX_RX_DIFF;
				pBand->ht_cap.mcs.tx_params |=
					(((mt_band_info->TxStream - 1) & 0x03) <<
					IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT);
			}
		}
		mt_cfg80211_sband_data_init(
			mt_priv, mt_band_info, IEEE80211_BAND_2GHZ, pBand, re_init);
		wiphy->bands[IEEE80211_BAND_2GHZ] = pBand;
	} else {
		wiphy->bands[IEEE80211_BAND_2GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
		pBand->n_iftype_data = 0;
	}

	/* 8. Fill the Band 5GHz */
	pBand = &mt_priv->Cfg80211_bands[IEEE80211_BAND_5GHZ];

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_5GHZ) {
		pBand->n_channels = channel_num_5g;
		pBand->n_bitrates = NumOfRate - 4;	/*Disable 11B rate*/
		pBand->channels = &pChannels[channel_num_2g];
		pBand->bitrates = &pRates[4];
		if (mt_band_info->phy_caps & MTK_PHY_CAP_HT) {
			/* for HT, assign pBand->ht_cap */
			pBand->ht_cap.ht_supported = true;
			pBand->ht_cap.cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
								IEEE80211_HT_CAP_SM_PS |
								IEEE80211_HT_CAP_SGI_40 |
								IEEE80211_HT_CAP_SGI_20 |
								IEEE80211_HT_CAP_TX_STBC |
							    IEEE80211_HT_CAP_RX_STBC |
								IEEE80211_HT_CAP_DSSSCCK40 |
								IEEE80211_HT_CAP_LDPC_CODING |
								IEEE80211_HT_CAP_MAX_AMSDU |
								IEEE80211_HT_CAP_GRN_FLD;
			pBand->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K; /* 2 ^ 16 */
			pBand->ht_cap.ampdu_density = mt_band_info->MpduDensity;
			memset(&pBand->ht_cap.mcs, 0, sizeof(pBand->ht_cap.mcs));
			MT_DEBUG_TRACE("80211> TxStream = %d, RxStream = %d\n",
				mt_band_info->TxStream, mt_band_info->RxStream);
			switch (mt_band_info->RxStream) {
			case 1:
			default:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(150);
				break;

			case 2:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(300);
				break;

			case 3:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(450);
				break;

			case 4:
				pBand->ht_cap.mcs.rx_mask[0] = 0xff;
				pBand->ht_cap.mcs.rx_mask[1] = 0xff;
				pBand->ht_cap.mcs.rx_mask[2] = 0xff;
				pBand->ht_cap.mcs.rx_mask[3] = 0xff;
				pBand->ht_cap.mcs.rx_highest = cpu_to_le16(600);
				break;
			}

			pBand->ht_cap.mcs.rx_mask[4] = 0x01; /* 40MHz*/
			pBand->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
			if (mt_band_info->TxStream != mt_band_info->RxStream) {
				pBand->ht_cap.mcs.tx_params |= IEEE80211_HT_MCS_TX_RX_DIFF;
				pBand->ht_cap.mcs.tx_params |=
					(((mt_band_info->TxStream - 1) & 0x03) <<
					IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT);
			}
		}
		if (mt_band_info->phy_caps & MTK_PHY_CAP_VHT) {
			pBand->vht_cap.vht_supported = true;
			pBand->vht_cap.cap = IEEE80211_VHT_CAP_RXLDPC  |
						IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
						IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
						IEEE80211_VHT_CAP_SHORT_GI_80	|
						IEEE80211_VHT_CAP_SHORT_GI_160 |
						IEEE80211_VHT_CAP_TXSTBC |
						IEEE80211_STA_RX_BW_80 |
						IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK |
						IEEE80211_VHT_CAP_RXSTBC_MASK |
						IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE |
						IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |
						IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE |
						IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE |
						IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN |
						IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN;
			vht_mcs = get_vht_mcs_map(mt_band_info->RxStream,
				IEEE80211_VHT_MCS_SUPPORT_0_9);
			pBand->vht_cap.vht_mcs.rx_mcs_map = vht_mcs;
			pBand->vht_cap.vht_mcs.rx_highest =
				cpu_to_le16(mtk_vht_high_rate_bw80[2][mt_band_info->RxStream&0x03]);

			vht_mcs = get_vht_mcs_map(mt_band_info->TxStream,
				IEEE80211_VHT_MCS_SUPPORT_0_9);

			pBand->vht_cap.vht_mcs.tx_mcs_map = vht_mcs;
			pBand->vht_cap.vht_mcs.tx_highest =
				cpu_to_le16(mtk_vht_high_rate_bw80[2][mt_band_info->TxStream&0x03]);
		}
		mt_cfg80211_sband_data_init(mt_priv, mt_band_info, IEEE80211_BAND_5GHZ, pBand, re_init);
		wiphy->bands[IEEE80211_BAND_5GHZ] = pBand;
	} else {
		wiphy->bands[IEEE80211_BAND_5GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
		pBand->n_iftype_data = 0;
	}

	/* 9. Fill the Band 6GHz */
	pBand = &mt_priv->Cfg80211_bands[IEEE80211_BAND_6GHZ];

	if (mt_band_info->phy_caps & MTK_PHY_CAP_RFIC_6GHZ) {
		pBand->n_channels = channel_num_6g;
		pBand->n_bitrates = NumOfRate - 4;	/*Disable 11B rate*/
		pBand->channels =
			&pChannels[channel_num_2g + channel_num_5g];
		pBand->bitrates = &pRates[4];
		mt_cfg80211_sband_data_init(mt_priv, mt_band_info, IEEE80211_BAND_6GHZ, pBand, re_init);
		wiphy->bands[IEEE80211_BAND_6GHZ] = pBand;
	} else {
		wiphy->bands[IEEE80211_BAND_6GHZ] = NULL;
		pBand->channels = NULL;
		pBand->bitrates = NULL;
		pBand->n_iftype_data = 0;
	}

	/* 10. re-assign to mainDevice info */
	mt_priv->pCfg80211_Channels = pChannels;
	mt_priv->pCfg80211_Rates = pRates;
	return true;
}

static bool CFG80211_FILL_BSS_PARAM(
	struct mt_cfg80211_sta_bss_info *pbss_info,
	struct station_info *pstainfo)
{
	if (NULL == pbss_info || NULL == pstainfo) {
		MT_DEBUG_TRACE("ERROR, pbss_info = %p, pstainfo = %p;\n",
		pbss_info, pstainfo);
		return false;
	}
	MT_DEBUG_TRACE("80211> ==>\n");

	pstainfo->filled |= BIT(NL80211_STA_INFO_BSS_PARAM);
	/*dtim and beacon interval*/
	pstainfo->bss_param.dtim_period = pbss_info->dtim_period;
	pstainfo->bss_param.beacon_interval = pbss_info->beacon_interval;

	if (pbss_info->flags & BSS_PARAM_FLAGS_CTS_PROT) {
		MT_DEBUG_TRACE("\nBSS_PARAM_FLAGS_CTS_PROT: TRUE\n");
		pstainfo->bss_param.flags |= BSS_PARAM_FLAGS_CTS_PROT;
	} else {
		MT_DEBUG_TRACE("\nBSS_PARAM_FLAGS_CTS_PROT: FALSE\n");
		pstainfo->bss_param.flags &= ~BSS_PARAM_FLAGS_CTS_PROT;
	}

	if (pbss_info->flags & BSS_PARAM_FLAGS_SHORT_PREAMBLE) {
		pstainfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_PREAMBLE;
	} else {
		pstainfo->bss_param.flags &= ~BSS_PARAM_FLAGS_SHORT_PREAMBLE;
	}

	if (pbss_info->flags & BSS_PARAM_FLAGS_SHORT_SLOT_TIME) {
		pstainfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
	} else {
		pstainfo->bss_param.flags &= ~BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
	}

	return true;
}

static int CFG80211_FILL_BW(u8 *bw)
{

	if (NULL == bw) {
		MT_DEBUG_TRACE("ERROR, Invalid data bw = NULL;\n");
		return -EINVAL;
	}

	MT_DEBUG_TRACE("80211> ==> mtk priv bw = %d\n", *bw);
	switch (*bw) {
	case MT_NL80211_CHAN_WIDTH_5:
		*bw = RATE_INFO_BW_5;
		break;
	case MT_NL80211_CHAN_WIDTH_10:
		*bw = RATE_INFO_BW_10;
		break;
	case MT_NL80211_CHAN_WIDTH_20:
		*bw = RATE_INFO_BW_20;
		break;
	case MT_NL80211_CHAN_WIDTH_40:
		*bw = RATE_INFO_BW_40;
		break;
	case MT_NL80211_CHAN_WIDTH_80:
		*bw = RATE_INFO_BW_80;
		break;
	case MT_NL80211_CHAN_WIDTH_160:
	case MT_NL80211_CHAN_WIDTH_80P80:
		*bw = RATE_INFO_BW_160;
		break;
	case MT_NL80211_CHAN_WIDTH_320:
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 5, 0) <= CFG_CFG80211_VERSION)
		*bw = RATE_INFO_BW_320;
#else
		*bw = 7; /* RATE_INFO_BW_320 */
#endif
	default:
		MT_DEBUG_TRACE("default ERROR, Invalid data bw = %d;\n", *bw);
		break;
	}

	MT_DEBUG_TRACE("80211> ==> cfg80211 bw = %d\n", *bw);
	return 0;
}

static bool CFG80211_FILL_STA_FLAGS(
	struct mt_nl80211_sta_flag drv_sta_flags,
	struct station_info *pstainfo)
{
	struct nl80211_sta_flag_update *sta_flags;

	MT_DEBUG_TRACE("80211> ==>\n");

	if (NULL == pstainfo) {
		MT_DEBUG_TRACE("ERROR, Invalid data bw = NULL;\n");
		return false;
	}

	pstainfo->filled |= BIT(NL80211_STA_INFO_STA_FLAGS);
	sta_flags = &pstainfo->sta_flags;

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_AUTHORIZED);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_AUTHORIZED);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_AUTHORIZED));
		}
	}

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_SHORT_PREAMBLE)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_SHORT_PREAMBLE)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_SHORT_PREAMBLE));
		}
	}

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_WME)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_WME);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_WME)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_WME);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_WME));
		}
	}

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_MFP)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_MFP);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_MFP)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_MFP);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_MFP));
		}
	}

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_AUTHENTICATED)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_AUTHENTICATED);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_AUTHENTICATED)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_AUTHENTICATED);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_AUTHENTICATED));
		}
	}

	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_TDLS_PEER);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_TDLS_PEER);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_TDLS_PEER));
		}
	}
	if (drv_sta_flags.mask & BIT(NL80211_STA_FLAG_ASSOCIATED)) {
		sta_flags->mask |= BIT(NL80211_STA_FLAG_ASSOCIATED);
		if (drv_sta_flags.set & BIT(NL80211_STA_FLAG_ASSOCIATED)) {
			sta_flags->set |= BIT(NL80211_STA_FLAG_ASSOCIATED);
		} else {
			sta_flags->set &= ~(BIT(NL80211_STA_FLAG_ASSOCIATED));
		}
	}

	return true;
}

static int mt_cfg80211_sta_set_sinfo(
	struct station_info *pSinfo,
	struct mt_priv_ops_sta_info *pStaInfo)
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	u8 i;
#endif
	/* fill tx rx rate */
	pSinfo->txrate.flags = pStaInfo->txrate.flags;
	pSinfo->txrate.mcs = pStaInfo->txrate.mcs;
	pSinfo->txrate.legacy = pStaInfo->txrate.legacy;
	pSinfo->txrate.nss = pStaInfo->txrate.nss;
	pSinfo->txrate.bw = pStaInfo->txrate.bw;
	pSinfo->rxrate.flags = pStaInfo->rxrate.flags;
	pSinfo->rxrate.mcs = pStaInfo->rxrate.mcs;
	pSinfo->rxrate.legacy = pStaInfo->rxrate.legacy;
	pSinfo->rxrate.nss = pStaInfo->rxrate.nss;
	pSinfo->rxrate.bw = pStaInfo->rxrate.bw;
	CFG80211_FILL_BW(&pSinfo->txrate.bw);
	CFG80211_FILL_BW(&pSinfo->rxrate.bw);

	/* phymode bw mcs sanity check */
	if (pSinfo->txrate.flags & RATE_INFO_FLAGS_VHT_MCS)
		if (pSinfo->txrate.mcs > 9 || pSinfo->txrate.bw > RATE_INFO_BW_160)
			goto ERROR;
	if (pSinfo->rxrate.flags & RATE_INFO_FLAGS_VHT_MCS)
		if (pSinfo->rxrate.mcs > 9 || pSinfo->rxrate.bw > RATE_INFO_BW_160)
			goto ERROR;

	if (pSinfo->txrate.flags & RATE_INFO_FLAGS_HE_MCS)
		if (pSinfo->txrate.mcs > 11 || pSinfo->txrate.bw > RATE_INFO_BW_160)
			goto ERROR;
	if (pSinfo->rxrate.flags & RATE_INFO_FLAGS_HE_MCS)
		if (pSinfo->rxrate.mcs > 11 || pSinfo->rxrate.bw > RATE_INFO_BW_160)
			goto ERROR;

#ifdef IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ
	if (pSinfo->txrate.flags & RATE_INFO_FLAGS_EHT_MCS)
		if (pSinfo->txrate.mcs > 13 || pSinfo->txrate.bw > RATE_INFO_BW_320)
			goto ERROR;
	if (pSinfo->rxrate.flags & RATE_INFO_FLAGS_EHT_MCS)
		if (pSinfo->rxrate.mcs > 13 || pSinfo->rxrate.bw > RATE_INFO_BW_320)
			goto ERROR;
#endif /* IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ */


	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BITRATE);
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_BITRATE);

	/* fill signal */
	pSinfo->signal = pStaInfo->Signal;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_SIGNAL);

	/* fill ack signal */
	pSinfo->ack_signal = pStaInfo->ack_signal;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_ACK_SIGNAL);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	/* fill chain signal */
	pSinfo->chains = pStaInfo->chains;
	for (i = 0; i < IEEE80211_MAX_CHAINS; i++)
		pSinfo->chain_signal[i] = pStaInfo->chain_signal[i];
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL);
#endif

	/*tx retries*/
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_RETRIES);
	pSinfo->tx_retries = (u32)pStaInfo->tx_retries;

	/*tx_failed*/
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_FAILED);
	pSinfo->tx_failed = (u32)pStaInfo->tx_failed;

	/* fill tx/rx count */
	pSinfo->rx_bytes = pStaInfo->rx_bytes;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_BYTES);

	pSinfo->tx_bytes = pStaInfo->tx_bytes;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BYTES);

	pSinfo->rx_packets = pStaInfo->rx_packets;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_PACKETS);

	pSinfo->tx_packets = pStaInfo->tx_packets;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_PACKETS);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	/* fill tx duration */
	pSinfo->tx_duration = pStaInfo->tx_duration;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_DURATION);

	/* fill rx duration*/
	pSinfo->rx_duration = pStaInfo->rx_duration;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_RX_DURATION);
#endif
	/* fill inactive time */
	pSinfo->inactive_time = pStaInfo->InactiveTime;
	pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_INACTIVE_TIME);

	/*fill stainfos*/
	CFG80211_FILL_STA_FLAGS(pStaInfo->sta_flags, pSinfo);

	if (pStaInfo->sta_flags.mask & BIT_ULL(NL80211_STA_FLAG_ASSOCIATED)) {

		/* fill connected time */
		pSinfo->connected_time = pStaInfo->connected_time;
		pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_CONNECTED_TIME);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
		/* fill assoc at boottime */
		pSinfo->assoc_at = pStaInfo->assoc_at;
		pSinfo->filled |= BIT_ULL(NL80211_STA_INFO_ASSOC_AT_BOOTTIME);
#endif
		/*fill bss parameter*/
		CFG80211_FILL_BSS_PARAM(&pStaInfo->bss_param, pSinfo);
	}

	return 0;

ERROR:
	MT_DEBUG_TRACE("Tx: pSinfo->flag=%d, pSinfo->mcs=%d, pSinfo->legacy=%d, pSinfo->nss=%d, pSinfo->bw=%d\n",
		pSinfo->txrate.flags, pSinfo->txrate.mcs, pSinfo->txrate.legacy, pSinfo->txrate.nss, pSinfo->txrate.bw);
	MT_DEBUG_TRACE("Rx: pSinfo->flag=%d, pSinfo->mcs=%d, pSinfo->legacy=%d, pSinfo->nss=%d, pSinfo->bw=%d\n",
		pSinfo->rxrate.flags, pSinfo->rxrate.mcs, pSinfo->rxrate.legacy, pSinfo->rxrate.nss, pSinfo->rxrate.bw);

	return -EINVAL;

}

static int mt_cfg80211_ops_set_bit_rate(
	struct wiphy *wiphy,
	struct net_device *netdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	unsigned int link_id,
#endif
	const u8 *addr,
	const struct cfg80211_bitrate_mask *mask)
{
	struct mt_phy_cap mt_phy_cap;
	u8 band, i;
	struct mt_cfg80211_bit_rate_info bit_rate_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!wiphy || !netdev || !mask) {
		MT_DEBUG_TRACE("failed - [ERROR]input NULL pointer.\n");
		return 0;
	}

	MT_DEBUG_TRACE("80211> ==>\n");
	memset(&mt_phy_cap, 0, sizeof(mt_phy_cap));
	mt_cfg80211_init_ctrl(
		wiphy, MT_80211_GET_PHY_CAP, &mt_phy_cap, sizeof(struct mt_phy_cap));
	MT_DEBUG_TRACE("80211> mt_phy_cap.phy_mode = %d\n", mt_phy_cap.phy_mode);
	band = mt_cfg80211_get_nl80211_band(mt_phy_cap.phy_mode);
	memset(&bit_rate_info, 0, sizeof(struct mt_cfg80211_bit_rate_info));
	bit_rate_info.he_gi = MTK_CAPI_HE_GI_NUM;
	bit_rate_info.he_ltf = MTK_CAPI_HE_LTF_NUM;
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE || defined(BACKPORT_NOSTDINC)
	switch (mask->control[band].he_gi) {
	case NL80211_RATE_INFO_HE_GI_0_8:
		bit_rate_info.he_gi = MTK_CAPI_HE_08_GI;
		break;
	case NL80211_RATE_INFO_HE_GI_1_6:
		bit_rate_info.he_gi = MTK_CAPI_HE_16_GI;
		break;
	case NL80211_RATE_INFO_HE_GI_3_2:
		bit_rate_info.he_gi = MTK_CAPI_HE_32_GI;
		break;
	default:
		MT_DEBUG_TRACE("Wrong Input: band = %d, he_gi = %d!\n",
			band, mask->control[band].he_gi);
		break;
	}

	switch (mask->control[band].he_ltf) {
	case NL80211_RATE_INFO_HE_1XLTF:
		bit_rate_info.he_ltf = MTK_CAPI_HE_1x_LTF;
		break;
	case NL80211_RATE_INFO_HE_2XLTF:
		bit_rate_info.he_ltf = MTK_CAPI_HE_2x_LTF;
		break;
	case NL80211_RATE_INFO_HE_4XLTF:
		bit_rate_info.he_ltf = MTK_CAPI_HE_4x_LTF;
		break;
	default:
		MT_DEBUG_TRACE("Wrong Input: band = %d, he_ltf = %d!\n",
			band, mask->control[band].he_ltf);
		break;
	}
#endif
	bit_rate_info.legacy = mask->control[band].legacy;
	for (i = 0; i < MTK_MAX_STREAM; i++) {
		bit_rate_info.ht_mcs[i] = mask->control[band].ht_mcs[i];
		bit_rate_info.vht_mcs[i] = mask->control[band].vht_mcs[i];
	}
	ctrl_data_ptr->data = &bit_rate_info;
	ctrl_data_ptr->data_len = sizeof(bit_rate_info);
	mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_set_bit_rate,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static struct wireless_dev *mt_cfg80211_ops_virtual_inf_add(
	struct wiphy *wiphy,
	const char *name,
	unsigned char name_assign_type,
	enum nl80211_iftype type,
	struct vif_params *params)
{
	struct mt_priv_ops_vif_set vif_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	int ret = 0;

	MT_DEBUG_TRACE("80211> ==>type: %d, name: %s\n", type, name);

	if (mt_cfg80211_get_mac_ad(wiphy) == NULL)
		return 0;
	vif_info.vifType = type;
	vif_info.vifNameLen = strlen(name);
	vif_info.wl_dev = NULL;

	if (vif_info.vifNameLen < IFNAMSIZ) {
		memset(vif_info.vifName, 0, sizeof(vif_info.vifName));
		memcpy(vif_info.vifName, name, vif_info.vifNameLen);
	} else {
		MT_DEBUG_TRACE("Too long interface name!!!\n");
		return ERR_PTR(-EINVAL);
	}

	if ((wiphy->flags) & (WIPHY_FLAG_4ADDR_STATION))
		MT_DEBUG_TRACE("Support WIPHY_FLAG_4ADDR_STATION\n");

	vif_info.vif_exist = 0;
	vif_info.is_main_if = 0;
	vif_info.flags = wiphy->flags;
	ctrl_data_ptr->data = &vif_info;
	ctrl_data_ptr->data_len = sizeof(vif_info);
	ret = mt_cfg80211_ops_ctrl(wiphy, NULL, mtk_ops_add_vif_inf,
		ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));

	MT_DEBUG_TRACE("80211> ==>ret: %d, wl_dev: %p\n", ret, vif_info.wl_dev);
	if (ret == 0)
		return vif_info.wl_dev;
	if (vif_info.vif_exist || vif_info.is_main_if)
		return ERR_PTR(-ENFILE);
	return ERR_PTR(-EINVAL);

}

static int mt_cfg80211_ops_virtual_inf_del(
	struct wiphy *wiphy,
	struct wireless_dev *pwdev)
{
	struct net_device *dev = NULL;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	dev = pwdev->netdev;
	if (!dev)
		return 0;
	MT_DEBUG_TRACE("80211> %s [%d]==> dev: %p\n", dev->name, dev->ieee80211_ptr->iftype, dev);
	ctrl_data_ptr->uint_val = dev->ieee80211_ptr->iftype;
	ctrl_data_ptr->net_dev = dev;

	if (dev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP_VLAN)
		mt_cfg80211_ops_ctrl(wiphy, NULL, mtk_ops_del_vif_inf,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	else
		mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_del_vif_inf,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

#define MTK_OFDM_RATE_IDX_OFFSET 4
static int mt_cfg80211_ops_start_ap(
	struct wiphy *wiphy,
	struct net_device *netdev,
	struct cfg80211_ap_settings *settings)
{
	struct mt_priv_ops_ap_setting ap_setting;
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	unsigned char *beacon_head_buf = NULL, *beacon_tail_buf = NULL;

	ctrl_data_ptr->net_dev = NULL;
	if (netdev) {
		MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);
		ctrl_data_ptr->net_dev = netdev;
	}

	if (settings->chandef.chan == NULL) {
		MT_DEBUG_TRACE("failed - [ERROR]chandef.chan NULL.\n");
		ret = -EINVAL;
		goto end;
	}

	ap_setting.bcn_cfg.flags = 0;
#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE
	if (settings->flags & AP_SETTINGS_DOT11VMBSSID_SUPPORT)
		ap_setting.bcn_cfg.flags |= MT_FEATURE_11V_MBSS;
#endif
	if (settings->beacon.head_len > 0) {
		beacon_head_buf = (unsigned char *)kmalloc(settings->beacon.head_len, GFP_ATOMIC);
		if (beacon_head_buf == NULL) {
			MT_DEBUG_TRACE("memory alloc failed\n");
			ret = -ENOMEM;
			goto end;
		}
		memcpy(beacon_head_buf, settings->beacon.head, settings->beacon.head_len);
	}

	if (settings->beacon.tail_len > 0) {
		beacon_tail_buf = (unsigned char *)kmalloc(settings->beacon.tail_len, GFP_ATOMIC);
		if (beacon_tail_buf == NULL) {
			MT_DEBUG_TRACE("memory alloc failed\n");
			ret = -ENOMEM;
			goto end;
		}
		memcpy(beacon_tail_buf, settings->beacon.tail, settings->beacon.tail_len);
	}
	memset(&ap_setting, 0, sizeof(ap_setting));
	ap_setting.bcn.beacon_head_len = settings->beacon.head_len;
	ap_setting.bcn.beacon_tail_len = settings->beacon.tail_len;
	ap_setting.bcn.beacon_head = beacon_head_buf;
	ap_setting.bcn.beacon_tail = beacon_tail_buf;
	ap_setting.bcn_cfg.dtim_period = settings->dtim_period;
	ap_setting.bcn_cfg.interval = settings->beacon_interval;
	ap_setting.bcn_cfg.ssid_len = settings->ssid_len;
	ap_setting.bcn_cfg.privacy = settings->privacy;
	ap_setting.bcn_cfg.band = settings->chandef.chan->band;
	if (ap_setting.bcn_cfg.band < 0 || ap_setting.bcn_cfg.band >= NUM_NL80211_BANDS) {
		MT_DEBUG_TRACE("invalid band:%d\n", ap_setting.bcn_cfg.band);
		ret = -EINVAL;
		goto end;
	}

	if (settings->beacon_rate.control[ap_setting.bcn_cfg.band].legacy > 0) {
		u8 rate_idx = 0;
		u32 arr_size = ARRAY_SIZE(mtk_cfg80211_sup_rate);

		rate_idx = ffs(settings->beacon_rate.control[ap_setting.bcn_cfg.band].legacy) - 1;
		if (rate_idx >= arr_size) {
			MT_DEBUG_TRACE("invalid rate_idx:%d\n", rate_idx);
			ret = -EINVAL;
			goto end;
		}
		if (ap_setting.bcn_cfg.band == NL80211_BAND_2GHZ) {
			ap_setting.bcn_cfg.bitrate = mtk_cfg80211_sup_rate[rate_idx].bitrate;
		} else {
			if (rate_idx + MTK_OFDM_RATE_IDX_OFFSET >= arr_size) {
				MT_DEBUG_TRACE("invalid rate_idx:%d\n", rate_idx);
				ret = -EINVAL;
				goto end;
			}
			ap_setting.bcn_cfg.bitrate =
				mtk_cfg80211_sup_rate[rate_idx + MTK_OFDM_RATE_IDX_OFFSET].bitrate;
		}
	}

	if (settings->crypto.akm_suites[0] == WLAN_AKM_SUITE_8021X) {
		MT_DEBUG_TRACE("80211> This is a 1X wdev\n");
		ap_setting.bcn_cfg.enable_1x = 1;
	}

	memset(ap_setting.bcn_cfg.ssid, 0, MTK_MAX_LEN_OF_SSID);
	if (settings->ssid_len <= 32)
		memcpy(ap_setting.bcn_cfg.ssid, settings->ssid, settings->ssid_len);
	ap_setting.bcn_cfg.auth_type = settings->auth_type;
	ap_setting.bcn_cfg.hidden_ssid = settings->hidden_ssid;

	ap_setting.bcn_cfg.unsol_6g_interval = 0;
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE || defined(BACKPORT_NOSTDINC)
	if ((ap_setting.bcn_cfg.band == NL80211_BAND_6GHZ)
		&& (settings->unsol_bcast_probe_resp.interval > 0))
		ap_setting.bcn_cfg.unsol_6g_interval = settings->unsol_bcast_probe_resp.interval;
#endif

	/* get channel number */
	ap_setting.chan_info.ChanId = ieee80211_frequency_to_channel(
		settings->chandef.chan->center_freq);
	ap_setting.chan_info.CenterChanId = ieee80211_frequency_to_channel(
		settings->chandef.center_freq1);
	MT_DEBUG_TRACE("80211> Channel = %d, CenterChanId = %d\n",
		ap_setting.chan_info.ChanId, ap_setting.chan_info.CenterChanId);

	ap_setting.chan_info.IfType = NL80211_IFTYPE_P2P_GO;

	MT_DEBUG_TRACE("80211> ChanInfo.IfType == %d!\n", ap_setting.chan_info.IfType);

	switch (settings->chandef.width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_NOHT;
		break;

	case NL80211_CHAN_WIDTH_20:
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_HT20;
		break;

	case NL80211_CHAN_WIDTH_40:
		if (settings->chandef.center_freq1 > settings->chandef.chan->center_freq)
			ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_HT40PLUS;
		else
			ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_HT40MINUS;
		break;

	case NL80211_CHAN_WIDTH_80:
		MT_DEBUG_TRACE("80211> NL80211_CHAN_WIDTH_80 CtrlCh: %d, CentCh: %d\n",
			ap_setting.chan_info.ChanId, ap_setting.chan_info.CenterChanId);
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_VHT80;
		break;

		/* Separated BW 80 and BW 160 is not supported yet */
	case NL80211_CHAN_WIDTH_80P80:
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_VHT80P80;
		break;
	case NL80211_CHAN_WIDTH_160:
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_VHT160;
		break;

	default:
		MT_DEBUG_TRACE("80211> Unsupported Chan Width: %d\n",
			settings->chandef.width);
		ap_setting.chan_info.ChanType = MTK_CMD_80211_CHANTYPE_NOHT;
		break;
	}

	MT_DEBUG_TRACE("80211> ChanInfo.ChanType == %d!\n",
		ap_setting.chan_info.ChanType);

	/*ht green filed*/
	ap_setting.bcn_cfg.ht_cap = MTK_NON_HT;
	if (settings->ht_cap) {
		if (settings->ht_cap->cap_info & IEEE80211_HT_CAP_GRN_FLD)
			ap_setting.bcn_cfg.ht_cap = MTK_HTMODE_GF;
		else
			ap_setting.bcn_cfg.ht_cap = MTK_HTMODE_MM;
	}
	ctrl_data_ptr->data = &ap_setting;
	ctrl_data_ptr->data_len = sizeof(ap_setting);
	mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_start_ap,
		ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
end:
	if (beacon_head_buf)
		kfree(beacon_head_buf);
	if (beacon_tail_buf)
		kfree(beacon_tail_buf);
	return ret;
}

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 7, 0) <= CFG_CFG80211_VERSION)
static int mt_cfg80211_ops_change_beacon(
	struct wiphy *wiphy,
	struct net_device *netdev,
	struct cfg80211_ap_update *info)
#else
static int mt_cfg80211_ops_change_beacon(
	struct wiphy *wiphy,
	struct net_device *netdev,
	struct cfg80211_beacon_data *info)
#endif
{
	struct mt_priv_ops_beacon bcn;
	unsigned char *beacon_head_buf = NULL;
	unsigned char *beacon_tail_buf = NULL;
	unsigned char *assoc_rsp_buf = NULL;
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 7, 0) <= CFG_CFG80211_VERSION)
	struct cfg80211_beacon_data *beacon = &info->beacon;
#else
	struct cfg80211_beacon_data *beacon = info;
#endif

	memset(&bcn, 0, sizeof(bcn));

	ctrl_data_ptr->net_dev = NULL;
	if (netdev) {
		MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);
		ctrl_data_ptr->net_dev = netdev;
	}

	if (beacon->head_len > 0) {
		beacon_head_buf = (unsigned char *)kmalloc(beacon->head_len, GFP_ATOMIC);
		if (beacon_head_buf == NULL) {
			MT_DEBUG_TRACE("memory alloc failed\n");
			ret = -ENOMEM;
			goto end;
		}
		memcpy(beacon_head_buf, beacon->head, beacon->head_len);
	} else {
		ret = -EINVAL;
		goto end;
	}

	if (beacon->tail_len > 0) {
		beacon_tail_buf = (unsigned char *)kmalloc(beacon->tail_len, GFP_ATOMIC);
		if (beacon_tail_buf == NULL) {
			MT_DEBUG_TRACE("memory alloc failed\n");
			ret = -ENOMEM;
			goto end;
		}
		memcpy(beacon_tail_buf, beacon->tail, beacon->tail_len);
	} else {
		ret = -EINVAL;
		goto end;
	}

	bcn.beacon_head_len = beacon->head_len;
	bcn.beacon_tail_len = beacon->tail_len;
	bcn.beacon_head = beacon_head_buf;
	bcn.beacon_tail = beacon_tail_buf;

	bcn.assocresp_ies = NULL;
	bcn.assocresp_ies_len = 0;
	if (beacon->assocresp_ies_len && beacon->assocresp_ies) {
		assoc_rsp_buf = (unsigned char *)kmalloc(beacon->assocresp_ies_len, GFP_ATOMIC);
		if (assoc_rsp_buf == NULL) {
			MT_DEBUG_TRACE("memory alloc failed\n");
			ret = -ENOMEM;
			goto end;
		}
		bcn.assocresp_ies_len = beacon->assocresp_ies_len;
		memcpy(assoc_rsp_buf, beacon->assocresp_ies, beacon->assocresp_ies_len);
		bcn.assocresp_ies = assoc_rsp_buf;
	}
	ctrl_data_ptr->data = &bcn;
	ctrl_data_ptr->data_len = sizeof(bcn);
	mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_change_beacon,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
end:
	if (beacon_head_buf)
		kfree(beacon_head_buf);
	if (beacon_tail_buf)
		kfree(beacon_tail_buf);
	if (assoc_rsp_buf)
		kfree(assoc_rsp_buf);

	return ret;

}

static int mt_cfg80211_ops_stop_ap(
	struct wiphy *wiphy,
	struct net_device *netdev
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	, unsigned int link_id
#endif
	)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (netdev)
		MT_DEBUG_TRACE("80211> %s ==>\n", netdev->name);

	ctrl_data_ptr->net_dev = netdev;
	ctrl_data_ptr->wiphy = wiphy;
	mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_stop_ap,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;

}

static int mt_cfg80211_ops_set_chanwidth(
	struct wiphy *wiphy,
	struct net_device *dev,
#if (KERNEL_VERSION(5, 19, 2) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	unsigned int link_id,
#endif
	struct cfg80211_chan_def *chandef)
{
	int ret = 0;
	struct mt_priv_ops_chan_info chaninfo;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	MT_DEBUG_TRACE("80211> ==>\n");

	if (NULL == wiphy || NULL == dev || NULL == chandef) {
		MT_DEBUG_TRACE("ERROR, Invalid parameter: pWiphy = %p;net_dev = %p; chandef = %p\n",
		wiphy, dev, chandef);
		return -EINVAL;
	}

	/*interface infos*/
	MT_DEBUG_TRACE("request infname: %s, mac:"MT_MACSTR";\n",
		dev->name, MT_MAC2STR(dev->dev_addr));

	/*init data*/
	memset(&chaninfo, 0, sizeof(struct mt_priv_ops_chan_info));

	/*get wireless device*/
	chaninfo.wl_dev = dev->ieee80211_ptr;/*wireless device kernel struct*/

	/*channel*/
	chaninfo.ChanId = ieee80211_frequency_to_channel(chandef->chan->center_freq);
	chaninfo.CenterChanId = ieee80211_frequency_to_channel(chandef->center_freq1);

	/*bw*/
	/*bw infos*/
	if (chandef->width == NL80211_CHAN_WIDTH_20_NOHT)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_20_NOHT;
	else if (chandef->width == NL80211_CHAN_WIDTH_20)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_20;
	else if (chandef->width == NL80211_CHAN_WIDTH_40)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_40;
	else if (chandef->width == NL80211_CHAN_WIDTH_80)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_80;
	else if (chandef->width == NL80211_CHAN_WIDTH_80P80)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_80P80;
	else if (chandef->width == NL80211_CHAN_WIDTH_160)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_160;
	else {
		MT_DEBUG_TRACE("ERROR! channel bandwidth: %d not support.\n", chandef->width);
		return -EOPNOTSUPP;
	};

	MT_DEBUG_TRACE("ChanType = %d\n", chaninfo.ChanType);
	ctrl_data_ptr->data = &chaninfo;
	ctrl_data_ptr->data_len = sizeof(chaninfo);
	ret = mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_set_chanwidth,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret) {
		MT_DEBUG_TRACE("ERROR! RTMP_DRIVER_AP_80211_CHAN_SET: FAIL\n");
		return -EOPNOTSUPP;
	}

	MT_DEBUG_TRACE("80211> end.\n");
	return 0;
}

static int mt_cfg80211_ops_chan_switch(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct cfg80211_csa_settings *params)
{
	int ret = 0;
	struct cfg80211_chan_def *chandef;
	struct mt_priv_ops_chan_info chaninfo;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (NULL == wiphy || NULL == dev || NULL == params) {
		MT_DEBUG_TRACE("ERROR, Invalid parameter: pWiphy = %p;net_dev = %p; params = %p\n",
			wiphy, dev, params);
		return -EINVAL;
	}

	/*interface infos*/
	MT_DEBUG_TRACE("Request infname: %s, mac:"MT_MACSTR";\n",
		dev->name, MT_MAC2STR(dev->dev_addr));

	/*init data*/
	memset(&chaninfo, 0, sizeof(struct mt_priv_ops_chan_info));

	/*get wireless device*/
	chaninfo.wl_dev = dev->ieee80211_ptr;/*wireless device kernel struct*/

	/*channel*/
	chandef = &params->chandef;
	chaninfo.ChanId = ieee80211_frequency_to_channel(chandef->chan->center_freq);
	chaninfo.CenterChanId = ieee80211_frequency_to_channel(chandef->center_freq1);
	MT_DEBUG_TRACE("Channel = %d, CenterChanId = %d\n",
			chaninfo.ChanId, chaninfo.CenterChanId);

	/*bw*/
	/*bw infos*/
	if (chandef->width == NL80211_CHAN_WIDTH_20_NOHT)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_20_NOHT;
	else if (chandef->width == NL80211_CHAN_WIDTH_20)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_20;
	else if (chandef->width == NL80211_CHAN_WIDTH_40)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_40;
	else if (chandef->width == NL80211_CHAN_WIDTH_80)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_80;
	else if (chandef->width == NL80211_CHAN_WIDTH_80P80)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_80P80;
	else if (chandef->width == NL80211_CHAN_WIDTH_160)
		chaninfo.ChanType = MT_NL80211_CHAN_WIDTH_160;
	else {
		MT_DEBUG_TRACE("ERROR! channel bandwidth: %d not support.\n", chandef->width);
		return -EOPNOTSUPP;
	};

	MT_DEBUG_TRACE("ChanType = %d\n", chaninfo.ChanType);

	ctrl_data_ptr->data = &chaninfo;
	ctrl_data_ptr->data_len = sizeof(chaninfo);
	ret = mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_chan_switch,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret) {
		MT_DEBUG_TRACE("ERROR! RTMP_DRIVER_AP_80211_CHAN_SET: FAIL\n");
		return -EOPNOTSUPP;
	}

	MT_DEBUG_TRACE("80211> end.\n");
	return 0;
}

static void *mt_cfg80211_find_chan(
	struct wireless_dev *wl_dev,
	struct mt_priv_ops_chan_info *chaninfo)
{
	struct wiphy *wiphy = wl_dev->wiphy;
	struct mt_cfg80211_priv *mt_priv;
	u32 IdChan, ChanNum, ChanFreq;
	enum nl80211_band band_idx;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_supported_band *pBand;

	if (!wiphy)
		return NULL;
	mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));
	if (!mt_priv)
		return NULL;
#if (KERNEL_VERSION(5, 19, 2) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	chandef = wdev_chandef(wl_dev, 0);
#else
	chandef = &wl_dev->chandef;
#endif
	if (!chandef) {
		MT_DEBUG_TRACE("chandef is NULL\n");
		return NULL;
	}

	band_idx = mt_cfg80211_get_nl80211_band(chaninfo->phy_mode);
	pBand = &mt_priv->Cfg80211_bands[band_idx];
	if (chaninfo->ChanId == 0)
		ChanFreq = chaninfo->PrimFreq;
	else
		ChanFreq = ieee80211_channel_to_frequency(chaninfo->ChanId, band_idx);
	ChanNum = mt_priv->Cfg80211_bands[band_idx].n_channels;

	for (IdChan = 0; IdChan < ChanNum; IdChan++) {
		if (pBand->channels[IdChan].center_freq == ChanFreq)
			return &pBand->channels[IdChan];
	}

	MT_DEBUG_TRACE("Find fail for chan %d.\n", chaninfo->ChanId);
	return NULL;
}

static int mt_cfg80211_ops_get_channel(
	struct wiphy *wiphy,
	struct wireless_dev *wl_dev,
#if (KERNEL_VERSION(5, 19, 2) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	unsigned int link_id,
#endif
	struct cfg80211_chan_def *chandef)
{
	int ret = 0;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_chan_info chaninfo;
	struct net_device *netdev = NULL;

	if (!wiphy) {
		MT_DEBUG_TRACE("wiphy NULL!\n");
		return -ENODEV;
	}
	if (!chandef){
		MT_DEBUG_TRACE("chandef NULL!\n");
		return -EFAULT;
	}
#if 0
	if (wl_dev) {
		netdev = wl_dev->netdev;
		MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);
	}
#endif
	memset(&chaninfo, 0, sizeof(chaninfo));
	ctrl_data_ptr->data = &chaninfo;
	ctrl_data_ptr->data_len = sizeof(chaninfo);
	ret = mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_get_channel,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret)
		return -EFAULT;
	if (wl_dev)
		chandef->chan = mt_cfg80211_find_chan(wl_dev, &chaninfo);
	else
		chandef->chan = NULL;
	if (!chandef->chan) {
		MT_DEBUG_TRACE("CFG80211_FindChan failed!\n");
		return -EFAULT;
	}
	chandef->width = mt_phy_bw_2_nl_bw(chaninfo.bw);
	if (chaninfo.CenterChanId == 0)
		chandef->center_freq1 = chaninfo.CenterFreq;
	else
		chandef->center_freq1 = ieee80211_channel_to_frequency(
				chaninfo.CenterChanId, chandef->chan->band);
	chandef->center_freq2 = 0;
#if 0
	MT_DEBUG_TRACE(
		"chan=0x%p, width=%d, center_freq1=%d, center_freq2=%d, chaninfo->ChanId=%d\n",
		chandef->chan, chandef->width, chandef->center_freq1, chandef->center_freq2,
		chaninfo.ChanId);
#endif
	return 0;
}

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 12, 6) <= CFG_CFG80211_VERSION)
static int mt_cfg80211_ops_start_radar_detect(
	struct wiphy *wiphy,
	struct net_device *netdev,
	struct cfg80211_chan_def *chandef,
	u32 cac_time_ms,
	int link_id)
#else
static int mt_cfg80211_ops_start_radar_detect(
	struct wiphy *wiphy,
	struct net_device *netdev,
	struct cfg80211_chan_def *chandef,
	u32 cac_time_ms)
#endif
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!wiphy || !netdev) {
		MT_DEBUG_TRACE("ERROR, Invalid parameter: pWiphy = %p, netdev = %p\n",
		wiphy, netdev);
		return -EINVAL;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);
	MT_DEBUG_TRACE("cac_time = %u (secs) (cac_time_ms=%u)\n",
			(u32)MSEC_TO_SEC(cac_time_ms), cac_time_ms);
	ctrl_data_ptr->uint_val = MSEC_TO_SEC(cac_time_ms);
	mt_cfg80211_ops_ctrl(
		wiphy, netdev, mtk_ops_start_radar_detect,
		ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));

	MT_DEBUG_TRACE("80211> %s end.\n", __func__);

	return 0;
}

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 12, 6) <= CFG_CFG80211_VERSION)
static void mt_cfg80211_ops_end_cac(
	struct wiphy *wiphy,
	struct net_device *netdev,
	unsigned int link_id)
#else
static void mt_cfg80211_ops_end_cac(
	struct wiphy *wiphy,
	struct net_device *netdev)
#endif
{
	if (!wiphy || !netdev) {
		MT_DEBUG_TRACE("ERROR, Invalid parameter: pWiphy = %p, netdev = %p\n",
		wiphy, netdev);
		return;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);

	mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_end_cac, NULL, 0);
}

static int mt_cfg80211_ops_scan(
	struct wiphy *wiphy,
	struct cfg80211_scan_request *pRequest)
{
	struct net_device *pNdev = NULL;
	struct wireless_dev *pWdev = NULL;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (pRequest == NULL) {
		MT_DEBUG_TRACE("pRequest is NULL!!\n");
		return -EOPNOTSUPP;
	}

	if (pRequest->ssids)
		MT_DEBUG_TRACE("80211> ==>ssid:%s, pRequest->bssid:%pM\n",
			pRequest->ssids->ssid, pRequest->bssid);

	pWdev = pRequest->wdev;
	if (pWdev == NULL) {
		MT_DEBUG_TRACE("pWdev is NULL!!\n");
		return -EOPNOTSUPP;
	}

	pNdev = pWdev->netdev;
	if (pNdev == NULL) {
		MT_DEBUG_TRACE("pNdev is NULL!!\n");
		return -EOPNOTSUPP;
	}
	MT_DEBUG_TRACE("80211> %s ==>\n", pNdev->name);
	ctrl_data_ptr->uint_val = pWdev->iftype;
	ctrl_data_ptr->data = pRequest;
	mt_cfg80211_ops_ctrl(wiphy, pNdev, mtk_ops_scan,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));

	return 0;
}

static void mt_cfg80211_ops_abort_scan(
	struct wiphy *wiphy,
	struct wireless_dev *wl_dev)
{
	if (!wiphy || !wl_dev) {
		MT_DEBUG_TRACE("ERROR, Invalid pointer: pWiphy = %p, wdev = %p\n", wiphy, wl_dev);
		return;
	}

	MT_DEBUG_TRACE("80211> ==>\n");
	mt_cfg80211_ops_ctrl(wiphy, wl_dev->netdev, mtk_ops_abort_scan, NULL, 0);
}

static int mt_cfg80211_ops_sta_get(
	struct wiphy *wiphy,
	struct net_device *netdev,
	const u8 *mac_addr,
	struct station_info *pSinfo)
{
	struct mt_priv_ops_sta_info mtk_sta_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (netdev)
		MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);

	/* init */
	memset(pSinfo, 0, sizeof(*pSinfo));
	memset(&mtk_sta_info, 0, sizeof(mtk_sta_info));
	memcpy(mtk_sta_info.MAC, mac_addr, 6);

	ctrl_data_ptr->data = &mtk_sta_info;
	ctrl_data_ptr->data_len = sizeof(mtk_sta_info);
	/* get sta information */
	if (mt_cfg80211_ops_ctrl(wiphy, netdev, mtk_ops_sta_get,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl)))
		return -ENOENT;

	if (mt_cfg80211_sta_set_sinfo(pSinfo, &mtk_sta_info))
		return -EINVAL;

	return 0;
}

static int mt_cfg80211_ops_sta_dump(
	struct wiphy *wiphy,
	struct net_device *pNdev,
	int Idx,
	u8 *pMac,
	struct station_info *pSinfo)
{
	struct mt_priv_ops_sta_info mtk_sta_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (NULL == wiphy || NULL == pNdev || NULL == pSinfo) {
		MT_DEBUG_TRACE("ERROR, Invalid data: pWiphy = %p; pNdev = %p; pSinfo = %p\n",
				wiphy, pNdev, pSinfo);
		return -ENOENT;
	}

	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP)
		MT_DEBUG_TRACE("80211> ==> AP dump station\n");
	else if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_STATION)
		MT_DEBUG_TRACE("80211> ==> STA dump station\n");

	MT_DEBUG_TRACE("request infname: %s", pNdev->name);

	/*init data*/
	memset(pMac, 0, MT_MAC_ADDR_LEN);
	memset(pSinfo, 0, sizeof(*pSinfo));
	memset(&mtk_sta_info, 0, sizeof(mtk_sta_info));

	ctrl_data_ptr->data = &mtk_sta_info;
	ctrl_data_ptr->data_len = sizeof(mtk_sta_info);
	ctrl_data_ptr->uint_val = Idx;
	if (mt_cfg80211_ops_ctrl(wiphy, pNdev, mtk_ops_sta_dump,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl)))
		return -ENOENT;

	memcpy(pMac, mtk_sta_info.MAC, MT_MAC_ADDR_LEN);
	if (mt_cfg80211_sta_set_sinfo(pSinfo, &mtk_sta_info))
		return -EINVAL;

	return 0;

	return -EOPNOTSUPP;
} /* End of CFG80211_OpsStaDump */

static int mt_cfg80211_ops_wiphy_params_set(
	struct wiphy *pWiphy,
	u32 Changed)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	MT_DEBUG_TRACE("80211> ==>\n");
	if (Changed & WIPHY_PARAM_RTS_THRESHOLD) {
		ctrl_data_ptr->uint_val = pWiphy->rts_threshold;
		ctrl_data_ptr->sub_id = MTK_OPS_SET_PARAM_RTS_THRESHOLD;
		mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_wiphy_params_set,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
		MT_DEBUG_TRACE("80211> ==> rts_threshold(%d)\n", pWiphy->rts_threshold);
		return 0;
	} else if (Changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		ctrl_data_ptr->uint_val = pWiphy->frag_threshold;
		ctrl_data_ptr->sub_id = MTK_OPS_SET_PARAM_FRAG_THRESHOLD;
		mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_wiphy_params_set,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
		MT_DEBUG_TRACE("80211> ==> frag_threshold(%d)\n", pWiphy->frag_threshold);
		return 0;
	} else if (Changed & WIPHY_PARAM_COVERAGE_CLASS) {
		unsigned int ack_time = 0;

		if (pWiphy->coverage_class > 255) {
			MT_DEBUG_TRACE("80211> ==>COVERAGE_threshold(%d) is invalid!\n",
				pWiphy->coverage_class);
			return -EOPNOTSUPP;
		}
		ctrl_data_ptr->sub_id = MTK_OPS_SET_PARAM_COVERAGE_CLASS;
		ack_time = (unsigned int) (pWiphy->coverage_class * 3);
		/* IEEE 802.11-2007 table 7-27*/
		if (0 == ack_time)
			ack_time = 1;

		ctrl_data_ptr->uint_val = ack_time;
		MT_DEBUG_TRACE("80211> ==>distance or COVERAGE_threshold(%d), ack_time=%d us\n",
			pWiphy->coverage_class, ack_time);
		if (mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_wiphy_params_set,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl)) == 0)
			MT_DEBUG_TRACE("SET ACK TIMEOUT SUCCESS!\n");
		return 0;
	}
	return -EOPNOTSUPP;
}

static int mt_cfg80211_ops_add_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif
	u8 KeyIdx,
	bool Pairwise,
	const u8 *pMacAddr,
	struct key_params *pParams)
{
	struct mt_priv_ops_key_info KeyInfo;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	ctrl_data_ptr->uint_val = 0xFFFF;
	if (pNdev) {
		MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);
		ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	} else {
		return -EINVAL;
	}

	MT_DEBUG_TRACE("80211> KeyIdx = %d\n", KeyIdx);

	if (pParams->key_len >= sizeof(KeyInfo.KeyBuf) || pParams->key_len < 0)
		return -EINVAL;

	/* init */
	memset(&KeyInfo, 0, sizeof(KeyInfo));
	memcpy(KeyInfo.KeyBuf, pParams->key, pParams->key_len);
	KeyInfo.KeyBuf[pParams->key_len] = 0x00;
	KeyInfo.KeyId = KeyIdx;
	KeyInfo.bPairwise = Pairwise;
	KeyInfo.KeyLen = pParams->key_len;
	KeyInfo.cipher = pParams->cipher;
	KeyInfo.pNetDev = pNdev;
	if (pMacAddr) {
		MT_DEBUG_TRACE("80211> KeyAdd STA("MT_MACSTR") ==>\n", MT_MAC2STR(pMacAddr));
		memcpy(KeyInfo.MAC, pMacAddr, MT_MAC_ADDR_LEN);
	}
	ctrl_data_ptr->data = &KeyInfo;
	ctrl_data_ptr->data_len = sizeof(KeyInfo);


	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP_VLAN)
		mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_add_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	else
		mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_add_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_get_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif /* CFG_CFG80211_VERSION */
	u8 KeyIdx,
	bool Pairwise,
	const u8 *pMacAddr,
	void *pCookie,
	void (*pCallback)(void *pCookie,
	struct key_params *key_params))
{
	struct key_params key_params;
	u8 tx_tsc[MTK_MAX_TSC_SIZE] = {0};
	bool b_get_tsc = false;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	int ret = 0;

	if (!pNdev)
		return -ENODEV;
	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);
	memset(ctrl_data_ptr, 0, sizeof(ctrl_data));
	memset(&key_params, 0, sizeof(key_params));
	memset(&tx_tsc, 0, sizeof(tx_tsc));
	ctrl_data_ptr->data = tx_tsc;
	ctrl_data_ptr->data_len = MTK_MAX_TSC_SIZE;
	if (!Pairwise && (KeyIdx == 6 || KeyIdx == 7)) {
		key_params.seq = tx_tsc;
		key_params.seq_len = MTK_LEN_WPA_TSC;
		b_get_tsc = true;
		ctrl_data_ptr->uint_val |= MTK_TSC_TYPE_BIGTK_PN;
	}
	ret = mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_get_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (b_get_tsc && !ret){
		pCallback(pCookie, &key_params);
		return 0;
	}
	return -ENOENT;
}

static int mt_cfg80211_ops_del_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif
	u8 KeyIdx,
	bool Pairwise,
	const u8 *pMacAddr)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_key_info KeyInfo;

	MT_DEBUG_TRACE("80211> ==>\n");

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	if (pMacAddr) {
		MT_DEBUG_TRACE("80211> KeyDel STA("MT_MACSTR") ==>\n",
			MT_MAC2STR(pMacAddr));
		memcpy(KeyInfo.MAC, pMacAddr, MT_MAC_ADDR_LEN);
	}
	KeyInfo.KeyId = KeyIdx;
	KeyInfo.pNetDev = pNdev;
	MT_DEBUG_TRACE("80211> KeyDel isPairwise %d\n", Pairwise);
	KeyInfo.bPairwise = Pairwise;
	ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	ctrl_data_ptr->data = &KeyInfo;
	ctrl_data_ptr->data_len = sizeof(KeyInfo);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_del_key,
		ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_set_default_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif
	u8 KeyIdx,
	bool Unicast,
	bool Multicast)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_key_info KeyInfo;

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	MT_DEBUG_TRACE("80211> ==>\n");
	MT_DEBUG_TRACE("80211> Default KeyIdx = %d\n", KeyIdx);
	KeyInfo.KeyId = KeyIdx;
	KeyInfo.pNetDev = pNdev;
	ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	ctrl_data_ptr->data = &KeyInfo;
	ctrl_data_ptr->data_len = sizeof(KeyInfo);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_set_default_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
} /* End of CFG80211_OpsKeyDefaultSet */

#if (KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
static int mt_cfg80211_ops_set_default_beacon_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif
	u8 KeyIdx)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_key_info KeyInfo;

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	MT_DEBUG_TRACE("80211> ==>\n");
	MT_DEBUG_TRACE("80211> Default KeyIdx = %d\n", KeyIdx);
	ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	KeyInfo.KeyId = KeyIdx;
	KeyInfo.pNetDev = pNdev;
	ctrl_data_ptr->data = &KeyInfo;
	ctrl_data_ptr->data_len = sizeof(KeyInfo);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_set_default_beacon_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
} /* End of CFG80211_OpsKeyDefaultSet */
#endif

static int mt_cfg80211_ops_set_default_mgmt_key(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	int link_id,
#endif /* CFG_CFG80211_VERSION */
	u8 KeyIdx)

{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_key_info KeyInfo;

	memset(&KeyInfo, 0, sizeof(KeyInfo));
	MT_DEBUG_TRACE("80211> ==>\n");
	MT_DEBUG_TRACE("80211> Default Mgmt KeyIdx = %d\n", KeyIdx);

	ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	KeyInfo.KeyId = KeyIdx;
	KeyInfo.pNetDev = pNdev;
	ctrl_data_ptr->data = &KeyInfo;
	ctrl_data_ptr->data_len = sizeof(KeyInfo);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_set_default_mgmt_key,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_connect(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
	struct cfg80211_connect_params *pSme)
{
	struct cfg80211_crypto_settings *crypto = &pSme->crypto;
	struct mt_priv_ops_connect_info ops_connect;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	void *wsc_ie = NULL;
	u32 wps_oui = 0x0050F2;
	u8 wps_oui_type = 4;

	if (!pNdev || !pWiphy)
		return -ENODEV;
	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);

	memset(&ops_connect, 0, sizeof(ops_connect));
	if (crypto->wpa_versions & NL80211_WPA_VERSION_1)
		ops_connect.conn_info.WpaVer = 1;
	if (crypto->wpa_versions & NL80211_WPA_VERSION_2)
		ops_connect.conn_info.WpaVer = 2;
	MT_DEBUG_TRACE("WpaVer: %02x\n", ops_connect.conn_info.WpaVer);

	switch (pSme->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		ops_connect.conn_info.AuthType = MTK_AUTH_OPEN;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		ops_connect.conn_info.AuthType = MTK_AUTH_SHARED;
		break;
	case NL80211_AUTHTYPE_SAE:
		ops_connect.conn_info.AuthType = MTK_AUTH_SAE;
		break;
	default:
		MT_DEBUG_TRACE("Unknown auth_type: %d\n", pSme->auth_type);
		ops_connect.conn_info.AuthType = MTK_AUTH_AUTO_SWITCH;
	}
	MT_DEBUG_TRACE("AuthType: %d\n", ops_connect.conn_info.AuthType);

	// wpa_supplicant sets 1 akm-suite and pairwise-cipher
	ops_connect.conn_info.AkmSuite = crypto->akm_suites[0];
	MT_DEBUG_TRACE("AkmSuite: %08x\n", ops_connect.conn_info.AkmSuite);

	ops_connect.conn_info.Pairwise = crypto->ciphers_pairwise[0];
	MT_DEBUG_TRACE("Pairwise: %08x\n", ops_connect.conn_info.Pairwise);

	ops_connect.conn_info.Group = crypto->cipher_group;
	MT_DEBUG_TRACE("Group: %08x\n", ops_connect.conn_info.Group);

	ops_connect.conn_info.pKey = (u8 *) pSme->key;
	ops_connect.conn_info.KeyLen = pSme->key_len;
	ops_connect.conn_info.KeyIdx = pSme->key_idx;
	MT_DEBUG_TRACE("KeyIdx=%hu, KeyLen=%hu\n",
		ops_connect.conn_info.KeyIdx, ops_connect.conn_info.KeyLen);

	ops_connect.conn_info.pSsid = (u8 *) pSme->ssid;
	ops_connect.conn_info.SsidLen = pSme->ssid_len;

	if (pSme->bssid != NULL) {
		ops_connect.conn_info.pBssid = (u8 *) pSme->bssid;
		MT_DEBUG_TRACE("Bssid: " MT_MACSTR "\n",
			MT_MAC2STR(ops_connect.conn_info.pBssid));
	}

	ops_connect.conn_info.mfpc = pSme->mfp != NL80211_MFP_NO;
	ops_connect.conn_info.mfpr = pSme->mfp == NL80211_MFP_REQUIRED;
	MT_DEBUG_TRACE("mfpc=%d, mfpr=%d\n",
		ops_connect.conn_info.mfpc, ops_connect.conn_info.mfpr);

	memset(&ops_connect.assoc_ie, 0, sizeof(ops_connect.assoc_ie));
	ops_connect.assoc_ie.ie = (u8 *)pSme->ie;
	ops_connect.assoc_ie.ie_len = pSme->ie_len;
	ops_connect.pNetDev = pNdev;

	wsc_ie = (void *)cfg80211_find_vendor_ie(
			wps_oui, wps_oui_type, pSme->ie, pSme->ie_len);
	if (wsc_ie) {
		MT_DEBUG_TRACE("80211> find the wsc ie.\n");
		ops_connect.conn_info.bWpsConnection = 1;
	}
	ctrl_data_ptr->uint_val = pNdev->ieee80211_ptr->iftype;
	ctrl_data_ptr->data = &ops_connect;
	ctrl_data_ptr->data_len = sizeof(ops_connect);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_connect,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_disconnect(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
	u16 ReasonCode)
{
	if (!pNdev || !pWiphy)
		return -ENODEV;
	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);
	MT_DEBUG_TRACE("80211> ReasonCode = %d\n", ReasonCode);

	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_disconnect, NULL, 0);
	cfg80211_disconnected(pNdev, ReasonCode, NULL, 0, true, GFP_KERNEL);

	return 0;
}

static int mt_cfg80211_ops_external_auth(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
	struct cfg80211_external_auth_params *params)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!pWiphy || !pNdev || !params) {
		MT_DEBUG_TRACE("pWiphy:%p, pNdev:%p, params:%p\n", pWiphy, pNdev, params);
		return 0;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);

	if (pNdev->ieee80211_ptr->iftype != NL80211_IFTYPE_STATION) {
		MT_DEBUG_TRACE("iftype:%d\n", pNdev->ieee80211_ptr->iftype);
		return 0;
	}

	MT_DEBUG_TRACE("action:%d, bssid:%pM, ssid:%s, ssid_len:%d\n",
		params->action, params->bssid, params->ssid.ssid, params->ssid.ssid_len);
#if defined(BACKPORT_NOSTDINC)
	MT_DEBUG_TRACE("key_mgmt_suite:0x%x, status:0x%x, pmkid:%p\n",
		params->key_mgmt_suite, params->status, params->pmkid);
#endif /* BACKPORT_NOSTDINC */

	ctrl_data_ptr->ushort_val = params->status;
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_external_auth,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static void mt_cfg80211_ops_rfkill_poll(
	struct wiphy *pWiphy)
{
	bool active = false;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (pWiphy) {
		MT_DEBUG_TRACE("80211> ==>\n");
		ctrl_data_ptr->u8_val = 0;
		mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_rfkill_poll,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
		if (ctrl_data_ptr->u8_val)
			active = true;
		wiphy_rfkill_set_hw_state(pWiphy, !active);
	}
}

static int mt_cfg80211_ops_survey_get(
	struct wiphy *pWiphy,
	struct net_device *netdev,
	int idx,
	struct survey_info *survey)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
	struct mt_priv_chan_survey_info mt_chan_survey_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	int ret = 0;

	if (netdev)
		MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);

	sband = pWiphy->bands[NL80211_BAND_2GHZ];
	if (sband && idx >= sband->n_channels) {
		idx -= sband->n_channels;
		sband = NULL;
	}

	if (!sband)
		sband = pWiphy->bands[NL80211_BAND_5GHZ];

	if (sband && idx >= sband->n_channels)  {
		idx -= sband->n_channels;
		sband = NULL;
	}
	if (!sband)
		sband = pWiphy->bands[NL80211_BAND_6GHZ];

	if (!sband || idx >= sband->n_channels)
		return -ENOENT;

	chan = &sband->channels[idx];

	memset(survey, 0, sizeof(*survey));
	memset(&mt_chan_survey_info, 0, sizeof(mt_chan_survey_info));
	ctrl_data_ptr->data = &mt_chan_survey_info;
	ctrl_data_ptr->data_len = sizeof(mt_chan_survey_info);
	ctrl_data_ptr->uint_val = idx;
	ret = mt_cfg80211_ops_ctrl(pWiphy, netdev, mtk_ops_survey_get,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret)
		return -ENOENT;
	if (ieee80211_frequency_to_channel(chan->center_freq) == ctrl_data_ptr->u8_val)
		survey->filled |= SURVEY_INFO_IN_USE;
	survey->filled |= SURVEY_INFO_TIME_BUSY;
	survey->filled |= SURVEY_INFO_NOISE_DBM;
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	survey->filled |= SURVEY_INFO_TIME_BSS_RX;
#endif
	survey->filled |=  SURVEY_INFO_TIME_TX | SURVEY_INFO_TIME_RX | SURVEY_INFO_TIME;

	survey->channel = chan;
	survey->time_busy = div_u64(mt_chan_survey_info.busytime, 1000);
	survey->time_rx = div_u64(mt_chan_survey_info.rx_time,  1000);
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	survey->time_bss_rx = div_u64(mt_chan_survey_info.bss_rx_time, 1000);
#endif
	survey->time_tx = div_u64(mt_chan_survey_info.tx_time, 1000);
	survey->noise = mt_chan_survey_info.ch_AvgNF;
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	survey->time = div_u64(mt_chan_survey_info.active_time, 1000);
#endif
	return 0;
}

static int mt_cfg80211_ops_pmksa_set(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
	struct cfg80211_pmksa *pPmksa)
{
	if (!pWiphy || !pNdev) {
		MT_DEBUG_TRACE("80211> pWiphy = %p, pNdev = %p\n", pWiphy, pNdev);
		return -ENOENT;
	}

	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);

	if ((pPmksa->bssid == NULL) || (pPmksa->pmkid == NULL))
		return -ENOENT;

	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP) {
		/*  NL80211_EXT_FEATURE_AP_PMKSA_CACHING */
		/* no support */
		return 0;
	}

	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_STATION) {
		struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
		struct mt_priv_pmksa_info mt_priv_info;

		memset(&mt_priv_info, 0, sizeof(mt_priv_info));
		mt_priv_info.Cmd = MTK_CMD_STA_IOCTL_PMA_SA_ADD;
		mt_priv_info.pBssid = (u8 *)pPmksa->bssid;
		mt_priv_info.pPmkid = (u8 *)pPmksa->pmkid;
		mt_priv_info.pNetDev = pNdev;
		ctrl_data_ptr->data = &mt_priv_info;
		ctrl_data_ptr->data_len = sizeof(mt_priv_info);
		MT_DEBUG_TRACE("80211> STA PMK Cache Set!\n");
		mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_pmksa_set,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	}

	return 0;
}

static int mt_cfg80211_ops_pmksa_flush(
	struct wiphy *pWiphy,
	struct net_device *pNdev)
{
	if (!pWiphy || !pNdev) {
		MT_DEBUG_TRACE("80211> pWiphy = %p, pNdev = %p\n", pWiphy, pNdev);
		return -ENOENT;
	}

	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);

	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP) {
		/*  NL80211_EXT_FEATURE_AP_PMKSA_CACHING */
		/* no support */
		return 0;
	}
	if (pNdev->ieee80211_ptr->iftype == NL80211_IFTYPE_STATION) {
		struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
		struct mt_priv_pmksa_info mt_priv_info;

		mt_priv_info.Cmd = MTK_CMD_STA_IOCTL_PMA_SA_FLUSH;
		mt_priv_info.pNetDev = pNdev;
		ctrl_data_ptr->data = &mt_priv_info;
		ctrl_data_ptr->data_len = sizeof(mt_priv_info);
		mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_pmksa_flush,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	}
	return 0;
}

static int mt_cfg80211_ops_mgmt_tx(
	struct wiphy *pWiphy,
	struct wireless_dev *wdev,
	struct cfg80211_mgmt_tx_params *params,
	u64 *pCookie)
{
	struct ieee80211_channel *pChan;
	struct mt_priv_ops_mgmt_tx_info mt_priv_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct net_device *net_dev;
	struct ieee80211_mgmt *mgmt;

	if (!pWiphy || !wdev || !params) {
		MT_DEBUG_TRACE("80211> pWiphy = %p, pNdev = %p, params = %p\n",
			pWiphy, wdev, params);
		return -ENOENT;
	}
	net_dev = wdev->netdev;
	mgmt = (struct ieee80211_mgmt *)params->buf;
	if (mgmt == NULL)
		return 0;
	/*MT_DEBUG_TRACE("80211> ==>\n");*/
	memset(&mt_priv_info, 0, sizeof(mt_priv_info));
	pChan = params->chan;
	if (pChan == NULL)
		mt_priv_info.ChanId = 0;
	else
		mt_priv_info.ChanId = ieee80211_frequency_to_channel(pChan->center_freq);
	/*MT_DEBUG_TRACE("80211> Mgmt Channel = %d\n", mt_priv_info.ChanId);*/
	mt_priv_info.pBuf = (u8 *)params->buf;
	mt_priv_info.Len = params->len;
	mt_priv_info.no_cck = params->no_cck;

	*pCookie = MTK_COOKIE_VALUE;
	if (ieee80211_is_action(mgmt->frame_control)) {
		mt_priv_info.action_frame = 1;
		mt_priv_info.action_category = mgmt->u.action.category;
		MT_DEBUG_TRACE("[cfg80211] send action frame to "MT_MACSTR".\n",
			MT_MAC2STR(mgmt->da));
	}
	if (ieee80211_is_auth(mgmt->frame_control)) {
		mt_priv_info.auth_frame = 1;
		mt_priv_info.auth_alg = mgmt->u.auth.auth_alg;
		MT_DEBUG_TRACE("[cfg80211] send auth req to "MT_MACSTR
			" (alg=%d, transaction=%d, status=%d)...\n",
			MT_MAC2STR(mgmt->da), mgmt->u.auth.auth_alg,
			mgmt->u.auth.auth_transaction, mgmt->u.auth.status_code);
	}
	ctrl_data_ptr->data = &mt_priv_info;
	ctrl_data_ptr->data_len = sizeof(mt_priv_info);
	mt_cfg80211_ops_ctrl(pWiphy, net_dev, mtk_ops_mgmt_tx,
		ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));

	return 0;
}

#ifdef BACKPORT_NOSTDINC
static void mt_cfg80211_ops_update_mgmt_frame_registrations(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	struct mgmt_frame_regs *upd)
{
	u16 new_mask = upd->interface_stypes;
	u16 old_mask = 0;
	static const struct {
		u16 mask;
	} updates[] = {
		{
			.mask = BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
				BIT(IEEE80211_STYPE_ASSOC_REQ >> 4),
		},
		{
			.mask = BIT(IEEE80211_STYPE_AUTH >> 4),
		},
		{
			.mask = BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
		},
		{
			.mask = BIT(IEEE80211_STYPE_ACTION >> 4),
		},
	};
	unsigned int i;
	struct net_device *dev;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	int ret = 0;

	if (!wdev)
		return;
	dev = wdev->netdev;
	if (dev)
		MT_DEBUG_TRACE("80211> ==> %s\n", dev->name);

	/* Get old mask */
	ctrl_data_ptr->sub_id = MTK_GET_MASK;
	ret = mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_update_mgmt_frame_reg,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret)
		return;
	old_mask = ctrl_data_ptr->ushort_val;
	MT_DEBUG_TRACE("80211> ==> old_mask: 0x%x\n", old_mask);
	ctrl_data_ptr->uint_val = 0;
	for (i = 0; i < ARRAY_SIZE(updates); i++) {
		u16 mask = updates[i].mask;
		bool reg;

		if (!(new_mask & mask) == !(old_mask & mask)) {
			MT_DEBUG_TRACE("mgmt frame: 0x%x REG state does not change\n", mask);
			continue;
		}

		reg = new_mask & mask;

		if (mask == BIT(IEEE80211_STYPE_PROBE_REQ >> 4)) {
			if (reg)
				ctrl_data_ptr->uint_val |= MTK_MGMT_PROBE_REQ_SET;
			else
				ctrl_data_ptr->uint_val |= MTK_MGMT_PROBE_REQ_CLEAR;
			/*RTMP_DRIVER_80211_MGMT_FRAME_REG(pAd, dev, reg);*/
			MT_DEBUG_TRACE("PROBE frame was %sregistered\n",
				(reg ? ("") : ("un")));
		} else if (mask == BIT(IEEE80211_STYPE_ACTION >> 4)) {
			if (reg)
				ctrl_data_ptr->uint_val |= MTK_MGMT_ACTION_SET;
			else
				ctrl_data_ptr->uint_val |= MTK_MGMT_ACTION_CLEAR;
			/*RTMP_DRIVER_80211_ACTION_FRAME_REG(pAd, dev, reg);*/
			MT_DEBUG_TRACE("ACTION frame was %sregistered\n",
				(reg ? ("") : ("un")));
		} else {
			MT_DEBUG_TRACE("UNSUPPORT mgmt frame type mask: 0x%x\n",
				mask);
		}
	}
	/* update mask */
	MT_DEBUG_TRACE("80211> ==> new_mask: 0x%x\n", new_mask);
	ctrl_data_ptr->ushort_val = new_mask;
	ctrl_data_ptr->sub_id = MTK_SET_MASK_V2;
	mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_update_mgmt_frame_reg,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
}
#else /* BACKPORT_NOSTDINC */
static void mt_cfg80211_ops_mgmt_frame_register(
	struct wiphy *pWiphy,
	struct wireless_dev *wdev,
	u16 frame_type,
	bool reg)
{
	struct net_device *dev = NULL;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!pWiphy || !wdev || !wdev->netdev)
		return;
	dev = wdev->netdev;
	MT_DEBUG_TRACE("80211> ==> %s\n", dev->name);
	MT_DEBUG_TRACE("frame_type = %x, req = %d , (%d)\n",
		frame_type, reg,  dev->ieee80211_ptr->iftype);
	ctrl_data_ptr->uint_val = 0;
	if (frame_type == IEEE80211_STYPE_PROBE_REQ) {
		if (reg)
			ctrl_data_ptr->uint_val |= MTK_MGMT_PROBE_REQ_SET;
		else
			ctrl_data_ptr->uint_val |= MTK_MGMT_PROBE_REQ_CLEAR;
		/*RTMP_DRIVER_80211_MGMT_FRAME_REG(pAd, dev, reg);*/
	}
	else if (frame_type == IEEE80211_STYPE_ACTION) {
		if (reg)
			ctrl_data_ptr->uint_val |= MTK_MGMT_ACTION_SET;
		else
			ctrl_data_ptr->uint_val |= MTK_MGMT_ACTION_CLEAR;
		/*RTMP_DRIVER_80211_ACTION_FRAME_REG(pAd, dev, reg);*/
	}
	else
		MT_DEBUG_TRACE("Unknown frame_type = %x, req = %d\n", frame_type, reg);
	/* update mask */
	ctrl_data_ptr->sub_id = MTK_SET_MASK_V1;
	mt_cfg80211_ops_ctrl(pWiphy, dev, mtk_ops_update_mgmt_frame_reg,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
}
#endif /* !BACKPORT_NOSTDINC */

static int mt_cfg80211_ops_set_qos_map(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct cfg80211_qos_map *qos_map)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!wiphy || !dev || !qos_map) {
		MT_DEBUG_TRACE("80211> ==> wiphy = %p, dev = %p, qos_map = %p\n",
			wiphy, dev, qos_map);
		return -EINVAL;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", dev->name);
	ctrl_data_ptr->data = qos_map;
	mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_set_qos_map,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));

	return 0;
}

static int mt_cfg80211_ops_set_antenna(
	struct wiphy *wiphy,
	u32 tx_ant,
	u32 rx_ant)
{
	int ret = 0;
	u8 tx_ant_count = 0;
	u8 rx_ant_count = 0;
	u32 ant_bitmap = 0;
	struct mt_priv_ops_antenna_info ant_cnt;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	MT_DEBUG_TRACE("80211> ==>\n");

	memset(&ant_cnt, 0, sizeof(ant_cnt));
	ant_bitmap = tx_ant;
	while (ant_bitmap & 0x1) {
		tx_ant_count++;
		ant_bitmap >>= 1;
	}

	ant_bitmap = rx_ant;
	while (ant_bitmap & 0x1) {
		rx_ant_count++;
		ant_bitmap >>= 1;
	}

	if (tx_ant_count == 0 || rx_ant_count == 0) {
		ret = -EINVAL;
		MT_DEBUG_TRACE("80211> Wrong input parameter!\n");
	} else {
		MT_DEBUG_TRACE("80211> TX ant count = %x, RX ant count = %x\n",
			tx_ant_count, rx_ant_count);
		ant_cnt.tx_ant = tx_ant_count;
		ant_cnt.rx_ant = rx_ant_count;
		ctrl_data_ptr->data = &ant_cnt;
		ctrl_data_ptr->data_len = sizeof(ant_cnt);
		ret = mt_cfg80211_ops_ctrl(wiphy, NULL, mtk_ops_set_antenna,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	}

	return ret;
}

static int mt_cfg80211_ops_get_antenna(
	struct wiphy *wiphy,
	u32 *tx_ant,
	u32 *rx_ant)
{
	int ret = 0;
	struct mt_priv_ops_antenna_info ant_cnt;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	memset(&ant_cnt, 0, sizeof(ant_cnt));
	MT_DEBUG_TRACE("80211> ==>\n");
	ctrl_data_ptr->data = &ant_cnt;
	ctrl_data_ptr->data_len = sizeof(ant_cnt);
	ret = mt_cfg80211_ops_ctrl(wiphy, NULL, mtk_ops_get_antenna,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret == 0) {
		*tx_ant = BIT(ant_cnt.tx_ant) - 1;
		*rx_ant = BIT(ant_cnt.rx_ant) - 1;
		MT_DEBUG_TRACE("80211> ==> tx_ant = %d, rx_ant = %d\n", *tx_ant, *rx_ant);
	}
	return ret;
}

static int mt_cfg80211_ops_change_bss(
	struct wiphy *pWiphy,
	struct net_device *netdev,
	struct bss_parameters *params)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (!pWiphy || !netdev || !params) {
		MT_DEBUG_TRACE("pWiphy:%p, netdev:%p, params:%p\n", pWiphy, netdev, params);
		return 0;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", netdev->name);

	/* ap isolate */
	if (params->ap_isolate == 1)
		ctrl_data_ptr->u8_val = 1;
	else if (params->ap_isolate == 0)
		ctrl_data_ptr->u8_val = 0;

	mt_cfg80211_ops_ctrl(pWiphy, netdev, mtk_ops_change_bss,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_sta_del(
	struct wiphy *pWiphy,
	struct net_device *dev,
	struct station_del_parameters *params)
{
	const u8 *pMacAddr = params->mac;
	struct mt_priv_ops_sta_del_info mt_priv_info;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;

	if (dev)
		MT_DEBUG_TRACE("80211> ==> %s\n", dev->name);
	memset(&mt_priv_info, 0, sizeof(mt_priv_info));
	mt_priv_info.subtype = params->subtype;
	mt_priv_info.reason = params->reason_code;
	mt_priv_info.pSta_MAC = NULL;
	if (pMacAddr) {
		MT_DEBUG_TRACE("80211> Delete STA("MT_MACSTR") ==>\n", MT_MAC2STR(pMacAddr));
		mt_priv_info.pSta_MAC = (u8 *)pMacAddr;
	}
	ctrl_data_ptr->data = &mt_priv_info;
	ctrl_data_ptr->data_len = sizeof(mt_priv_info);
	mt_cfg80211_ops_ctrl(pWiphy, dev, mtk_ops_sta_del,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

static int mt_cfg80211_ops_sta_add(
	struct wiphy *wiphy,
	struct net_device *dev,
	const u8 *mac,
	struct station_parameters *params)
{
	MT_DEBUG_TRACE("80211> ==>\n");
	return 0;
}

#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
static int mt_cfg80211_ops_add_intf_link(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	unsigned int link_id)
{
	MT_DEBUG_TRACE("[%s][%d]:80211> ==>\n", __func__, __LINE__);
	return 0;
}

static void mt_cfg80211_ops_del_intf_link(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	unsigned int link_id)
{
	MT_DEBUG_TRACE("[%s][%d]:80211> ==>\n", __func__, __LINE__);
}

static int mt_cfg80211_ops_add_link_sta(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct link_station_parameters *params)
{
	MT_DEBUG_TRACE("[%s][%d]:80211> ==>\n", __func__, __LINE__);
	return 0;
}

static int mt_cfg80211_ops_mod_link_sta(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct link_station_parameters *params)
{
	MT_DEBUG_TRACE("[%s][%d]:80211> ==>\n", __func__, __LINE__);
	return 0;
}

static int mt_cfg80211_ops_del_link_sta(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct link_station_del_parameters *params)
{
	MT_DEBUG_TRACE("[%s][%d]:80211> ==>\n", __func__, __LINE__);
	return 0;
}
#endif

static void mt_cfg80211_trigger_garp_for_sta(struct net_device *pNdev, unsigned char *sta_mac)
{
	struct sk_buff *skb = NULL;
	unsigned char bc_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	unsigned char zero_mac_addr[6]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	skb = arp_create(ARPOP_REQUEST, ETH_P_ARP, 0xffffffff, pNdev,
				0, bc_addr, sta_mac, zero_mac_addr);
	if (skb) {
		MT_DEBUG_TRACE("garp create success for sta("MACSTR") connected to bss-%s\n",
			MAC2STR(sta_mac), pNdev->name);
		eth_type_trans(skb, skb->dev);
		netif_rx_ni(skb);
	} else {
		MT_DEBUG_TRACE("garp create fail\n");
	}
}


static int mt_cfg80211_ops_sta_chg(
	struct wiphy *pWiphy,
	struct net_device *pNdev,
	const u8 *pMacAddr,
	struct station_parameters *params)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_ops_change_sta_info mt_priv_info;

	if (!pWiphy || !pNdev || !params || !pMacAddr) {
		MT_DEBUG_TRACE("pWiphy:%p, pNdev:%p\n", pWiphy, pNdev);
		MT_DEBUG_TRACE("params:%p, pMacAddr:%p\n", params, pMacAddr);
		return -EINVAL;
	}
	MT_DEBUG_TRACE("80211> ==> %s\n", pNdev->name);
	MT_DEBUG_TRACE("80211> Change STA("MT_MACSTR") ==>\n", MT_MAC2STR(pMacAddr));
	if ((pNdev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP) &&
		(pNdev->ieee80211_ptr->iftype != NL80211_IFTYPE_P2P_GO) &&
		(pNdev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP_VLAN))
		return -EOPNOTSUPP;

	memset(&mt_priv_info, 0, sizeof(mt_priv_info));
	mt_priv_info.mac_addr = (u8 *)pMacAddr;
/*  vikas: used for VLAN, along with auth flag
	if(!(params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)))
	{
		CFG80211DBG(DBG_LVL_ERROR, ("80211> %x ==>\n", params->sta_flags_mask));
		return -EOPNOTSUPP;
	}
*/
	if (params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
		if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
			mt_priv_info.authorized = 1;
			mt_cfg80211_trigger_garp_for_sta(pNdev, (unsigned char *)pMacAddr);
		}
	}

#ifdef BACKPORT_NOSTDINC
	mt_priv_info.vlan_id = params->vlan_id;
	mt_priv_info.vlan_dev = params->vlan;
#endif /* BACKPORT_NOSTDINC */
	ctrl_data_ptr->data = &mt_priv_info;
	ctrl_data_ptr->data_len = sizeof(mt_priv_info);
	mt_cfg80211_ops_ctrl(pWiphy, pNdev, mtk_ops_sta_chg,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}

#ifdef CONFIG_NL80211_TESTMODE
static int mt_cfg80211_ops_test_mode_cmd(
	struct wiphy *pWiphy,
	struct wireless_dev *wdev,
	void *data,
	int datalen)
{
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	struct mt_priv_pmkid pmkid_entry = {0};

	/*copy pentry details from data variable*/
	if (datalen <= sizeof(pmkid_entry))
		memcpy(&pmkid_entry, data, datalen);
	else
		return -EINVAL;

	MT_DEBUG_TRACE("80211> datalen:%d==>\n", datalen);
	ctrl_data_ptr->data = &pmkid_entry;
	ctrl_data_ptr->data_len = datalen;
	mt_cfg80211_ops_ctrl(pWiphy, NULL, mtk_ops_test_mode_cmd,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	return 0;
}
#endif /* CONFIG_NL80211_TESTMODE */

static int mt_cfg80211_ops_get_tx_power(
		struct wiphy *wiphy,
		struct wireless_dev *wdev,
		int *dbm)
{
	struct net_device *dev = NULL;
	struct mt_cfg80211_vndr_ctrl ctrl_data, *ctrl_data_ptr = &ctrl_data;
	int power, ret;

	if (!wiphy || !wdev || !wdev->netdev)
		return -ENOENT;

	dev = wdev->netdev;
	MT_DEBUG_TRACE("80211> %s ==>\n", dev->name);

	ctrl_data_ptr->data = &power;
	ctrl_data_ptr->data_len = sizeof(int);
	ret = mt_cfg80211_ops_ctrl(wiphy, dev, mtk_ops_get_tx_power,
			ctrl_data_ptr, sizeof(struct mt_cfg80211_vndr_ctrl));
	if (ret)
		return -EFAULT;

	*dbm = power;

	return 0;
}

struct cfg80211_ops mtk_cfg80211_ops = {
	.set_bitrate_mask	= mt_cfg80211_ops_set_bit_rate,
	.add_virtual_intf	= mt_cfg80211_ops_virtual_inf_add,
	.del_virtual_intf	= mt_cfg80211_ops_virtual_inf_del,
	.start_ap		= mt_cfg80211_ops_start_ap,
	.change_beacon		= mt_cfg80211_ops_change_beacon,
	.stop_ap		= mt_cfg80211_ops_stop_ap,
	.set_ap_chanwidth	= mt_cfg80211_ops_set_chanwidth,
	.channel_switch		= mt_cfg80211_ops_chan_switch,
	.get_channel		= mt_cfg80211_ops_get_channel,
	.start_radar_detection	= mt_cfg80211_ops_start_radar_detect,
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(5, 4, 18) <= CFG_CFG80211_VERSION)
	.end_cac		= mt_cfg80211_ops_end_cac,
#endif
	/* change type/configuration of virtual interface */
	.change_virtual_intf	= NULL, /* not support */

	/* request to do a scan */
	/*
	 *	Note: must exist whatever AP or STA mode; Or your kernel will crash
	 *	in v2.6.38.
	 */
	.scan			= mt_cfg80211_ops_scan,
	.abort_scan		= mt_cfg80211_ops_abort_scan,

	/* configure WLAN power management */
	.set_power_mgmt		= NULL,
	/* get station information for the station identified by @mac */
	.get_station		= mt_cfg80211_ops_sta_get,
	/* dump station callback */
	.dump_station		= mt_cfg80211_ops_sta_dump,
	/* notify that wiphy parameters have changed */
	.set_wiphy_params	= mt_cfg80211_ops_wiphy_params_set,
	/* add a key with the given parameters */
	.add_key		= mt_cfg80211_ops_add_key,
	/* get information about the key with the given parameters */
	.get_key		= mt_cfg80211_ops_get_key,
	/* remove a key given the @mac_addr */
	.del_key		= mt_cfg80211_ops_del_key,
	/* set the default key on an interface */
	.set_default_key	= mt_cfg80211_ops_set_default_key,
#if (KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	.set_default_beacon_key	= mt_cfg80211_ops_set_default_beacon_key,
#endif
	/* set the default mgmt key on an interface */
	.set_default_mgmt_key	= mt_cfg80211_ops_set_default_mgmt_key,
	/* connect to the ESS with the specified parameters */
	.connect		= mt_cfg80211_ops_connect,
	/* disconnect from the BSS/ESS */
	.disconnect		= mt_cfg80211_ops_disconnect,
	.external_auth		= mt_cfg80211_ops_external_auth,

	/* polls the hw rfkill line */
	.rfkill_poll		= mt_cfg80211_ops_rfkill_poll,
	/* get site survey information */
	.dump_survey		= mt_cfg80211_ops_survey_get,
	/* cache a PMKID for a BSSID */
	.set_pmksa		= mt_cfg80211_ops_pmksa_set,
	/* flush all cached PMKIDs */
	.flush_pmksa		= mt_cfg80211_ops_pmksa_flush,

	/*
	 *	Request the driver to remain awake on the specified
	 *	channel for the specified duration to complete an off-channel
	 *	operation (e.g., public action frame exchange).
	 */
	.remain_on_channel	= NULL,
	/* cancel an on-going remain-on-channel operation */
	.cancel_remain_on_channel	=  NULL,
	.mgmt_tx		= mt_cfg80211_ops_mgmt_tx,
	.mgmt_tx_cancel_wait	= NULL,

	/* configure connection quality monitor RSSI threshold */
	.set_cqm_rssi_config	= NULL,

	/* notify driver that a management frame type was registered */
#ifdef BACKPORT_NOSTDINC
	.update_mgmt_frame_registrations =
		mt_cfg80211_ops_update_mgmt_frame_registrations,
#else
	.mgmt_frame_register	= mt_cfg80211_ops_mgmt_frame_register,
#endif
	.set_qos_map		= mt_cfg80211_ops_set_qos_map,
	/* set antenna configuration (tx_ant, rx_ant) on the device */
	.set_antenna		= mt_cfg80211_ops_set_antenna,
	/* get current antenna configuration from device (tx_ant, rx_ant) */
	.get_antenna		= mt_cfg80211_ops_get_antenna,
	.change_bss		= mt_cfg80211_ops_change_bss,
	.del_station		= mt_cfg80211_ops_sta_del,
	.add_station		= mt_cfg80211_ops_sta_add,
#if defined(CFG_CFG80211_VERSION) && (KERNEL_VERSION(6, 1, 0) <= CFG_CFG80211_VERSION)
	.add_intf_link		= mt_cfg80211_ops_add_intf_link,
	.del_intf_link		= mt_cfg80211_ops_del_intf_link,
	.add_link_station	= mt_cfg80211_ops_add_link_sta,
	.mod_link_station	= mt_cfg80211_ops_mod_link_sta,
	.del_link_station	= mt_cfg80211_ops_del_link_sta,
#endif
	.change_station		= mt_cfg80211_ops_sta_chg,
#ifdef CONFIG_NL80211_TESTMODE
	.testmode_cmd		= mt_cfg80211_ops_test_mode_cmd,
#endif
	.get_tx_power		= mt_cfg80211_ops_get_tx_power,
};

#ifndef __MOCK
static struct wireless_dev *mt_cfg80211_wireless_dev_alloc(
	struct device *dev,
	struct net_device *net_dev,
	struct mt_os_net_dev_op_hook *net_dev_op_hook)
{
	struct wireless_dev *wl_dev;
	struct wiphy *wiphy = NULL;
	struct mt_cfg80211_priv *pPriv;
	char phy_name[10];
	int ret;
	unsigned int mtk_dev_features = net_dev_op_hook->mtk_dev_features;
	struct mt_cfg80211_band mt_band_info;

	wl_dev = (struct wireless_dev *)kmalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!wl_dev) {
		MT_DEBUG_TRACE("80211> Wireless device allocation fail!\n");
		return NULL;
	}
	memset(wl_dev, 0, sizeof(struct wireless_dev));
	if (mtk_dev_features & MT_FEATURE_SEL_DUAL_BAND) {
		ret = sprintf(phy_name, "phy%d", net_dev_op_hook->dev_idx);
		if (ret < 0)
			MT_DEBUG_TRACE("80211> Sprintf error!\n");
		wl_dev->wiphy = wiphy_new_nm(
			&mtk_cfg80211_ops,
			sizeof(struct mt_cfg80211_priv),
			phy_name);
	} else
		wl_dev->wiphy = wiphy_new(
			&mtk_cfg80211_ops, sizeof(struct mt_cfg80211_priv));

	if (wl_dev->wiphy == NULL) {
		MT_DEBUG_TRACE("80211> Wiphy device allocation fail!\n");
		goto LabelErrWiphyNew;
	} /* End of if */
	wiphy = wl_dev->wiphy;

	/*netlink vendor cmds and events*/
	mtk_cfg80211_vndr_init(wiphy);

	if (!memcmp(mt_zero_mac_addr, net_dev_op_hook->permanent_addr, MT_MAC_ADDR_LEN))
		memcpy(wiphy->perm_addr, net_dev_op_hook->permanent_addr, MT_MAC_ADDR_LEN);

	MT_DEBUG_TRACE("PermanentAddress MAC: ="MT_MACSTR"\n",
			 MT_MAC2STR(net_dev_op_hook->permanent_addr));
	/* keep pAd pointer */
	pPriv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));
	pPriv->mac_ad = mt_os_net_dev_get_priv(net_dev);
	pPriv->net_dev = net_dev;
	pPriv->mtk_dev_features = net_dev_op_hook->mtk_dev_features;
	pPriv->pCfg80211_Wdev = wl_dev;
	set_wiphy_dev(wiphy, dev);
	/* max_scan_ssids means in each scan request, how many ssids can driver handle to send probe-req.
	 *  In current design, we only support 1 ssid at a time. So we should set to 1.
	*/
	/* pWdev->wiphy->max_scan_ssids = pBandInfo->MaxBssTable; */
	wiphy->max_scan_ssids = 1;
	mt_cfg80211_wiphy_feature_set(wiphy, net_dev_op_hook->mtk_cfg80211_features);
	wiphy->interface_modes = BIT(NL80211_IFTYPE_AP) | BIT(NL80211_IFTYPE_STATION);
	if (mtk_dev_features & MT_FEATURE_VLAN_GTK_INTF)
		wl_dev->wiphy->interface_modes |= BIT(NL80211_IFTYPE_AP_VLAN);
	 wl_dev->wiphy->reg_notifier = mt_cfg80211_reg_notifier;
	/* init channel information */
	memset(&mt_band_info, 0, sizeof(struct mt_cfg80211_band));
	mt_cfg80211_init_ctrl(
		wiphy, MT_80211_GET_BAND_INFO, &mt_band_info, sizeof(struct mt_cfg80211_band));
	mt_cfg80211_sup_band_init(wiphy, &mt_band_info, 0, 0, false);

	/* CFG80211_SIGNAL_TYPE_MBM: signal strength in mBm (100*dBm) */
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;
	wiphy->max_num_pmkids = 4;
	wiphy->max_remain_on_channel_duration = 5000;
	wiphy->mgmt_stypes = mtk_mgmt_stypes;
	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);
	wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
	/*what if you get compile error for below flag, please add the patch into your kernel*/
	/* 018-cfg80211-internal-ap-mlme.patch */
	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
	/*what if you get compile error for below flag, please add the patch into your kernel*/
	/* 008-cfg80211-offchan-flags.patch */
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;

	wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;

#if (KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	if (mtk_dev_features & MT_FEATURE_MLO) {
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_MLO;
		wiphy->iftype_ext_capab = add_iftypes_ext_capa;
		wiphy->num_iftype_ext_capab = ARRAY_SIZE(add_iftypes_ext_capa);
	}
#endif
	wiphy->max_num_csa_counters = 10;
#if (KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	if (mtk_dev_features & MT_FEATURE_11V_MBSS)
		wiphy->support_mbssid = 1;
#endif
	wiphy->iface_combinations = mtk_iface_combinations_p2p;
	wiphy->n_iface_combinations = ARRAY_SIZE(mtk_iface_combinations_p2p);
	wiphy->available_antennas_tx = BIT(mt_band_info.max_nss) - 1;
	wiphy->available_antennas_rx = BIT(mt_band_info.max_nss) - 1;

	if (mtk_dev_features & MT_FEATURE_4ADDR_STATION)
		wiphy->flags |= WIPHY_FLAG_4ADDR_STATION;

#if (KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE) || defined(BACKPORT_NOSTDINC)
	wiphy->max_num_akm_suites = 10;
#endif

	if (wiphy_register(wiphy) < 0) {
		MT_DEBUG_TRACE("80211> Register wiphy device fail!\n");
		goto LabelErrReg;
	}

	/*add to support DPP action frame Tx*/
	wiphy->flags |= WIPHY_FLAG_OFFCHAN_TX;

	return wl_dev;
LabelErrReg:
	wiphy_free(wiphy);
	wl_dev->wiphy = NULL;
LabelErrWiphyNew:
	kfree(wl_dev);
	return NULL;
} /* End of CFG80211_WdevAlloc */
#endif /* !__MOCK */

enum nl80211_band mt_cfg80211_get_nl80211_band(unsigned short phy_mode)
{
	switch (phy_mode) {
	case MTK_PHY_MODE_2G:
		return NL80211_BAND_2GHZ;
	case MTK_PHY_MODE_5G:
		return NL80211_BAND_5GHZ;
	case MTK_PHY_MODE_6G:
		return NL80211_BAND_6GHZ;
	default:
		MT_DEBUG_TRACE("PhyMode(%d) invalid, return NL80211_BAND_5GHZ!\n", phy_mode);
		return NL80211_BAND_5GHZ;
	}
}

enum nl80211_chan_width mt_phy_bw_2_nl_bw(int mtk_width)
{
	switch (mtk_width) {
	case MT_NL80211_CHAN_WIDTH_20:
		return NL80211_CHAN_WIDTH_20;
	case MT_NL80211_CHAN_WIDTH_40:
		return NL80211_CHAN_WIDTH_40;
	case MT_NL80211_CHAN_WIDTH_80:
		return NL80211_CHAN_WIDTH_80;
	case MT_NL80211_CHAN_WIDTH_80P80:
		return NL80211_CHAN_WIDTH_80P80;
	case MT_NL80211_CHAN_WIDTH_160:
		return NL80211_CHAN_WIDTH_160;
#ifdef IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ
	case MT_NL80211_CHAN_WIDTH_320:
		return NL80211_CHAN_WIDTH_320;
#endif /* IEEE80211_EHT_OPER_CHAN_WIDTH_320MHZ */
	default:
		return NL80211_CHAN_WIDTH_20_NOHT;
	}
}

bool mt_cfg80211_register(
	struct device *dev,
	struct net_device *net_dev,
	struct mt_os_net_dev_op_hook *net_dev_op_hook)
{
#ifndef __MOCK
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *wl_dev;
	unsigned int mtk_dev_feature = 0;

	if (!net_dev_op_hook)
		return false;
	mtk_dev_feature = net_dev_op_hook->mtk_dev_features;
	MT_DEBUG_TRACE("mtk_features = 0x%x\n", mtk_dev_feature);
	/* allocate wireless device */
	wl_dev = mt_cfg80211_wireless_dev_alloc(dev, net_dev, net_dev_op_hook);
	if (!wl_dev) {
		MT_DEBUG_TRACE("80211> Allocate Wdev fail!\n");
		return false;
	}
	prWiphy = wl_dev->wiphy;

	/* bind wireless device with net device */
	wl_dev->iftype = NL80211_IFTYPE_AP;
	net_dev->ieee80211_ptr = wl_dev;
	SET_NETDEV_DEV(net_dev, wiphy_dev(prWiphy));
	if (mtk_dev_feature & MT_FEATURE_RFKILL_HW_SUPPORT)
		wiphy_rfkill_start_polling(prWiphy);
	mt_cfg80211_init_ctrl(prWiphy, MT_80211_DRIVER_REGISTER, wiphy_priv(prWiphy), 0);
	mt_cfg80211_scan_status_lock_init(prWiphy, true);

	MT_DEBUG_TRACE("80211> end\n");
#endif /* !__MOCK */
	return true;
}

void mt_cfg80211_unregister(
	struct net_device *net_dev)
{
	struct wiphy *wiphy = 0;
	struct mt_cfg80211_priv *mt_priv = 0;
	struct wireless_dev *wl_dev = 0;

	if (net_dev)
		wl_dev = net_dev->ieee80211_ptr;
	if (wl_dev)
		wiphy = wl_dev->wiphy;
	if (wiphy) {
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));

		MT_DEBUG_TRACE("80211> unregister/free wireless device\n");
		mt_cfg80211_init_ctrl(wiphy, MT_80211_DRIVER_UNREGISTER, NULL, 0);
		/*
		 *	Must unregister, or you will suffer problem when you change
		 *	regulatory domain by using iw.
		 */
		if (mt_priv) {
			if (mt_priv->mtk_dev_features & MT_FEATURE_RFKILL_HW_SUPPORT)
				wiphy_rfkill_stop_polling(wiphy);
			if (mt_priv->pCfg80211_Channels)
				kfree(mt_priv->pCfg80211_Channels);
			if (mt_priv->pCfg80211_Rates)
				kfree(mt_priv->pCfg80211_Rates);
		}
		wiphy_unregister(wiphy);
		wiphy_free(wiphy);
		kfree(net_dev->ieee80211_ptr);
		net_dev->ieee80211_ptr = 0;
		SET_NETDEV_DEV(net_dev, 0);
	}
}
EXPORT_SYMBOL(mt_cfg80211_unregister);

bool mt_cfg80211_sup_band_reinit(
	void *net_dev_,
	struct mt_cfg80211_band *mt_band_info)
{
	struct net_device *net_dev = (struct net_device *)net_dev_;
	struct wireless_dev *wl_dev;
	struct wiphy *wiphy;
	struct mt_cfg80211_priv *mt_priv;

	if (!net_dev)
		return false;
	wl_dev = net_dev->ieee80211_ptr;
	if (!wl_dev)
		return false;
	wiphy = wl_dev->wiphy;
	if (wiphy) {
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));
		MT_DEBUG_TRACE("80211> re-init bands...\n");
		/* re-init bands */
		if (mt_priv)
			mt_cfg80211_sup_band_init(wiphy, mt_band_info,
					mt_priv->pCfg80211_Channels,
					mt_priv->pCfg80211_Rates,
					true);
		/* re-init PHY */
		wiphy->rts_threshold = mt_band_info->RtsThreshold;
		wiphy->frag_threshold = mt_band_info->FragmentThreshold;
		wiphy->retry_short = mt_band_info->RetryMaxCnt & 0xff;
		wiphy->retry_long = (mt_band_info->RetryMaxCnt & 0xff00) >> 8;
		regulatory_hint(wiphy, (const char *)(mt_band_info->CountryCode));
		return true;
	}

	return false;
}
EXPORT_SYMBOL(mt_cfg80211_sup_band_reinit);

bool mt_cfg80211_chan_info_init(
	void *wiphy_,
	u32 InfoIndex,
	u8 ChanId)
{
	struct wiphy *wiphy = (struct wiphy *)wiphy_;
	struct ieee80211_channel *pChan;
	struct mt_cfg80211_priv *mt_priv = NULL;

#ifdef __MOCK
	return true;
#endif /* __MOCK */

	if (wiphy)
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(wiphy));

	if (!mt_priv)
		return false;

	if (InfoIndex >= MAX_NUM_OF_CHANNELS)
		return false;

	pChan = (struct ieee80211_channel *) &(mt_priv->ChanInfo[InfoIndex]);
	memset(pChan, 0, sizeof(*pChan));

	if (ChanId > 14)
		pChan->band = IEEE80211_BAND_5GHZ;
	else
		pChan->band = IEEE80211_BAND_2GHZ;

	pChan->center_freq = ieee80211_channel_to_frequency(ChanId, pChan->band);
	return true;
}
EXPORT_SYMBOL(mt_cfg80211_chan_info_init);

void *mt_cfg80211_get_mac_ad(void *_wiphy)
{
	struct wiphy *pWiphy = (struct wiphy *)_wiphy;
	struct mt_cfg80211_priv *mt_priv;

	if (pWiphy) {
		mt_priv = (struct mt_cfg80211_priv *)(wiphy_priv(pWiphy));
		if (mt_priv)
			return mt_priv->mac_ad;
	}
	return NULL;
}
EXPORT_SYMBOL(mt_cfg80211_get_mac_ad);

void *mt_cfg80211_get_wiphy(void *_wl_dev)
{
	struct wireless_dev *wl_dev = (struct wireless_dev *)_wl_dev;

	if (wl_dev)
		return wl_dev->wiphy;
	return NULL;
}
EXPORT_SYMBOL(mt_cfg80211_get_wiphy);
#endif /* CFG80211_SUPPORT */

DECLARE_SUB_MODULE_INIT(mt_osal_cfg80211_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_cfg80211_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_cfg80211_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_cfg80211_exit);

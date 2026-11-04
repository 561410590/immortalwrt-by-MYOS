/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_CFG80211_H_
#define __MT_OSAL_CFG80211_H_

#ifdef CFG80211_SUPPORT
#include <linux/version.h>
#include <linux/ieee80211.h>
#include <mt_osal_type.h>

#ifdef __MOCK
#define NL80211_BAND_6GHZ 3
#define NLA_MIN_LEN	20
#endif /* __MOCK */

#if (KERNEL_VERSION(4, 7, 0) <= LINUX_VERSION_CODE)
#define IEEE80211_NUM_BANDS NUM_NL80211_BANDS
#define IEEE80211_BAND_2GHZ NL80211_BAND_2GHZ
#define IEEE80211_BAND_5GHZ NL80211_BAND_5GHZ
#define IEEE80211_BAND_6GHZ NL80211_BAND_6GHZ
#endif

#define MT_MAX_NUM_OF_CHS	(54 + 5) /* 5 channels for central channel of VHT 80MHz */
#define MT_IEEE80211_NUM_TIDS		16

/* 2412 ~ 2472  */
#define MTK_CFG80211_NUM_OF_CHAN_2GHZ_5M_SHIFT			13
/* 4920 ~ 5905 */
#define MTK_CFG80211_NUM_OF_CHAN_5GHZ_5M_SHIFT			198
/* 5735 ~ 7115 */
#define MTK_CFG80211_NUM_OF_CHAN_6GHZ_5M_SHIFT			277

/* 1 ~ 14 */
#define MTK_CFG80211_NUM_OF_CHAN_2GHZ			14
/* 36 ~ 64, 100 ~ 136, 140 ~ 161 */
#define MTK_CFG80211_NUM_OF_CHAN_5GHZ			28
#define MTK_CFG80211_NUM_OF_CHAN_6GHZ			59
#define MTK_MAX_STREAM 4
#define MTK_MAX_LEN_OF_SSID                 32
#define MTK_MAX_TSC_SIZE 20
#define MTK_LEN_WPA_TSC 6
#define MTK_GET_MASK    1
#define MTK_SET_MASK_V1 2
#define MTK_SET_MASK_V2 3
#define MTK_COOKIE_VALUE 5678

struct freqlist {
	u32 startFreq;
	u32 endFreq;
};

enum mt_cfg80211_feature_type {
	MT_CFG80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE      = 0x00000001,
	MT_CFG80211_FEATURE_SAE                            = 0x00000002,
	MT_CFG80211_FEATURE_OPERATING_CHANNEL_VALIDATION   = 0x00000004,
	MT_CFG80211_FEATURE_MGMT_TX_RANDOM_TA              = 0x00000008,
	MT_CFG80211_FEATURE_BEACON_PROTECTION              = 0x00000010,
	MT_CFG80211_FEATURE_UNSOL_BCAST_PROBE_RESP         = 0x00000020,
	MT_CFG80211_FEATURE_RADAR_BACKGROUND               = 0x00000040,
	MT_CFG80211_FEATURE_VLAN_OFFLOAD                   = 0x00000080,
};

enum mt_cfg80211_phy_mode {
	MTK_PHY_MODE_2G = 1,
	MTK_PHY_MODE_5G,
	MTK_PHY_MODE_6G,
};

enum mt_cfg80211_phy_cap {
	MTK_PHY_CAP_RFIC_24GHZ = 0x00000001,
	MTK_PHY_CAP_RFIC_5GHZ  = 0x00000002,
	MTK_PHY_CAP_RFIC_6GHZ  = 0x00000004,
	MTK_PHY_CAP_BW160C_STD = 0x00000008,
	MTK_PHY_CAP_BW160NC    = 0x00000010,
	MTK_PHY_CAP_BW160C     = 0x00000020,
	MTK_PHY_CAP_BW80       = 0x00000040,
	MTK_PHY_CAP_BW320      = 0x00000080,
	MTK_PHY_CAP_HT         = 0x00000100,
	MTK_PHY_CAP_VHT        = 0x00000200,
	MTK_PHY_CAP_HE         = 0x00000400,
	MTK_PHY_CAP_EHT        = 0x00000800,
	MTK_PHY_CAP_5M_SHIFT   = 0x00001000,
};

enum mt_cfg80211_auth_mode {
	MTK_AUTH_OPEN,
	MTK_AUTH_SHARED,
	MTK_AUTH_AUTO_SWITCH,
	MTK_AUTH_WPA,
	MTK_AUTH_WPAPSK,
	MTK_AUTH_WPANone,
	MTK_AUTH_WPA2,
	MTK_AUTH_WPA2PSK,
	MTK_AUTH_WPA1WPA2,
	MTK_AUTH_WPA1PSKWPA2PSK,
	MTK_AUTH_WAICERT,	/* WAI certificate authentication */
	MTK_AUTH_WAIPSK,	/* WAI pre-shared key */
	MTK_AUTH_SAE,
	MTK_AUTH_Max /* Not a real mode, defined as upper bound */
};

enum mt_cfg80211_mgmt_frame {
	MTK_MGMT_PROBE_REQ_SET     = 0x00000001,
	MTK_MGMT_PROBE_RSP_SET     = 0x00000002,
	MTK_MGMT_AUTH_SET          = 0x00000004,
	MTK_MGMT_ASSOC_REQ_SET     = 0x00000008,
	MTK_MGMT_REASSOC_REQ_SET   = 0x00000010,
	MTK_MGMT_ACTION_SET        = 0x00000020,

	MTK_MGMT_PROBE_REQ_CLEAR   = 0x00010000,
	MTK_MGMT_PROBE_RSP_CLEAR   = 0x00020000,
	MTK_MGMT_AUTH_CLEAR        = 0x00040000,
	MTK_MGMT_ASSOC_REQ_CLEAR   = 0x00080000,
	MTK_MGMT_REASSOC_REQ_CLEAR = 0x00100000,
	MTK_MGMT_ACTION_CLEAR      = 0x00200000,
};

enum mt_cfg80211_init_cmd {
	MT_80211_INIT_REG_NOTIFY_TO,
	MT_80211_DRIVER_REGISTER,
	MT_80211_DRIVER_UNREGISTER,
	MT_80211_GET_BAND_INFO,
	MT_80211_GET_PHY_CAP,
};

enum mt_cfg80211_ops_cmd {
	mtk_ops_set_bit_rate = 1,
	mtk_ops_add_vif_inf,
	mtk_ops_del_vif_inf,
	mtk_ops_start_ap,
	mtk_ops_change_beacon,
	mtk_ops_stop_ap,
	mtk_ops_set_chanwidth,
	mtk_ops_chan_switch,
	mtk_ops_get_channel,
	mtk_ops_start_radar_detect,
	mtk_ops_end_cac,
	mtk_ops_scan,
	mtk_ops_abort_scan,
	mtk_ops_sta_get,
	mtk_ops_sta_dump,
	mtk_ops_wiphy_params_set,
	mtk_ops_add_key,
	mtk_ops_get_key,
	mtk_ops_del_key,
	mtk_ops_set_default_key,
	mtk_ops_set_default_beacon_key,
	mtk_ops_set_default_mgmt_key,
	mtk_ops_connect,
	mtk_ops_disconnect,
	mtk_ops_external_auth,
	mtk_ops_rfkill_poll,
	mtk_ops_survey_get,
	mtk_ops_pmksa_set,
	mtk_ops_pmksa_flush,
	mtk_ops_mgmt_tx,
	mtk_ops_update_mgmt_frame_reg,
	mtk_ops_set_qos_map,
	mtk_ops_set_antenna,
	mtk_ops_get_antenna,
	mtk_ops_change_bss,
	mtk_ops_sta_del,
	mtk_ops_sta_chg,
	mtk_ops_test_mode_cmd,
	mtk_ops_get_tx_power,
};

/* HE GI */
enum {
	MTK_CAPI_HE_08_GI,
	MTK_CAPI_HE_16_GI,
	MTK_CAPI_HE_32_GI,
	MTK_CAPI_HE_GI_NUM
};

/* HE LTF */
enum {
	MTK_CAPI_HE_1x_LTF,
	MTK_CAPI_HE_2x_LTF,
	MTK_CAPI_HE_4x_LTF,
	MTK_CAPI_HE_LTF_NUM
};

/* HT CAP */
enum {
	MTK_NON_HT,
	MTK_HTMODE_MM,
	MTK_HTMODE_GF,
};

/* mtk_ops_wiphy_params_set */
enum {
	MTK_OPS_SET_PARAM_RTS_THRESHOLD,
	MTK_OPS_SET_PARAM_FRAG_THRESHOLD,
	MTK_OPS_SET_PARAM_COVERAGE_CLASS,
};

enum mt_tsc_key_type {
	MTK_TSC_TYPE_GTK_PN   = 0x00000001,
	MTK_TSC_TYPE_IGTK_PN  = 0x00000002,
	MTK_TSC_TYPE_BIGTK_PN = 0x00000004,
};

/* Must sync with nl80211_channel_type@nl80211.h */
#define MTK_CMD_80211_CHANTYPE_NOHT		0x00
#define MTK_CMD_80211_CHANTYPE_HT20		0x01
#define MTK_CMD_80211_CHANTYPE_HT40MINUS	0x02
#define MTK_CMD_80211_CHANTYPE_HT40PLUS		0x03
#define MTK_CMD_80211_CHANTYPE_VHT80		0x04
#define MTK_CMD_80211_CHANTYPE_VHT80P80		0x05
#define MTK_CMD_80211_CHANTYPE_VHT160		0x06

#define MTK_CMD_80211_KEY_WEP40			0x00
#define MTK_CMD_80211_KEY_WEP104		0x01
#define MTK_CMD_80211_KEY_WPA			0x02
#define MTK_CMD_80211_KEY_AES_CMAC		0x03
#define MTK_CMD_80211_KEY_AES_GMAC128		0x04
#define MTK_CMD_80211_KEY_AES_GMAC256		0x05
#define MTK_CMD_80211_KEY_AES_CMAC256		0x06

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
/*this nl80211_channel_type must be synced with kernel*/
enum mt_nl80211_chan_width {
	MT_NL80211_CHAN_WIDTH_20_NOHT,
	MT_NL80211_CHAN_WIDTH_20,
	MT_NL80211_CHAN_WIDTH_40,
	MT_NL80211_CHAN_WIDTH_80,
	MT_NL80211_CHAN_WIDTH_80P80,
	MT_NL80211_CHAN_WIDTH_160,
	MT_NL80211_CHAN_WIDTH_5,
	MT_NL80211_CHAN_WIDTH_10,
	MT_NL80211_CHAN_WIDTH_320,
	};
#endif/*KERNEL_VERSION(4, 0, 0)*/

struct mt_cfg80211_band {
	u8 RFICType;
	u8 MpduDensity;
	u8 TxStream;
	u8 RxStream;
	u32 MaxTxPwr;
	u32 MaxBssTable;

	u16 RtsThreshold;
	u16 FragmentThreshold;
	u32 RetryMaxCnt; /* bit0~7: short; bit8 ~ 15: long */
	unsigned short phy_mode;
	enum mt_cfg80211_phy_cap phy_caps;
	u8 CountryCode[2];
	u8 max_nss;
	u8 FlgIsBMode;
};

struct mt_cfg80211_bit_rate_info {
	u8 he_gi;
	u8 he_ltf;
	u32 legacy;
	u8 ht_mcs[MTK_MAX_STREAM];
	u16 vht_mcs[MTK_MAX_STREAM];
};

struct mt_cfg80211_priv {
	void *net_dev;
	void *mac_ad;
	unsigned int mtk_dev_features;
	struct ieee80211_sband_iftype_data sband_data[NUM_NL80211_BANDS];
	/* we can change channel/rate information on the fly so we backup them */
	struct ieee80211_supported_band Cfg80211_bands[IEEE80211_NUM_BANDS];
	struct ieee80211_channel *pCfg80211_Channels;
	struct ieee80211_rate *pCfg80211_Rates;

	/* used in wiphy_unregister */
	struct wireless_dev *pCfg80211_Wdev;

	/* used in scan end */
	struct cfg80211_scan_request *pCfg80211_ScanReq;

	/* monitor filter */
	u32 MonFilterFlag;

	/* channel information */
	struct ieee80211_channel ChanInfo[MT_MAX_NUM_OF_CHS];

	/* to protect scan status */
	spinlock_t scan_notify_lock;
};

struct mt_cfg80211_io_reg_notify {
	unsigned char Alpha2[2];
	void *pWiphy;
};

struct mt_phy_cap {
	unsigned short phy_mode;
	unsigned char channel;
};

struct mt_priv_ops_vif_set {
	int vifType;
	char vifName[IFNAMSIZ];
	int vifNameLen;
	int vif_exist;
	unsigned int flags;
	void *wl_dev;
	bool is_main_if;
};

struct mt_priv_ops_chan_info {
	u8 ChanId;
	u8 CenterChanId;
	u32 PrimFreq;
	u32 CenterFreq;
	u8 IfType;
	u8 ChanType;
	u8 bw;
	unsigned short phy_mode;
	u32 MonFilterFlag;
	u64 cookie;
	void *chan;
#if (KERNEL_VERSION(3, 6, 0) <= LINUX_VERSION_CODE)
	struct wireless_dev *wl_dev;
#endif /* LINUX_VERSION_CODE: 3.6.0 */
};

struct mt_priv_ops_scan_req {
	u8 FlgScanThisSsid;
	u32 SsidLen;
	char *pSsid;
	u32 ScanType;
	u16 duration;
};

struct mt_priv_ops_beacon {
	u8 *beacon_head;
	u8 *beacon_tail;
	u32 beacon_head_len; /* Before TIM IE */
	u32 beacon_tail_len; /* After TIM IE */
	u32 apidx;
	struct net_device *pNetDev;
	u8 *beacon_ies;
	u32 beacon_ies_len;
	u8 *proberesp_ies;
	u32 proberesp_ies_len;
	u8 *assocresp_ies;
	u32 assocresp_ies_len;
	u8 *probe_resp;
	u32 probe_resp_len;
};

struct mt_priv_ops_beacon_cfg {
	u32 interval;
	u32 dtim_period;
	u32 unsol_6g_interval;
	u32 flags;
	u8 ssid[32];
	u32 ssid_len;
	struct cfg80211_crypto_settings crypto;
	u8 hidden_ssid;
	u8 privacy;
	u8 auth_type;
	u8 enable_1x;
	u32 inactivity_timeout;
	u16 bitrate;
	enum nl80211_band band;
	u8 ht_cap;
};

struct mt_priv_ops_ap_setting {
	struct mt_priv_ops_beacon bcn;
	struct mt_priv_ops_beacon_cfg bcn_cfg;
	struct mt_priv_ops_chan_info chan_info;
};

struct mt_priv_rate_info {
	u8 flags;
	u8 mcs;
	u16 legacy;
	u8 nss;
	u8 bw;
};

struct mt_nl80211_sta_flag {
	u32 mask;
	u32 set;
} __attribute__((packed));

struct mt_cfg80211_tid_stats {
	u32 filled;
	u64 rx_msdu;
	u64 tx_msdu;
	u64 tx_msdu_retries;
	u64 tx_msdu_failed;
};

struct mt_cfg80211_sta_bss_info {
	u8 flags;
	u8 dtim_period;
	u16 beacon_interval;
};


struct mt_priv_ops_sta_info {
	u8 MAC[MT_MAC_ADDR_LEN];
	u64 DataRate;

	u32 TxRateFlags;

	u32 TxRateMCS;
	int Signal;
	signed char ack_signal;
	u8 chains;
	signed char chain_signal[IEEE80211_MAX_CHAINS];
	u32 TxPacketCnt;
	u32 InactiveTime;

	u32 rx_packets;
	u32 tx_packets;
	u64 rx_bytes;
	u64 tx_bytes;
	u32 tx_retries;
	u32 tx_failed;
	u8 rx_beacon_signal_avg;
	u32 RxRateMCS;
	int signal_avg;
	u32 connected_time;
	u32 beacon_loss_count;
	u64 rx_beacon;
	u64 beacon_mask;
	u64 tx_duration;
	u64 rx_duration;
	u64 assoc_at;
	struct mt_priv_rate_info txrate;
	struct mt_priv_rate_info rxrate;
	struct mt_nl80211_sta_flag sta_flags;/*mask of station flags to set*/
	struct mt_cfg80211_tid_stats pertid[MT_IEEE80211_NUM_TIDS + 1];
	struct mt_cfg80211_sta_bss_info bss_param;
};

#define MTK_MAX_KEY_SIZE 50
struct mt_priv_ops_key_info {
	u8 KeyType;
	u8 KeyId;
	u8 bPairwise;
	u8 KeyLen;
	u8 KeyBuf[MTK_MAX_KEY_SIZE];
	u32 cipher;
	u8 MAC[MT_MAC_ADDR_LEN];
	void *pNetDev;
};

struct mt_priv_connect_info {
	u8 WpaVer;
	u8 AuthType;

	u32 AkmSuite;
	u32 Pairwise;
	u32 Group;

	u8 *pKey;
	u8 KeyLen;
	u8 KeyIdx;

	u8 *pSsid;
	u32 SsidLen;
	u8 *pBssid;

	u8 bWpsConnection;

	u8 mfpc;
	u8 mfpr;
};

struct mt_priv_extra_assoc_ie {
	u8 *ie;
	u32 ie_len;
};

struct mt_priv_ops_connect_info {
	struct mt_priv_connect_info conn_info;
	struct mt_priv_extra_assoc_ie assoc_ie;
	void *pNetDev;
};

struct mt_priv_ops_change_sta_info {
	u16 vlan_id;
	void *vlan_dev;
	u8 *mac_addr;
	u8 authorized;
};

struct mt_priv_ops_sta_del_info {
	u32 reason;
	u8 *pSta_MAC;
	u8 subtype;
};

struct mt_priv_ops_mgmt_tx_info {
	u8 *pBuf;
	size_t Len;
	u32 ChanId;
	u32 action_category;
	u32 auth_alg;
	u8 no_cck;
	u8 action_frame;
	u8 auth_frame;
};

struct mt_priv_ops_antenna_info {
	u32 tx_ant;
	u32 rx_ant;
};

struct mt_priv_chan_survey_info {
	u64 busytime;  /*scan channel busy time*/
	u64 tx_time; /*scan tx air time*/
	u64 rx_time; /*scan rx air time*/
	u64 bss_rx_time; /*other bss tx air time*/
	u64 active_time;
	int ch_AvgNF;
};

#define MTK_LEN_PMKID		16

struct mt_priv_pmkid {
	u8 bssid[ETH_ALEN]; /* AP's MLD addr */
	u8 sta[ETH_ALEN]; /* STA's MLD addr */
	u8 link_addr[ETH_ALEN]; /* Add: STA's link addr, Remove: AP's link addr */
	u8 pmkid[MTK_LEN_PMKID];
	u8 AddRemove;	/*1- Add, 0- Remove*/
	int akmp;	/* AKM protocol */
};

#define MTK_CMD_STA_IOCTL_PMA_SA_FLUSH		0x01
#define MTK_CMD_STA_IOCTL_PMA_SA_REMOVE		0x02
#define MTK_CMD_STA_IOCTL_PMA_SA_ADD		0x03
struct mt_priv_pmksa_info {
	u32 Cmd;
	u8 *pBssid;
	u8 *pPmkid;
	void *pNetDev;
};

enum nl80211_band mt_cfg80211_get_nl80211_band(unsigned short phy_mode);
enum nl80211_chan_width mt_phy_bw_2_nl_bw(int mtk_width);

bool mt_cfg80211_register(
	struct device *dev,
	struct net_device *net_dev,
	struct mt_os_net_dev_op_hook *net_dev_op_hook);
void mt_cfg80211_unregister(
	struct net_device *net_dev);
bool mt_cfg80211_sup_band_reinit(
	void *net_dev_,
	struct mt_cfg80211_band *mt_band_info);
bool mt_cfg80211_chan_info_init(
	void *wiphy_,
	u32 InfoIndex,
	u8 ChanId);
void *mt_cfg80211_get_mac_ad(void *_wiphy);
void *mt_cfg80211_get_wiphy(void *_wl_dev);
#endif /* CFG80211_SUPPORT */

#endif /* __MT_OSAL_CFG80211_H_ */


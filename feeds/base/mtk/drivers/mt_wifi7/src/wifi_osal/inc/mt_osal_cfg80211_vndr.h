/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_CFG80211_VNDR_H_
#define __MT_OSAL_CFG80211_VNDR_H_

#ifdef CFG80211_SUPPORT
#include <linux/version.h>
#include <linux/ieee80211.h>
#include "mtk_vendor_nl80211.h"

#define MT_MAX_VENDOR_MSG_LEN 4095
#define MT_MAX_MGMT_PKT_LEN	2304 /* MMPDU size limit */
#define MT_WMM_NUM_OF_AC 4 /* AC0, AC1, AC2, and AC3 */
#define MT_MAX_NUM_OF_MONITOR_STA		16
#define MT_BSS_MNGR_MAX_BAND_NUM		3
#define MT_BMGR_MAX_MLD_GRP_CNT	(1 + (64) + 1)
#define MT_BMGR_MAX_MLD_STA_CNT	256
#define MT_MAX_SERV_IOCTLBUFF 5200
#define MT_EEPROM_SIZE					0x1E00
#define MT_MLD_LINK_MAX 16 /* this is link_id(0~15) of mld protocol */
#define MT_MAX_BAND_NUM	3
#define MT_MAX_RSSI_LEN 5

enum mt_cfg80211_vndr_attr_type {
	mt_cfg80211_vndr_attr_get_band_info = 1,
	mt_cfg80211_vndr_attr_show,
	mt_cfg80211_vndr_attr_set,
	mt_cfg80211_vndr_attr_mac,
	mt_cfg80211_vndr_attr_statistic,
	mt_cfg80211_vndr_attr_get_cap,
	mt_cfg80211_vndr_attr_get_runtime_info,
	mt_cfg80211_vndr_attr_get_statistic,
	mt_cfg80211_vndr_attr_get_static_info,
	mt_cfg80211_vndr_attr_wapp_req,
	mt_cfg80211_vndr_attr_mcast_snoop,
	mt_cfg80211_vndr_attr_set_ftm,
	mt_cfg80211_vndr_attr_set_get_ftm_stat,
	mt_cfg80211_vndr_attr_set_set_ap_wireless,
	mt_cfg80211_vndr_attr_set_ap_rfeature,
	mt_cfg80211_vndr_attr_set_country,
	mt_cfg80211_vndr_attr_set_channel,
	mt_cfg80211_vndr_attr_set_acl_policy,
	mt_cfg80211_vndr_attr_set_vow,
	mt_cfg80211_vndr_attr_set_mwds,
	mt_cfg80211_vndr_attr_set_auto_ch_sel,
	mt_cfg80211_vndr_attr_set_scan,
	mt_cfg80211_vndr_attr_set_bss,
	mt_cfg80211_vndr_attr_set_radio,
	mt_cfg80211_vndr_attr_ft_ie,
	mt_cfg80211_vndr_attr_mlo_ie,
	mt_cfg80211_vndr_attr_del_mlo_sta_entry,
	mt_cfg80211_vndr_attr_set_cosr_ie,
	mt_cfg80211_vndr_attr_wapp_cosr_info,
	mt_cfg80211_vndr_attr_set_pmkid,
	mt_cfg80211_vndr_attr_set_mac_addr,
	mt_cfg80211_vndr_attr_set_seq_num,
	mt_cfg80211_vndr_attr_set_wmm,
	mt_cfg80211_vndr_attr_set_monitor,
	mt_cfg80211_vndr_attr_clean_sta,
	mt_cfg80211_vndr_attr_set_ba,
	mt_cfg80211_vndr_attr_set_action,
	mt_cfg80211_vndr_attr_qos,
	mt_cfg80211_vndr_attr_offchannel_info,
	mt_cfg80211_vndr_attr_dfs,
	mt_cfg80211_vndr_attr_easymesh,
	mt_cfg80211_vndr_attr_set_vlan,
	mt_cfg80211_vndr_attr_set_mbo,
	mt_cfg80211_vndr_attr_mlo_preset_linkid,
	mt_cfg80211_vndr_attr_tx_power,
	mt_cfg80211_vndr_attr_edca,
	mt_cfg80211_vndr_attr_vie,
	mt_cfg80211_vndr_attr_set_radio_stats,
	mt_cfg80211_vndr_attr_get_radio_stats,
	mt_cfg80211_vndr_attr_get_bss_stats,
	mt_cfg80211_vndr_attr_get_sta,
	mt_cfg80211_vndr_attr_hqa,
	mt_cfg80211_vndr_attr_set_v10converter,
	mt_cfg80211_vndr_attr_set_apropxyrefresh,
	mt_cfg80211_vndr_attr_e2p,
	mt_cfg80211_vndr_attr_testengine,
	mt_cfg80211_vndr_attr_mlo_info,
	mt_cfg80211_vndr_attr_mlo_switch,
	mt_cfg80211_vndr_attr_get_bss_mlo_info,
	mt_cfg80211_vndr_attr_offload_acs,
	mt_cfg80211_vndr_attr_bh_status,
	mt_cfg80211_vndr_attr_get_connected_sta_mld,
	mt_cfg80211_vndr_attr_set_config,
	mt_cfg80211_vndr_attr_swaci_lna,
	mt_cfg80211_vndr_attr_set_ap_mld,
	mt_cfg80211_vndr_attr_set_zr_hdo_rm,
	mt_cfg80211_vndr_attr_get_zr_hdo_rm,
	mt_cfg80211_vndr_attr_set_ap_ptk,
};

enum mt_cfg80211_scan_type {
	mt_scan_type_full,
	mt_scan_type_partial,
	mt_scan_type_off_ch,
	mt_scan_type_2040_overlay,
};

struct mt_auto_ch_sel_param {
	unsigned long flags;
	unsigned char is_6g_psc;
	void *skip_list;
	unsigned int skip_list_len;
	void *prefer_list;
	unsigned int prefer_list_len;
	unsigned char partial;
	unsigned short scanning_dwell;
	unsigned short restore_dwell;
	unsigned char partial_scan_ch_num;
	unsigned char sel_alg;
	unsigned int periodic_check_time;
	unsigned short periodic_sta_num_thr;
	unsigned short data_rate_wt;
	unsigned short ch_prio_wt;
	unsigned char tx_power_cons;
	unsigned char ch_util_thr;
	unsigned char ice_ch_util_thr;
	unsigned char max_acs_times;
	unsigned char switch_thr;
	unsigned char is_bgnd_scan;
};

struct mt_cfg80211_vndr_ctrl {
	unsigned int sub_id;
	unsigned int uint_val;
	unsigned long ulong_val;
	unsigned short ushort_val;
	unsigned char u8_val;
	void *data;
	unsigned int data_len;
	void *rsp_data;
	unsigned int rsp_data_len;
	int status;
	void *wiphy;
	void *net_dev;
	void *wl_dev;
};

struct mt_off_ch_scan_param {
	unsigned int target_ch;
	unsigned int duration;
	unsigned char active;
};

struct mt_ch_switch_param {
	unsigned char ch;
	unsigned int bw;
	unsigned char ext_ch;
	unsigned char ht_coex;
	unsigned char cac_required;
	unsigned int flags;
};

struct mt_affiliated_aps {
	unsigned char valid;
	unsigned char link_id;
	unsigned char dis_subch_enable;
	unsigned char emlsr_mr;
	unsigned short dis_subch_bitmap;
	unsigned char bssid[MT_MAC_ADDR_LEN];
};

struct mt_affiliated_stas {
	unsigned char if_addr[MT_MAC_ADDR_LEN];
	unsigned char link_addr[MT_MAC_ADDR_LEN];
	unsigned char valid;
	unsigned char single_link_en;
	unsigned char emlsr_mr;
};

struct mt_mlo_dev_info {
	/* MLD info */
	unsigned char mld_grp;
	unsigned char mld_type;
	unsigned char mld_addr[MT_MAC_ADDR_LEN];
	unsigned char peer_mld_addr[MT_MAC_ADDR_LEN];
	unsigned char peer_mld_valid;
	struct mt_affiliated_aps aps[MT_BSS_MNGR_MAX_BAND_NUM];
	struct mt_affiliated_stas stas[MT_MLD_LINK_MAX];
};

struct mt_peer_mld_link_info {
	bool	active;	/* indicate success assoc */
	struct station_information sta_info;
	signed char sta_rssi[4];
	u8 link_addr[MT_MAC_ADDR_LEN];
	u8 link_bssid[MT_MAC_ADDR_LEN];
	u8 link_id;
	u8 tid_map_ul;
	u8 tid_map_dl;
	u8 snr;
	u64 tx_bytes;
	u64 rx_bytes;
	u32 tx_packets;
	u32 rx_packets;
	u32 tx_msdu_retry_cnt;
	u64 tx_fail_count;
	u64 data_rate;
	u64 tx_time;
	u64 rx_time;
	u8 tx_sgi;
	u8 rx_sgi;
};

struct mt_peer_mld_sta_info {
	bool valid;
	u16 idx;	/* non-AP MLD mld_sta_idx */
	u8 bss_idx_setup; /* bss_idx of setup link */
	u8 mld_grp_idx; /* AP MLD that non-AP MLD asscoiated */
	u16 aid;
	u8 emlsr_en;
	u8 emlsr_sup;
	u16 emlsr_link_bmap;
	u16 mld_caps; /* MLD Capabilities subfield in Basic ML IE */
	u8 mld_addr[MT_MAC_ADDR_LEN];
	struct mt_peer_mld_link_info mld_link[MT_MAX_BAND_NUM];
	u8 emlmr_sup;
	u8 freq_sep_str;
	u32 sta_connect_time;
};

int mt_cfg80211_vndr_cmd_handler(
	struct wiphy *wiphy,
	struct wireless_dev *wifi_dev,
	enum mt_cfg80211_vndr_attr_type vndr_attr_type,
	struct mt_cfg80211_vndr_ctrl *data);
int mt_cfg80211_vndr_cmd_get_handler(
	struct wiphy *wiphy,
	struct wireless_dev *wifi_dev,
	enum mt_cfg80211_vndr_attr_type vndr_attr_type,
	unsigned int attr_cmd,
	struct mt_cfg80211_vndr_ctrl *ctrl_data_ptr);
int mt_cfg80211_vndr_cmd_get_handler_without_alloc(
	struct wiphy *wiphy,
	struct wireless_dev *wifi_dev,
	enum mt_cfg80211_vndr_attr_type vndr_attr_type,
	unsigned int attr_cmd,
	struct mt_cfg80211_vndr_ctrl *ctrl_data_ptr);

int mtk_cfg80211_reply_connected_sta_mld_info(
	struct mt_cfg80211_vndr_ctrl *ctrl_data);
int mtk_cfg80211_vndr_doit_subcmd_test(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_cap(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_runtime_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_statistic(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_static_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_band_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_wapp_req(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_mcast_snoop(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ftm(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_ftm_stat(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ap_wireless(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ap_rfeature(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_country(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_channel(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_acl(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_vow(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_mwds(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_auto_ch_sel(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_scan(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_bss(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_radio(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_ft_ie(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_mlo_ie(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_del_mlo_sta_entry(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_cosr_bcn_ie(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_wapp_cosr_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_pmkid(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ap_ptk(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_mac_addr(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_seq_num(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_wmm(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_monitor(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_show(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_mac(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_statistic(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_clean_sta(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ba(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_action(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_qos(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_offchannel_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_dfs(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_easymesh(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_vlan(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_mbo(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_mlo_preset_linkid(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_tx_power(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_edca(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_vie(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_radio_stats(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_radio_stats(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_bss_stats(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_sta(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_hqa(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_v10converter(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_apropxyrefresh(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_e2p(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_testengine(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_mlo_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_mlo_switch(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_bss_mlo_info(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_offload_acs(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_bh_status(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_connected_sta_mld(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_config(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_swaci_lna(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_ap_mld(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_set_zr_hdo_rm(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
int mtk_cfg80211_vndr_doit_get_zr_hdo_rm(
	struct wiphy *wiphy, struct wireless_dev *wifi_dev, const void *data, int len);
void mtk_cfg80211_vndr_init(struct wiphy *prWiphy);
#endif /* CFG80211_SUPPORT */


#endif /* __MT_OSAL_CFG80211_VNDR_H_ */


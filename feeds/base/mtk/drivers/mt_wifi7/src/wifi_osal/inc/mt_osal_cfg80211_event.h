/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_CFG80211_EVENT_H_
#define __MT_OSAL_CFG80211_EVENT_H_

#ifdef CFG80211_SUPPORT
#include <linux/version.h>
#include <linux/ieee80211.h>

int mtk_nl80211_send_5m_shift_enbale(
	void *ctrl_data_);
int mtk_nl80211_send_acs_complete_event(
	void *ctrl_data_);
int mtk_nl80211_send_event_ACS_per_channel_info(
	void *ctrl_data_);
int mtk_nl80211_send_event_bhstatus(
	void *ctrl_data_);

int mtk_cfg80211_send_connect_params_event(
	void *ctrl_data_);
int mtk_cfg80211_send_pmksa_event(
	void *ctrl_data_);
int mtk_nl80211_send_sta_link_mac_event(
	void *ctrl_data_);
int mtk_nl80211_send_offchannel_info_event(
	void *ctrl_data_);
int mtk_nl80211_send_stop_disassoc_timer_event(
	void *ctrl_data_);
int mtk_nl80211_send_wapp_qry_rsp_event(
	void *ctrl_data_);
int mtk_nl80211_send_cosr_event(
	void *ctrl_data_);
int mtk_cfg80211_send_bss_ml_info_event(
	void *ctrl_data_);
int mtk_cfg80211_send_reconf_sm_event(
	void *ctrl_data_);
int mtk_nl80211_send_andlink_wifi_change_event(
	void *ctrl_data_);
int mtk_cfg80211_APStaDelSendVendorEvent(
	void *ctrl_data_);
int mtk_cfg80211os_VendorEventMloResponse(
	void *ctrl_data_);
int mtk_cfg80211_send_bss_ml_sta_profile_info_event(
	void *ctrl_data_);
int mtk_nl80211_send_zr_hdo_rm_event(
	void *ctrl_data_);
#endif /* CFG80211_SUPPORT */

#endif /* __MT_OSAL_CFG80211_EVENT_H_ */


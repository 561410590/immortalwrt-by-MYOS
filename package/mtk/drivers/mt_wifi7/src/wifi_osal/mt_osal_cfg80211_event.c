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
#include <net/netlink.h>

#define SUBMODULE "mt_osal_wifi"
#include "mtk_vendor_nl80211.h"
#include <mt_osal_debug.h>
#include <mt_osal_net.h>
#include <mt_osal_wifi.h>
#include <mt_osal_cfg80211.h>
#include <mt_osal_cfg80211_vndr.h>
#include <mt_osal_cfg80211_event.h>
#include <mt_osal_module.h>


#ifdef CFG80211_SUPPORT
int mtk_nl80211_send_5m_shift_enbale(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(
		wiphy, NULL, 4, MTK_NL80211_VENDOR_EVENT_5M_SHIFT_ENABLE, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put_u16(skb, 0, 1)) {
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_5m_shift_enbale);

int mtk_nl80211_send_acs_complete_event(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct mt_phy_cap *mt_phy_cap = ctrl_data->rsp_data;
	struct sk_buff *skb;
struct acs_req_params {
	u32 pri_frequency;
	u32 sec_frequency;
	u8 hw_mode;
	u8 edmg_channel;
	u8 seg0_center_channel;
	u8 seg1_center_channel;
	u16 ch_bw;
} params = {0};
	int len = sizeof(struct acs_req_params);
	enum nl80211_band band_id;


	if (!mt_phy_cap)
		return -EINVAL;

	MT_DEBUG_TRACE("phy_mode: 0x%x, channel = %d\n",
		mt_phy_cap->phy_mode, mt_phy_cap->channel);
	band_id = mt_cfg80211_get_nl80211_band(mt_phy_cap->phy_mode);

/* hostapd just need the pri_frequency, so other params are zero.
 * hw_mode is set to MTK_ACS_MODE_IEEE80211A for upping hostapd`s interface.
 */
	params.pri_frequency = ieee80211_channel_to_frequency(mt_phy_cap->channel, band_id);

	if (band_id == NL80211_BAND_2GHZ)
		params.hw_mode = MTK_ACS_MODE_IEEE80211G;
	else
		params.hw_mode = MTK_ACS_MODE_IEEE80211A;

	MT_DEBUG_TRACE("\n");

	skb = cfg80211_vendor_event_alloc(
		wiphy, NULL, len, MTK_NL80211_VENDOR_EVENT_ACS_COMPLETE_EVENT, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put_u32(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_PRIMARY_FREQUENCY, params.pri_frequency) ||
		nla_put_u32(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_SECONDARY_FREQUENCY, params.sec_frequency) ||
		nla_put_u8(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_HW_MODE, params.hw_mode) ||
		nla_put_u8(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_EDMG_CHANNEL, params.edmg_channel) ||
		nla_put_u8(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_VHT_SEG0_CENTER_CHANNEL, params.seg0_center_channel) ||
		nla_put_u8(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_VHT_SEG1_CENTER_CHANNEL, params.seg1_center_channel) ||
		nla_put_u16(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ACS_COMPLETE_CHWIDTH, params.ch_bw)) {
		MT_DEBUG_TRACE("nla_put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_acs_complete_event);

int mtk_nl80211_send_event_ACS_per_channel_info(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct sk_buff *skb;

	MT_DEBUG_TRACE("event len:%d\n", ctrl_data->rsp_data_len);
	skb = cfg80211_vendor_event_alloc(
		wiphy, NULL, ctrl_data->rsp_data_len,
		MTK_NL80211_VENDOR_EVENT_ACS_PER_CH_INFO, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_EVENT_ACS_PER_CH_INFO,
			ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("nla_put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_event_ACS_per_channel_info);

int mtk_nl80211_send_event_bhstatus(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct net_device *net_dev = (struct net_device *)ctrl_data->net_dev;
	struct wireless_dev *wl_dev = (struct wireless_dev *)net_dev->ieee80211_ptr;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct sk_buff *skb;

	if (!wl_dev) {
		MT_DEBUG_TRACE("no wireless dev\n");
		return -EINVAL;
	}
	MT_DEBUG_TRACE("[DFS-SLAVE][%s] len:%d wdev:%p\n",
			__func__, ctrl_data->rsp_data_len, wl_dev);
	skb = cfg80211_vendor_event_alloc(
		wiphy, wl_dev, ctrl_data->rsp_data_len,
		MTK_NL80211_VENDOR_EVENT_BH_STATUS, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_DFS_SLAVE_BH_STATUS,
			ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("nla_put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_event_bhstatus);

int mtk_cfg80211_send_connect_params_event(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	unsigned char *pmk = (unsigned char *)ctrl_data->rsp_data;
	int pmk_len = ctrl_data->rsp_data_len;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
							pmk_len, 0, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("connect params info cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}
	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, pmk_len, pmk)) {
		MT_DEBUG_TRACE("connect params nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_send_connect_params_event);

int mtk_cfg80211_send_pmksa_event(
	void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	unsigned char *pmksa_event = (unsigned char *)ctrl_data->rsp_data;
	int pmksa_event_len = ctrl_data->rsp_data_len;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
						pmksa_event_len, MTK_NL80211_VENDOR_EVENT_RSP_WAPP_EVENT, GFP_ATOMIC);

	if (!skb) {
		MT_DEBUG_TRACE("pmksa cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, pmksa_event_len, pmksa_event)) {
		MT_DEBUG_TRACE("pmksa nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_send_pmksa_event);

int mtk_nl80211_send_sta_link_mac_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned char *data = (unsigned char *)ctrl_data->rsp_data;
	int len = ctrl_data->rsp_data_len;

	if (!wl_dev) {
		MT_DEBUG_TRACE("no wireless dev\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, len, MTK_NL80211_VENDOR_EVENT_SEND_MLO_STA_LINK_MAC, GFP_ATOMIC);

	if (!skb) {
		MT_DEBUG_TRACE("sta link mac cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, len, data)) {
		MT_DEBUG_TRACE("sta link mac nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_sta_link_mac_event);

int mtk_nl80211_send_offchannel_info_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned char *data = (unsigned char *)ctrl_data->rsp_data;
	int len = ctrl_data->rsp_data_len;

	if (!wl_dev) {
		MT_DEBUG_TRACE("no wireless dev\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, len, MTK_NL80211_VENDOR_EVENT_OFFCHANNEL_INFO, GFP_ATOMIC);

	if (!skb) {
		MT_DEBUG_TRACE("offchannel info cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_OFF_CHANNEL_INFO, len, data)) {
		MT_DEBUG_TRACE("offchannel info nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_offchannel_info_event);

int mtk_nl80211_send_stop_disassoc_timer_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;

	if (!wl_dev) {
		MT_DEBUG_TRACE("no wireless dev\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, ctrl_data->rsp_data_len, MTK_NL80211_VENDOR_EVENT_RX_T2LM_STOP_DISASSOC_TIMER, GFP_ATOMIC);

	if (!skb) {
		MT_DEBUG_TRACE("disassoc timer cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("disassoc timer nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_stop_disassoc_timer_event);

int mtk_nl80211_send_wapp_qry_rsp_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, ctrl_data->rsp_data_len, MTK_NL80211_VENDOR_EVENT_RSP_WAPP_EVENT, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("wapp qry rsp cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_RSP_WAPP_EVENT, ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("wapp qry rsp nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_wapp_qry_rsp_event);

int mtk_nl80211_send_cosr_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned int buflen = ctrl_data->rsp_data_len;
	unsigned int event_attr = ctrl_data->sub_id;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, buflen, MTK_NL80211_VENDOR_EVENT_MLO_EVENT, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cosr event cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	switch (event_attr) {
	case MTK_NL80211_VENDOR_ATTR_STA_RSSI_CHANGE:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_STA_RSSI_CHANGE, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("sta rssichange nla put fail!!\n");
			dev_kfree_skb(skb);
			skb = NULL;
			return -EINVAL;
		}
		break;
	case MTK_NL80211_VENDOR_ATTR_COAP_UPDATED:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_COAP_UPDATED, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("coap updated nla put fail!!\n");
			dev_kfree_skb(skb);
			skb = NULL;
			return -EINVAL;
		}
		break;
	case MTK_NL80211_VENDOR_ATTR_COAP_FOUND:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_COAP_FOUND, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("coap found nla put fail!!\n");
			dev_kfree_skb(skb);
			skb = NULL;
			return -EINVAL;
		}
		break;
	case MTK_NL80211_VENDOR_ATTR_COSR_FRAME:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_COSR_FRAME, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("cosr frame nla put fail!!\n");
			dev_kfree_skb(skb);
			skb = NULL;
			return -EINVAL;
		}
		break;
	default:
		dev_kfree_skb(skb);
		skb = NULL;
		return -EINVAL;
	}

	if (skb)
		cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_cosr_event);

int mtk_cfg80211_send_bss_ml_info_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned int buflen = ctrl_data->rsp_data_len;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, buflen, MTK_NL80211_VENDOR_EVENT_SEND_ML_INFO, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("bss ml info cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_EVENT_SEND_ML_INFO, buflen, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("bss ml info nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_send_bss_ml_info_event);

int mtk_cfg80211_send_reconf_sm_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned int buflen = ctrl_data->rsp_data_len;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, buflen, MTK_NL80211_VENDOR_EVENT_MLO_RECONF, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("reconf sm cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_MLO_RECONF_SM, buflen, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("reconf sm nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_send_reconf_sm_event);

int mtk_nl80211_send_andlink_wifi_change_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	unsigned int len = ctrl_data->rsp_data_len;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL, len, MTK_NL80211_VENDOR_EVENT_ANDLINK, GFP_ATOMIC);

	if (!skb) {
		MT_DEBUG_TRACE("addlink wifi change cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ANDLINK_WIFI_CHANGE_EVENT, len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("addlink wifi change nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_andlink_wifi_change_event);

int mtk_cfg80211_APStaDelSendVendorEvent(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned int event_len = ctrl_data->uint_val;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, event_len,
		MTK_NL80211_VENDOR_EVENT_DISC_STA, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("ap sta del cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_DISC_STA_MAC, ctrl_data->data_len, ctrl_data->data)) {
		MT_DEBUG_TRACE("dis sta mac nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	if (nla_put_u16(skb, MTK_NL80211_VENDOR_ATTR_EVENT_DISC_STA_REASON, ctrl_data->sub_id)) {
		MT_DEBUG_TRACE("dis sta reason nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	if (ctrl_data->rsp_data != NULL && ctrl_data->rsp_data_len != 0) {
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_DISC_STA_STATISTIC, ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("dis sta statistic nla put fail!!\n");
			dev_kfree_skb(skb);
			return -EINVAL;
		}
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_APStaDelSendVendorEvent);

int mtk_cfg80211os_VendorEventMloResponse(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
			ctrl_data->rsp_data_len, MTK_NL80211_VENDOR_EVENT_MLO_EVENT, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("vendor event mlo response cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("vendor event mlo response nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211os_VendorEventMloResponse);

int mtk_cfg80211_send_bss_ml_sta_profile_info_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
			ctrl_data->rsp_data_len, MTK_NL80211_VENDOR_EVENT_STA_PROFILE_EVENT, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("bss mlo sta profile cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	if (nla_put(skb, NL80211_ATTR_VENDOR_DATA, ctrl_data->rsp_data_len, ctrl_data->rsp_data)) {
		MT_DEBUG_TRACE("bss mlo sta profile nla put fail!!\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}
	cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_cfg80211_send_bss_ml_sta_profile_info_event);
int mtk_nl80211_send_zr_hdo_rm_event(void *ctrl_data_)
{
#ifndef __MOCK
	struct sk_buff *skb;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;
	struct wiphy *wiphy = (struct wiphy *)ctrl_data->wiphy;
	struct wireless_dev *wl_dev = (struct wireless_dev *)ctrl_data->wl_dev;
	unsigned int buflen = ctrl_data->rsp_data_len;
	unsigned int event_attr = ctrl_data->sub_id;

	skb = cfg80211_vendor_event_alloc(wiphy, wl_dev, buflen, MTK_NL80211_VENDOR_EVENT_ZR_HDO_RM, GFP_ATOMIC);
	if (!skb) {
		MT_DEBUG_TRACE("cosr event cfg80211_vendor_event_alloc fail!!\n");
		return -EINVAL;
	}

	switch (event_attr) {
	case MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_JOIN:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_JOIN, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("sta rssichange nla put fail!!\n");
			dev_kfree_skb(skb);
			return -EINVAL;
		}
		break;
	case MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_LEAVE:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_LEAVE, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("coap updated nla put fail!!\n");
			dev_kfree_skb(skb);
			return -EINVAL;
		}
		break;
	case MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_STEER_COMPLETE:
		if (nla_put(skb, MTK_NL80211_VENDOR_ATTR_EVENT_ZR_HDO_RM_STA_STEER_COMPLETE, buflen, ctrl_data->rsp_data)) {
			MT_DEBUG_TRACE("coap found nla put fail!!\n");
			dev_kfree_skb(skb);
			return -EINVAL;
		}
		break;
	default:
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	if (skb)
		cfg80211_vendor_event(skb, GFP_ATOMIC);
#endif /* !__MOCK */
	return 0;
}
EXPORT_SYMBOL(mtk_nl80211_send_zr_hdo_rm_event);
#endif /* CFG80211_SUPPORT */

DECLARE_SUB_MODULE_INIT(mt_osal_cfg80211_event_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_cfg80211_event_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_cfg80211_event_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_cfg80211_event_exit);

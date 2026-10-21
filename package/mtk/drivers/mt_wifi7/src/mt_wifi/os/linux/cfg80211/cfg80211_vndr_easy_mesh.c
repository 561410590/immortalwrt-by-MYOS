#ifdef MTK_LICENSE
/****************************************************************************
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
 ***************************************************************************/
#endif /* MTK_LICENSE */
/****************************************************************************

	Abstract:

	All related CFG80211 Easy Mesh Vendor Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

#ifdef CONFIG_MAP_SUPPORT

#ifdef DOT11_EHT_BE
extern struct bss_manager bss_mngr;
#endif

/*******************************************************************************
 *                           D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           Functions
 *******************************************************************************
 */

int cfg80211_drv_vndr_easymesh(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	char *cmd;
	int ret = -RTMP_IO_EOPNOTSUPP, cmd_len;
	UCHAR map_en = 0;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

#ifdef CONFIG_MAP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_SSID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_ssid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ssid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_WIRELESS_MODE) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_wireless_mode(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_wireless_mode fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_CHANNEL) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_channel(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_channel fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_HTBW) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_htbw(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_htbw fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_VHTBW) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_vhtbw(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_vhtbw fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_EHTBW) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_ehtbw(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ehtbw fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_CREATE) {
		struct mld_group *req_mld;
		struct bmgr_mlo_dev *mld;
		int req_mld_len = ctrl_data->data_len;

		if (req_mld_len < sizeof(struct mld_group)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]req_mld_len(=%d) < sizeof(struct mld_group).\n",
				req_mld_len);
			return -RTMP_IO_EINVAL;
		}
		req_mld = ctrl_data->data;

		ret = mtk_nl80211_set_mld_group_create(mac_ad, wdev, req_mld);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_set_mld_group_create fail .\n");
			return -RTMP_IO_EINVAL;
		}

		if (req_mld->mld_group_idx >= BMGR_MAX_MLD_GRP_CNT) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]invalid mld group idx(=%d) .\n",
				req_mld->mld_group_idx);
			return -RTMP_IO_EINVAL;
		}
#ifdef MLD_GROUP_ID_REMAPPING_V1
		mld = mld_idx_mapping_v1_load(req_mld->mld_group_idx);
#else
		mld = GET_MLD_BY_GRP_IDX(req_mld->mld_group_idx);
#endif
		if (!BMGR_VALID_MLO_DEV(mld)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]invalid mld group.\n");
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_cfg80211_reply_ap_mld(mld, ctrl_data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"mtk_cfg80211_reply_ap_mld failed. ret=%x.\n", ret);
			return -RTMP_IO_EINVAL;
		}
	}
#ifdef MAP_R6
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_LINK_RECONF) {
		int req_mld_len = ctrl_data->data_len;

		if (req_mld_len < sizeof(struct mld_reconf)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]req_mld_len(=%d) < sizeof(struct mld_reconf).\n",
				req_mld_len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_nl80211_set_mld_link_reconf(mac_ad, wdev, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_set_map_channel fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif /* MAP_R6 */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_DESTROY) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_mld_group_delete(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_mld_group_del fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_LINK_ADD) {
		int req_mld_len = ctrl_data->data_len;

		if (req_mld_len < sizeof(struct mld_add_link)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
			"failed - [ERROR]req_mld_len(=%d) < sizeof(struct mld_add_link).\n",
			req_mld_len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_nl80211_easymesh_set_mld_link_add(mac_ad, wdev, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh__add_mld_link fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_LINK_DEL) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_mld_del_link(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_del_link fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MLD_LINK_TRANSFER) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_mld_link_transfer(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_tranfer_mld_link fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SR_ENABLE) {
		cmd = ctrl_data->data;
		ret = mtk_nl80211_easymesh_sr_enable(mac_ad, wdev, cmd);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"failed - [ERROR]nl80211_easymesh_sr_enable fail.\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_BH_SR_BITMAP) {
		cmd = ctrl_data->data;
		if (ctrl_data->data_len < sizeof(struct SR_MESH_SRG_BITMAP_T))
			return -RTMP_IO_EINVAL;

		ret = mtk_nl80211_easymesh_set_bh_sr_bitmap(mac_ad, wdev, cmd);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"failed - [ERROR]nl80211_easymesh_sr_enable fail.\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_HIDDEN_SSID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_hidden_ssid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_hidden_ssid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_AUTH_MODE) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_auth_mode(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_auth_mode fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_ENCRYP_TYPE) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_encrypt_type(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_encrypt_type fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_DEFAULT_KEY_ID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_default_key_id(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_default_key_id fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}


	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_KEY1) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_key1(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_key1 fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_WPA_PSK) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_wpa_psk(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_wpa_psk fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_RADIO_ON) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_radio_on(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_radio_on fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_DISCONNECT_STA) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_disconnect_sta(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_disconnect_sta fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_TS_BH_PRIMARY_VID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_ts_bh_primary_vid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ts_bh_primary_vid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_TS_BH_PRIMARY_PCP) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_ts_bh_primary_pcp(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ts_bh_primary_pcp fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_TS_BH_VID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_ts_bh_vid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ts_bh_vid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_TS_FH_VID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_ts_fh_vid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_ts_fh_vid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_TRANSPARENT_VID) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_easymesh_set_transparent_vid(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_transparent_vid fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

#ifdef DOT11K_RRM_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_BCN_REQ) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (strlen(cmd) > cmd_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]invalid nla length%lu, %d .\n",
				strlen(cmd), cmd_len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_nl80211_easymesh_set_bcn_req(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_RRM, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_bcn_req fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_HTBSSCOEX) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_HtBssCoex(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_HtBssCoex fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_PMFMFPC) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_PMFMFPC(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_PMFMFPC fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_AUTO_ROAMING) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_AutoRoaming(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_AutoRoaming fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

#ifdef QOS_R2
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_DSCP_POLICY_ENABLE) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_DSCPPolicyEnable(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_QOS_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_DSCPPolicyEnable fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_QOS_MAP_CAPA) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_QosMapCapaEnable(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_QOS_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]nl80211_easymesh_set_QosMapCapa fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
#ifdef MAP_R5
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_RESET_DSCP2UP_TABLE) {
		cmd = ctrl_data->data;
		if (cmd && *cmd)
			QoS_Init_DSCP2UP_Mapping(mac_ad);
	}
#endif
#endif

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_PSK) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_psk(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_easymesh_set_psk fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

#ifdef DPP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_PMK) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_easymesh_set_pmk(mac_ad, wdev, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_DPP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_easymesh_set_pmk fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_GET_DPP_FRAME) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_send_dpp_frame(mac_ad, wdev, ctrl_data, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_DPP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_send_dpp_frame fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif /* DPP_SUPPORT */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_GET_ASSOC_REQ_FRAME) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_get_assoc_req_frame(mac_ad, wdev, ctrl_data, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_send_dpp_frame fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_GET_SPT_REUSE_REQ) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		ret = mtk_nl80211_get_spt_reuse_req(mac_ad, wdev, ctrl_data, cmd, cmd_len);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_send_spt_reuse_req_frame fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_WAPP_IE) {
		char *ie = ctrl_data->data;
		unsigned int ie_len = ctrl_data->data_len;
		INT ifIndex;

		if (wdev->wdev_type == WDEV_TYPE_AP)
			ifIndex = wdev->func_idx;
		else
			ifIndex = WDEV_NOT_FOUND;
		if (ifIndex == WDEV_NOT_FOUND) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_ERROR,
				"failed - [ERROR]can't find ifIndex in driver .\n");
			return -RTMP_IO_EINVAL;
		}

		if (!ie || !ie_len) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_DEBUG,
				"failed - invalid data.\n");
			return -RTMP_IO_EINVAL;
		}
		/* default use ap ifIndex here */
		wapp_set_ap_ie(mac_ad, ie, ie_len, ifIndex);
	}
#endif /* CONFIG_MAP_SUPPORT */
#ifdef CHANNEL_SWITCH_MONITOR_CONFIG
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_CANCEL_ROC)
		mtk_cancel_roc(mac_ad, wdev);
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_START_ROC) {
		if (ctrl_data->data_len < sizeof(struct roc_req))
			return -RTMP_IO_EINVAL;
		mtk_start_roc(mac_ad, wdev, (struct roc_req *)ctrl_data->data);
	}
#endif /* CHANNEL_SWITCH_MONITOR_CONFIG */

#ifdef MAP_R3
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_DEL_CCE_IE) {
		if (!mac_ad->bDppEnable) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"DPP Disabled please enable it\n");
			return -RTMP_IO_EINVAL;
		}

		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"Del::OID_CCe_IE_DEL\n");
		if (wdev->DPPCfg.cce_ie_len) {
			os_zero_mem(wdev->DPPCfg.cce_ie_buf, 6);
			wdev->DPPCfg.cce_ie_len = 0;
			if (wdev->bAllowBeaconing)
				UpdateBeaconHandler(mac_ad, wdev, BCN_REASON(BCN_UPDATE_IE_CHG));
		} else {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"CCE IE not enabled for %s wdev\n",
					RtmpOsGetNetDevName(wdev->if_dev));
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_R3_DPP_URI) {
		cmd = ctrl_data->data;
		cmd_len = ctrl_data->data_len;

		if (!mac_ad->bDppEnable) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"DPP Disabled please enable it\n");
			return -RTMP_IO_EINVAL;
		}

		/* If len coming from wapp is zero then clear the    *
		 * existing allocated buffer so legacy WPS can work  *
		 */
		if (cmd_len == 0) {
			if (mac_ad->dpp_uri_ptr) {
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
						"dpp uri ptr is not NULL and len is zero!\n");
				os_free_mem(mac_ad->dpp_uri_ptr);
				mac_ad->dpp_uri_ptr = NULL;
			} else
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
						"dpp uri ptr is already NULL and len is zero!\n");
			return 0;
		}

		if (mac_ad->dpp_uri_ptr) {
			if (cmd_len < mac_ad->dpp_uri_len) {
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
						"invalid nla length %d\n", cmd_len);
				return -RTMP_IO_EINVAL;
			}
			/* URI already present now need to replace it if not same*/
			if (memcmp(mac_ad->dpp_uri_ptr, cmd, mac_ad->dpp_uri_len) != 0) {
				os_free_mem(mac_ad->dpp_uri_ptr);
				mac_ad->dpp_uri_ptr = NULL;
			}
		}

		if (mac_ad->dpp_uri_ptr == NULL) {
			os_alloc_mem(mac_ad->dpp_uri_ptr, (UCHAR **)&mac_ad->dpp_uri_ptr, cmd_len);

			if (mac_ad->dpp_uri_ptr == NULL) {
				MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
						"dpp uri ptr is null, return!\n");
				return -RTMP_IO_EINVAL;
			}
			mac_ad->dpp_uri_len = (UCHAR)cmd_len;
			memcpy(mac_ad->dpp_uri_ptr, cmd, cmd_len);
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"URI copied for wdev:%s URI:%s len:%u\n",
				RtmpOsGetNetDevName(
					wdev->if_dev), mac_ad->dpp_uri_ptr, mac_ad->dpp_uri_len);
		} else {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"URI already present for wdev:%s URI:%s len:%u\n",
				RtmpOsGetNetDevName(
					wdev->if_dev), mac_ad->dpp_uri_ptr, mac_ad->dpp_uri_len);
			return -RTMP_IO_EINVAL;
		}
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_R3_1905_SEC_ENABLED) {
		cmd = ctrl_data->data;

		if (!mac_ad->bDppEnable) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"DPP Disabled please enable it\n");
			return -RTMP_IO_EINVAL;
		}

		mac_ad->map_sec_enable = *cmd;
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"setting the security flag %u for wdev:%s\n",
				 mac_ad->map_sec_enable, RtmpOsGetNetDevName(wdev->if_dev));
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_R3_ONBOARDING_TYPE) {
		cmd = ctrl_data->data;

		if (!mac_ad->bDppEnable) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
					"DPP Disabled please enable it\n");
			return -RTMP_IO_EINVAL;
		}

		mac_ad->map_onboard_type = *cmd;
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"setting the onboarding type %u for wdev:%s\n",
				 mac_ad->map_onboard_type, RtmpOsGetNetDevName(wdev->if_dev));
	}
#endif /* MAP_R3 */

#ifdef CONFIG_MAP_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_BHBSS) {
		cmd = ctrl_data->data;

		/* only do this for AP MBSS, ignore other inf type */
		if (wdev->wdev_type != WDEV_TYPE_AP)
			return -RTMP_IO_EINVAL;

		if (*cmd) {
			wdev->MAPCfg.DevOwnRole |= BIT(MAP_ROLE_BACKHAUL_BSS);
			mac_ad->bh_bss_wdev = wdev;
		} else {
			wdev->MAPCfg.DevOwnRole &= ~BIT(MAP_ROLE_BACKHAUL_BSS);
		}

		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"%s bandIdx = %d ,DevOwnRole 0x%x\n",
				wdev->if_dev->name, HcGetBandByWdev(wdev), wdev->MAPCfg.DevOwnRole);
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_FHBSS) {
#ifdef WSC_INCLUDED
		PWSC_CTRL pWscControl = &wdev->WscControl;
#endif /* WSC_INCLUDED */
		unsigned int i;

		cmd = (char *)ctrl_data->data;

		/* only do this for AP MBSS, ignore other inf type */
		if (wdev->wdev_type != WDEV_TYPE_AP)
			return -RTMP_IO_EINVAL;

		if (*cmd) {
			wdev->MAPCfg.DevOwnRole |= BIT(MAP_ROLE_FRONTHAUL_BSS);
		} else {
			wdev->MAPCfg.DevOwnRole &= ~BIT(MAP_ROLE_FRONTHAUL_BSS);
#ifdef WSC_INCLUDED
			/* reset wsc backhaul profiles */
			for (i = 0; i < pWscControl->WscBhProfiles.ProfileCnt; i++)
				NdisZeroMemory(
					&pWscControl->WscBhProfiles.Profile[i],
					sizeof(WSC_CREDENTIAL));
			pWscControl->WscBhProfiles.ProfileCnt = 0;
#endif /* WSC_INCLUDED */
		}
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_NOTICE,
				"bandIdx = %d ,DevOwnRole 0x%x\n",
				HcGetBandByWdev(wdev), wdev->MAPCfg.DevOwnRole);
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MAP_CH) {
		if (ctrl_data->data_len < sizeof(struct map_ch)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]data_len(=%d) < sizeof(struct map_ch).\n",
				ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_nl80211_set_map_channel(mac_ad, wdev, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_set_map_channel fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_MAP_ENABLE) {
		cmd = ctrl_data->data;

		map_en = *cmd;
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"MAP En value from wapp:%d\n", map_en);

		ret = mtk_nl80211_set_map_enable(mac_ad, map_en);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_set_map_enable fail\n");
			return -RTMP_IO_EINVAL;
		}

	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_BH_ASSOC_CTRL) {
		UCHAR apIdx = wdev->func_idx;
		struct bh_assoc_disallow_info *bh_disallow_info;
		struct bh_assoc_disallow_info *info;

		if (ctrl_data->data_len < sizeof(struct bh_assoc_disallow_info)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
			"failed - [ERROR]data_len(=%d) < sizeof(struct bh_assoc_disallow_info).\n",
			ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}

		bh_disallow_info = &mac_ad->ApCfg.MBSSID[apIdx].bh_disallow_info;
		info = ctrl_data->data;
		RTMPZeroMemory(bh_disallow_info, sizeof(struct bh_assoc_disallow_info));
		NdisMoveMemory(bh_disallow_info, info, sizeof(struct bh_assoc_disallow_info));
		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
			"BH BSSID ["MACSTR"], disallow profile P1 %d, P2 %d!!!\n",
			MAC2STR(mac_ad->ApCfg.MBSSID[apIdx].bh_disallow_info.bssid),
			mac_ad->ApCfg.MBSSID[apIdx].bh_disallow_info.profile1_bh_assoc_disallow,
			mac_ad->ApCfg.MBSSID[apIdx].bh_disallow_info.profile2_bh_assoc_disallow);
	}
#if defined(MTK_HOSTAPD_SUPPORT) && defined(CONFIG_MAP_SUPPORT) && defined(MAP_R3)
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_DPP_STAMAC) {
		char *peer_mac = ctrl_data->data;
		struct dpp_gas_frame_list *gas_frame = NULL;
		struct dpp_gas_frame_list *gas_frame_tmp = NULL;

		MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_INFO,
				"Peer mac addr from wapp:"MACSTR"\n", MAC2STR(peer_mac));

		OS_SEM_LOCK(&mac_ad->gas_frame_list_lock);
		/* If any gas statme machime is ongoing then clear it */
		if (!DlListEmpty(&mac_ad->dpp_gas_event_list)
				&& mac_ad->is_dpp_gas_list_init == TRUE) {
			DlListForEachSafe(gas_frame, gas_frame_tmp, &mac_ad->dpp_gas_event_list,
					struct dpp_gas_frame_list, List) {
				if (NdisEqualMemory(gas_frame->sta_mac, peer_mac, MAC_ADDR_LEN)) {
					MTWF_DBG(NULL, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
						"Free gas list with "MACSTR"\n",
						MAC2STR(gas_frame->sta_mac));
					DlListDel(&gas_frame->List);
					os_free_mem(gas_frame);
					break;
				}
			}
		}
		OS_SEM_UNLOCK(&mac_ad->gas_frame_list_lock);
	}
#endif /* MTK_HOSTAPD_SUPPORT && CONFIG_MAP_SUPPORT && MAP_R3 */
#endif /* CONFIG_MAP_SUPPORT */
#ifdef MAP_VENDOR_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_VENDOR_NOP_STATE) {
		if (ctrl_data->data_len < sizeof(struct cont_nop_info)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]data_len(=%d) < sizeof(struct cont_nop_info).\n",
				ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}

		ret = mtk_nl80211_set_map_vendor_nop_info(mac_ad, wdev, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]mtk_nl80211_set_wapp_vendor_nop_info fail .\n");
			return -RTMP_IO_EINVAL;
		}
	}
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_EASYMESH_SET_VENDOR_CH_PREF_STATE) {
		if (ctrl_data->data_len < sizeof(struct cont_ch_info)) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR]data_len(=%d) < sizeof(struct cont_ch_info).\n",
				ctrl_data->data_len);
			return -RTMP_IO_EINVAL;
		}
		ret = mtk_nl80211_set_map_vendor_ch_info(
				mac_ad, wdev, ctrl_data->data);
		if (ret) {
			MTWF_DBG(mac_ad, DBG_CAT_PROTO, CATPROTO_MAP, DBG_LVL_ERROR,
				"failed - [ERROR] mtk_nl80211_set_map_vendor_ch_info .\n");
			return -RTMP_IO_EINVAL;
		}
	}
#endif /* MAP_VENDOR_SUPPORT */

	return 0;
}
#endif /* CONFIG_MAP_SUPPORT */
#endif /* RT_CFG80211_SUPPORT */


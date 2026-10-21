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

	All related CFG80211 Config Vendor Command Function Body.

	History:

***************************************************************************/

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
struct config_set_device_proc {
	char *name;
	int (*config_set_proc)(struct physical_device *ph_dev, char *arg);
};

struct config_fwdl_action {
	enum fwdl_action action_val;
	char *name;
};

/*******************************************************************************
 *                           Set Device API
 *******************************************************************************
 */
static int config_set_e2p_access_mode(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get E2pAccessMode=%d\n", ph_dev->E2pAccessMode);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value >= NUM_OF_E2P_MODE)
		return -EINVAL;

	PD_SET_E2P_ACCESS_MODE(ph_dev, value);
	MTWF_PRINT("Set E2pAccessMode=%d\n", ph_dev->E2pAccessMode);
	return 0;
}

#ifdef TCSUPPORT_NPU_WIFI_OFFLOAD
static int config_set_npu_enable(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue;

	if (!arg) {
		MTWF_PRINT("Get glb_npu_en_node=%d\n", glb_npu_en_node);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	glb_npu_en_node = 1;
	if (!lvalue)
		glb_npu_en_node = 0;
	MTWF_PRINT("Set glb_npu_en_node=%d.\n", glb_npu_en_node);

	return 0;
}
#endif /* TCSUPPORT_NPU_WIFI_OFFLOAD */

static int config_set_local_max_txpwr_bw20(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get LocalMaxTxPwrBW20=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw20);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	ph_dev->txpwr_cfg.local_max_txpwr_bw20 = value;
	MTWF_PRINT("Set LocalMaxTxPwrBW20=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw20);
	return 0;
}

static int config_set_local_max_txpwr_bw40(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get LocalMaxTxPwrBW40=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw40);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	ph_dev->txpwr_cfg.local_max_txpwr_bw40 = value;
	MTWF_PRINT("Set LocalMaxTxPwrBW40=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw40);
	return 0;
}

static int config_set_local_max_txpwr_bw80(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get LocalMaxTxPwrBW80=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw80);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	ph_dev->txpwr_cfg.local_max_txpwr_bw80 = value;
	MTWF_PRINT("Set LocalMaxTxPwrBW80=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw80);
	return 0;
}

static int config_set_local_max_txpwr_bw160(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get LocalMaxTxPwrBW160=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw160);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	ph_dev->txpwr_cfg.local_max_txpwr_bw160 = value;
	MTWF_PRINT("Set LocalMaxTxPwrBW160=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw160);
	return 0;
}

static int config_set_local_max_txpwr_bw320(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get LocalMaxTxPwrBW320=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw320);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	ph_dev->txpwr_cfg.local_max_txpwr_bw320 = value;
	MTWF_PRINT("Set LocalMaxTxPwrBW320=%d\n", ph_dev->txpwr_cfg.local_max_txpwr_bw320);
	return 0;
}

#ifdef WHNAT_SUPPORT
static int config_set_whnat_enable(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get WHNAT=%d\n", ph_dev->whnat_en);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue)
		ph_dev->whnat_en = TRUE;
	else
		ph_dev->whnat_en = FALSE;
	MTWF_PRINT("Set WHNAT=%d\n", ph_dev->whnat_en);

	return 0;
}
#endif /* WHNAT_SUPPORT */

#ifdef PEAK_ENHANCE
static int config_set_rx_enhance_enable(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get RxEnhanceEn=%d\n", ph_dev->peak_rx_ctrl.enable);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue == 0)
		ph_dev->peak_rx_ctrl.enable = FALSE;
	else
		ph_dev->peak_rx_ctrl.enable = TRUE;
	MTWF_PRINT("Set RxEnhanceEn=%d\n", ph_dev->peak_rx_ctrl.enable);
	return 0;
}
#endif /* PEAK_ENHANCE */

#ifdef CONFIG_3_WIRE_SUPPORT
static int config_set_three_wire_function_enable(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get ThreeWireFunctionEnable=%d\n", ph_dev->threeWire_function_enable);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue)
		ph_dev->threeWire_function_enable = TRUE;
	else
		ph_dev->threeWire_function_enable = FALSE;
	MTWF_PRINT("Set ThreeWireFunctionEnable=%d\n", ph_dev->threeWire_function_enable);
	return 0;
}
#endif /* CONFIG_3_WIRE_SUPPORT */

#ifdef CFG_RED_SUPPORT
static int config_set_red_enable(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get RED_Enable --> %d\n", PD_GET_RED_ENABLE(ph_dev));
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue)
		PD_SET_RED_ENABLE(ph_dev, TRUE);
	else
		PD_SET_RED_ENABLE(ph_dev, FALSE);
	MTWF_PRINT("Set RED_Enable --> %d\n", PD_GET_RED_ENABLE(ph_dev));

	return 0;
}
#endif /* CFG_RED_SUPPORT */

#ifdef DOT11_EHT_BE
static int config_set_mlo_str_bit_map(
	struct physical_device *ph_dev, char *arg)
{
	UINT8 value = 0;

	if (!arg) {
		MTWF_PRINT("Get MloStrBitMap=0x%x\n", PD_GET_MLO_STR_BITMAP(ph_dev));
		return -EINVAL;
	}

	if (kstrtou8(arg, 16, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	PD_SET_MLO_CONFIG_OP_SET(ph_dev, TRUE);
	PD_SET_MLO_STR_BITMAP(ph_dev, value);
	MTWF_PRINT("Set MloStrBitMap=0x%x\n", PD_GET_MLO_STR_BITMAP(ph_dev));

	return 0;
}

static int config_set_mlo_sync_tx(
	struct physical_device *ph_dev, char *arg)
{
	UINT8 value = 0;

	if (!arg) {
		MTWF_PRINT("Get MloSyncTx=0x%x\n", PD_GET_MLO_SYNC_TX_ENABLE(ph_dev));
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	PD_SET_MLO_CONFIG_OP_SET(ph_dev, TRUE);
	PD_SET_MLO_SYNC_TX_ENABLE(ph_dev, value);
	MTWF_PRINT("Set MloSyncTx=0x%x\n", PD_GET_MLO_SYNC_TX_ENABLE(ph_dev));
	return 0;
}

static int config_set_mlo_v1(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get MloV1=0x%x\n", PD_GET_MLO_V1_ENABLE(ph_dev));
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue == 1)
		PD_MTK_MLO_V1_ENABLE(ph_dev);
	else
		PD_MTK_MLO_V1_DISABLE(ph_dev);

	MTWF_PRINT("Set MloV1=0x%x\n", PD_GET_MLO_V1_ENABLE(ph_dev));
	return 0;
}
#endif /* DOT11_EHT_BE */

static int config_set_wtbl_dup_num(
	struct physical_device *ph_dev, char *arg)
{
	UINT16 value = 0;
	struct _RTMP_CHIP_CAP *chip_cap = PD_GET_CHIP_CAP_PTR(ph_dev);

	if (!arg) {
		MTWF_PRINT("Get WtblDupNum=%d\n", chip_cap->WtblDupNum);
		return -EINVAL;
	}

	if (kstrtou16(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	chip_cap->WtblDupNum = value;
	MTWF_PRINT("Set WtblDupNum=%d\n", chip_cap->WtblDupNum);
	return 0;
}

static int config_set_test_mode_enable(
	struct physical_device *ph_dev, char *arg)
{
	UINT8 value = 0;

	if (!arg) {
		MTWF_PRINT("Get TestModeEn=%d\n", ph_dev->TestModeEn);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	/* 0 : NormalMode; 1 : TestMode */
	if (ph_dev->TestModeEn != value)
		PD_SET_STOP_OPT(ph_dev, 0);
	ph_dev->TestModeEn = value;
	MTWF_PRINT("Set TestModeEn=%d\n", ph_dev->TestModeEn);
	return 0;
}

#ifdef MAC_ADDR_ADJACENT_CHK
static int config_set_mac_addr_adj(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get MacAddrAdj=0x%x\n", PD_GET_MAC_ADDR_ADJ(ph_dev));
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n",
			__func__, arg);
		return -EINVAL;
	}
	if (lvalue == 0)
		PD_SET_MAC_ADDR_ADJ(ph_dev, FALSE);
	else
		PD_SET_MAC_ADDR_ADJ(ph_dev, TRUE);
	MTWF_PRINT("Set MacAddrAdj=0x%x\n", PD_GET_MAC_ADDR_ADJ(ph_dev));
	return 0;
}
#endif /* MAC_ADDR_ADJACENT_CHK */

#ifdef DOT11V_MBSSID_SUPPORT
static int config_set_11v_host_dup_bcn(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue = 0;

	if (!arg) {
		MTWF_PRINT("Get Dot11vHostDupBcn=0x%x\n", PD_GET_11V_BCN_DUP(ph_dev));
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n",
			__func__, arg);
		return -EINVAL;
	}

	if (lvalue == 0)
		PD_SET_11V_BCN_DUP(ph_dev, FALSE);
	else
		PD_SET_11V_BCN_DUP(ph_dev, TRUE);
	MTWF_PRINT("Set Dot11vHostDupBcn=0x%x\n", PD_GET_11V_BCN_DUP(ph_dev));

	return 0;
}
#endif /* DOT11V_MBSSID_SUPPORT */

#ifdef SW_CONNECT_SUPPORT
static int config_set_sw_sta_enable(
	struct physical_device *ph_dev, char *arg)
{
	UINT8 value = 0;

	if (!arg) {
		MTWF_PRINT("Get SwStaEnable=%d\n", ph_dev->bSwSta);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n",
			__func__, arg);
		return -EINVAL;
	}

	/* 0 : off; 1 : on */
	ph_dev->bSwSta = value;
	MTWF_PRINT("Set SwStaEnable=%d\n", ph_dev->bSwSta);
	return 0;
}
#endif /* SW_CONNECT_SUPPORT */


static int config_set_rro_step_one_timeout(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue;
	struct _RTMP_CHIP_CAP *chip_cap = PD_GET_CHIP_CAP_PTR(ph_dev);

	if (!arg) {
		MTWF_PRINT("Get step_one_timeout=%d\n", chip_cap->step_one_timeout);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (lvalue < 32 || lvalue > 16384)
		MTWF_PRINT("%s: Error input RRO_STEP_ONE_TIMEOUT(=%s)\n",
			__func__, arg);
	else {
		chip_cap->step_one_timeout = lvalue;
		PD_SET_RRO_TIMEOUT_PROFILE_SET(ph_dev, TRUE);
	}
	MTWF_PRINT("Set step_one_timeout=%d\n", chip_cap->step_one_timeout);

	return 0;
}

static int config_set_rro_flush_all_timeout(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue;
	struct _RTMP_CHIP_CAP *chip_cap = PD_GET_CHIP_CAP_PTR(ph_dev);

	if (!arg) {
		MTWF_PRINT("Get flush_all_timeout=%d\n", chip_cap->flush_all_timeout);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (lvalue < 32 || lvalue > 32768)
		MTWF_PRINT("%s: Error input RRO_FLUSH_ALL_TIMEOUT(=%s)\n",
			__func__, arg);
	else {
		chip_cap->flush_all_timeout = lvalue;
		PD_SET_RRO_TIMEOUT_PROFILE_SET(ph_dev, TRUE);
	}
	MTWF_PRINT("Set flush_all_timeout=%d\n", chip_cap->flush_all_timeout);

	return 0;
}

/* RRO RSS RING MAPPING 0 */
static int config_set_rro_rss_map0(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue;
	struct _RTMP_CHIP_CAP *chip_cap = PD_GET_CHIP_CAP_PTR(ph_dev);

	if (!arg) {
		MTWF_PRINT("Get RSSMAP0=0x%x\n", chip_cap->rss_map.map0);
		return -EINVAL;
	}

	if (kstrtol(arg, 16, &lvalue)) {
		MTWF_PRINT("%s: Error input RSSMAP0(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	chip_cap->rss_map.map0 = lvalue;
	MTWF_PRINT("Set RSSMAP0=0x%x\n", chip_cap->rss_map.map0);

	return 0;
}

/* RRO RSS RING MAPPING 1 */
static int config_set_rro_rss_map1(
	struct physical_device *ph_dev, char *arg)
{
	long lvalue;
	struct _RTMP_CHIP_CAP *chip_cap = PD_GET_CHIP_CAP_PTR(ph_dev);

	if (!arg) {
		MTWF_PRINT("Get RSSMAP1=0x%x\n", chip_cap->rss_map.map1);
		return -EINVAL;
	}

	if (kstrtol(arg, 16, &lvalue)) {
		MTWF_PRINT("%s: Error input RSSMAP0(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	chip_cap->rss_map.map1 = lvalue;
	MTWF_PRINT("Set RSSMAP1=0x%x\n", chip_cap->rss_map.map1);

	return 0;
}

static int config_set_edcca_region(
	struct physical_device *ph_dev, char *arg)
{
	UINT8 value;

	if (!arg) {
		MTWF_PRINT("Get EDCCARegion:%d\n", ph_dev->edcca_region);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	ph_dev->edcca_region = value;
	MTWF_PRINT("Set EDCCARegion:%d\n", ph_dev->edcca_region);
	return 0;
}

static int config_set_rro_support(
	struct physical_device *ph_dev, char *arg)
{
	unsigned char value;
	struct _RTMP_CHIP_CAP *cap = &ph_dev->chip_cap;

	if (!arg) {
		MTWF_PRINT("Get hw_rro_en=%d\n", cap->hw_rro_en);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		cap->hw_rro_en = TRUE;
	else
		cap->hw_rro_en = FALSE;
	MTWF_PRINT("Set hw_rro_en:%d\n", cap->hw_rro_en);
	return 0;
}

struct config_set_device_proc CONFIG_SET_DEVICE_PROC[] = {
	{"E2pAccessMode", config_set_e2p_access_mode},
#ifdef TCSUPPORT_NPU_WIFI_OFFLOAD
	{"NPU_EN", config_set_npu_enable},
#endif /* TCSUPPORT_NPU_WIFI_OFFLOAD */
	{"LocalMaxTxPwrBW20", config_set_local_max_txpwr_bw20},
	{"LocalMaxTxPwrBW40", config_set_local_max_txpwr_bw40},
	{"LocalMaxTxPwrBW80", config_set_local_max_txpwr_bw80},
	{"LocalMaxTxPwrBW160", config_set_local_max_txpwr_bw160},
	{"LocalMaxTxPwrBW320", config_set_local_max_txpwr_bw320},
#ifdef WHNAT_SUPPORT
	{"WHNAT", config_set_whnat_enable},
#endif /* WHNAT_SUPPORT */
#ifdef PEAK_ENHANCE
	{"RxEnhanceEn", config_set_rx_enhance_enable},
#endif /* PEAK_ENHANCE */
#ifdef CONFIG_3_WIRE_SUPPORT
	{"ThreeWireFunctionEnable", config_set_three_wire_function_enable},
#endif /* CONFIG_3_WIRE_SUPPORT */
#ifdef CFG_RED_SUPPORT
	{"RED_Enable", config_set_red_enable},
#endif /* CFG_RED_SUPPORT */
#ifdef DOT11_EHT_BE
	{"MloStrBitMap", config_set_mlo_str_bit_map},
	{"MloSyncTx", config_set_mlo_sync_tx},
	{"MloV1", config_set_mlo_v1},
#endif /* DOT11_EHT_BE */
	{"WtblDupNum", config_set_wtbl_dup_num},
	{"TestModeEn", config_set_test_mode_enable},
#ifdef MAC_ADDR_ADJACENT_CHK
	{"MacAddrAdj", config_set_mac_addr_adj},
#endif /* MAC_ADDR_ADJACENT_CHK */
#ifdef DOT11V_MBSSID_SUPPORT
	{"Dot11vHostDupBcn", config_set_11v_host_dup_bcn},
#endif /* DOT11V_MBSSID_SUPPORT */
#ifdef SW_CONNECT_SUPPORT
	{"SwStaEnable", config_set_sw_sta_enable},
#endif /* SW_CONNECT_SUPPORT */
	{"RroStepOneTimeout", config_set_rro_step_one_timeout},
	{"RroFlushAllTimeout", config_set_rro_flush_all_timeout},
	{"RSSMAP0", config_set_rro_rss_map0},
	{"RSSMAP1", config_set_rro_rss_map1},
	{"EDCCARegion", config_set_edcca_region},
	{"RROSupport", config_set_rro_support},
	{NULL,}
};


/*******************************************************************************
 *                           Set Band API
 *******************************************************************************
 */

struct config_set_band_proc {
	char *name;
	int (*config_set_proc)(struct _RTMP_ADAPTER *mac_ad, char *arg);
};

static int config_set_wifi_cert(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get wifi_cert=%d\n", mac_ad->CommonCfg.wifi_cert);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.wifi_cert = TRUE;
	else
		mac_ad->CommonCfg.wifi_cert = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
				"WifiCert=%d\n", mac_ad->CommonCfg.wifi_cert);
	return 0;
}

static int config_set_tgn_wmm_cert(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get tgn_wmm_cert=%d\n", mac_ad->CommonCfg.tgn_wmm_cert);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.tgn_wmm_cert = TRUE;
	else
		mac_ad->CommonCfg.tgn_wmm_cert = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
				"tgn_wmm_cert=%d\n", mac_ad->CommonCfg.tgn_wmm_cert);
	return 0;
}

static int config_set_country_region(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	int retval;

	if (!arg) {
		MTWF_PRINT("Get CountryRegion=%d\n", mac_ad->CommonCfg.CountryRegion);
		return -EINVAL;
	}

	retval = RT_CfgSetCountryRegion(mac_ad, arg, BAND_24G);
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"CountryRegion=%d\n", mac_ad->CommonCfg.CountryRegion);
	return retval;
}

static int config_set_country_region_Aband(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	int retval;

	if (!arg) {
		MTWF_PRINT("Get CountryRegionForABand=%d\n",
			mac_ad->CommonCfg.CountryRegionForABand);
		return -EINVAL;
	}

	retval = RT_CfgSetCountryRegion(mac_ad, arg, BAND_5G);
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"CountryRegionABand=%d\n", mac_ad->CommonCfg.CountryRegionForABand);
	return retval;
}

static int config_set_freq_list(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	CfgSetFreqList(mac_ad, arg);
	return 0;
}

static int config_set_country_code(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	if (mac_ad->CommonCfg.bCountryFlag == 0)
		RTMPSetCountryCode(mac_ad, arg);
	return 0;
}

#ifdef MT_DFS_SUPPORT
static int config_set_dfs_calibration(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get DisableDfsCal=%d\n", mac_ad->CommonCfg.DfsParameter.DisableDfsCal);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.DisableDfsCal = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.DisableDfsCal = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"DisableDfsCal=%d\n", mac_ad->CommonCfg.DfsParameter.DisableDfsCal);
	return 0;
}

static int config_set_dfs_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get bDfsEnable=%d\n", mac_ad->CommonCfg.DfsParameter.bDfsEnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.bDfsEnable = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.bDfsEnable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"bDfsEnable=%d\n", mac_ad->CommonCfg.DfsParameter.bDfsEnable);
	return 0;
}

#ifdef MT_BAND4_DFS_SUPPORT
static int config_set_band4_dfs_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get band4DfsEnable=%d\n",
			mac_ad->CommonCfg.DfsParameter.band4DfsEnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.band4DfsEnable = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.band4DfsEnable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"band4DfsEnable=%d\n", mac_ad->CommonCfg.DfsParameter.band4DfsEnable);
	return 0;
}
#endif /* MT_BAND4_DFS_SUPPORT */

#ifdef CONFIG_RCSA_SUPPORT
static int config_set_dfs_use_csa_cfg(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get fUseCsaCfg=%d\n",
			mac_ad->CommonCfg.DfsParameter.fUseCsaCfg);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.fUseCsaCfg = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.fUseCsaCfg = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"fUseCsaCfg=%d\n", mac_ad->CommonCfg.DfsParameter.fUseCsaCfg);
	return 0;
}

static int config_set_dfs_rcsa_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get bRCSAEn=%d\n",
			mac_ad->CommonCfg.DfsParameter.bRCSAEn);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.bRCSAEn = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.bRCSAEn = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"fUseCsaCfg=%d\n", mac_ad->CommonCfg.DfsParameter.bRCSAEn);
	return 0;
}
#endif /* CONFIG_RCSA_SUPPORT */

static int config_set_dfs_zero_wait_cac_time(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get DfsZeroWaitCacTime=%d\n",
			mac_ad->CommonCfg.DfsParameter.DfsZeroWaitCacTime);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	mac_ad->CommonCfg.DfsParameter.DfsZeroWaitCacTime = value; /* Unit is minute */
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"DfsZeroWaitCacTime=%d\n",
		mac_ad->CommonCfg.DfsParameter.DfsZeroWaitCacTime);
	return 0;
}

static int config_set_dfs_ocac_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get bOcacEnable=%d\n",
			mac_ad->CommonCfg.DfsParameter.bOcacEnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	dfs_set_ocac_enable(mac_ad, value);
	return 0;
}

#endif /* MT_DFS_SUPPORT */


static int config_set_auto_channel_select(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	auto_ch_select_set_cfg(mac_ad, arg);
	return 0;
}

static int config_set_auto_channel_skip_list(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	int i, count;
	unsigned char *ptr;

	if (!arg)
		return -EINVAL;

	count = delimitcnt(arg, ";");
	if (count) {
		if (count > MAX_NUM_OF_CHANNELS) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
				" Invalid setting, out of bound setting.(count=%d)\n", count);
			return -EINVAL;
		}
		mac_ad->ApCfg.AutoChannelSkipListNum = count + 1;
		for (i = 0, ptr = rstrtok(arg, ";"); ptr; ptr = rstrtok(NULL, ";"), i++) {
			if (i < mac_ad->ApCfg.AutoChannelSkipListNum) {
				mac_ad->ApCfg.AutoChannelSkipList[i] = os_str_tol(ptr, 0, 10);
				MTWF_DBG(NULL,
					DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
					" AutoChannelSkipList[%d]= %d\n", i,
					mac_ad->ApCfg.AutoChannelSkipList[i]);
			} else
				break;
		}
	} else
		mac_ad->ApCfg.AutoChannelSkipListNum = 0;

#ifdef CONFIG_6G_AFC_SUPPORT
	afc_save_autochannel_skip_init(mac_ad);
#endif /*CONFIG_6G_AFC_SUPPORT*/
	return 0;
}

static int config_set_acs_scan_mode(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_scan_mode(mac_ad, arg);
	return 0;
}

static int config_set_acs_scan_dwell(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_scan_dwell(mac_ad, arg);
	return 0;
}

static int config_set_acs_restore_dwell(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_restore_dwell(mac_ad, arg);
	return 0;
}

static int config_set_acs_partial_scan_ch_num(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_partial_scan_ch_num(mac_ad, arg);
	return 0;
}

static int config_set_acs_ch_util_thr(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_ch_util_thr(mac_ad, arg);
	return 0;
}

static int config_set_acs_ch_ice_ch_util_thr(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_ch_ice_ch_util_thr(mac_ad, arg);
	return 0;
}

static int config_set_acs_max_acs_times(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_max_acs_times(mac_ad, arg);
	return 0;
}

static int config_set_acs_sta_num_thr(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_sta_num_thr(mac_ad, arg);
	return 0;
}

static int config_set_sw_ch_thr(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	acs_set_switch_thr(mac_ad, arg);
	return 0;
}

#ifdef CONFIG_6G_SUPPORT
static int config_set_psc_acs(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	auto_ch_select_PSC_cfg(mac_ad, arg);
	return 0;
}
#ifdef CONFIG_6G_AFC_SUPPORT
static int config_set_afc_device_type(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get AfcDeviceType=%d\n", mac_ad->CommonCfg.AfcDeviceType);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value >= AFC_MAX_DEVICE_TYPE)
		MTWF_PRINT("Invalid afc device parameter, enter 1 for standard device else 0!\n");
	else
		mac_ad->CommonCfg.AfcDeviceType = (UINT8)value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"AfcDeviceType=%d\n", mac_ad->CommonCfg.AfcDeviceType);
	return 0;
}

static int config_set_afc_sp_bw_dup(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get AfcSpBwDup=%d\n", mac_ad->CommonCfg.AfcSpBwDup);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.AfcSpBwDup = TRUE;
	else
		mac_ad->CommonCfg.AfcSpBwDup = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"AfcSpBwDup=%d\n", mac_ad->CommonCfg.AfcSpBwDup);
	return 0;
}

static int config_set_afc_freq_range_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	RTMP_STRING *macptr = NULL;
	INT i = 0, j = 0;
	long NumOfFreqRange = 0;
	long Frequency = 0;
	long ret;

	for (i = 0, macptr = rstrtok(arg, ":"); macptr; macptr = rstrtok(NULL, ":"), i++) {
		if (i == 0) {
			if (kstrtol(macptr, 10, &NumOfFreqRange) == 0) {
				afc_update_freq_num(mac_ad, NumOfFreqRange);
				continue;
			} else {
				MTWF_PRINT("%s: Error input (=%s)\n", __func__, macptr);
				return -EINVAL;
			}

		}

		ret = kstrtol(macptr, 10, &Frequency);
		if (ret == 0)
			afc_update_freq_range(i, j, (UINT16)Frequency);
		else {
			MTWF_PRINT("%s: Error input (=%s)\n", __func__, macptr);
			return -EINVAL;
		}
		if (i % 2 == 0)
			j++;
	}
	return 0;
}

static int config_set_acs_after_afc(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get AcsAfterAfc=%d\n", mac_ad->CommonCfg.AcsAfterAfc);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.AcsAfterAfc = TRUE;
	else
		mac_ad->CommonCfg.AcsAfterAfc = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"AcsAfterAfc=%d\n", mac_ad->CommonCfg.AcsAfterAfc);
	return 0;
}

static int config_set_afc_spectrum_type(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get AfcSpectrumType=%d\n", mac_ad->CommonCfg.AfcSpectrumType);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value >= AFC_MAX_SPECTRUM_TYPE)
		MTWF_PRINT("Invalid afc spectrum type.\n");
	else
		mac_ad->CommonCfg.AfcSpectrumType = (UINT8)value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"AfcSpectrumType=%d\n", mac_ad->CommonCfg.AfcSpectrumType);
	return 0;
}

static int config_set_afc_opclass_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg, UINT OpClass)
{
	RTMP_STRING *macptr = NULL;
	INT i = 0, count = 0;
	long Enable = 0;
	long Channel = 0;
	long ret;

	for (i = 0, macptr = rstrtok(arg, ";"); macptr; macptr = rstrtok(NULL, ";"), i++) {
		if (i == 0) {
			if (kstrtol(macptr, 10, &Enable) == 0) {
				afc_update_op_class_en(OpClass, (UINT8)Enable);
				continue;
			} else {
				MTWF_PRINT("%s: Error input (=%s)\n", __func__, macptr);
				return -EINVAL;
			}
		}

		ret = kstrtol(macptr, 10, &Channel);
		if (ret == 0) {
			afc_update_op_class_channel(OpClass, (UINT8)count, (UINT8)Channel);
			count++;
		} else {
			MTWF_PRINT("%s: Error input (=%s)\n", __func__, macptr);
			return -EINVAL;
		}
	}
	afc_update_op_class_channel_count(OpClass, (UINT8)count);
	return 0;
}

static int config_set_afc_opclass_131_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_131);
}

static int config_set_afc_opclass_132_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_132);
}

static int config_set_afc_opclass_133_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_133);
}

static int config_set_afc_opclass_134_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_134);
}

static int config_set_afc_opclass_135_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_135);
}

static int config_set_afc_opclass_136_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_136);
}

static int config_set_afc_opclass_137_param(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return config_set_afc_opclass_param(mac_ad, arg, AFC_OP_CLASS_137);
}

#endif /* CONFIG_6G_AFC_SUPPORT */
#endif /* CONFIG_6G_SUPPORT */

static int config_set_seamless_csa(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get bIEEE80211H=%d\n", mac_ad->CommonCfg.bIEEE80211H);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.bIEEE80211H = TRUE;
	else
		mac_ad->CommonCfg.bIEEE80211H = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"bIEEE80211H=%d\n", mac_ad->CommonCfg.bIEEE80211H);
	return 0;
}

static int config_set_basic_rate(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned long ulvalue;

	if (!arg) {
		MTWF_PRINT("Get BasicRate=%ld\n", mac_ad->CommonCfg.BasicRateBitmap);
		return -EINVAL;
	}

	if (kstrtoul(arg, 10, &ulvalue)) {
		MTWF_PRINT("%s: Error input (=%s)\n", __func__, arg);
		return -EINVAL;
	}
	mac_ad->CommonCfg.BasicRateBitmap = ulvalue;
	mac_ad->CommonCfg.BasicRateBitmapOld = ulvalue;
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"BasicRate=%ld\n", mac_ad->CommonCfg.BasicRateBitmap);
	return 0;
}

#ifdef GN_MIXMODE_SUPPORT
static int config_set_gn_mix_mode(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get GNMixMode=%d\n", mac_ad->CommonCfg.GNMixMode);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.GNMixMode = TRUE;
	else
		mac_ad->CommonCfg.GNMixMode = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"GNMixMode=%d\n", mac_ad->CommonCfg.GNMixMode);
	return 0;
}
#endif /* GN_MIXMODE_SUPPORT */

static int config_set_beacon_period(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned short value;

	if (!arg) {
		MTWF_PRINT("Get BeaconPeriod=%d\n", mac_ad->CommonCfg.BeaconPeriod);
		return -EINVAL;
	}

	if (kstrtou16(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input (=%s)\n", __func__, arg);
		return -EINVAL;
	}
	/* The acceptable is 20~1000 ms. Refer to WiFi test plan. */
	if (value >= 20 && value <= 1000)
		mac_ad->CommonCfg.BeaconPeriod = value;
	else
		mac_ad->CommonCfg.BeaconPeriod = 100;	/* Default value*/
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"BeaconPeriod=%d\n", mac_ad->CommonCfg.BeaconPeriod);
	return 0;
}

#ifdef BAND_STEERING
static int config_set_band_steering(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get BandSteering=%d\n", mac_ad->ApCfg.BandSteering);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->ApCfg.BandSteering = TRUE;
	else
		mac_ad->ApCfg.BandSteering = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"BandSteering=%d\n", mac_ad->ApCfg.BandSteering);
	return 0;
}
#endif /* BAND_STEERING */

static int config_set_tx_power(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get ucTxPowerPercentage=%d\n", mac_ad->CommonCfg.ucTxPowerPercentage);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	mac_ad->CommonCfg.ucTxPowerPercentage = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"ucTxPowerPercentage=%d\n", mac_ad->CommonCfg.ucTxPowerPercentage);

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(mac_ad) {
		mac_ad->CommonCfg.ucTxPowerDefault =
			mac_ad->CommonCfg.ucTxPowerPercentage;
	}
#endif /* CONFIG_STA_SUPPORT */

	return 0;
}

#ifdef SINGLE_SKU_V2
static int config_set_sku_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get SKUenable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SKUenable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.SKUenable = TRUE;
	else
		mac_ad->CommonCfg.SKUenable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]SKUenable=%d\n", hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SKUenable);
	return 0;
}

static int config_set_bf_backoff_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get BFBACKOFFenable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.BFBACKOFFenable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.BFBACKOFFenable = TRUE;
	else
		mac_ad->CommonCfg.BFBACKOFFenable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]BFBACKOFFenable=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.BFBACKOFFenable);
	return 0;
}

static int config_set_pus_sku_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get PsuSkuEnable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PsuSkuEnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.PsuSkuEnable = TRUE;
	else
		mac_ad->CommonCfg.PsuSkuEnable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]PsuSkuEnable=%d\n", hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PsuSkuEnable);
	return 0;
}

static int config_set_psu_sku_table_idx(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get PsuSkuTableIdx=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PsuSkuTableIdx);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	mac_ad->CommonCfg.PsuSkuTableIdx = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]PsuSkuTableIdx=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PsuSkuTableIdx);
	return 0;
}
#endif /* SINGLE_SKU_V2 */

static int config_set_sku_table_idx(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get SKUTableIdx=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SKUTableIdx);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	mac_ad->CommonCfg.SKUTableIdx = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]SKUTableIdx=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SKUTableIdx);
	return 0;
}

static int config_set_percentag_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get PERCENTAGEenable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PERCENTAGEenable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.PERCENTAGEenable = TRUE;
	else
		mac_ad->CommonCfg.PERCENTAGEenable = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]PERCENTAGEenable=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.PERCENTAGEenable);
	return 0;
}

static int config_set_bg_protection(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned long ulvalue;

	if (!arg) {
		MTWF_PRINT("Get BGProtection=%ld\n", mac_ad->CommonCfg.UseBGProtection);
		return -EINVAL;
	}

	if (kstrtoul(arg, 10, &ulvalue)) {
		MTWF_PRINT("%s: Error input (=%s)\n", __func__, arg);
		return -EINVAL;
	}

	switch (ulvalue) {
	case 1: /*Always On*/
		mac_ad->CommonCfg.UseBGProtection = 1;
		break;

	case 2: /*Always OFF*/
		mac_ad->CommonCfg.UseBGProtection = 2;
		break;

	case 0: /*AUTO*/
	default:
		mac_ad->CommonCfg.UseBGProtection = 0;
		break;
	}

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"BGProtection=%ld\n", mac_ad->CommonCfg.UseBGProtection);
	return 0;
}

static int config_set_disable_olbc(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("Get OLBCDetection=%ld\n", mac_ad->CommonCfg.DisableOLBCDetect);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.DisableOLBCDetect = TRUE;
	else
		mac_ad->CommonCfg.DisableOLBCDetect = FALSE;
	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"OLBCDetection=%ld\n", mac_ad->CommonCfg.DisableOLBCDetect);
	return 0;
}

static int config_set_tx_preamble(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned long ulvalue;

	if (!arg) {
		MTWF_PRINT("Get TxPreamble=%ld\n", mac_ad->CommonCfg.TxPreamble);
		return -EINVAL;
	}

	if (kstrtoul(arg, 10, &ulvalue)) {
		MTWF_PRINT("%s: Error input (=%s)\n", __func__, arg);
		return -EINVAL;
	}

	switch (ulvalue) {
	case Rt802_11PreambleShort:
		mac_ad->CommonCfg.TxPreamble = Rt802_11PreambleShort;
		break;

	case Rt802_11PreambleAuto:
		mac_ad->CommonCfg.TxPreamble = Rt802_11PreambleAuto;
		break;

	case Rt802_11PreambleLong:
	default:
		mac_ad->CommonCfg.TxPreamble = Rt802_11PreambleLong;
		break;
	}

	MTWF_DBG(NULL, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"TxPreamble=%ld\n", mac_ad->CommonCfg.TxPreamble);
	return 0;
}

#ifdef VLAN_SUPPORT
static int config_set_vlan_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get PERCENTAGEenable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bEnableVlan);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.bEnableVlan = TRUE;
	else
		mac_ad->CommonCfg.bEnableVlan = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]bEnableVlan=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bEnableVlan);
	return 0;
}
#endif /* VLAN_SUPPORT */

static int config_set_tx_burst(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get bEnableTxBurst=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bEnableTxBurst);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.bEnableTxBurst = TRUE;
	else
		mac_ad->CommonCfg.bEnableTxBurst = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]bEnableTxBurst=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bEnableTxBurst);
	return 0;
}

static int config_set_no_forwarding_bt_nbssid(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get IsolateInterStaTrafficBTNBSSID=%ld\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.IsolateInterStaTrafficBTNBSSID);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->ApCfg.IsolateInterStaTrafficBTNBSSID = TRUE;
	else
		mac_ad->ApCfg.IsolateInterStaTrafficBTNBSSID = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d]IsolateInterStaTrafficBTNBSSID=%ld\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.IsolateInterStaTrafficBTNBSSID);
	return 0;
}

#ifdef BACKGROUND_SCAN_SUPPORT
static int config_set_dfs_zero_wait(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char DfsZeroWait;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &DfsZeroWait)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if ((DfsZeroWait == 1)
#ifdef MT_DFS_SUPPORT
		&& IS_SUPPORT_MT_DFS(mac_ad)
#endif
	   ) {
		mac_ad->BgndScanCtrl.DfsZeroWaitSupport = TRUE;/*Enable*/
#ifdef MT_DFS_SUPPORT
		UPDATE_MT_ZEROWAIT_DFS_Support(mac_ad, TRUE);
#endif
	} else {
		mac_ad->BgndScanCtrl.DfsZeroWaitSupport = FALSE;
#ifdef MT_DFS_SUPPORT
		UPDATE_MT_ZEROWAIT_DFS_Support(mac_ad, FALSE);
#endif
	}

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"DfsZeroWait Support=%d/%d\n", DfsZeroWait,
		mac_ad->BgndScanCtrl.DfsZeroWaitSupport);
	return 0;
}

#ifdef MT_DFS_SUPPORT
static int config_set_dfs_dedicated_zero_wait(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char DfsDedicatedZeroWait;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &DfsDedicatedZeroWait)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	dfs_set_dedicated_zero_wait(mac_ad, DfsDedicatedZeroWait);
	return 0;
}

static int config_set_dfs_zero_wait_default(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.DfsParameter.bDedicatedZeroWaitDefault = TRUE;
	else
		mac_ad->CommonCfg.DfsParameter.bDedicatedZeroWaitDefault = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] bDedicatedZeroWaitDefault=%d\n",
		hc_get_hw_band_idx(mac_ad),
		mac_ad->CommonCfg.DfsParameter.bDedicatedZeroWaitDefault);
	return 0;
}

static int config_set_dfs_zero_wait_band_idx(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	dfs_set_zero_wait_band_idx(mac_ad, value);
	return 0;
}

static int config_set_dfs_target_ch(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	return dfs_set_target_ch(mac_ad, arg);
}

static int config_set_dfs_ch_sel_prefer(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value >= RadarDetectSelectNum) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "Invalid parameter!\n");
		return -EINVAL;
	}
	mac_ad->CommonCfg.DfsParameter.DfsChSelPrefer = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] DfsChSelPrefer=%d\n",
		hc_get_hw_band_idx(mac_ad),
		mac_ad->CommonCfg.DfsParameter.DfsChSelPrefer);
	return 0;
}

static int config_set_dfs_nop_expire_set_ch_policy(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value >= DFS_NOP_EXPIRE_SET_CH_POLICY_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
			"Invalid parameter!\n");
		return -EINVAL;
	}
	mac_ad->CommonCfg.DfsParameter.DfsNopExpireSetChPolicy = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] DfsNopExpireSetChPolicy=%d\n",
		hc_get_hw_band_idx(mac_ad),
		mac_ad->CommonCfg.DfsParameter.DfsNopExpireSetChPolicy);
	return 0;
}

static int config_set_dfs_dedicated_rx_preselect_ch(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg)
		return -EINVAL;

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value >= DFS_PRE_SELECT_CH_NUM) {
		MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
			"Invalid parameter!\n");
		return -EINVAL;
	}
	mac_ad->CommonCfg.DfsParameter.bDfsDedicatedRxPreselectCh = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] bDfsDedicatedRxPreselectCh=%d\n",
		hc_get_hw_band_idx(mac_ad),
		mac_ad->CommonCfg.DfsParameter.bDfsDedicatedRxPreselectCh);
	return 0;
}
#endif /* MT_DFS_SUPPORT */
#endif /* BACKGROUND_SCAN_SUPPORT */

static int config_set_edcca_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get u1EDCCACtrl=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.u1EDCCACtrl);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.u1EDCCACtrl = TRUE;
	else
		mac_ad->CommonCfg.u1EDCCACtrl = FALSE;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] u1EDCCACtrl=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.u1EDCCACtrl);
	return 0;
}

static int config_set_acs_check_time(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned int value;
	AUTO_CH_CTRL *pAutoChCtrl = NULL;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get ACSCheckTime=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.AcsCfg.ACSCheckTime);
		return -EINVAL;
	}

	if (kstrtou32(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	pAutoChCtrl = HcGetAutoChCtrl(mac_ad);
	if (pAutoChCtrl == NULL) {
		MTWF_PRINT("pAutoChCtrl is NULL!\n");
		return -EINVAL;
	}

	mac_ad->ApCfg.AcsCfg.ACSCheckTime = value;
	pAutoChCtrl->AutoChSelCtrl.AutoCheckStartTime = 0;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] ACSCheckTime=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.AcsCfg.ACSCheckTime);
	return 0;
}

static int config_set_acs_check_min_time(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned int value;
	AUTO_CH_CTRL *pAutoChCtrl = NULL;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get ACSCheckTime=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.AcsCfg.ACSCheckTime);
		return -EINVAL;
	}

	if (kstrtou32(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	pAutoChCtrl = HcGetAutoChCtrl(mac_ad);
	if (pAutoChCtrl == NULL) {
		MTWF_PRINT("pAutoChCtrl is NULL!\n");
		return -EINVAL;
	}

	value = value * 60;/* minute to second */
	mac_ad->ApCfg.AcsCfg.ACSCheckTime = value;
	pAutoChCtrl->AutoChSelCtrl.AutoCheckStartTime = 0;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] ACSCheckTime=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->ApCfg.AcsCfg.ACSCheckTime);
	return 0;
}

static int config_set_short_slot(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	long lvalue;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get SlotTime=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SlotTime);
		return -EINVAL;
	}

	if (kstrtol(arg, 10, &lvalue)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (lvalue) {
		mac_ad->CommonCfg.bUseShortSlotTime = TRUE;
		mac_ad->CommonCfg.SlotTime = 9;
	} else {
		mac_ad->CommonCfg.bUseShortSlotTime = FALSE;
		mac_ad->CommonCfg.SlotTime = 20;
	}
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] SlotTime=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.SlotTime);
	return 0;
}

static int config_set_rd_region(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	if (!arg) {
		MTWF_PRINT("[BAND%d]Get RDDurRegion=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.RDDurRegion);
		return -EINVAL;
	}

	set_radar_detect_region(mac_ad, arg);
	return 0;
}

#ifdef QOS_R1
static int config_set_qos_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get bQoSEnable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->bQoSEnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value) {
		mac_ad->bQoSEnable = TRUE;
#ifdef DSCP_PRI_SUPPORT
		QoS_Init_DSCP2UP_Mapping(mac_ad);
#endif /* DSCP_PRI_SUPPORT */
	} else
		mac_ad->bQoSEnable = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] bQoSEnable=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->bQoSEnable);
	return 0;
}
#endif /* QOS_R1 */

static int config_set_se_off(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get bSeOff=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bSeOff);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	if (value)
		mac_ad->CommonCfg.bSeOff = TRUE;
	else
		mac_ad->CommonCfg.bSeOff = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] bSeOff=%d\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bSeOff);
	return 0;
}

static int config_set_bcn_data_rate(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	UINT16 value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get BcnDataRate=0x%x\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.BcnDataRate);
		return -EINVAL;
	}

	if (kstrtou16(arg, 16, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}
	mac_ad->CommonCfg.BcnDataRate = value;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO,
		"[BAND%d] BcnDataRate=0x%x\n",
		hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.BcnDataRate);
	return 0;
}


#ifdef DOT11_HE_AX
static int config_set_bss_color_value(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	if (!arg) {
		MTWF_PRINT("[BAND%d]Get bss_color_enable=%d\n",
			hc_get_hw_band_idx(mac_ad),
			mac_ad->ApCfg.bss_color_cfg.bss_color_enable);
		return -EINVAL;
	}
	bss_color_profile_enable(mac_ad, arg);
	return 0;
}
#endif /* DOT11_HE_AX */

static int config_set_wifi6g_cap(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get Wifi6gCap=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->Wifi6gCap);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->Wifi6gCap = TRUE;
	else
		mac_ad->Wifi6gCap = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "Wifi6gCap=%d\n",
			mac_ad->Wifi6gCap);
	return 0;
}

static int config_set_lpi_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get LpiEn=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.LpiEn);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.LpiEn = TRUE;
	else
		mac_ad->CommonCfg.LpiEn = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "LpiEn=%d\n",
			mac_ad->CommonCfg.LpiEn);
	return 0;
}

#ifdef MLR_SUPPORT
static int config_set_mlr_enable(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get bMLREnable=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.bMLREnable);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->CommonCfg.bMLREnable = TRUE;
	else
		mac_ad->CommonCfg.bMLREnable = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "bMLREnable=%d\n",
			mac_ad->CommonCfg.bMLREnable);
	return 0;
}
#endif /* MLR_SUPPORT */

#ifdef WIFI_MD_COEX_SUPPORT
static int config_set_idc_state(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get idcState=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->idcState);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	if (value)
		mac_ad->idcState = TRUE;
	else
		mac_ad->idcState = FALSE;

	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "idcState=%d\n",
			mac_ad->idcState);
	return 0;
}
#endif /* WIFI_MD_COEX_SUPPORT */

static int config_set_single_rnr(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	unsigned char value;

	if (!arg) {
		MTWF_PRINT("[BAND%d]Get idcState=%d\n",
			hc_get_hw_band_idx(mac_ad), mac_ad->CommonCfg.single_rnr);
		return -EINVAL;
	}

	if (kstrtou8(arg, 10, &value)) {
		MTWF_PRINT("%s: Error input(=%s)\n", __func__, arg);
		return -EINVAL;
	}

	mac_ad->CommonCfg.single_rnr = value;
	MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_PROFILE, DBG_LVL_INFO, "single_rnr=%d\n",
			mac_ad->CommonCfg.single_rnr);
	return 0;
}

static int config_set_channel_priority(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{
	if (!arg)
		return -EINVAL;

	/*Get the channel priority segment*/
	/*data struct: Channel_priority=36-48:1,52-64:3,10*/
	/*not defined means priority 0, priority 0 channel won't be selected,*/
	/*priority:0~14*/
	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_DFS, DBG_LVL_INFO,
		"[ch prio] begin parsing ch priority :%s\n", arg);

	ParseChPriority(mac_ad, arg);
	return 0;
}

static int config_set_forbid_bw40(
	struct _RTMP_ADAPTER *mac_ad, char *arg)
{

	if (!arg)
		return -EINVAL;

	MTWF_DBG(mac_ad, DBG_CAT_CHN, CATCHN_DFS, DBG_LVL_INFO,
		"[Forbid BW40] begin parsing Forbid_BW40 :%s\n", arg);

	ParseForbidBW40(mac_ad, arg);
	return 0;
}

struct config_set_band_proc CONFIG_SET_BAND_PROC[] = {
	{"WifiCert", config_set_wifi_cert},
	{"TgnWMMCert", config_set_tgn_wmm_cert},
	{"CountryRegion", config_set_country_region},
	{"CountryRegionABand", config_set_country_region_Aband},
	{"FreqList", config_set_freq_list},
	{"CountryCode", config_set_country_code},
#ifdef MT_DFS_SUPPORT
	{"DfsCalibration", config_set_dfs_calibration},
	{"DfsEnable", config_set_dfs_enable},
#ifdef MT_BAND4_DFS_SUPPORT
	{"Band4DfsEnable", config_set_band4_dfs_enable},
#endif /* MT_BAND4_DFS_SUPPORT */
#ifdef CONFIG_RCSA_SUPPORT
	{"DfsUseCsaCfg", config_set_dfs_use_csa_cfg},
	{"DfsRCSAEn", config_set_dfs_rcsa_enable},
#endif /* CONFIG_RCSA_SUPPORT */
	{"DfsZeroWaitCacTime", config_set_dfs_zero_wait_cac_time},
	{"OcacEnable", config_set_dfs_ocac_enable},
#endif /* MT_DFS_SUPPORT */
	{"AutoChannelSelect", config_set_auto_channel_select},
	{"AutoChannelSkipList", config_set_auto_channel_skip_list},
	{"ACSScanMode", config_set_acs_scan_mode},
	{"ACSScanDwell", config_set_acs_scan_dwell},
	{"ACSRestoreDwell", config_set_acs_restore_dwell},
	{"ACSPartialScanChNum", config_set_acs_partial_scan_ch_num},
	{"ACSChUtilThr", config_set_acs_ch_util_thr},
	{"ACSIceChUtilThr", config_set_acs_ch_ice_ch_util_thr},
	{"ACSMaxACSTimes", config_set_acs_max_acs_times},
	{"ACSStaNumThr", config_set_acs_sta_num_thr},
	{"ACSSwChThr", config_set_sw_ch_thr},
#ifdef CONFIG_6G_SUPPORT
	{"PSC_ACS", config_set_psc_acs},
#ifdef CONFIG_6G_AFC_SUPPORT
	{"AfcDeviceType", config_set_afc_device_type},
	{"AfcSpBwDup", config_set_afc_sp_bw_dup},
	{"AfcFreqrange", config_set_afc_freq_range_param},
	{"ACSAfterAFC", config_set_acs_after_afc},
	{"AfcSpectrumType", config_set_afc_spectrum_type},
	{"AfcOpClass131", config_set_afc_opclass_131_param},
	{"AfcOpClass132", config_set_afc_opclass_132_param},
	{"AfcOpClass133", config_set_afc_opclass_133_param},
	{"AfcOpClass134", config_set_afc_opclass_134_param},
	{"AfcOpClass135", config_set_afc_opclass_135_param},
	{"AfcOpClass136", config_set_afc_opclass_136_param},
	{"AfcOpClass137", config_set_afc_opclass_137_param},
#endif /* CONFIG_6G_AFC_SUPPORT */
#endif /* CONFIG_6G_SUPPORT */
	{"SeamlessCSA", config_set_seamless_csa},
	{"BasicRate", config_set_basic_rate},
#ifdef GN_MIXMODE_SUPPORT
	{"GNMixMode", config_set_gn_mix_mode},
#endif /* GN_MIXMODE_SUPPORT */
	{"BeaconPeriod", config_set_beacon_period},
#ifdef BAND_STEERING
	{"BandSteering", config_set_band_steering},
#endif /* BAND_STEERING */
	{"TxPower", config_set_tx_power},
#ifdef SINGLE_SKU_V2
	{"SKUenable", config_set_sku_enable},
	{"BFBACKOFFenable", config_set_bf_backoff_enable},
	{"PsuSkuEnable", config_set_pus_sku_enable},
	{"PsuSkuTableIdx", config_set_psu_sku_table_idx},
#endif /* SINGLE_SKU_V2 */
	{"SkuTableIdx", config_set_sku_table_idx},
	{"PERCENTAGEenable", config_set_percentag_enable},
	{"BGProtection", config_set_bg_protection},
	{"DisableOLBC", config_set_disable_olbc},
	{"TxPreamble", config_set_tx_preamble},
#ifdef VLAN_SUPPORT
	{"VLANEn", config_set_vlan_enable},
#endif /* VLAN_SUPPORT */
	{"TxBurst", config_set_tx_burst},
	{"NoForwardingBTNBSSID", config_set_no_forwarding_bt_nbssid},
#ifdef BACKGROUND_SCAN_SUPPORT
	{"DfsZeroWait", config_set_dfs_zero_wait},
#ifdef MT_DFS_SUPPORT
	{"DfsDedicatedZeroWait", config_set_dfs_dedicated_zero_wait},
	{"DfsZeroWaitDefault", config_set_dfs_zero_wait_default},
	{"DfsZeroWaitBandidx", config_set_dfs_zero_wait_band_idx},
	{"DfsTargetCh", config_set_dfs_target_ch},
	{"DfsChSelPrefer", config_set_dfs_ch_sel_prefer},
	{"DfsNopExpireSetChPolicy", config_set_dfs_nop_expire_set_ch_policy},
	{"DfsDedicatedRxPreselectCh", config_set_dfs_dedicated_rx_preselect_ch},
#endif /* MT_DFS_SUPPORT */
#endif /* BACKGROUND_SCAN_SUPPORT */
	{"EDCCAEnable", config_set_edcca_enable},
	{"ACSCheckTime", config_set_acs_check_time},
	{"ACSCheckMinTime", config_set_acs_check_min_time},
	{"ShortSlot", config_set_short_slot},
	{"RDRegion", config_set_rd_region},
#ifdef QOS_R1
	{"QoSEnable", config_set_qos_enable},
#endif /* QOS_R1 */
	{"SE_OFF", config_set_se_off},
	{"BcnDataRate", config_set_bcn_data_rate},
#ifdef DOT11_HE_AX
	{"BSSColorValue", config_set_bss_color_value},
#endif /* DOT11_HE_AX */
	{"Wifi6GCap", config_set_wifi6g_cap},
	{"LPIEnable", config_set_lpi_enable},
#ifdef MLR_SUPPORT
	{"MLREnable", config_set_mlr_enable},
#endif /* MLR_SUPPORT */
#ifdef WIFI_MD_COEX_SUPPORT
	{"IdcState", config_set_idc_state},
#endif /* WIFI_MD_COEX_SUPPORT */
	{"Single_RNR", config_set_single_rnr},
	{"Channel_priority", config_set_channel_priority},
	{"Forbid_BW40", config_set_forbid_bw40},
	{NULL,}
};

struct config_fwdl_action CONFIG_FWDL_ACTION[] = {
	{FWDL_ACTION_DOWNLOAD, "download"},
	{FWDL_ACTION_REDOWNLOAD, "re-download"},
	{FWDL_ACTION_REMOVE, "remove"},
	{FWDL_ACTION_RFCAL, "rf-cal"},
	{FWDL_ACTION_OPT_OFF, "opt_off"},
	{FWDL_ACTION_OPT_SKIP_STOP, "opt_skip_stop"},
	{0, NULL}
};

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

int cfg80211_drv_vndr_set_config(
	struct _RTMP_ADAPTER *mac_ad,
	struct wifi_dev *wdev,
	void *ctrl_data_)
{
	char *cmd_string;
	int cmd_len;
	struct mt_cfg80211_vndr_ctrl *ctrl_data = (struct mt_cfg80211_vndr_ctrl *)ctrl_data_;

#ifdef NEW_FWDL_SUPPORT
	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_CONFIG_FWDL_ACTION) {
		struct config_fwdl_action *action;

		cmd_string = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (!cmd_string || !cmd_len)
			return -RTMP_IO_EINVAL;
		if (strlen(cmd_string) > cmd_len)
			return -RTMP_IO_EINVAL;

		for (action = CONFIG_FWDL_ACTION; action->name; action++) {
			if (rtstrcasecmp(cmd_string, action->name) == TRUE) {
				MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
					"nl80211::(mwctl fwdl) action = %d\n",
					action->action_val);
				hc_fwdl_action(mac_ad, action->action_val);
				break;
			}
		}
		if (action->name == NULL) {
			/*Not found argument */
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"nl80211::(mwctl fwdl) Command not Support [%s]\n",
				cmd_string);
			return -RTMP_IO_EINVAL;
		}
		return 0;
	}
#endif /* NEW_FWDL_SUPPORT */

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_CONFIG_MAC_ADDR) {
		UINT8 input_len = ctrl_data->data_len;
		unsigned char *input_data = ctrl_data->data;
		struct physical_device *ph_dev = mac_ad->physical_dev;

		if (!input_data	|| (input_len != (1 + MAC_ADDR_LEN)))
			return -RTMP_IO_EINVAL;

		switch (input_data[0]) {
		case BAND0:
			mac_ad = physical_device_get_mac_adapter_by_band(ph_dev, BAND0);
			PD_SET_ADMIN_ADDRESS(ph_dev, &input_data[1]);
			break;
		case BAND1:
			mac_ad = physical_device_get_mac_adapter_by_band(ph_dev, BAND1);
			PD_SET_ADMIN_ADDRESS1(ph_dev, &input_data[1]);
			break;
		case BAND2:
			mac_ad = physical_device_get_mac_adapter_by_band(ph_dev, BAND2);
			PD_SET_ADMIN_ADDRESS2(ph_dev, &input_data[1]);
			break;
		}
		if (mac_ad) {
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_NOTICE,
				"[Band%d] %02X:%02X:%02X:%02X:%02X:%02X\n",
				input_data[0],
				input_data[1], input_data[2], input_data[3],
				input_data[4], input_data[5], input_data[6]);
			COPY_MAC_ADDR(mac_ad->CurrentAddress, &input_data[1]);
			COPY_MAC_ADDR(mac_ad->PermanentAddress, mac_ad->CurrentAddress);
			mac_ad->bLocalAdminMAC = TRUE;
			RtmpOSNetDevAddrSet(
				mac_ad->OpMode,
				mac_ad->net_dev,
				mac_ad->CurrentAddress,
				NULL);
		}
		return 0;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_CONFIG_SET_DEVICE_CMD_STR) {
		char *value, *item_name;
		struct config_set_device_proc *set_proc;

		cmd_string = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (!cmd_string || !cmd_len)
			return -RTMP_IO_EINVAL;
		if (strlen(cmd_string) > cmd_len)
			return -RTMP_IO_EINVAL;

		item_name = cmd_string;

		value = strchr(item_name, '=');
		if (value != NULL)
			*value++ = 0;

		for (set_proc = CONFIG_SET_DEVICE_PROC; set_proc->name; set_proc++) {
			if (rtstrcasecmp(item_name, set_proc->name) == TRUE) {
				if (set_proc->config_set_proc(mac_ad->physical_dev, value))
					return -RTMP_IO_EINVAL;

				break;
			}
		}

		if (set_proc->name == NULL) {
			/*Not found argument */
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"nl80211::(mwctl) Command not Support [%s=%s]\n",
				item_name, value);
			return -RTMP_IO_EINVAL;
		}
		return 0;
	}

	if (ctrl_data->sub_id == MTK_NL80211_VENDOR_ATTR_CONFIG_SET_BAND_CMD_STR) {
		char *value, *item_name;
		struct config_set_band_proc *set_proc;

		cmd_string = ctrl_data->data;
		cmd_len = ctrl_data->data_len;
		if (!cmd_string || !cmd_len)
			return -RTMP_IO_EINVAL;
		if (strlen(cmd_string) > cmd_len)
			return -RTMP_IO_EINVAL;

		item_name = cmd_string;

		value = strchr(item_name, '=');
		if (value != NULL)
			*value++ = 0;

		for (set_proc = CONFIG_SET_BAND_PROC; set_proc->name; set_proc++) {
			if (rtstrcasecmp(item_name, set_proc->name) == TRUE) {
				if (set_proc->config_set_proc(mac_ad, value))
					return -RTMP_IO_EINVAL;

				break;
			}
		}

		if (set_proc->name == NULL) {
			/*Not found argument */
			MTWF_DBG(mac_ad, DBG_CAT_CFG, CATCFG_DBGLOG, DBG_LVL_NOTICE,
				"nl80211::(mwctl) Command not Support [%s=%s]\n",
				item_name, value);
			return -RTMP_IO_EINVAL;
		}
		return 0;
	}

	return -RTMP_IO_EOPNOTSUPP;
}
#endif /* RT_CFG80211_SUPPORT */


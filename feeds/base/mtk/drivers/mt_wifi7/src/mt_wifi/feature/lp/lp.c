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
 ***************************************************************************/

/****************************************************************************

    Abstract:

    Support AP Low Power function.

***************************************************************************/

#ifdef LP_SUPPORT

#include "rt_config.h"

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                P R I V A T E    F U N C T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void lp_init_nic(struct physical_device *ph_dev)
{
	struct low_power_option_ctrl *plp_ctrl = &ph_dev->lp_option_ctrl;
	struct tpt_option_ctrl *ptpt_ctrl = &ph_dev->tpt_option_ctrl;

	/*L1ss default disable */
	ph_dev->pci_l1ss_enable = 0;

	/* AP Low Power : WIFI Part LP function in F/W , default disable */
	memset(plp_ctrl->option_raw_arg, 0, 10);
	/* profile init */
	memset(plp_ctrl->profile_option_raw_arg, 0, 10);
	/* TPO Raw all off */
	strlcpy(plp_ctrl->option_raw_arg, "0:0", 4);
	/* profile init */
	strlcpy(plp_ctrl->profile_option_raw_arg, "0:0", 4);

	/* TPO (Transmit Power Optimization) global default disable */
	ptpt_ctrl->tpo_enable = 0;
	ptpt_ctrl->pb_tpo_band_mask = 0;
	ptpt_ctrl->lp_tpo_band_mask = 0;
	ptpt_ctrl->min_tx_tpo_band_mask = 0;
}

void lp_exit_nic(struct physical_device *ph_dev)
{
	/* N/A*/
}

void lp_cfg_nic(struct _RTMP_ADAPTER *ad)
{
	struct physical_device *ph_dev = (struct physical_device *)ad->physical_dev;
	RTMP_STRING *lp_option_arg = ph_dev->lp_option_ctrl.option_raw_arg;

#ifdef CONFIG_ATE
	/* No need to perform this routine when ATE is running. */
	if (ATE_ON(ad))
		return;
#endif /* CONFIG_ATE */

	if (strlen(lp_option_arg)) {
		set_lp_option(ad, lp_option_arg);
		/* clear parameter */
		memset(lp_option_arg, 0, strlen(lp_option_arg));
	}

	if (ph_dev->tpt_option_ctrl.tpo_enable)
		set_tpt_option(ad, "0:1");
	else
		set_tpt_option(ad, "0:0");

	if (ph_dev->pci_l1ss_enable)
		set_pci_l1ss(ad, "1");
	else
		set_pci_l1ss(ad, "0");
}

void lp_init_band(struct _RTMP_ADAPTER *ad)
{
	/* PST */
	ad->bPST = FALSE;

	/* TPO */
	ad->pb_tpo_enable = 0;
	ad->lp_tpo_enable = 0;
	ad->mintx_tpo_enable = 0;

	/* 1RPD */
	ad->lp_1rpd_ctl.mode = FORCE_OFF;
	ad->lp_1rpd_ctl.rssi_th = MAX_1RPD_RSSI_TH;
	ad->lp_1rpd_ctl.defer_time = MAX_1RPD_DEFER_TIME;
	ad->lp_1rpd_ctl.last_on_off = 0; /* off */
	ad->lp_1rpd_ctl.trig_on_cnt = 0;
	ad->lp_1rpd_ctl.trig_off_cnt = 0;
}

/* per band PST cfg */
void set_pst_cfg_band(RTMP_ADAPTER *ad)
{
	int reason = 0;
	int ret = 0;
	char buf[20] = {0};
	UCHAR band_idx = hc_get_hw_band_idx(ad);

	if (!IS_LP_CAP(ad, WF_PST_CAP)) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return;
	}

	if (band_idx >= CFG_WIFI_RAM_BAND_NUM) {
		reason = -1;
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_ERROR,
			"ERR band_idx=%u\n", band_idx);
		goto done;
	}

	if (ad->bPST) {
		/* ON */
		ret = snprintf(buf, sizeof(buf), "%d-0-1", band_idx);
		if (os_snprintf_error(sizeof(buf), ret)) {
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_ERROR,
				"snprintf error of ON\n");
			reason = -2;
			goto done;
		}
	} else {
		/* OFF */
		ret = snprintf(buf, sizeof(buf), "%d-0-0", band_idx);
		if (os_snprintf_error(sizeof(buf), ret)) {
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_ERROR,
				"snprintf error of OFF\n");
			reason = -3;
			goto done;
		}
	}

	if (UniCmdMuruParameterSet(ad, buf, UNI_CMD_MURU_SET_LOW_PWR) == FALSE)
		reason = -4;

done:
	MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO, "bn[%u] <--- reason=%d\n",
		band_idx, reason);
}

/* per band PB-TPO & LP-TPO cfg */
void set_tpo_cfg_band(RTMP_ADAPTER *ad)
{
	UCHAR band_idx = hc_get_hw_band_idx(ad);
	char option_raw_arg[20] = {0};

	int ret;

	if (!IS_LP_CAP(ad, WF_TPO_CAP)) {
		MTWF_PRINT("%s(): cap only %x\n", __func__, GET_LP_CAP(ad));
		return;
	}

	if (band_idx >= CFG_WIFI_RAM_BAND_NUM) {
		MTWF_PRINT("%s: ERR band_idx=%u\n", __func__, band_idx);
		return;
	}

	/* PB-TPO */
	if (ad->pb_tpo_enable)
		PB_TPO_SET_BAND_EN(ad, band_idx); /* ON */
	else
		PB_TPO_CLR_BAND_EN(ad, band_idx); /* OFF */

	ret = snprintf(option_raw_arg, sizeof(option_raw_arg), "%d:%x", PB_TPO, PB_TPO_GET_BAND_MASK(ad));
	if (os_snprintf_error(sizeof(option_raw_arg), ret)) {
		MTWF_PRINT("%s: final_name snprintf error!\n", __func__);
		return;
	}
	set_tpt_option(ad, option_raw_arg);

	/* LP-TPO */
	if (ad->lp_tpo_enable)
		LP_TPO_SET_BAND_EN(ad, band_idx); /* ON */
	else
		LP_TPO_CLR_BAND_EN(ad, band_idx); /* OFF */

	ret = snprintf(option_raw_arg, sizeof(option_raw_arg), "%d:%x", LP_TPO, LP_TPO_GET_BAND_MASK(ad));
	if (os_snprintf_error(sizeof(option_raw_arg), ret)) {
		MTWF_PRINT("%s: final_name snprintf error!\n", __func__);
		return;
	}
	set_tpt_option(ad, option_raw_arg);

	/* MinTx-TPO */
	if (ad->mintx_tpo_enable)
		MIN_TX_TPO_SET_BAND_EN(ad, band_idx); /* ON */
	else
		MIN_TX_TPO_CLR_BAND_EN(ad, band_idx); /* OFF */

	ret = snprintf(option_raw_arg, sizeof(option_raw_arg), "%d:%x", MinTx_TPO, MIN_TX_TPO_GET_BAND_MASK(ad));
	if (os_snprintf_error(sizeof(option_raw_arg), ret)) {
		MTWF_PRINT("%s: final_name snprintf error!\n", __func__);
		return;
	}
	set_tpt_option(ad, option_raw_arg);
}

/* per band 1RPD cfg */
void set_1rpd_cfg_band(struct _RTMP_ADAPTER *ad, u8 on)
{
	if (!IS_LP_CAP(ad, WF_1RPD_PHY)) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return;
	}

	if (on) { /* on */
		set_lp_option(ad, "1:4:1:0");
		SET_LP_1RPD_LAST_ON(ad, 1);
	} else { /* off */
		set_lp_option(ad, "1:4:0:0");
		SET_LP_1RPD_LAST_ON(ad, 0);
	}
}

void lp_cfg_band(struct _RTMP_ADAPTER *ad)
{
	set_pst_cfg_band(ad);
	set_tpo_cfg_band(ad);
#ifdef CONFIG_ATE
	/* No need to perform this routine when ATE is running. */
	if (ATE_ON(ad))
		return;
#endif /* CONFIG_ATE */
	set_1rpd_cfg_band(ad, ad->lp_1rpd_ctl.mode);
}

int set_lp_option(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	u8 value[4] = {0};
	RTMP_STRING *token;
	int i = 0;
	char option_raw_arg[20] = {0};
	int ret;

	MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
		"%s\n", arg);

	if (!IS_LP_CAP(ad, (WF_MSIC_CAP | WF_1RPD_PHY))) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return FALSE;
	}

	if (strlen(arg)) {
		ret = snprintf(option_raw_arg, sizeof(option_raw_arg), "%s", arg);
		if (os_snprintf_error(sizeof(option_raw_arg), ret)) {
			MTWF_PRINT("%s: final_name snprintf error!\n", __func__);
			return FALSE;
		}
	} else
		return FALSE;

	token = rstrtok(option_raw_arg, ":");

	while (token != NULL && i < 4) {
		value[i] = (UINT8)os_str_tol(token, 0, 10);
		token = rstrtok(NULL, ":");
		i++;
	}

	if (i >= 2 && i <= 4) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"i=%d:Set LPOption as(%u:%u:%u:%u)\n",
			i, value[0], value[1], value[2], value[3]);

			ad->physical_dev->lp_option_ctrl.select = value[0];
			ad->physical_dev->lp_option_ctrl.value1 = value[1];
			ad->physical_dev->lp_option_ctrl.value2 = value[2];
			ad->physical_dev->lp_option_ctrl.value3 = value[3];

			UniCmdLPOption(ad, value[0], value[1], value[2], value[3]);
			return TRUE;
	} else {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"Please follow iwpriv ra0 set LPOption=seclect:vlaue1[:value2][:value3]\n");
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect:0~2,value1:0~1,value2:0~1,value3:feature defined parameters\n");

		/* For All SubFeatures ON /OFF */
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect=2 for ALL FEATURE OFF: iwpriv ra0 set LPOption=2:0\n");
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect=2 for ALL FEATURE ON: iwpriv ra0 set LPOption=2:1\n");

		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect=1 for Per Feature: iwpriv ra0 set LPOption=1:vlaue1:value2:value3\n");

		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect=0 for Engine: iwpriv ra0 set LPOption=0:0\n");
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
			"seclect=0 for Engine: iwpriv ra0 set LPOption=0:1\n");

		return FALSE;
	}
}


/* for debug cmd usage */
int set_1rppd_option(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	RTMP_STRING *token;
	char sep = ':';
	int i = 0;
	UINT16 wtbl_max_num = WTBL_MAX_NUM(ad);
	UINT16 idx;

	MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_NOTICE,
		"%s\n", arg);

	if (!IS_LP_CAP(ad, (WF_MSIC_CAP | WF_1RPD_PHY))) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return FALSE;
	}

	token = rstrtok(arg, &sep);

	while (token != NULL && i <= 4) {
		if (i == 0) {
			ad->lp_1rpd_ctl.mode = (int)os_str_tol(token, 0, 10);
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_NOTICE,
				"Lp1RPD_mode: %x\n", ad->lp_1rpd_ctl.mode);
			ad->lp_1rpd_ctl.trig_on_cnt = 0; /* reset debug cnts */
			ad->lp_1rpd_ctl.trig_off_cnt = 0; /* reset debug cnts */
		}

		if (i == 1) {
			ad->lp_1rpd_ctl.rssi_th = (int)os_str_tol(token, 0, 10);
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_NOTICE,
				"\tLp1RPD_RSSI_TH : %d\n", ad->lp_1rpd_ctl.rssi_th);
		}

		if (i == 2) {
			ad->lp_1rpd_ctl.defer_time = (u16)os_str_tol(token, 0, 10);
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_NOTICE,
				"\tLp1RPD_DeferTime : %u\n",  ad->lp_1rpd_ctl.defer_time);
		}

		if (i == 3) {
			ad->lp_1rpd_ctl.last_on_off = (u8)os_str_tol(token, 0, 10);
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_1RPD, DBG_LVL_NOTICE,
				"\tLp1RPD LAST_ON_OFF: %x\n",  ad->lp_1rpd_ctl.last_on_off);
		}

		token = rstrtok(NULL, &sep);
		i++;
	}

	if (IS_LP_1RPD_FORCE_ON(ad) || IS_LP_1RPD_FORCE_OFF(ad)) {
		/* force to on or off , be care this only quick debug w/o profile setting */
		RTMP_SET_UPDATE_1RPD(ad, ad->lp_1rpd_ctl.mode);

		/* Reset per Band All Peer's CNT on Adjust mode */
		for (idx = 0; idx < wtbl_max_num; idx++) {
			PMAC_TABLE_ENTRY pEntry = entry_get(ad, idx);
			/* STA or APCLI Link */
			if (pEntry && (pEntry->pAd == ad) && (IS_ENTRY_CLIENT(pEntry) || IS_ENTRY_PEER_AP(pEntry))
				&& (pEntry->Sst == SST_ASSOC)) {
				pEntry->rssi_1rpd_hit_cnt = 0;
				pEntry->rssi_1rpd_triggered = 0;
			}
		}
	} else if (IS_LP_1RPD_ADJUST_MODE(ad) && (i == 4))
		RTMP_SET_UPDATE_1RPD(ad, ad->lp_1rpd_ctl.last_on_off); /* let MacTableMaintenance() reset */

	MTWF_PRINT("i = %x\n", i);

	return TRUE;
}

int set_pci_l1ss(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	u8 value;
	MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
		"%s\n", arg);

	if (!IS_LP_CAP(ad, WF_L1SS_CAP)) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return FALSE;
	}

	value = (u8) os_str_tol(arg, 0, 10);
	if (value) {
		ad->physical_dev->pci_l1ss_enable = 1;
		RTMP_CHIP_PCI_L1ss(ad, TRUE);
	} else {
		ad->physical_dev->pci_l1ss_enable = 0;
		RTMP_CHIP_PCI_L1ss(ad, FALSE);
	}

	MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_NOTICE,
		"%s():pci_l1ss_enable=%u\n", __func__, ad->physical_dev->pci_l1ss_enable);

	return TRUE;
}

int set_tpt_option(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	u8 band_idx;
	u8 option_type, value;
	RTMP_STRING *token;
	char sep = ':';
	char option_raw_arg[20] = {0};
	int ret;
	struct _RTMP_ADAPTER *mac_ad = NULL;

	if (!IS_LP_CAP(ad, WF_TPO_CAP)) {
		MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
			"cap only %x\n", GET_LP_CAP(ad));
		return FALSE;
	}

	if (strlen(arg)) {
		ret = snprintf(option_raw_arg, sizeof(option_raw_arg), "%s", arg);
		if (os_snprintf_error(sizeof(option_raw_arg), ret)) {
			MTWF_PRINT("%s: final_name snprintf error!\n", __func__);
			return FALSE;
		}
	} else
		return FALSE;

	token = strchr(option_raw_arg, sep);

	if (token != NULL) {
		*token = '\0';

		if (strlen(option_raw_arg) && strlen(token + 1)) {
			option_type = (UINT8)os_str_tol(option_raw_arg, 0, 10);
			value = (UINT8)os_str_tol(token + 1, 0, 10);

			/* Leep Last Options */
			ad->physical_dev->tpt_option_ctrl.option_type = option_type;
			ad->physical_dev->tpt_option_ctrl.value = value;
			MTWF_DBG(ad, DBG_CAT_AP, CATAP_APLP_CMM, DBG_LVL_INFO,
					"Set TptOption as(%d:%d)\n", option_type, value);

			/* OFF ==> 0:0 , ON ==> 0:1 */
			if (option_type == TPO_GLOBAL) { /* Global Enable */
				ad->physical_dev->tpt_option_ctrl.tpo_enable = value;
			}

			if (option_type == PB_TPO) { /* PB-TPO */
				ad->physical_dev->tpt_option_ctrl.pb_tpo_band_mask = value;

				for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
					mac_ad = physical_device_get_mac_adapter_by_band(
						ad->physical_dev, band_idx);

					if (mac_ad == NULL)
						continue;

					if (PB_TPO_GET_BAND_EN(ad, band_idx))
						mac_ad->pb_tpo_enable = 1; /* ON */
					else
						mac_ad->pb_tpo_enable = 0; /* OFF */
				}
			}

			if (option_type == LP_TPO) { /* LP-TPO */
				ad->physical_dev->tpt_option_ctrl.lp_tpo_band_mask = value;

				for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
					mac_ad = physical_device_get_mac_adapter_by_band(
						ad->physical_dev, band_idx);

					if (mac_ad == NULL)
						continue;

					if (LP_TPO_GET_BAND_EN(ad, band_idx))
						mac_ad->lp_tpo_enable = 1; /* ON */
					else
						mac_ad->lp_tpo_enable = 0; /* OFF */
				}
			}

			if (option_type == MinTx_TPO) { /* MinTx-TPO */
				ad->physical_dev->tpt_option_ctrl.min_tx_tpo_band_mask = value;

				for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
					mac_ad = physical_device_get_mac_adapter_by_band(
						ad->physical_dev, band_idx);

					if (mac_ad == NULL)
						continue;

					if (MIN_TX_TPO_GET_BAND_EN(ad, band_idx))
						mac_ad->mintx_tpo_enable = 1; /* ON */
					else
						mac_ad->mintx_tpo_enable = 0; /* OFF */
				}
			}

			UniCmdTptOption(ad, option_type, value);
			return TRUE;
		}
	}

	return FALSE;
}


int show_lp_info_nic(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	u8 adie_slide_pa = 0;
	/* AP Low Power Settings */
	/* Per NIC */
	/* L1ss */
	if (IS_LP_CAP(ad, WF_L1SS_CAP))
		MTWF_PRINT("L1ss: %x\n", ad->physical_dev->pci_l1ss_enable);

	if (IS_LP_CAP(ad, (WF_MSIC_CAP | WF_1RPD_PHY))) {
		/* LpOption of profile */
		MTWF_PRINT("Profile LPOption: (%s)\n",
		ad->physical_dev->lp_option_ctrl.profile_option_raw_arg);

		/* Last LpOption : */
		MTWF_PRINT("Last LPOption: (%x,%x,%x,%x)\n",
			ad->physical_dev->lp_option_ctrl.select,
			ad->physical_dev->lp_option_ctrl.value1,
			ad->physical_dev->lp_option_ctrl.value2,
			ad->physical_dev->lp_option_ctrl.value3);
	}

	if (IS_LP_CAP(ad, WF_TPO_CAP)) {
		/* TPO */
		MTWF_PRINT("TpoEn: %x\n", ad->physical_dev->tpt_option_ctrl.tpo_enable);
		MTWF_PRINT("Last TptOption: (%x,%x)\n",
			ad->physical_dev->tpt_option_ctrl.option_type,
			ad->physical_dev->tpt_option_ctrl.value);
		/* Pb/Lp TPO Per Band */
		MTWF_PRINT("\tPbTpo: %x\n", ad->pb_tpo_enable);
		MTWF_PRINT("\tLpTpo: %x\n", ad->lp_tpo_enable);
		MTWF_PRINT("\tMinTxTpo: %x\n", ad->mintx_tpo_enable);
	}

	if (IS_LP_CAP(ad, WF_PST_CAP)) /* PST Per Band */
		MTWF_PRINT("PST: %x\n", ad->bPST);

	if (IS_LP_CAP(ad, WF_1RPD_PHY)) {
		/* 1RPD Ctrl Setting */
		MTWF_PRINT("Lp1RPD_mode: %x\n",
			ad->lp_1rpd_ctl.mode);
		MTWF_PRINT("\tLp1RPD_RSSI_TH : %d\n",
			ad->lp_1rpd_ctl.rssi_th);
		MTWF_PRINT("\tLp1RPD_DeferTime : %u\n",
			ad->lp_1rpd_ctl.defer_time);
		MTWF_PRINT("\tLp1RPD LAST_ON_OFF: %s\n",
			ad->lp_1rpd_ctl.last_on_off ? "ON" : "OFF");
		MTWF_PRINT("\tLp1RPD TRIG_ON_CNT %x\n",
			ad->lp_1rpd_ctl.trig_on_cnt);
		MTWF_PRINT("\tLp1RPD TRIG_OFF_CNT: %x\n",
			ad->lp_1rpd_ctl.trig_off_cnt);
	}

	/* A-Die Slice PA of E2P Setting */
	RTMP_CHIP_ASIC_ADIE_SLICE_PA_GET(ad, ad->band_idx, &adie_slide_pa);
	MTWF_PRINT("A-Die Slice PA: band[%u] : %u\n", ad->band_idx, adie_slide_pa);

	return TRUE;
}

int show_1rpd_info_band(RTMP_ADAPTER *ad, RTMP_STRING *arg)
{
	UINT16 wtbl_max_num = WTBL_MAX_NUM(ad);
	UINT16 blue_zone_cnt = 0;
	UINT16 red_zone_cnt = 0;
	UINT16 bn_peer_cnt = 0;
	CHAR avgRssi0 = 0;
	UINT16 idx;

	if (!IS_LP_CAP(ad, WF_1RPD_PHY))
		return TRUE;

	/* 1RPD Ctrl Setting */
	MTWF_PRINT("Lp1RPD_mode: %x\n", ad->lp_1rpd_ctl.mode);
	MTWF_PRINT("\tLp1RPD_RSSI_TH : %d\n", ad->lp_1rpd_ctl.rssi_th);
	MTWF_PRINT("\tLp1RPD_DeferTime : %u\n",  ad->lp_1rpd_ctl.defer_time);
	MTWF_PRINT("\tLp1RPD LAST_ON_OFF: %s\n",  ad->lp_1rpd_ctl.last_on_off ? "ON" : "OFF");
	MTWF_PRINT("\tLp1RPD TRIG_ON_CNT %x\n",  ad->lp_1rpd_ctl.trig_on_cnt);
	MTWF_PRINT("\tLp1RPD TRIG_OFF_CNT: %x\n",  ad->lp_1rpd_ctl.trig_off_cnt);

	MTWF_PRINT("Infomation: Peer\n");
	MTWF_PRINT("BN\tBSS\t\t\tAID\tWCID\tAVG_RSSI0\tHIT_CNT\tTRIG");
	MTWF_PRINT("\n");

	for (idx = 0; idx < wtbl_max_num; idx++) {
		PMAC_TABLE_ENTRY pEntry = entry_get(ad, idx);
		/* STA or APCLI Link */
		if (pEntry && (pEntry->pAd == ad) && (IS_ENTRY_CLIENT(pEntry) || IS_ENTRY_PEER_AP(pEntry))
			&& (pEntry->Sst == SST_ASSOC)) {
			avgRssi0 = pEntry->RssiSample.AvgRssi[0];

			bn_peer_cnt++; /* only cnt per band's peer */

			if (avgRssi0 <= GET_LP_1RPD_RSSI_TH(ad))
				red_zone_cnt++;
			else
				blue_zone_cnt++;

			MTWF_PRINT("(%d)\t(%d)"MACSTR"\t%d\t%d\t%d\t\t%u\t%u",
			ad->band_idx,
			pEntry->func_tb_idx,
					 MAC2STR(pEntry->Addr),
					 pEntry->Aid,
					 pEntry->wcid,
					 avgRssi0,
					 pEntry->rssi_1rpd_hit_cnt,
					 pEntry->rssi_1rpd_triggered);

			MTWF_PRINT("\n");
		}
	}

	MTWF_PRINT("\n\nBN=%d, bn_peer_cnt=%d, blue sta_cnt=%d, red sta_cnt=%d\n", ad->band_idx, bn_peer_cnt, blue_zone_cnt, red_zone_cnt);

	return TRUE;
}
#endif /* LP_SUPPORT */


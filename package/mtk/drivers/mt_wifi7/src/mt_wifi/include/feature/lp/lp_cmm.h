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
/*
 ***************************************************************************
 ***************************************************************************

    Module Name:
    lp_cmm.h

    Abstract:
    Support AP Low Power function.

*/
#ifndef __LP_CMM_H__
#define __LP_CMM_H__

#ifdef LP_SUPPORT

enum lp_select_mode {
	DBG_MODE = 0,
	PER_FUNC_MODE   = 1,
	GLOBAL_MODE   = 2,
};

enum lp_1rpd_mode {
	FORCE_OFF = 0,
	FORCE_ON    = 1,
	ADJUST_MODE   = 2,
};

enum WF_LP_CAPS {
	WF_1RPD_PHY = BIT(0),
	WF_MSIC_CAP = BIT(1),
	WF_L1SS_CAP = BIT(2),
	WF_TPO_CAP = BIT(3),
	WF_PST_CAP = BIT(4),
};

#define GET_LP_CAP(_ad)		(hc_get_chip_cap(_ad->hdev_ctrl)->wf_lp_caps)
#define IS_LP_CAP(_ad, _caps)		((hc_get_chip_cap(_ad->hdev_ctrl)->wf_lp_caps & (_caps)) != 0)

/* 1RPD */
#define IS_LP_1RPD_FORCE_ON(_ad) \
	(IS_LP_CAP(_ad, WF_1RPD_PHY) && (_ad->lp_1rpd_ctl.mode == FORCE_ON))
#define IS_LP_1RPD_FORCE_OFF(_ad) \
	(IS_LP_CAP(_ad, WF_1RPD_PHY) && (_ad->lp_1rpd_ctl.mode == FORCE_OFF))
#define IS_LP_1RPD_ADJUST_MODE(_ad) \
	(IS_LP_CAP(_ad, WF_1RPD_PHY) && (_ad->lp_1rpd_ctl.mode == ADJUST_MODE))

#define GET_LP_1RPD_RSSI_TH(_ad)	(_ad->lp_1rpd_ctl.rssi_th)
#define GET_LP_1RPD_DEFER_CNT(_ad)	(_ad->lp_1rpd_ctl.defer_time)
#define GET_LP_1RPD_LAST_ON(_ad)	(_ad->lp_1rpd_ctl.last_on_off)

#define SET_LP_1RPD_RSSI_TH(_ad, _var)	(_ad->lp_1rpd_ctl.rssi_th = _var)
#define SET_LP_1RPD_DEFER_CNT(_ad, _var)	(_ad->lp_1rpd_ctl.defer_time = _var)
#define SET_LP_1RPD_LAST_ON(_ad, _var)	(_ad->lp_1rpd_ctl.last_on_off = _var)


#define MAX_1RPD_DEFER_TIME (3600)
#define DEFAULT_1RPD_DEFER_TIME (5)
#define MIN_1RPD_DEFER_TIME (1)

#define MAX_1RPD_RSSI_TH (0)
#define MIN_1RPD_RSSI_TH (-100)

enum TPO_MODE {
	TPO_GLOBAL = 0,
	PB_TPO = 1,
	LP_TPO = 2,
	MinTx_TPO = 3,
};

/* PB-TPO */
#define PB_TPO_GET_BAND_MASK(_ad) \
	((_ad->physical_dev)->tpt_option_ctrl.pb_tpo_band_mask)

#define PB_TPO_SET_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.pb_tpo_band_mask |= (BIT(_bn_idx)))

#define PB_TPO_CLR_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.pb_tpo_band_mask &= ~(BIT(_bn_idx)))

#define PB_TPO_GET_BAND_EN(_ad, _bn_idx) \
	(((_ad->physical_dev)->tpt_option_ctrl.pb_tpo_band_mask & (BIT(_bn_idx))) >> _bn_idx)


/* LP-TPO */
#define LP_TPO_GET_BAND_MASK(_ad) \
	((_ad->physical_dev)->tpt_option_ctrl.lp_tpo_band_mask)

#define LP_TPO_SET_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.lp_tpo_band_mask |= (BIT(_bn_idx)))

#define LP_TPO_CLR_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.lp_tpo_band_mask &= ~(BIT(_bn_idx)))

#define LP_TPO_GET_BAND_EN(_ad, _bn_idx) \
	(((_ad->physical_dev)->tpt_option_ctrl.lp_tpo_band_mask & (BIT(_bn_idx))) >> _bn_idx)

/* MinTx-TPO */
#define MIN_TX_TPO_GET_BAND_MASK(_ad) \
	((_ad->physical_dev)->tpt_option_ctrl.min_tx_tpo_band_mask)

#define MIN_TX_TPO_SET_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.min_tx_tpo_band_mask |= (BIT(_bn_idx)))

#define MIN_TX_TPO_CLR_BAND_EN(_ad, _bn_idx) \
	((_ad->physical_dev)->tpt_option_ctrl.min_tx_tpo_band_mask &= ~(BIT(_bn_idx)))

#define MIN_TX_TPO_GET_BAND_EN(_ad, _bn_idx) \
	(((_ad->physical_dev)->tpt_option_ctrl.min_tx_tpo_band_mask & (BIT(_bn_idx))) >> _bn_idx)


/* AP Low Power : WIFI NIC Part LP MISC function in F/W */
struct low_power_option_ctrl {
	/* Last raw Option */
	u8 select;
	u8 value1;
	u8 value2;
	u8 value3;
	char option_raw_arg[10];

	/* profile or init setting*/
	char profile_option_raw_arg[10];
};

struct lp_1rpd_ctrl {
	enum lp_1rpd_mode mode;
	int rssi_th; /* threshold of dynamic off 1RPD ex: RSSI :  MAX_1RPD_RSSI_TH ~ -MIN_1RPD_RSSI_TH */
	u16 defer_time; /* unit in seconds */

	u8 last_on_off; /* last on/off flag of cmd or adjust mode */
	u32 trig_on_cnt; /* debug cnt */
	u32 trig_off_cnt; /* debug cnt */
};

/* AP Low Power : TPO (Transmit Power Optimization in RateAlgo) */
struct tpt_option_ctrl {
	u8 tpo_enable; /* Global Enable */
	/* Collect per band enable mask */
	u8 pb_tpo_band_mask;
	u8 lp_tpo_band_mask;
	u8 min_tx_tpo_band_mask;
	/* last cmd raw input */
	u8 option_type;
	u8 value;
};

#endif /* LP_SUPPORT */

#endif /* __LP_CMM_H__ */


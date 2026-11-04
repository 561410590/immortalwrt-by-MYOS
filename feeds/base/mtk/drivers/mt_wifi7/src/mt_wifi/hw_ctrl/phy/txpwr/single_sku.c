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
	cmm_single_sku.c
*/

/*******************************************************************************
 *    INCLUDED COMMON FILES
 ******************************************************************************/

#include "rt_config.h"

/*******************************************************************************
 *    INCLUDED EXTERNAL FILES
 ******************************************************************************/


/*******************************************************************************
 *    INCLUDED INTERNAL FILES
 ******************************************************************************/
#include	"txpwr/PowerLimit_sku.h"

/*******************************************************************************
 *   PRIVATE DEFINITIONS
 ******************************************************************************/


/*******************************************************************************
 *    PRIVATE TYPES
 ******************************************************************************/


/*******************************************************************************
 *    PRIVATE FUNCTION PROTOTYPES
 ******************************************************************************/


/*******************************************************************************
 *    PRIVATE DATA
 ******************************************************************************/

struct pwr_table_list gMergedTbl[MAX_BAND_NUM], gLgOfdmDupTbl[MAX_BAND_NUM];


/*******************************************************************************
 *    PUBLIC DATA
 ******************************************************************************/
UINT8 powerLimitDataRowNum[SINGLE_SKU_TABLE_TYPE_NUM] = {
	SINGLE_SKU_TABLE_CCK_LENGTH, SINGLE_SKU_TABLE_OFDM_LENGTH, SINGLE_SKU_TABLE_HT_20_LENGTH,
	SINGLE_SKU_TABLE_HT_40_LENGTH, SINGLE_SKU_TABLE_VHT_LENGTH, SINGLE_SKU_TABLE_VHT_LENGTH, SINGLE_SKU_TABLE_VHT_LENGTH,
	SINGLE_SKU_TABLE_VHT_LENGTH, SINGLE_SKU_TABLE_HE_LENGTH, SINGLE_SKU_TABLE_HE_LENGTH, SINGLE_SKU_TABLE_HE_LENGTH,
	SINGLE_SKU_TABLE_HE_LENGTH,	SINGLE_SKU_TABLE_HE_LENGTH, SINGLE_SKU_TABLE_HE_LENGTH, SINGLE_SKU_TABLE_HE_LENGTH
	, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH,
	SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH,
	SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH,
	SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH, SINGLE_SKU_TABLE_EHT_LENGTH};
UINT8 backOffDataRowNum[BF_BACKOFF_TYPE_NUM] = {
	BF_BACKOFF_CCK_LENGTH, BF_BACKOFF_OFDM_OFF_LENGTH, BF_BACKOFF_OFDM_ON_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH, BF_BACKOFF_LENGTH
	};
UINT8 lgOfdmDupDataRowNum[LG_OFDM_DUP_TYPE_NUM] = {
	LG_OFDM_DUP_BW40_LENGTH, LG_OFDM_DUP_BW80_LENGTH, LG_OFDM_DUP_BW160_LENGTH, LG_OFDM_DUP_BW320_LENGTH
	};

static const CHAR SkuParseTypeName[BF_BACKOFF_TYPE_NUM][20] = {
		"CCK", "OFDM", "HT20", "HT40", "VHT20", "VHT40", "VHT80", "VHT160", "HERU26", "HERU52", "HERU106", "HERU242", "HERU484"
		, "HERU996", "HERU996X2", "EHTRU26", "EHTRU52", "EHTRU106", "EHTRU242", "EHTRU484", "EHTRU996", "EHTRU996X2", "EHTRU996X4"
		, "EHTRU26_52", "EHTRU26_106", "EHTRU484_242", "EHTRU996_484", "EHTRU996_484_242", "EHTRU996X2_484", "EHTRU996X3", "EHTRU996X3_484"};
static const CHAR BackoffParseTypeName[BF_BACKOFF_TYPE_NUM][15] = {
		"BFOFF_CCK", "BF_OFF_OFDM", "BF_ON_OFDM", "BF_OFF_RU26", "BF_ON_RU26", "BF_OFF_RU52", "BF_ON_RU52", "BF_OFF_RU78", "BF_ON_RU78"
		, "BF_OFF_RU106", "BF_ON_RU106", "BF_OFF_RU132", "BF_ON_RU132", "BF_OFF_VHT20", "BF_ON_VHT20", "BF_OFF_VHT40", "BF_ON_VHT40"
		, "BF_OFF_RU726", "BF_ON_RU726", "BF_OFF_VHT80", "BF_ON_VHT80", "BF_OFF_RU1480", "BF_ON_RU1480", "BF_OFF_RU1722", "BF_ON_RU1722"
		, "BF_OFF_VHT160", "BF_ON_VHT160", "BF_OFF_RU2476", "BF_ON_RU2476", "BF_OFF_RU2988", "BF_ON_RU2988", "BF_OFF_RU3472", "BF_ON_RU3472"
		, "BF_OFF_EHT320", "BF_ON_EHT320"};
static const CHAR LgOfdmDupParseTypeName[LG_OFDM_DUP_TYPE_NUM][15] = {
		"DUP_BW40", "DUP_BW80", "DUP_BW160", "DUP_BW320"};


/*******************************************************************************
 *    EXTERNAL DATA
 ******************************************************************************/

extern RTMP_STRING *__rstrtok;

/*******************************************************************************
 *    EXTERNAL FUNCTION PROTOTYPES
 ******************************************************************************/


/*******************************************************************************
 *    PRIVATE FUNCTIONS
 ******************************************************************************/

NDIS_STATUS MtPwrLimitLoadParamHandle(RTMP_ADAPTER *pAd, UINT8 u1Type)
{
	RTMP_CHIP_CAP *cap = hc_get_chip_cap(pAd->hdev_ctrl);

	if (u1Type == POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP) {
		if ((cap->power_ctrl_cap & fPWR_CTRL_CAP_LG_OFDM_DUP) == 0) {
			MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_WARN,
				"Chip cap dont support LG_OFDM_DUP\n");
				return NDIS_STATUS_SUCCESS;
		}
	}

	/* enable flag for Read Power limit table pass */
		pAd->fgPwrLimitRead[u1Type] = TRUE;

	/* print out power limit table info */
	if (MtShowPwrLimitTable(pAd, u1Type, 0, DBG_LVL_DEBUG) != NDIS_STATUS_SUCCESS)
		goto error2;

	return NDIS_STATUS_SUCCESS;


error2:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR, "Show Power Table Error!!\n");
	return NDIS_STATUS_FAILURE;

}

NDIS_STATUS MtPwrLimitUnloadParamHandle(RTMP_ADAPTER *pAd, UINT8 u1Type)
{
	/* free allocated memory for power limit table */
	if (pAd->fgPwrLimitRead[u1Type]) {
		/* disable flag for Read Power limit table pass */
		pAd->fgPwrLimitRead[u1Type] = FALSE;
	}

	return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS MtShowPwrLimitTable(RTMP_ADAPTER *pAd, UINT8 u1Type, UINT16 channel, UINT8 u1DebugLevel)
{
	UINT16 u1ColIdx, u1ParamType;
	UINT32 u1ParamIdx;
	struct pwr_table_list *pTbl = NULL, *pPsuTbl = NULL;
	UINT8 sku_tbl_idx = 0, psu_sku_tbl_idx = 0;
	UINT32 rowNum = 0, i = 0;
	UINT8 u1TypeParseNum[POWER_LIMIT_TABLE_TYPE_NUM] = {SINGLE_SKU_TABLE_TYPE_NUM, BF_BACKOFF_TYPE_NUM, LG_OFDM_DUP_TYPE_NUM};
	UINT32 band_idx = hc_get_hw_band_idx(pAd);

	if (u1DebugLevel != 0)
		return NDIS_STATUS_SUCCESS;

	if (band_idx >= MAX_BAND_NUM) {
		MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
				"wrong band idx:%d\n", band_idx);
		goto error1;
	}

	/* query sku table index */
	chip_get_sku_tbl_idx(pAd, &sku_tbl_idx, &psu_sku_tbl_idx);
	if (sku_tbl_idx >= MAX_SKU_TABLE_NUM)
		goto error1;

#if defined(CONFIG_6G_SUPPORT) && defined(CONFIG_6G_AFC_SUPPORT) && defined(DOT11_HE_AX)
		if (pAd->afc_ctrl.AfcStateMachine.CurrState == AFC_RUN) {
			MTWF_PRINT("Device is standard power\n");

			sku_tbl_idx = AFC_STD_PWR_SKUTABLE_IDX;
		}
#endif /*CONFIG_6G_SUPPORT &&*/

	if (u1Type == POWER_LIMIT_TABLE_TYPE_SKU) {
		pTbl = pcptrSkuTbl[sku_tbl_idx];
		pPsuTbl = pcptrSkuTbl[psu_sku_tbl_idx];
		rowNum = ARRAY_SIZE(Sku_01);

		if (pAd->CommonCfg.PsuSkuEnable == 1 && pAd->CommonCfg.SKUenable == 0)
			pTbl = pcptrSkuTbl[psu_sku_tbl_idx];
	} else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF) {
		pTbl = pcptrBackoffTbl[sku_tbl_idx];
		rowNum = ARRAY_SIZE(Backoff_01);
	} else if (u1Type == POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP) {
		pTbl = &gLgOfdmDupTbl[band_idx];
		rowNum = 1;
	}

	MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
		"-----------------------------------------------------------------\n");
	MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
		"SKU table index: %d\n", sku_tbl_idx);

	for (i = 0; i < rowNum; i++) {
		if (channel != 0 && channel != pTbl[i].channel)
			continue;

		MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
			"\033[1;33mchannel: %d  Band: %d, SKUenable:%d, PsuSku:%d\033[0m\n", pTbl[i].channel,
				pTbl[i].chBand, pAd->CommonCfg.SKUenable, pAd->CommonCfg.PsuSkuEnable);

		/* check if different info contents for different channel (CCK, OFDM, VHT20/40/80/160, RU26/52/106/242/484/996/996X2) */
		for (u1ParamType = 0, u1ParamIdx = 0; u1ParamType < u1TypeParseNum[u1Type]; u1ParamType++) {

			if (u1Type == POWER_LIMIT_TABLE_TYPE_SKU) {
				MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU,
					u1DebugLevel, "%s: ", SkuParseTypeName[u1ParamType]);
				for (u1ColIdx = 0; u1ColIdx < powerLimitDataRowNum[u1ParamType]; u1ColIdx++, u1ParamIdx++) {
					if (pAd->CommonCfg.PsuSkuEnable && pAd->CommonCfg.SKUenable &&
						(pTbl[i].pwrLimit[u1ParamIdx] > pPsuTbl[i].pwrLimit[u1ParamIdx])) {
						MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU,
							u1DebugLevel, "\033[1;32m%d\033[0m ",
							min(pTbl[i].pwrLimit[u1ParamIdx], pPsuTbl[i].pwrLimit[u1ParamIdx]));
					} else
						MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU,
							u1DebugLevel, "%d ", pTbl[i].pwrLimit[u1ParamIdx]);
				}

			} else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF) {
				MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU,
					u1DebugLevel, "%s: ", BackoffParseTypeName[u1ParamType]);
				for (u1ColIdx = 0; u1ColIdx < backOffDataRowNum[u1ParamType]; u1ColIdx++, u1ParamIdx++)
					MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
						"%d ", pTbl[i].pwrLimit[u1ParamIdx]);
			} else if (u1Type == POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP) {
				MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU,
					u1DebugLevel, "%s: ", LgOfdmDupParseTypeName[u1ParamType]);
				for (u1ColIdx = 0; u1ColIdx < lgOfdmDupDataRowNum[u1ParamType]; u1ColIdx++, u1ParamIdx++)
					MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
						"%d ", pTbl[i].pwrLimit[u1ParamIdx]);
			}
			MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel, "\n");
		}
	}
	MTWF_DBG_NP(DBG_CAT_POWER, CATPOWER_SKU, u1DebugLevel,
		"-----------------------------------------------------------------\n");

	return NDIS_STATUS_SUCCESS;

error1:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR, "invalid sku table index !!\n");
	return NDIS_STATUS_FAILURE;
}

VOID MtPwrLimitTblChProc(RTMP_ADAPTER *pAd, UINT8 u1BandIdx, UINT8 u1ChannelBand, UINT8 u1ControlChannel, UINT8 u1CentralChannel)
{
	UINT8 u1Type;

	for (u1Type = POWER_LIMIT_TABLE_TYPE_SKU; u1Type < POWER_LIMIT_TABLE_TYPE_NUM; u1Type++) {
		if (pAd->fgPwrLimitRead[u1Type])
			MtCmdPwrLimitTblUpdate(pAd, u1BandIdx, u1Type, u1ChannelBand, u1ControlChannel, u1CentralChannel);
	}
}

struct pwr_table_list *MergeBothSingleSkuAndPusSkuTbl(struct _RTMP_ADAPTER *pAd,
	struct pwr_table_list *pTbl,
	struct pwr_table_list *pPsuTbl)
{
	UINT32 i = 0, offset = 0;
	UINT32 bwType[BW_TYPE_NUM] = {SKU_1_EHT484_IDX, SKU_1_EHT996_IDX,
					SKU_1_EHT996X2_IDX, SKU_1_EHT996X4_IDX};
	UINT32 band_idx = hc_get_hw_band_idx(pAd);

	if (band_idx >= MAX_BAND_NUM) {
		MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
					"wrong band idx:%d\n", band_idx);
		return NULL;
	}

	/* Regulation sku and PSU sku merged */
	gMergedTbl[band_idx].chBand = pTbl->chBand;
	gMergedTbl[band_idx].channel = pTbl->channel;

	os_fill_mem(gMergedTbl[band_idx].pwrLimit, BACKOFF_PARAM_NUM_V1, 126);

	if (pAd->CommonCfg.SKUenable && pAd->CommonCfg.PsuSkuEnable) {
		for (i = 0; i < SINGLE_SKU_PARAM_NUM_V1; i++) {
			gMergedTbl[band_idx].pwrLimit[i] =
				min(pTbl->pwrLimit[i], pPsuTbl->pwrLimit[i]);
		}
	} else if (pAd->CommonCfg.PsuSkuEnable) {
		memmove(&gMergedTbl[band_idx], pPsuTbl, sizeof(struct pwr_table_list));
	} else if (pAd->CommonCfg.SKUenable) {
		memmove(&gMergedTbl[band_idx], pTbl, sizeof(struct pwr_table_list));
	}

	/* LgOfdmDup */
	gLgOfdmDupTbl[band_idx].chBand = pTbl->chBand;
	gLgOfdmDupTbl[band_idx].channel = pTbl->channel;

	os_fill_mem(&gLgOfdmDupTbl[band_idx].pwrLimit[0], BACKOFF_PARAM_NUM_V1, 126);

	for (i = 0; i < BW_TYPE_NUM; i++) {
		gLgOfdmDupTbl[band_idx].pwrLimit[offset] = gMergedTbl[band_idx].pwrLimit[bwType[i]];
		offset += 1;

		memmove(&gLgOfdmDupTbl[band_idx].pwrLimit[offset],
				&gMergedTbl[band_idx].pwrLimit[bwType[i]], SINGLE_SKU_TABLE_OFDM_LENGTH - 1);

		offset += (SINGLE_SKU_TABLE_OFDM_LENGTH - 1);
	}

	return &gMergedTbl[band_idx];
}

NDIS_STATUS MtPwrFillLimitParam(RTMP_ADAPTER *pAd, UINT8 ChBand, UINT8 u1ControlChannel,
				UINT8 u1CentralChannel, VOID *pi1PwrLimitParam, UINT8 u1Type)
{
	UINT32 u1ChListIdx;
	UINT8 u1PwrLimitChannel;
	UINT32 rowNum = 0;
	UINT8 sku_tbl_idx = 0, psu_sku_tbl_idx = 0;
	struct pwr_table_list *pTbl = NULL, *pPsuTbl = NULL, *pMegerdTbl = NULL;
#if defined(CONFIG_6G_SUPPORT) && defined(CONFIG_6G_AFC_SUPPORT) && defined(DOT11_HE_AX)
	UINT16 u2ParamIdx = 0, u2TblTypeLengh = 0;
	INT8 i1PwrValue = 0;
	UINT8 u1AfcChannelIdx;
#endif /*CONFIG_6G_SUPPORT &&*/
	UINT8 PsdLimit = CheckPSDLimitType(pAd);
	UINT16 sku_1_update_idx, sku_2_bf_off_update_idx, sku_2_bf_on_update_idx;
	UINT8 mcs_idx, bo_idx;
	UINT32 band_idx = hc_get_hw_band_idx(pAd);

	/* sanity check for null pointer */
	if (!pi1PwrLimitParam)
		goto error0;

	if (band_idx >= MAX_BAND_NUM) {
		MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
				"wrong band idx:%d\n", band_idx);
		goto error1;
	}

	/* query sku table index */
	chip_get_sku_tbl_idx(pAd, &sku_tbl_idx, &psu_sku_tbl_idx);
	if (sku_tbl_idx >= MAX_SKU_TABLE_NUM)
		goto error1;

	if (psu_sku_tbl_idx >= MAX_SKU_TABLE_NUM)
		goto error1;


#if defined(CONFIG_6G_SUPPORT) && defined(CONFIG_6G_AFC_SUPPORT) && defined(DOT11_HE_AX)
		if (pAd->afc_ctrl.AfcStateMachine.CurrState == AFC_RUN) {
			MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
						"Device is standard power\n");

			sku_tbl_idx = AFC_STD_PWR_SKUTABLE_IDX;
		}
#endif /*CONFIG_6G_SUPPORT &&*/

	switch (u1Type) {
		case POWER_LIMIT_TABLE_TYPE_SKU:
			pTbl = pcptrSkuTbl[sku_tbl_idx];
			pPsuTbl = pcptrSkuTbl[psu_sku_tbl_idx];
			rowNum = ARRAY_SIZE(Sku_01);
			break;
		case POWER_LIMIT_TABLE_TYPE_BACKOFF:
			pTbl = pcptrBackoffTbl[sku_tbl_idx];
			rowNum = ARRAY_SIZE(Backoff_01);
			break;
		case POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP:
			pTbl = &gLgOfdmDupTbl[band_idx];
			rowNum = 1;
			break;
		default:
			rowNum = 0;
			MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_WARN,
				"unknown power limit type\n");
			break;
	}

	/* Check G-Band/A-Band and power limit channel */
	u1PwrLimitChannel = (ChBand) ? (u1ControlChannel) : (u1CentralChannel);

	/* search for specific channel */
	for (u1ChListIdx = 0; u1ChListIdx < rowNum; u1ChListIdx++) {
		/* check Channel Band and Channel */
		if ((ChBand == pTbl[u1ChListIdx].chBand) && (u1PwrLimitChannel == pTbl[u1ChListIdx].channel)) {

			if (u1Type == POWER_LIMIT_TABLE_TYPE_SKU) {
				/* MTWF_PRINT(
					"\033[1;33mSingle_SKU, Channel: %d Ch_Band: %d\033[0m\n",
					u1PwrLimitChannel, ChBand); */
				pMegerdTbl = MergeBothSingleSkuAndPusSkuTbl(pAd, &pTbl[u1ChListIdx], &pPsuTbl[u1ChListIdx]);

				memmove(pi1PwrLimitParam, pMegerdTbl->pwrLimit, SINGLE_SKU_PARAM_NUM_V1);
				/* MtShowFWSingleSkuTable(pAd, pMegerdTbl->pwrLimit,
					BACKOFF_PARAM_NUM_V1); */
			} else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF) {
				memmove(pi1PwrLimitParam, pcptrBackoffTbl[sku_tbl_idx][u1ChListIdx].pwrLimit,
					BACKOFF_PARAM_NUM_V1);
			} else if (u1Type == POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP) {
				/* MTWF_PRINT(
					"\033[1;33mLG_OFDM_DUP Channel: %d Ch_Band: %d\033[0m\n",
					u1PwrLimitChannel, ChBand); */
				memmove(pi1PwrLimitParam, pTbl->pwrLimit, LG_OFDM_DUP_PARAM_NUM_V1);
				/* EventTxPowerLegacyOfdmDupShowInfo(pAd,
					pTbl->pwrLimit - 4,
					LG_OFDM_DUP_PARAM_NUM_V1); */
			}

			/* stop channel list search loop */
			break;
		}
	}

#ifdef CONFIG_6G_AFC_SUPPORT
	if (ChBand == 2 && (pAd->afc_ctrl.AfcStateMachine.CurrState == AFC_RUN)){
		u1AfcChannelIdx = afc_get_channel_index(u1PwrLimitChannel);
		if (u1AfcChannelIdx == AFC_INVALID_CH_INDEX) {
			MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
			"AFC channel is not available !!\n");
			return NDIS_STATUS_FAILURE;
		}
		if (u1Type == POWER_LIMIT_TABLE_TYPE_SKU)
			u2TblTypeLengh = SINGLE_SKU_PARAM_NUM_V1;
		else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF)
			u2TblTypeLengh = BACKOFF_PARAM_NUM_V1;
		for (u2ParamIdx = 0; u2ParamIdx < u2TblTypeLengh; u2ParamIdx++) {
			i1PwrValue = afc_pwr_calculation(pAd, u2ParamIdx, u1AfcChannelIdx, *((INT8 *)pi1PwrLimitParam + u2ParamIdx), u1Type);
			*((INT8 *)pi1PwrLimitParam + u2ParamIdx) = i1PwrValue;
		}
	} else
#endif /*CONFIG_6G_SUPPORT &&*/
	{
	if (pAd->CommonCfg.SKU_DUP_Patch_enable) {
		/* Duplicate mode patch */
		/* Step 1. Get current operating BW. */
		/* if (6G && 6G BW320 center channel)
		   else if (6G && 6G BW160 center channel) || (5G && 5G BW160 center channel)
		   else if (6G && 6G BW80 center channel) || (5G && 5G BW80 center channel)
		   else if (u1ControlChannel != u1CentralChannel) => BW40
		   else => BW20
		*/
		if ((ChBand == 2) &&
			(u1CentralChannel == 31 || u1CentralChannel == 63
			|| u1CentralChannel == 95 || u1CentralChannel == 127
			|| u1CentralChannel == 159 || u1CentralChannel == 191)) {
			/* BW_320 */
			sku_1_update_idx = SKU_1_EHT996X4_IDX;
			sku_2_bf_off_update_idx = SKU_2_BW320_BF_OFF_IDX;
			sku_2_bf_on_update_idx = SKU_2_BW320_BF_ON_IDX;
		} else if (((ChBand == 2) &&
			(u1CentralChannel == 15 || u1CentralChannel == 47
			|| u1CentralChannel == 79 || u1CentralChannel == 111
			|| u1CentralChannel == 143 || u1CentralChannel == 175
			|| u1CentralChannel == 207))
			|| ((ChBand == 1) &&
			(u1CentralChannel == 50 || u1CentralChannel == 114 || u1CentralChannel == 163))) {
			/* BW_160 */
			sku_1_update_idx = SKU_1_EHT996X2_IDX;
			sku_2_bf_off_update_idx = SKU_2_BW160_BF_OFF_IDX;
			sku_2_bf_on_update_idx = SKU_2_BW160_BF_ON_IDX;
		} else if (((ChBand == 2) &&
			(u1CentralChannel == 7 || u1CentralChannel == 23 || u1CentralChannel == 39
			|| u1CentralChannel == 55 || u1CentralChannel == 71 || u1CentralChannel == 87
			|| u1CentralChannel == 103 || u1CentralChannel == 119 || u1CentralChannel == 135
			|| u1CentralChannel == 151 || u1CentralChannel == 167 || u1CentralChannel == 183
			|| u1CentralChannel == 199 || u1CentralChannel == 215))
			|| ((ChBand == 1) &&
			(u1CentralChannel == 42 || u1CentralChannel == 58 || u1CentralChannel == 106
			|| u1CentralChannel == 122 || u1CentralChannel == 138 || u1CentralChannel == 155
			|| u1CentralChannel == 171))) {
			/* BW_80 */
			sku_1_update_idx = SKU_1_EHT996_IDX;
			sku_2_bf_off_update_idx = SKU_2_BW80_BF_OFF_IDX;
			sku_2_bf_on_update_idx = SKU_2_BW80_BF_ON_IDX;
		} else if (u1ControlChannel != u1CentralChannel) {
			/* BW_40 */
			sku_1_update_idx = SKU_1_EHT484_IDX;
			sku_2_bf_off_update_idx = SKU_2_BW40_BF_OFF_IDX;
			sku_2_bf_on_update_idx = SKU_2_BW40_BF_ON_IDX;
		} else {
			/* BW_20 */
			sku_1_update_idx = SKU_1_EHT242_IDX;
			sku_2_bf_off_update_idx = SKU_2_BW20_BF_OFF_IDX;
			sku_2_bf_on_update_idx = SKU_2_BW20_BF_ON_IDX;
		}

		/* Duplicate mode patch */
		/* 2. Update SKU_1 & SKU_2 */
		/* Spectial Case Handle: if (6G && PDS_LIMIT) => do not update SKU_1 */
		if ((u1Type == POWER_LIMIT_TABLE_TYPE_SKU) && !((ChBand == 2) && (PsdLimit == 1))) {
			for (mcs_idx = 1; mcs_idx < SINGLE_SKU_TABLE_OFDM_LENGTH; mcs_idx++)
				*((INT8 *)pi1PwrLimitParam + SKU_1_OFDM_IDX + mcs_idx)
					= *((INT8 *)pi1PwrLimitParam + sku_1_update_idx + mcs_idx);
		} else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF) {
			for (bo_idx = 0; bo_idx < BF_BACKOFF_OFDM_OFF_LENGTH; bo_idx++)
				*((INT8 *)pi1PwrLimitParam + SKU_2_OFDM_BF_OFF_IDX + bo_idx)
					= *((INT8 *)pi1PwrLimitParam + sku_2_bf_off_update_idx + bo_idx);
			for (bo_idx = 0; bo_idx < BF_BACKOFF_OFDM_ON_LENGTH; bo_idx++)
				*((INT8 *)pi1PwrLimitParam + SKU_2_OFDM_BF_ON_IDX + bo_idx)
					= *((INT8 *)pi1PwrLimitParam + sku_2_bf_on_update_idx + 1 + bo_idx);
		}

		/* Duplicate mode patch */
		/* 3. Special Case Handle for (2G && BW40)  */
		/* Refill by Using Control channel's table value*/
		if ((ChBand == 0) && (u1ControlChannel != u1CentralChannel)) {
			u1PwrLimitChannel = u1ControlChannel;
			for (u1ChListIdx = 0; u1ChListIdx < rowNum; u1ChListIdx++) {
				/* check Channel Band and Channel */
				if ((ChBand == pTbl[u1ChListIdx].chBand)
					&& (u1PwrLimitChannel == pTbl[u1ChListIdx].channel)
					&& (u1Type == POWER_LIMIT_TABLE_TYPE_SKU)) {
					/* Update CCK, HT20, VHT20, HE242 and EHT242 in SKU_1.*/
					for (mcs_idx = 0; mcs_idx < SINGLE_SKU_TABLE_CCK_LENGTH; mcs_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_1_CCK_IDX + mcs_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_1_CCK_IDX + mcs_idx];
					for (mcs_idx = 0; mcs_idx < SINGLE_SKU_TABLE_HT_20_LENGTH; mcs_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_1_HT20_IDX + mcs_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_1_HT20_IDX + mcs_idx];
					for (mcs_idx = 0; mcs_idx < SINGLE_SKU_TABLE_VHT_LENGTH; mcs_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_1_VHT20_IDX + mcs_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_1_VHT20_IDX + mcs_idx];
					for (mcs_idx = 0; mcs_idx < SINGLE_SKU_TABLE_HE_LENGTH; mcs_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_1_HE242_IDX + mcs_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_1_HE242_IDX + mcs_idx];
					for (mcs_idx = 0; mcs_idx < SINGLE_SKU_TABLE_EHT_LENGTH; mcs_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_1_EHT242_IDX + mcs_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_1_EHT242_IDX + mcs_idx];
					/* stop channel list search loop */
					break;
				} else if ((ChBand == pTbl[u1ChListIdx].chBand)
					&& (u1PwrLimitChannel == pTbl[u1ChListIdx].channel)
					&& (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF)) {
					/* Update CCK and BW20 in SKU_2. */
					for (bo_idx = 0; bo_idx < BF_BACKOFF_CCK_LENGTH; bo_idx++)
						*((INT8 *)pi1PwrLimitParam + SKU_2_CCK_BF_OFF_IDX + bo_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_2_CCK_BF_OFF_IDX + bo_idx];
					for (bo_idx = 0; bo_idx < BF_BACKOFF_LENGTH; bo_idx++) {
						*((INT8 *)pi1PwrLimitParam + SKU_2_BW20_BF_OFF_IDX + bo_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_2_BW20_BF_OFF_IDX + bo_idx];
						*((INT8 *)pi1PwrLimitParam + SKU_2_BW20_BF_ON_IDX + bo_idx)
							= pTbl[u1ChListIdx].pwrLimit[SKU_2_BW20_BF_ON_IDX + bo_idx];
					}
					/* stop channel list search loop */
					break;
				}

			}
		}
	}
	}
	return NDIS_STATUS_SUCCESS;

error0:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
		"null pointer for buffer to fill power limit table !!\n");
	return NDIS_STATUS_FAILURE;
error1:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
		"invalid sku table index !!\n");
	return NDIS_STATUS_FAILURE;

}

#ifdef SINGLE_SKU_V2
NDIS_STATUS RTMPSetSkuParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret;

	ret = MtPwrLimitLoadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_SKU);
	return ret;
}

NDIS_STATUS RTMPSetBackOffParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret;

	ret = MtPwrLimitLoadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_BACKOFF);
	return ret;
}

NDIS_STATUS RTMPSetLgOfdmDupParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret;

	ret = MtPwrLimitLoadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP);
	return ret;
}

NDIS_STATUS RTMPResetSkuParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret = NDIS_STATUS_SUCCESS;

	ret = MtPwrLimitUnloadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_SKU);
	return ret;
}

NDIS_STATUS RTMPResetBackOffParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret = NDIS_STATUS_SUCCESS;

	ret = MtPwrLimitUnloadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_BACKOFF);
	return ret;
}

NDIS_STATUS RTMPResetLgOfdmDupParam(RTMP_ADAPTER *pAd)
{
	NDIS_STATUS ret = NDIS_STATUS_SUCCESS;

	ret = MtPwrLimitUnloadParamHandle(pAd, POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP);
	return ret;
}

#define	SKU_PHYMODE_CCK_1M_2M				0
#define	SKU_PHYMODE_CCK_5M_11M				1
#define	SKU_PHYMODE_OFDM_6M_9M				2
#define	SKU_PHYMODE_OFDM_12M_18M			3
#define	SKU_PHYMODE_OFDM_24M_36M			4
#define	SKU_PHYMODE_OFDM_48M_54M			5
#define	SKU_PHYMODE_HT_MCS0_MCS1			6
#define	SKU_PHYMODE_HT_MCS2_MCS3			7
#define	SKU_PHYMODE_HT_MCS4_MCS5			8
#define	SKU_PHYMODE_HT_MCS6_MCS7			9
#define	SKU_PHYMODE_HT_MCS8_MCS9			10
#define	SKU_PHYMODE_HT_MCS10_MCS11			11
#define	SKU_PHYMODE_HT_MCS12_MCS13			12
#define	SKU_PHYMODE_HT_MCS14_MCS15			13
#define	SKU_PHYMODE_STBC_MCS0_MCS1			14
#define	SKU_PHYMODE_STBC_MCS2_MCS3			15
#define	SKU_PHYMODE_STBC_MCS4_MCS5			16
#define	SKU_PHYMODE_STBC_MCS6_MCS7			17


VOID InitSkuRateDiffTable(
	IN PRTMP_ADAPTER	pAd)
{
	USHORT		i, value = 0;
	CHAR		BasePwr, Pwr;

	RT28xx_EEPROM_READ16(pAd, EEPROM_TXPOWER_BYRATE_20MHZ_2_4G + 4, value);
	BasePwr = (value >> 8) & 0xFF;
	BasePwr = (BasePwr > 0x1F) ? BasePwr - 0x40 : BasePwr;

	for (i = 0; i < 9; i++) {
		RT28xx_EEPROM_READ16(pAd, EEPROM_TXPOWER_BYRATE_20MHZ_2_4G + i * 2, value);
		Pwr = value & 0xFF;
		Pwr = (Pwr > 0x1F) ? Pwr - 0x40 : Pwr;
		pAd->SingleSkuRatePwrDiff[i * 2] = Pwr - BasePwr;
		Pwr = (value >> 8) & 0xFF;
		Pwr = (Pwr > 0x1F) ? Pwr - 0x40 : Pwr;
		pAd->SingleSkuRatePwrDiff[i * 2 + 1] = Pwr - BasePwr;
	}
}


NDIS_STATUS MtShowFWSingleSkuTable(RTMP_ADAPTER *pAd, INT8 *data, UINT16 length)
{
	UINT32 i = 0, j = 0, offset = 0;

	MTWF_PRINT("==========================================================================\n");
	MTWF_PRINT("                     Single Sku Info from FW site\n");
	MTWF_PRINT("==========================================================================\n");

	for (i = 0; i < SINGLE_SKU_TABLE_TYPE_NUM; i++) {
		MTWF_PRINT("%s: ", SkuParseTypeName[i]);
		for (j = 0; j < powerLimitDataRowNum[i]; j++)
			MTWF_PRINT("%d ", data[j + offset]);

		MTWF_PRINT("\n");
		offset = offset + (powerLimitDataRowNum[i]);
	}

	MTWF_PRINT("-----------------------------------------------------------------\n");
	return NDIS_STATUS_SUCCESS;
}

BOOLEAN GetTxPowerforACS(
	IN struct _RTMP_ADAPTER *pAd)
{
	struct pwr_table_list *pTbl = NULL;
	UINT8 sku_tbl_idx = 0, u1ParamIdx = 0, psu_sku_tbl_idx;
	UINT32 i, rowNum = 0;
	UINT32 ChannelIdx;
	struct _AUTOCH_SEL_CTRL *pAutoChSelCtrl = NULL;

	if (!HcGetAutoChCtrl(pAd)) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_ERROR,
				"pAutoChCtrl = NULL!!!!\n");
		return FALSE;
	}
	pAutoChSelCtrl = &HcGetAutoChCtrl(pAd)->AutoChSelCtrl;
	if (pAutoChSelCtrl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_ERROR,
				"pAutoChSelCtrl = NULL!!!!\n");
		return FALSE;
	}

	/* query sku table index */
	chip_get_sku_tbl_idx(pAd, &sku_tbl_idx, &psu_sku_tbl_idx);
	if (sku_tbl_idx >= MAX_SKU_TABLE_NUM)
		return FALSE;
#if defined(CONFIG_6G_SUPPORT) && defined(CONFIG_6G_AFC_SUPPORT) && defined(DOT11_HE_AX)
		if (pAd->afc_ctrl.AfcStateMachine.CurrState == AFC_RUN) {
			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_ERROR,
				"Device is standard power\n");

			sku_tbl_idx = AFC_STD_PWR_SKUTABLE_IDX;
		}
#endif /*CONFIG_6G_SUPPORT &&*/
	pTbl = pcptrSkuTbl[sku_tbl_idx];
	rowNum = ARRAY_SIZE(Sku_01);
	if (pTbl == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_ERROR,
				"pAutoChSelCtrl = NULL!!!!\n");
		return FALSE;
	}

	/*assign the TxPower from sku table to AutoChSelChList (mcs11)*/
	u1ParamIdx += SINGLE_SKU_TABLE_CCK_LENGTH +
				SINGLE_SKU_TABLE_OFDM_LENGTH - 1;
	for (i = 0; i < rowNum; i++) {

		for (ChannelIdx = 0; ChannelIdx < pAutoChSelCtrl->ChListNum; ChannelIdx++) {
			if (pTbl[i].channel ==
				pAutoChSelCtrl->AutoChSelChList[ChannelIdx].Channel) {

				pAutoChSelCtrl->AutoChSelChList[ChannelIdx].TxPower
				= pTbl[i].pwrLimit[u1ParamIdx] / 2;

				MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_ACS, DBG_LVL_INFO,
				"ch %d Txpower is %d\n",
				pAutoChSelCtrl->AutoChSelChList[ChannelIdx].Channel,
				pAutoChSelCtrl->AutoChSelChList[ChannelIdx].TxPower);
				break;
			}
		}
	}
	return TRUE;
}
#endif /* SINGLE_SKU_V2 */
#ifdef REPORT_CH_TXPWR_SUPPORT
static inline BOOLEAN IsChannelPresent(
		IN PRTMP_ADAPTER pAd,
		IN UCHAR channel,
		IN CHANNEL_CTRL * pChCtrl)
{
	INT i;

	for (i = 0; i < pChCtrl->ChListNum; i++) {
		if (pChCtrl->ChList[i].Channel == channel)
			return TRUE;
	}

	return FALSE;
}

static inline void parse_pwr_info(
		IN PRTMP_ADAPTER pAd,
		IN struct pwr_table_list *pTbl,
		IN struct pwr_table_list *pPsuTbl,
		IN UINT8 idx,
		IN UINT8 u1Type,
		IN struct _wdev_tx_power_limit *txpwrinfo)
{
	UINT8 u1ColIdx, u1ParamType;
	UINT32 u1ParamIdx;
	INT8 pwr20 = 0, pwr40 = 0, pwr80 = 0, pwr160 = 0, pwr320 = 0, pwr = 0;
	UINT8 u1TypeParseNum[TABLE_PARSE_TYPE_NUM] = {SINGLE_SKU_TABLE_TYPE_NUM, BF_BACKOFF_TYPE_NUM};

	txpwrinfo->tx_pwr[txpwrinfo->ch_num].channel =   pTbl[idx].channel;

	for (u1ParamType = 0, u1ParamIdx = 0; u1ParamType < u1TypeParseNum[u1Type]; u1ParamType++) {
		for (u1ColIdx = 0; u1ColIdx < powerLimitDataRowNum[u1ParamType]; u1ColIdx++, u1ParamIdx++) {
			if (u1ColIdx == 0) { //Lowert data rate imdex
				if ((pAd->CommonCfg.PsuSkuEnable && pAd->CommonCfg.SKUenable) &&
					(pTbl[idx].pwrLimit[u1ParamIdx] > pPsuTbl[idx].pwrLimit[u1ParamIdx]))
					pwr = min(pTbl[idx].pwrLimit[u1ParamIdx],
						pPsuTbl[idx].pwrLimit[u1ParamIdx]);
				else
					pwr = (pTbl[idx].pwrLimit[u1ParamIdx] == 126) ? 0 : pTbl[idx].pwrLimit[u1ParamIdx];

				if ((u1ParamType == 2 || u1ParamType == 4 || u1ParamType == 11 || u1ParamType == 18)
						&& (pwr20 < pwr))
					pwr20 = pwr;
				else if ((u1ParamType == 3 || u1ParamType == 5 || u1ParamType == 12 || u1ParamType == 19)
						&& (pwr40 < pwr))
					pwr40 = pwr;
				else if ((u1ParamType == 6 || u1ParamType == 13 || u1ParamType == 20)
						&& (pwr80 < pwr))
					pwr80 = pwr;
				else if ((u1ParamType == 7 || u1ParamType == 14 || u1ParamType == 21)
						&& (pwr160 < pwr))
					pwr160 = pwr;
				else if (u1ParamType == 22 && (pwr320 < pwr))
					pwr320 = pwr;
			}
		}
	}

	txpwrinfo->tx_pwr[txpwrinfo->ch_num].pwrlimit[0] = pwr20;
	txpwrinfo->tx_pwr[txpwrinfo->ch_num].pwrlimit[1] = pwr40;
	txpwrinfo->tx_pwr[txpwrinfo->ch_num].pwrlimit[2] = pwr80;
	txpwrinfo->tx_pwr[txpwrinfo->ch_num].pwrlimit[3] = pwr160;
	txpwrinfo->tx_pwr[txpwrinfo->ch_num].pwrlimit[4] = pwr320;
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
			"ch_num:%d ch:%d pwr:%d/%d/%d/%d/%d\n", txpwrinfo->ch_num,
			txpwrinfo->tx_pwr[txpwrinfo->ch_num].channel, pwr20, pwr40,
			pwr80, pwr160, pwr320);
	txpwrinfo->ch_num += 1;
}

NDIS_STATUS MtGetMaxTxPwrLimit(RTMP_ADAPTER *pAd, UINT8 band_idx, struct _wdev_tx_power_limit *txpwrinfo, UINT8 u1Type)
{
	CHANNEL_CTRL *pChCtrl = NULL;
	struct pwr_table_list *pTbl = NULL, *pPsuTbl = NULL;
	UINT8 sku_tbl_idx = 0;
	UINT8 psu_sku_tbl_idx = 0;
	UINT32 rowNum = 0, i = 0;
	BOOLEAN ChPresent = FALSE;

	pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);

	if (!txpwrinfo)
		goto error0;

	chip_get_sku_tbl_idx(pAd, &sku_tbl_idx, &psu_sku_tbl_idx);
	if (sku_tbl_idx >= MAX_SKU_TABLE_NUM)
		goto error1;

#if defined(CONFIG_6G_SUPPORT) && defined(CONFIG_6G_AFC_SUPPORT) && defined(DOT11_HE_AX)
	if (pAd->afc_ctrl.AfcStateMachine.CurrState == AFC_RUN) {
		MTWF_PRINT("Device is standard power\n");

		sku_tbl_idx = AFC_STD_PWR_SKUTABLE_IDX;
	}
#endif /*CONFIG_6G_SUPPORT &&*/
	if (u1Type == POWER_LIMIT_TABLE_TYPE_SKU) {
		pTbl = pcptrSkuTbl[sku_tbl_idx];
		pPsuTbl = pcptrSkuTbl[psu_sku_tbl_idx];
		rowNum = ARRAY_SIZE(Sku_01);
		if (pAd->CommonCfg.PsuSkuEnable == 1 && pAd->CommonCfg.SKUenable == 0)
			pTbl = pcptrSkuTbl[psu_sku_tbl_idx];

	} else if (u1Type == POWER_LIMIT_TABLE_TYPE_BACKOFF) {
		pTbl = pcptrBackoffTbl[sku_tbl_idx];
		rowNum = ARRAY_SIZE(Backoff_01);
	}

	txpwrinfo->ch_num = 0;

	for (i = 0; i < rowNum; i++) {
		/* search for specific channel */
		ChPresent = IsChannelPresent(pAd,  pTbl[i].channel, pChCtrl);
		if ((band_idx == pTbl[i].chBand) && ChPresent)
			parse_pwr_info(pAd, pTbl, pPsuTbl, i, u1Type, txpwrinfo);
	}
	return NDIS_STATUS_SUCCESS;

error0:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
			"null pointer for buffer to update power limit table !!\n");
	return NDIS_STATUS_FAILURE;

error1:
	MTWF_DBG(pAd, DBG_CAT_POWER, CATPOWER_SKU, DBG_LVL_ERROR,
			"null pointer for parameter related to fill power limit table proc !!\n");
	return NDIS_STATUS_FAILURE;
}
#endif

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
	chlist.c

	Abstract:

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifndef __CHLIST_H__
#define __CHLIST_H__

#include "rtmp_type.h"
#include "rtmp_def.h"

#define CH_GROUP_BAND0		(1<<0)
#define CH_GROUP_BAND1		(1<<1)
#define CH_GROUP_BAND2		(1<<2)
#define CH_GROUP_BAND3		(1<<3)

#define A_BAND_6G_REGION_0	0
#define A_BAND_6G_REGION_1	1
#define A_BAND_6G_REGION_2	2
#define A_BAND_6G_REGION_3	3
#define A_BAND_6G_REGION_4	4
#define A_BAND_6G_REGION_5	5
#define A_BAND_6G_REGION_6	6
#define A_BAND_6G_REGION_7	7

#define G_BAND_REGION_0				0
#define G_BAND_REGION_1				1
#define G_BAND_REGION_2				2
#define G_BAND_REGION_3				3
#define G_BAND_REGION_4				4
#define G_BAND_REGION_5				5
#define G_BAND_REGION_6				6
#define G_BAND_REGION_7				7


#define COUNTRY_CN 18

enum BW_CHG_TYPE {
	CHAN_BW_NO_CHANGE,
	CHAN_BW_EXPAND,
	CHAN_BW_COMPRESS,
};

#ifdef RT_CFG80211_SUPPORT

#define MTK_CFG80211_CHAN_SET_FLAG_CHAN (1 << 0)
#define MTK_CFG80211_CHAN_SET_FLAG_BW (1 << 1)
#define MTK_CFG80211_CHAN_SET_FLAG_EXT_CHAN (1 << 2)
#define MTK_CFG80211_CHAN_SET_FLAG_COEX (1 << 3)
#define MTK_CFG80211_CHAN_SET_FLAG_CAC_REQUIRED (1 << 4)

enum mwctl_chan_width {
	MWCTL_CHAN_WIDTH_20,
	MWCTL_CHAN_WIDTH_40,
	MWCTL_CHAN_WIDTH_80,
	MWCTL_CHAN_WIDTH_160,
	MWCTL_CHAN_WIDTH_320,
};

enum nl80211_chan_width phy_bw_2_nl_bw(int width);

#endif

extern COUNTRY_CODE_TO_COUNTRY_REGION allCountry[];

typedef struct _CH_GROUP_DESC {
	UCHAR FirstChannel;
	UCHAR NumOfCh;
} CH_GROUP_DESC, *PCH_GROUP_DESC;

typedef struct _CH_DESC {
	UCHAR FirstChannel;
	UCHAR NumOfCh;
	UINT32 StartFreq; /* ch group frequency range */
	UINT32 EndFreq;    /* ch group frequency range */
	UCHAR ChannelProp;
} CH_DESC, *PCH_DESC;

typedef struct _COUNTRY_REGION_CH_DESC {
	UCHAR RegionIndex;
	PCH_DESC pChDesc;
} COUNTRY_REGION_CH_DESC, *PCOUNTRY_REGION_CH_DESC;



#define ODOR			0
#define IDOR			1
#define BOTH			2

typedef struct _CH_DESP {
	UCHAR FirstChannel;
	UCHAR NumOfCh;
	CHAR MaxTxPwr;			/* dBm */
	UCHAR Geography;			/* 0:out door, 1:in door, 2:both */
	BOOLEAN DfsReq;			/* Dfs require, 0: No, 1: yes. */
} CH_DESP, *PCH_DESP;

typedef struct _CH_REGION {
	UCHAR CountReg[3];
	UCHAR op_class_region;	/* 0: CE, 1: FCC, 2: JAP, 3:JAP_W53, JAP_W56  5:CHN*/
	BOOLEAN edcca_on;
	CH_DESP *pChDesp;
} CH_REGION, *PCH_REGION;

extern CH_REGION ChRegion[];

typedef struct _CH_FREQ_MAP_ {
	UINT16		channel;
	UINT16		freqKHz;
} CH_FREQ_MAP;


/* Check if it is Japan W53(ch52,56,60,64) channel. */
#define JapanChannelCheck(_ch)  ((_ch == 52) || (_ch == 56) || (_ch == 60) || (_ch == 64))

PCH_REGION GetChRegion(
	IN PUCHAR CountryCode);

#ifdef EXT_BUILD_CHANNEL_LIST
VOID BuildChannelListEx(
	IN PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev);

VOID BuildBeaconChList(
	IN PRTMP_ADAPTER pAd,
	IN struct wifi_dev *wdev,
	OUT PUCHAR pBuf,
	OUT	PULONG pBufLen);
#endif /* EXT_BUILD_CHANNEL_LIST */

UCHAR GetCountryRegionFromCountryCode(UCHAR *country_code);
UINT GetEDCCAStd(PRTMP_ADAPTER pAd, IN PUCHAR CountryCode, IN USHORT radioPhy);
#ifdef DOT11_N_SUPPORT
BOOLEAN IsValidChannel(PRTMP_ADAPTER pAd, UCHAR channel, struct wifi_dev *wdev);
BOOLEAN IsValidFreq(PRTMP_ADAPTER pAd, UINT32 Freq);
UCHAR GetExtCh(UCHAR Channel, UCHAR Direction);
BOOLEAN ExtChCheck(PRTMP_ADAPTER pAd, UCHAR Channel, UCHAR Direction, struct wifi_dev *wdev);
UCHAR N_SetCenCh(RTMP_ADAPTER *pAd, UCHAR channel, UCHAR ht_bw);
BOOLEAN N_ChannelGroupCheck(RTMP_ADAPTER *pAd, UCHAR channel, struct wifi_dev *wdev);
VOID ht_ext_cha_adjust(struct _RTMP_ADAPTER *pAd, UCHAR prim_ch, UCHAR *ht_bw, UCHAR *ext_cha, struct wifi_dev *wdev);
#endif /* DOT11_N_SUPPORT */

UINT8 GetCuntryMaxTxPwr(
	IN PRTMP_ADAPTER pAd,
	IN USHORT PhyMode,
	IN struct wifi_dev *wdev,
	IN UCHAR ht_bw);

VOID BuildCountryIEChList(
	RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	PUCHAR pBuf,
	PULONG pBufLen);

UCHAR GetChannel_5GHZ(
	IN PCH_DESC pChDesc,
	IN UCHAR index);

UCHAR GetChannel_2GHZ(
	IN PCH_DESC pChDesc,
	IN UCHAR index);

UCHAR GetChannelFlag(
	IN PCH_DESC pChDesc,
	IN UCHAR index);

UINT16 TotalChNum(
	IN PCH_DESC pChDesc);

#ifdef NON_WIFI_FREQ_5MHZ_SHIFT_SUPPORT
UCHAR get_bw_cap_by_frequency(struct wifi_dev *wdev, UINT32 freq);
#endif

UCHAR get_channel_bw_cap(struct wifi_dev *wdev, UCHAR channel);

BOOLEAN Is40MHzForbid(CHANNEL_CTRL *pChCtrl);

INT32 ChannelFreqToGroup(
	IN UINT32 ChannelFreq);

BOOLEAN MTChGrpValid(
	IN CHANNEL_CTRL *ChCtrl);

void MTSetChGrp(RTMP_ADAPTER *pAd, RTMP_STRING *buf);

BOOLEAN MTChGrpChannelChk(
	IN CHANNEL_CTRL *pChCtrl,
	IN UCHAR ch);

void RTMPSetCountryCode(RTMP_ADAPTER *pAd, RTMP_STRING *CountryCode);
INT Set_CountryString_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
INT Set_CountryCode_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
INT Set_CountryRegion_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);
INT Set_CountryRegionABand_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg);

INT apply_ht_extchan_cfg(RTMP_ADAPTER *pAd);

INT update_ht_extchan_cfg(RTMP_ADAPTER *pAd, UCHAR ext_chan, BOOLEAN *b_chg);

#ifdef NON_WIFI_FREQ_5MHZ_SHIFT_SUPPORT
BOOLEAN ChannelGroupCheck(RTMP_ADAPTER *pAd, UCHAR bw, UINT32 frequency);
VOID GetFreqRange(UINT32 freq, UCHAR bw, UINT32 *startfreq, UINT32 *endfreq);
#endif


#ifdef DOT11_EHT_BE
BOOLEAN prepare_mlo_csa(RTMP_ADAPTER *pAd, struct wifi_dev *wdev);
void remove_mlo_csa(RTMP_ADAPTER *pAd, struct wifi_dev *wdev);
#endif

#ifdef RT_CFG80211_SUPPORT
INT mtk_cfg80211_vndr_cmd_set_country_code(RTMP_ADAPTER *pAd, UCHAR *code);
INT mtk_cfg80211_vndr_cmd_set_country_region(RTMP_ADAPTER *pAd, UINT32 region);
INT mtk_cfg80211_vndr_cmd_set_country_string(RTMP_ADAPTER *pAd, UCHAR *string);
INT mtk_cfg80211_vndr_cmd_set_channel_attributes(
	IN struct _RTMP_ADAPTER *pAd,
	UINT32 set_flag,
	UCHAR chan,
	UINT32 bw,
	UCHAR ext_chan,
	UCHAR ht_coex,
	UCHAR bypass_cac);
#endif

VOID init_ch_chg_info(IN PRTMP_ADAPTER pAd);

INT32 ShowChTimeCost(struct _RTMP_ADAPTER *pAd,
						RTMP_STRING *arg);

INT32 ShowCoexObssPara(struct _RTMP_ADAPTER *pAd,
						RTMP_STRING *arg);

VOID RecordTimeNode(
		struct Channel_Ops_Time *pChOpTime,
		ULONG *PStorageLoca,
		UCHAR Lvl);


void set_at2lm_for_scan_status(BOOLEAN status);
void set_at2lm_for_ch_switch_status(BOOLEAN status);
BOOLEAN at2lm_for_scan_is_enabled(void);
BOOLEAN at2lm_for_ch_switch_is_enabled(void);


#ifdef DOT11_EHT_BE
BOOLEAN at2lm_for_scan(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, SCAN_CTRL *ScanCtrl);
void at2lm_for_ch_switch(RTMP_ADAPTER *pAd, UCHAR channel, BOOLEAN csa);
#else
static inline BOOLEAN at2lm_for_scan(RTMP_ADAPTER *pAd, struct wifi_dev *wdev, SCAN_CTRL *ScanCtrl)
{
	return FALSE;
}

static inline void at2lm_for_ch_switch(RTMP_ADAPTER *pAd, UCHAR channel, BOOLEAN csa)
{
	return FALSE;
}

#endif


#endif /* __CHLIST_H__ */


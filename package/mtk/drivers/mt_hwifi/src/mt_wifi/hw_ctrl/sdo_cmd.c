/*
 * Copyright (c) [2024], MediaTek Inc. All rights reserved.
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
	rro_cmd.c
*/
#include "sdo_cmd.h"
#include "os/hwifi_main.h"


/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/
/* SDO command TLV List */
enum _UNI_CMD_SDO_TAG_T {
	UNI_CMD_SDO_SET_OP = 1,
	UNI_CMD_SDO_QUERY_OP = 2,
	UNI_CMD_SDO_AUTO_BA = 3,
	UNI_CMD_SDO_DSCP_PRI = 4,
	UNI_CMD_SDO_HOTSPOT_INFO = 5,
	UNI_CMD_SDO_CP_MODE = 6,
	UNI_CMD_SDO_RED_SETTING = 7,
	UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG = 8,
	UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM = 9,
	UNI_CMD_SDO_OVERRIDE_CTRL = 10,
	UNI_CMD_SDO_CP_FIXEDRATE = 11,
	UNI_CMD_SDO_NUM
};

enum UNI_EVENT_SDO_TAG_T {
	UNI_EVENT_SDO_BSS_ACQ_PKT_NUM  = 1,
	UNI_EVENT_ID_SDO_MAX_NUM
};

#define UNI_EVENT_SDO_CFG_BSS_NUM		96
#define UNI_EVENT_SDO_CFG_BSS_MAP_WORDLEN	((UNI_EVENT_SDO_CFG_BSS_NUM + 31)/32)
#define UNI_EVENT_SDO_NUM_OF_WMM_AC		4

#define MAX_EVENT_FAIL_LOOP 32

/*******************************************************************************
 *    PRIVATE TYPES
 ******************************************************************************/
/** FW & DRV sync **/

/* ============== UNI_CMD_SDO START ============ */
/*  UNI_CMD_ID_SDO (0x88) */
struct GNU_PACKED _UNI_CMD_SDO_T
{
	/* fix field */
	UINT_8 au1Reserved[4];

	/* tlv */
	UINT_8 aucTlvBuffer[0];/**< the TLVs included in this field:
	*
	*   TAG                             | ID  | structure
	*   --------------------------------|-----|--------------
	*   UNI_CMD_SDO_SET_OP        | 0x1 | UNI_CMD_SDO_SET_OP_T
	*   UNI_CMD_SDO_QUERY_OP      | 0x2 | UNI_CMD_SDO_QUERY_OP_T
	*   UNI_CMD_SDO_AUTO_BA       | 0x3 | UNI_CMD_SDO_AUTO_BA_T
	*   UNI_CMD_SDO_DSCP_PRI      | 0x4 | UNI_CMD_SDO_DSCP_PRI_T
	*   UNI_CMD_SDO_HOTSPOT_INFO  | 0x5 | UNI_CMD_SDO_HOTSPOT_INFO_T
	*   UNI_CMD_SDO_CP_MODE       | 0x6 | UNI_CMD_SDO_CP_MODE_T
	*   UNI_CMD_SDO_RED_SETTING         | 0x7 | UNI_CMD_SDO_RED_SETTING_T
	*   UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG | 0x8 | UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG_T
	*   UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM | 0x9 | UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM_T
	*/
};

/** @} */
/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_SET_OP (0x1) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x1
 * @param[in] u2Length                the length of this TLV, should be sizeof(UNI_CMD_SDO_SET_OP_T)
 * @param[in] u4SdoSetArg0            Set parameter 0
 * @param[in] u4SdoSetArg1            Set parameter 1
 * @param[in] u4SdoSetArg2            Set parameter 2
 */
/* sdo set operation (Tag1) */
struct GNU_PACKED _UNI_CMD_SDO_SET_OP_T
{
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_32   u4SdoSetArg0;
	UINT_32   u4SdoSetArg1;
	UINT_32   u4SdoSetArg2;
};

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_QUERY_OP (0x2) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x2
 * @param[in] u2Length                the length of this TLV
 * @param[in] u4SdoQueryOptionArg0    Query parameter 0
 * @param[in] u4SdoQueryOptionArg1    Query parameter 1
 * @param[in] u4SdoQueryOptionArg2    Query parameter 2
 * @param[in] u4SdoQueryList          Query List
 */
/* sdo set operation (Tag2) */
struct GNU_PACKED _UNI_CMD_SDO_QUERY_OP_T
{
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_32   u4SdoQueryOptionArg0;
	UINT_32   u4SdoQueryOptionArg1;
	UINT_32   u4SdoQueryOptionArg2;
	UINT_32   u4SdoQueryList[0];
};

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_AUTO_BA (0x3) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x3
 * @param[in] u2Length                the length of this TLV, sizeof(UNI_CMD_SDO_AUTO_BA_T)
 * @param[in] ucAutoBaEnable          enable AutoBa
 * @param[in] ucTarget                Auto Ba target
 * @param[in] u4Timeout               timeout value(unit ms)
 */
/* sdo auto ba (Tag3) */
struct GNU_PACKED _UNI_CMD_SDO_AUTO_BA_T
{
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_8    ucAutoBaEnable;   // 0: No reload, 1: do reload
	UINT_8    ucTarget;
	UINT_8    aucReserve[2];
	UINT_32   u4Timeout;    /* timeout value, unit ms. */
};

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_DSCP_PRI (0x4) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x4
 * @param[in] u2Length                the length should be sizeof(UNI_CMD_SDO_DSCP_PRI_T)
 * @param[in] ucBssid                 bssid
 * @param[in] ucDscpPriEnable         enable/disable dscp priority
 * @param[in] aucDscpPriMap           dscp priority map
 */
/* sdo dscp pri (Tag4) */
struct GNU_PACKED _UNI_CMD_SDO_DSCP_PRI_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucBssid;
	UINT_8 ucDscpPriEnable;
	UINT_8 aucReserved[2];
	UINT_8 aucDscpPriMap[64];
};

#if defined(CONFIG_HOTSPOT) || defined(CONFIG_PROXY_ARP)
/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_HOTSPOT_INFO (0x5) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x5
 * @param[in] u2Length                the length should be sizeof(UNI_CMD_SDO_HOTSPOT_INFO_T)
 * @param[in] ucUpdateType
 * @param[in] ucHotspotBssFlags
 * @param[in] ucHotspotBssId
 * @param[in] ucStaQosMapFlagAndIdx
 * @param[in] u2Wcid
 * @param[in] ucPoolID
 * @param[in] rQosMapTable
 */
/* sdo hotspot info (Tag5) */
struct GNU_PACKED _UNI_CMD_SDO_HOTSPOT_INFO_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	/* hs bss flag */
	UINT_8 ucUpdateType;
	UINT_8 ucHotspotBssFlags;
	UINT_8 ucHotspotBssId;
	UINT_8 ucStaQosMapFlagAndIdx;
	/* sta DSCP */
	UINT_16 u2Wcid;
	UINT_8 ucPoolID;
	UINT_8 ucReserved;
	/* DSCP pool */
	QOS_MAP_TABLE_T rQosMapTable;
};
#endif

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_CP_MODE (0x6) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x6
 * @param[in] u2Length                the length should be sizeof(UNI_CMD_SDO_CP_MODE)
 * @param[in] ucCpMode                Cp mode
 */
/* sdo cp mode (Tag6) */
struct GNU_PACKED _UNI_CMD_SDO_CP_MODE_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucCpMode;
	UINT_8 aucReserved[3];
};


struct GNU_PACKED SDO_CP_FIXEDRATE_ENTRY
{
	UINT_8 ucCPType;
	UINT_8 isFr;
	UINT_16 u2FixedRateIdx;
};
/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_CP_FIXEDRATE (0xb) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x6
 * @param[in] u2Length                the length should be sizeof(UNI_CMD_SDO_CP_MODE)
 * @param[in] ucCpMode                Cp mode
 */
/* sdo cp mode (Tag6) */
struct GNU_PACKED _UNI_CMD_SDO_CP_FIXEDRATE_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucSdoBandIdx;
	UINT_8 ucCPEntryCount;
	UINT_16 u2Rsv;
	struct SDO_CP_FIXEDRATE_ENTRY arSdoCpFrEntry[0];
};



/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_RED_SETTING (0x7) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x7
 * @param[in] u2Length                the length of this TLV, should be
 *                                    sizeof(UNI_CMD_SDO_RED_SETTING_T)
 * @param[in] param_header            red setting header
 * @param[in] param_config            red setting config
 */
/* sdo red setting (Tag7) */
struct _UNI_CMD_SDO_RED_SETTING_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;

	struct red_cmd_header            param_header;
	struct red_cmd_admctrl_config    param_config;
};

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG (0x8) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x8
 * @param[in] u2Length                the length of this TLV, should be
 *                                    sizeof(UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG_T)
 * @param[in] ucBssIndex
 * @param[in] ucQueueNum
 * @param[in] ucWlanIdx
 * @param[in] ucBandIdx
 * @param[in] aucReserved[3]
 * @param[in] aacQue[UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE]

 */
/* sdo pkt budget ctrl (Tag8) */

#define UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE  (5)
#define UNI_CMD_PBC_BSS_IDX_FOR_ALL             (0xFF)
#define UNI_CMD_PBC_WLAN_IDX_FOR_ALL            (0xFFFF)
#define UNI_CMD_PBC_BOUNDARY_RESET_TO_DEFAULT   (0xFFFF)

struct _UNI_CMD_PKT_BUDGET_CTRL_ENTRY_T {
	UINT_16    lower_bound;
	UINT_16    upper_bound;
};

struct _UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;

	UINT_8    ucBssIndex;
	UINT_8    ucQueueNum;
	UINT_16   u2WlanIdx;
	UINT_8    ucBandIdx;
	UINT_8    aucReserved[3];
	struct _UNI_CMD_PKT_BUDGET_CTRL_ENTRY_T    aacQue[UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE];
};

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM (0x9) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO function mode
 *
 * @param[in] u2Tag                   should be 0x9
 * @param[in] u2Length                the length of this TLV, should be
 *                                    sizeof(UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM_T)
 * @param[in] au4BssMap                bss bit map
 */
/* sdo get bss acq pkt number (Tag9) */

/** @addtogroup UNI_CMD_ID_SDO
 * @{
 */
/**
 * This structure is used for UNI_CMD_SDO_OVERRIDE_CTRL (0xa) of UNI_CMD_ID_SDO command (0x88)
 * to set SDO override control setting
 *
 * @param[in] u2Tag            should be 0xa
 * @param[in] u2Length         the length of this TLV, should be sizeof(UNI_CMD_SDO_OVERRIDE_CTRL_T)
 * @param[in] ucCtrl0Value     Per sta override 0 value from sta_rec/cr(0) or hif_txd(1)
 * @param[in] ucCtrl1Value     Per sta override 1 value from sta_rec/cr(0) or hif_txd(1)
 * @param[in] ucPerCrOverrideSettingEntryCount entry count in rPerCrOverride
 * @param[in] ucPerStaOverrideSettingEntryCount entry count in rPerStaOverride
 * @param[in] au4GlobalOverrideMacTxdMask the mask of Global override setting
 * @param[in] rPerCrOverride   Per Cr override setting
 * @param[in] rPerStaOverride  Per Sta override setting
 */

#define SDO_GLOBAL_OVERRIDE_MAX_DW 8
#define SDO_PERCR_OVERRIDE_MAX_ENTRY 6
#define SDO_PERSTA_OVERRIDE_MAX_ENTRY 5

struct _SDO_PER_STA_OVERRIDE_SETTING_ENTRY {
	UINT_8 ucApplyIdx;
	UINT_8 ucMask;
	UINT_8 ucValue;
	UINT_8 ucCtrlIdx : 1;
	UINT_8 ucApplyAll : 1;
	UINT_8 ucRsv : 6;
};

/* sdo override control (Tag10) */
struct _UNI_CMD_SDO_OVERRIDE_CTRL_T {
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucCtrl0Value;
	UINT_8 ucCtrl1Value;
	UINT_8 ucPerCrOverrideSettingEntryCount;
	UINT_8 ucPerStaOverrideSettingEntryCount;
	UINT_32 au4GlobalOverrideMacTxdMask[SDO_GLOBAL_OVERRIDE_MAX_DW];
	struct _SDO_PER_STA_OVERRIDE_SETTING_ENTRY rPerCrOverride[SDO_PERCR_OVERRIDE_MAX_ENTRY];
	struct _SDO_PER_STA_OVERRIDE_SETTING_ENTRY rPerStaOverride[SDO_PERSTA_OVERRIDE_MAX_ENTRY];
};

#define UNI_SDO_BSS_ACQ_PKT_NUM_BSS_GROUP_DEFAULT	0xFFFFFFFF
#define UNI_SDO_GET_BSS_ACQ_PKT_NUM_CMD_DEFAULT		(UNI_SDO_GET_BSS_ACQ_PKT_NUM_BSS_GROUP_DEFAULT)

#define UNI_CMD_SDO_CFG_BSS_NUM		96
#define UNI_CMD_SDO_CFG_BSS_MAP_WORDLEN	((UNI_CMD_SDO_CFG_BSS_NUM+31)/32)

struct _UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;

	UINT_32   au4BssMap[UNI_CMD_SDO_CFG_BSS_MAP_WORDLEN];
};

/* ============== UNI_CMD_SDO END ============ */

/* ============== UNI_EVENT_SDO START ============ */
struct GNU_PACKED UNI_EVENT_SDO_T {
	/* fixed field */
	UINT_8 aucPadding[4];

	/* tlv */
	UINT_8 aucTlvBuffer[0];
};

struct GNU_PACKED UNI_EVENT_PER_BSS_ACQ_PKT_NUM_T {
	UINT32	au4AcqPktCnt[UNI_EVENT_SDO_NUM_OF_WMM_AC];
};

struct GNU_PACKED UNI_EVENT_BSS_ACQ_PKT_NUM_T {
	UINT32	au4BssMap[UNI_EVENT_SDO_CFG_BSS_MAP_WORDLEN];
	struct UNI_EVENT_PER_BSS_ACQ_PKT_NUM_T	bssPktInfo[UNI_EVENT_SDO_CFG_BSS_NUM];
};

struct GNU_PACKED UNI_EVENT_SDO_BSS_ACQ_PKT_NUM_T {
	/* DW_0 */
	UINT_16 u2Tag;
	UINT_16 u2Length;
	/* DW_1.. */
	struct UNI_EVENT_BSS_ACQ_PKT_NUM_T BssAcqPktNum;
};

/* ============== UNI_EVENT_SDO END ============ */


/*******************************************************************************
 *    PRIVATE FUNCTION PROTOTYPES
 ******************************************************************************/
/* For Command*/

/*******************************************************************************
 *    FUNCTIONS
 ******************************************************************************/
INT32 UniCmdGetSdo(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2,
	struct _CR4_QUERY_STRUC *cr4_query_list)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_QUERY_OP_T cmd_get_sdo = {0};
	struct _CR4_QUERY_STRUC *cr4_query = (struct _CR4_QUERY_STRUC *)cr4_query_list;
	UINT32 u4ComCmdSize = 0, u4CmdNeedMaxBufSize = 0, u4CmdMultiQuerySize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_get_sdo, sizeof(cmd_get_sdo));
	/* Multiple query */
	if (cr4_query)
		u4CmdMultiQuerySize = arg1 * sizeof(*(cr4_query->list));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_get_sdo) + u4CmdMultiQuerySize;

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_get_sdo.u2Tag = UNI_CMD_SDO_QUERY_OP;
	cmd_get_sdo.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
#ifdef CFG_BIG_ENDIAN
	cmd_get_sdo.u2Tag	= cpu2le16(cmd_get_sdo.u2Tag);
	cmd_get_sdo.u2Length = cpu2le16(cmd_get_sdo.u2Length);
#endif /* CFG_BIG_ENDIAN */
	cmd_get_sdo.u4SdoQueryOptionArg0 = cpu2le32(arg0);
	cmd_get_sdo.u4SdoQueryOptionArg1 = cpu2le32(arg1);
	cmd_get_sdo.u4SdoQueryOptionArg2 = cpu2le32(arg2);

	AndesAppendCmdMsg(msg, (char *)&cmd_get_sdo, sizeof(cmd_get_sdo));
	if (u4CmdMultiQuerySize)
		AndesAppendCmdMsg(msg, (char *)cr4_query->list, u4CmdMultiQuerySize);

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}

INT32 UniCmdSetSdo(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2,
	IN UINT32 u4ExtSize,
	IN PUINT8 pExtData)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_SET_OP_T cmd_set_sdo = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo, sizeof(cmd_set_sdo));

	/* Step 1: Count maximum buffer size from per TLV */
	if (u4ExtSize != 0 && pExtData != NULL)
		u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo) + u4ExtSize;
	else
		u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo.u2Tag = UNI_CMD_SDO_SET_OP;
	cmd_set_sdo.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo.u2Tag	= cpu2le16(cmd_set_sdo.u2Tag);
	cmd_set_sdo.u2Length = cpu2le16(cmd_set_sdo.u2Length);
#endif /* CFG_BIG_ENDIAN */
	cmd_set_sdo.u4SdoSetArg0 = cpu2le32(arg0);
	cmd_set_sdo.u4SdoSetArg1 = cpu2le32(arg1);
	cmd_set_sdo.u4SdoSetArg2 = cpu2le32(arg2);
	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo, sizeof(cmd_set_sdo));

	if (u4ExtSize != 0 && pExtData != NULL)
		AndesAppendCmdMsg(msg, (char *)pExtData, u4ExtSize);

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}

INT32 UniCmdSetSdoAutoBA(
	IN struct _RTMP_ADAPTER *pAd,
	IN BOOLEAN Enable,
	IN UINT32 Timeout)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_AUTO_BA_T cmd_set_sdo_auto_ba = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo_auto_ba, sizeof(cmd_set_sdo_auto_ba));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo_auto_ba);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo_auto_ba.u2Tag = UNI_CMD_SDO_AUTO_BA;
	cmd_set_sdo_auto_ba.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
	cmd_set_sdo_auto_ba.ucAutoBaEnable = Enable;
	cmd_set_sdo_auto_ba.ucTarget = 0;
	cmd_set_sdo_auto_ba.u4Timeout = Timeout;
#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo_auto_ba.u2Tag	= cpu2le16(cmd_set_sdo_auto_ba.u2Tag);
	cmd_set_sdo_auto_ba.u2Length = cpu2le16(cmd_set_sdo_auto_ba.u2Length);
	cmd_set_sdo_auto_ba.u4Timeout = cpu2le32(cmd_set_sdo_auto_ba.u4Timeout);
#endif /* CFG_BIG_ENDIAN */
	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo_auto_ba, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 UniCmdSetSdoDscpPri(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT8 inf_idx,
	IN INT if_type)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_DSCP_PRI_T cmd_set_sdo_dscp_pri = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;
	UINT8 ucDscpPriEnable = 0;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo_dscp_pri, sizeof(cmd_set_sdo_dscp_pri));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo_dscp_pri);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo_dscp_pri.u2Tag = UNI_CMD_SDO_DSCP_PRI;
	cmd_set_sdo_dscp_pri.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
	if (if_type == INT_APCLI) {
		cmd_set_sdo_dscp_pri.ucBssid = pAd->StaCfg[inf_idx].wdev.bss_info_argument.ucBssIndex;
#ifdef MAP_R5
		cmd_set_sdo_dscp_pri.ucDscpPriEnable = pAd->StaCfg[inf_idx].dscp_pri_map_enable;
		NdisCopyMemory(cmd_set_sdo_dscp_pri.aucDscpPriMap, pAd->StaCfg[inf_idx].dscp_pri_map, 64);
#endif
	} else {
		cmd_set_sdo_dscp_pri.ucBssid = pAd->ApCfg.MBSSID[inf_idx].wdev.bss_info_argument.ucBssIndex;
		cmd_set_sdo_dscp_pri.ucDscpPriEnable = pAd->ApCfg.MBSSID[inf_idx].dscp_pri_map_enable;
		NdisCopyMemory(cmd_set_sdo_dscp_pri.aucDscpPriMap, pAd->ApCfg.MBSSID[inf_idx].dscp_pri_map, 64);
	}
#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo_dscp_pri.u2Tag	= cpu2le16(cmd_set_sdo_dscp_pri.u2Tag);
	cmd_set_sdo_dscp_pri.u2Length = cpu2le16(cmd_set_sdo_dscp_pri.u2Length);
#endif /* CFG_BIG_ENDIAN */
	ucDscpPriEnable = cmd_set_sdo_dscp_pri.ucDscpPriEnable;

	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo_dscp_pri, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");

	if (pAd->CommonCfg.wifi_cert == 1) {
		if (ucDscpPriEnable) {
			if (SetCPSupportEnable(pAd, 1)) {
				pAd->cp_support = 1;
				pAd->cp_support_off_by_qos_map = TRUE;
			}
		} else {
			if (SetCPSupportEnable(pAd, pAd->cp_support_profile)) {
				pAd->cp_support = pAd->cp_support_profile;
				pAd->cp_support_off_by_qos_map = FALSE;
			}
		}
	}

error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}

#if defined(CONFIG_HOTSPOT_R2) || defined(CONFIG_PROXY_ARP)
INT32 UniCmdUpdateSdoHotspotInfo(
	IN struct _RTMP_ADAPTER *pAd,
	IN struct _MT_HOTSPOT_INFO_UPDATE *InfoUpdateT)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_HOTSPOT_INFO_T cmd_update_sdo_hotspot_info = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;
	UINT8 idx = 0;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_update_sdo_hotspot_info, sizeof(cmd_update_sdo_hotspot_info));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_update_sdo_hotspot_info);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_update_sdo_hotspot_info.u2Tag = UNI_CMD_SDO_HOTSPOT_INFO;
	cmd_update_sdo_hotspot_info.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
	/* hs bss flag */
	cmd_update_sdo_hotspot_info.ucUpdateType = InfoUpdateT->ucUpdateType;
	cmd_update_sdo_hotspot_info.ucHotspotBssFlags = InfoUpdateT->ucHotspotBssFlags;
	cmd_update_sdo_hotspot_info.ucHotspotBssId = InfoUpdateT->ucHotspotBssId;
	/* sta DSCP */
	cmd_update_sdo_hotspot_info.u2Wcid = InfoUpdateT->u2StaWcid;
	cmd_update_sdo_hotspot_info.ucStaQosMapFlagAndIdx = InfoUpdateT->ucStaQosMapFlagAndIdx;
    /* DSCP pool */
	cmd_update_sdo_hotspot_info.rQosMapTable.ucPoolValid = InfoUpdateT->ucTableValid;
	cmd_update_sdo_hotspot_info.rQosMapTable.ucDscpExceptionCount = InfoUpdateT->ucPoolDscpExceptionCount;
	cmd_update_sdo_hotspot_info.rQosMapTable.u4Ac = InfoUpdateT->u4Ac;

	for (idx = 0; idx < 8; idx++)
		cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpRange[idx] = InfoUpdateT->au2PoolDscpRange[idx];

	for (idx = 0; idx < 21; idx++)
		cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpException[idx] = InfoUpdateT->au2PoolDscpException[idx];

	cmd_update_sdo_hotspot_info.ucPoolID = InfoUpdateT->ucPoolID;

#ifdef CFG_BIG_ENDIAN
	cmd_update_sdo_hotspot_info.u2Tag	= cpu2le16(cmd_update_sdo_hotspot_info.u2Tag);
	cmd_update_sdo_hotspot_info.u2Length = cpu2le16(cmd_update_sdo_hotspot_info.u2Length);
	cmd_update_sdo_hotspot_info.u2Wcid = cpu2le16(cmd_update_sdo_hotspot_info.u2Wcid);
	cmd_update_sdo_hotspot_info.rQosMapTable.u4Ac = cpu2le32(cmd_update_sdo_hotspot_info.rQosMapTable.u4Ac);

	for (idx = 0; idx < 8; idx++)
		cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpRange[idx] = cpu2le16(cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpRange[idx]);

	for (idx = 0; idx < 21; idx++)
		cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpException[idx] = cpu2le16(cmd_update_sdo_hotspot_info.rQosMapTable.au2DscpException[idx]);

#endif /* CFG_BIG_ENDIAN */
	AndesAppendCmdMsg(msg, (char *)&cmd_update_sdo_hotspot_info, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}
#endif /* #if defined(CONFIG_HOTSPOT_R2) || defined(CONFIG_PROXY_ARP) */

INT32 UniCmdSetSdoCPSEnable(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT8 Mode)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_CP_MODE_T cmd_set_sdo_cp_mode = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo_cp_mode, sizeof(cmd_set_sdo_cp_mode));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo_cp_mode);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo_cp_mode.u2Tag = UNI_CMD_SDO_CP_MODE;
	cmd_set_sdo_cp_mode.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
	cmd_set_sdo_cp_mode.ucCpMode = Mode;
#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo_cp_mode.u2Tag	= cpu2le16(cmd_set_sdo_cp_mode.u2Tag);
	cmd_set_sdo_cp_mode.u2Length = cpu2le16(cmd_set_sdo_cp_mode.u2Length);
#endif /* CFG_BIG_ENDIAN */
	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo_cp_mode, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}


//mapping table
UCHAR fixedrateType_driver2fw_map[] = {
	SDO_CP_TYPE_ARP,
	SDO_CP_TYPE_DHCP,
	SDO_CP_TYPE_ICMP,
	SDO_CP_TYPE_EAPOL,
	SDO_CP_TYPE_DNS,
	SDO_CP_TYPE_MMPDU,
	SDO_CP_TYPE_WAI,
	SDO_CP_TYPE_PREAUTH,
	SDO_CP_TYPE_USER_DEFINE0,
	SDO_CP_TYPE_USER_DEFINE1,
	SDO_CP_TYPE_ETHER_TYPE_USER_DEFINE0,
	SDO_CP_TYPE_ETHER_TYPE_USER_DEFINE1,
	SDO_CP_TYPE_HOST_PRI_PKT,
	SDO_CP_TYPE_1905,
	SDO_CP_TYPE_MAX,
};

#ifdef CONFIG_RA_PHY_RATE_SUPPORT
UINT32 UniCmdSetHighpriFixedrate(IN struct _RTMP_ADAPTER *pAd,
	struct SDO_Fixed_Rate *sdo_fixed_rate,
	IN UINT32 count)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_CP_FIXEDRATE_T cmd_set_sdo_cp_fixed_rate = {0};
	struct SDO_CP_FIXEDRATE_ENTRY *sdo_cp_fixedrate_entry = NULL;
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;
	INT32 i = 0;

#define ucSdoBandIdxMLO 3

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	os_alloc_mem(pAd, (UCHAR **)&sdo_cp_fixedrate_entry, count * sizeof(struct SDO_CP_FIXEDRATE_ENTRY));
	if (!sdo_cp_fixedrate_entry) {
		MTWF_PRINT("Not enough memory for dynamic allocating !!\n");
		goto error;
	}


	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo_cp_fixed_rate, sizeof(cmd_set_sdo_cp_fixed_rate));
	os_zero_mem(sdo_cp_fixedrate_entry, count * sizeof(struct SDO_CP_FIXEDRATE_ENTRY));


	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo_cp_fixed_rate) + count * sizeof(struct SDO_CP_FIXEDRATE_ENTRY);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo_cp_fixed_rate.u2Tag = UNI_CMD_SDO_CP_FIXEDRATE;
	cmd_set_sdo_cp_fixed_rate.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
	if (sdo_fixed_rate[i].band_mlo)
		cmd_set_sdo_cp_fixed_rate.ucSdoBandIdx = ucSdoBandIdxMLO;
	else
		cmd_set_sdo_cp_fixed_rate.ucSdoBandIdx = hc_get_hw_band_idx(pAd);
	cmd_set_sdo_cp_fixed_rate.ucCPEntryCount = count;

	for (i = 0; i < count; i++) {
		sdo_cp_fixedrate_entry[i].ucCPType = fixedrateType_driver2fw_map[sdo_fixed_rate[i].ratetype];
		if (!sdo_fixed_rate[i].isAutoRate) {
			sdo_cp_fixedrate_entry[i].isFr = 1;
			sdo_cp_fixedrate_entry[i].u2FixedRateIdx = sdo_fixed_rate[i]._FRIDX_.value;
		} else {
			sdo_cp_fixedrate_entry[i].isFr = 0;
		}

#ifdef CFG_BIG_ENDIAN
	sdo_cp_fixedrate_entry[i].ucCPType = cpu2le16(sdo_cp_fixedrate_entry[i].ucCPType);
	sdo_cp_fixedrate_entry[i].isFr = cpu2le16(sdo_cp_fixedrate_entry[i].isFr);
	sdo_cp_fixedrate_entry[i].ucFixedRateIdx = cpu2le16(sdo_cp_fixedrate_entry[i].ucFixedRateIdx);
#endif /* CFG_BIG_ENDIAN */
	}

#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo_cp_fixed_rate.u2Tag = cpu2le16(cmd_set_sdo_cp_fixed_rate.u2Tag);
	cmd_set_sdo_cp_fixed_rate.u2Length = cpu2le16(cmd_set_sdo_cp_fixed_rate.u2Length);
	cmd_set_sdo_cp_fixed_rate.ucBandIdx = cpu2le16(cmd_set_sdo_cp_fixed_rate.ucBandIdx);
	cmd_set_sdo_cp_fixed_rate.ucCPEntryCount = cpu2le16(cmd_set_sdo_cp_fixed_rate.ucCPEntryCount);
#endif /* CFG_BIG_ENDIAN */

	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo_cp_fixed_rate, sizeof(cmd_set_sdo_cp_fixed_rate));
	AndesAppendCmdMsg(msg, (char *)sdo_cp_fixedrate_entry, (u4CmdNeedMaxBufSize - u4ComCmdSize - sizeof(cmd_set_sdo_cp_fixed_rate)));
	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	if (sdo_cp_fixedrate_entry)
		os_free_mem(sdo_cp_fixedrate_entry);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;
}
#endif

#ifdef CFG_RED_SUPPORT
INT32 UniCmdSetSdoRedSetting(
	IN struct _RTMP_ADAPTER *pAd,
	IN PUINT_8 prParam)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_RED_SETTING_T *prUniCmdSetSdoRedSetting = NULL;

	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);

	os_alloc_mem(NULL, (UCHAR **)&prUniCmdSetSdoRedSetting, sizeof(struct _UNI_CMD_SDO_RED_SETTING_T));

	if (prUniCmdSetSdoRedSetting == NULL) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(prUniCmdSetSdoRedSetting, sizeof(struct _UNI_CMD_SDO_RED_SETTING_T));

	/* Step 1: Count maximum buffer size from per TLV */

	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct _UNI_CMD_SDO_RED_SETTING_T);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	prUniCmdSetSdoRedSetting->u2Tag = UNI_CMD_SDO_RED_SETTING;
	prUniCmdSetSdoRedSetting->u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);

#ifdef CFG_BIG_ENDIAN
	prUniCmdSetSdoRedSetting->u2Tag	= cpu2le16(prUniCmdSetSdoRedSetting->u2Tag);
	prUniCmdSetSdoRedSetting->u2Length = cpu2le16(prUniCmdSetSdoRedSetting->u2Length);
#endif /* CFG_BIG_ENDIAN */

	os_move_mem(&prUniCmdSetSdoRedSetting->param_header, prParam, sizeof(struct red_cmd_header));
	os_move_mem(&prUniCmdSetSdoRedSetting->param_config, prParam + sizeof(struct red_cmd_header), sizeof(struct red_cmd_admctrl_config));

	AndesAppendCmdMsg(msg, (char *)prUniCmdSetSdoRedSetting, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:

	if (prUniCmdSetSdoRedSetting)
		os_free_mem(prUniCmdSetSdoRedSetting);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}
#endif /* CFG_RED_SUPPORT */


INT32 UniCmdSetSdoPktBudgetCtrl(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT_8 bss_idx,
	IN UINT_16 wcid,
	IN UCHAR type
	)
{

	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG_T UniCmdSdoPktBudgetCtrlCfg = {0};
	struct _UNI_CMD_PKT_BUDGET_CTRL_ENTRY_T *prEntry = NULL;
	UINT32 u4ComCmdSize = 0, u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;
	UINT_8 ucQueIdx = 0;
	RTMP_CHIP_CAP *pChipCap = hc_get_chip_cap(pAd->hdev_ctrl);

	if (type >= PBC_TYPE_END) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				 "set wrong type (%d) for PBC!\n", type);
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}


	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_NOTICE,
				"bssid(%d),wcid(%d),type(%d) dispath CMD start\n",
				bss_idx, wcid, type);

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&UniCmdSdoPktBudgetCtrlCfg, sizeof(UniCmdSdoPktBudgetCtrlCfg));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(UniCmdSdoPktBudgetCtrlCfg);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	UniCmdSdoPktBudgetCtrlCfg.u2WlanIdx = cpu2le16(wcid);
	UniCmdSdoPktBudgetCtrlCfg.ucBssIndex = bss_idx;
	UniCmdSdoPktBudgetCtrlCfg.ucQueueNum = UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE;
	UniCmdSdoPktBudgetCtrlCfg.ucBandIdx = hc_get_hw_band_idx(pAd);

	switch (type) {
	case PBC_TYPE_NORMAL: {
		for (ucQueIdx = 0; ucQueIdx < UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE; ucQueIdx++) {
			prEntry = &UniCmdSdoPktBudgetCtrlCfg.aacQue[ucQueIdx];
			if (pChipCap->asic_caps & fASIC_CAP_HW_ADMISSION_CTRL) {
				prEntry->lower_bound = 0;
				prEntry->upper_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
			} else {
				prEntry->lower_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
				prEntry->upper_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
			}
		}
	}
	break;
	case PBC_TYPE_WMM: {
		for (ucQueIdx = 0; ucQueIdx < UNI_CMD_PBC_NUM_OF_PKT_BUDGET_CTRL_QUE; ucQueIdx++) {
			prEntry = &UniCmdSdoPktBudgetCtrlCfg.aacQue[ucQueIdx];
			if (pChipCap->asic_caps & fASIC_CAP_HW_ADMISSION_CTRL) {

				prEntry->lower_bound = cpu2le16(pAd->pbc_bound[ucQueIdx]);

				if (pAd->CommonCfg.wifi_cert)
					prEntry->upper_bound = cpu2le16(pAd->pbc_bound[ucQueIdx]);
				else
					prEntry->upper_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
			} else {
				prEntry->lower_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
				prEntry->upper_bound = cpu2le16(pAd->pbc_bound[ucQueIdx]);
			}
		}
	}
	break;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	UniCmdSdoPktBudgetCtrlCfg.u2Tag = UNI_CMD_SDO_PKT_BUDGET_CTRL_CFG;
	UniCmdSdoPktBudgetCtrlCfg.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);

#ifdef CFG_BIG_ENDIAN
	UniCmdSdoPktBudgetCtrlCfg.u2Tag	= cpu2le16(UniCmdSdoPktBudgetCtrlCfg.u2Tag);
	UniCmdSdoPktBudgetCtrlCfg.u2Length = cpu2le16(UniCmdSdoPktBudgetCtrlCfg.u2Length);
#endif /* CFG_BIG_ENDIAN */

	AndesAppendCmdMsg(msg, (char *)&UniCmdSdoPktBudgetCtrlCfg, sizeof(UniCmdSdoPktBudgetCtrlCfg));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_NOTICE,
			"(ret = %d)\n", ret);
	return ret;

}

INT32 UniCmdGetSdoBssAcqPktNum(
	IN struct _RTMP_ADAPTER *pAd,
	IN UINT_32 *au4BssMap
	)
{
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM_T UniCmdSdoGetBssAcqPktNum = {0};
	UINT32 u4ComCmdSize = 0, u4CmdNeedMaxBufSize = 0;
	UINT_32 au4SetBssMap[UNI_CMD_SDO_CFG_BSS_MAP_WORDLEN] = {0};
	INT32 ret = NDIS_STATUS_SUCCESS;
	UINT_8 idx;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&UniCmdSdoGetBssAcqPktNum, sizeof(UniCmdSdoGetBssAcqPktNum));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(UniCmdSdoGetBssAcqPktNum);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	UniCmdSdoGetBssAcqPktNum.u2Tag = UNI_CMD_SDO_GET_BSS_ACQ_PKT_NUM;
	UniCmdSdoGetBssAcqPktNum.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);

	for (idx = 0; idx < UNI_CMD_SDO_CFG_BSS_MAP_WORDLEN; idx++) {
		au4SetBssMap[idx] = au4BssMap ? au4BssMap[idx] : UNI_SDO_BSS_ACQ_PKT_NUM_BSS_GROUP_DEFAULT;
		UniCmdSdoGetBssAcqPktNum.au4BssMap[idx] = au4SetBssMap[idx];

#ifdef CFG_BIG_ENDIAN
		UniCmdSdoGetBssAcqPktNum.au4BssMap[idx] = cpu2le32(UniCmdSdoGetBssAcqPktNum.au4BssMap[idx]);
#endif /* CFG_BIG_ENDIAN */
	}

#ifdef CFG_BIG_ENDIAN
	UniCmdSdoGetBssAcqPktNum.u2Tag	= cpu2le16(UniCmdSdoGetBssAcqPktNum.u2Tag);
	UniCmdSdoGetBssAcqPktNum.u2Length = cpu2le16(UniCmdSdoGetBssAcqPktNum.u2Length);
#endif /* CFG_BIG_ENDIAN */

	AndesAppendCmdMsg(msg, (char *)&UniCmdSdoGetBssAcqPktNum, sizeof(UniCmdSdoGetBssAcqPktNum));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}

INT32 UniCmdSetSdoOverride(
	IN struct _RTMP_ADAPTER *pAd,
	IN struct mtk_mac_sdo_override *override)
{
#define SDO_OVERRIDE_VALUE_FROM_STAREC_CR 0
#define SDO_OVERRIDE_VALUE_FROM_HIF_TXD 1
	struct cmd_msg *msg = NULL;
	struct _CMD_ATTRIBUTE attr = {0};
	struct _UNI_CMD_SDO_T UniCmdSDO = {0};
	struct _UNI_CMD_SDO_OVERRIDE_CTRL_T cmd_set_sdo_override = {0};
	UINT32 u4ComCmdSize = 0;
	UINT32 u4CmdNeedMaxBufSize = 0;
	INT32 ret = NDIS_STATUS_SUCCESS;
	UINT8 i;

	if (UniCmdCheckInitReady(pAd) == FALSE) {
		MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"System isn't ready for in-band command now!!!\n");
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
				"dispath CMD start\n");

	u4ComCmdSize = sizeof(UniCmdSDO);
	os_zero_mem(&UniCmdSDO, u4ComCmdSize);
	os_zero_mem(&cmd_set_sdo_override, sizeof(cmd_set_sdo_override));

	/* Step 1: Count maximum buffer size from per TLV */
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(cmd_set_sdo_override);

	/* Step 2: Allocate memory space */
	msg = AndesAllocUniCmdMsg(pAd, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_SDO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(pAd);

	/* Step 3: Fill and append common parameters here */
	/* Nothing to do */
	AndesAppendCmdMsg(msg, (char *)&UniCmdSDO, u4ComCmdSize);

	/* Step 4: Fill and append TLV parameters here */
	cmd_set_sdo_override.u2Tag = UNI_CMD_SDO_OVERRIDE_CTRL;
	cmd_set_sdo_override.u2Length = (u4CmdNeedMaxBufSize - u4ComCmdSize);
#ifdef CFG_BIG_ENDIAN
	cmd_set_sdo_override.u2Tag	= cpu2le16(cmd_set_sdo_override.u2Tag);
	cmd_set_sdo_override.u2Length = cpu2le16(cmd_set_sdo_override.u2Length);
#endif /* CFG_BIG_ENDIAN */
	os_move_mem(&cmd_set_sdo_override.au4GlobalOverrideMacTxdMask[0], &override->global_mac_txd_mask[0],
		sizeof(UINT_32) * SDO_GLOBAL_OVERRIDE_MAX_DW);
	cmd_set_sdo_override.ucCtrl0Value = override->ctrl0_value_from_hif_txd;
	cmd_set_sdo_override.ucCtrl1Value = override->ctrl1_value_from_hif_txd;
	cmd_set_sdo_override.ucPerCrOverrideSettingEntryCount = override->per_cr_override_entry_cnt;
	cmd_set_sdo_override.ucPerStaOverrideSettingEntryCount = override->per_sta_override_entry_cnt;
	for (i = 0; i < override->per_cr_override_entry_cnt; i++) {
		cmd_set_sdo_override.rPerCrOverride[i].ucApplyIdx = override->per_cr_override[i].bytes_idx;
		cmd_set_sdo_override.rPerCrOverride[i].ucMask = override->per_cr_override[i].mask;
		cmd_set_sdo_override.rPerCrOverride[i].ucValue = override->per_cr_override[i].value;
		cmd_set_sdo_override.rPerCrOverride[i].ucCtrlIdx = override->per_cr_override[i].ctrl_idx;
		cmd_set_sdo_override.rPerCrOverride[i].ucApplyAll = override->per_cr_override[i].apply_all;
	}
	for (i = 0; i < override->per_sta_override_entry_cnt; i++) {
		cmd_set_sdo_override.rPerStaOverride[i].ucApplyIdx = override->per_sta_override[i].bytes_idx;
		cmd_set_sdo_override.rPerStaOverride[i].ucMask = override->per_sta_override[i].mask;
		cmd_set_sdo_override.rPerStaOverride[i].ucValue = override->per_sta_override[i].value;
		cmd_set_sdo_override.rPerStaOverride[i].ucCtrlIdx = override->per_sta_override[i].ctrl_idx;
		cmd_set_sdo_override.rPerStaOverride[i].ucApplyAll = override->per_sta_override[i].apply_all;
	}
	AndesAppendCmdMsg(msg, (char *)&cmd_set_sdo_override, (u4CmdNeedMaxBufSize - u4ComCmdSize));

	/* Step 5: Send out cmd */
	ret = chip_cmd_tx(pAd, msg);

	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_INFO,
			 "dispath CMD complete\n");
error:
	MTWF_DBG(pAd, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG,
			"(ret = %d)\n", ret);
	return ret;

}

static VOID uni_event_bss_acq_pkt_num_handler(
		struct _RTMP_ADAPTER *pAd,
		UINT8 *data,
		UINT32 length)
{
	struct UNI_EVENT_SDO_BSS_ACQ_PKT_NUM_T *prUniEventSdoBssAcqPktNum = (struct UNI_EVENT_SDO_BSS_ACQ_PKT_NUM_T *) data;
	struct UNI_EVENT_BSS_ACQ_PKT_NUM_T *prEventBssAcqPktNum = NULL;
	UINT8 i = 0;
	UINT32 sum[UNI_EVENT_SDO_NUM_OF_WMM_AC] = {0};
	struct UNI_EVENT_PER_BSS_ACQ_PKT_NUM_T *prPerBssInfo = NULL;
	UINT_16 thr;
	UINT_8 idx;

	if (!data)
		return;

	prEventBssAcqPktNum = &prUniEventSdoBssAcqPktNum->BssAcqPktNum;

#ifdef CFG_BIG_ENDIAN
	for (idx = 0; idx < UNI_EVENT_SDO_CFG_BSS_NUM; idx++)
		prEventBssAcqPktNum->au4BssMap[idx] = le2cpu32(prEventBssAcqPktNum->au4BssMap[idx]);
#endif

	MTWF_DBG(NULL, DBG_CAT_FW, CATFW_SDO, DBG_LVL_DEBUG, "BSSAcqOktNum:BssMap[0]=0x%08X\n",
		prEventBssAcqPktNum->au4BssMap[0]);

	for (i = 0; i < UNI_EVENT_SDO_CFG_BSS_NUM; i++) {
		idx = (i >> 5);
		if (!(prEventBssAcqPktNum->au4BssMap[idx] & (1 << (i & 0x1F))))
			continue;

		prPerBssInfo = &prEventBssAcqPktNum->bssPktInfo[i];
#ifdef CFG_BIG_ENDIAN
		prPerBssInfo->au4AcqPktCnt[WMM_AC_BK] = le2cpu32(prPerBssInfo->au4AcqPktCnt[WMM_AC_BK]);
		prPerBssInfo->au4AcqPktCnt[WMM_AC_BE] = le2cpu32(prPerBssInfo->au4AcqPktCnt[WMM_AC_BE]);
		prPerBssInfo->au4AcqPktCnt[WMM_AC_VI] = le2cpu32(prPerBssInfo->au4AcqPktCnt[WMM_AC_VI]);
		prPerBssInfo->au4AcqPktCnt[WMM_AC_VO] = le2cpu32(prPerBssInfo->au4AcqPktCnt[WMM_AC_VO]);
#endif
		sum[WMM_AC_BE] += prPerBssInfo->au4AcqPktCnt[WMM_AC_BE];
		sum[WMM_AC_BK] += prPerBssInfo->au4AcqPktCnt[WMM_AC_BK];
		sum[WMM_AC_VI] += prPerBssInfo->au4AcqPktCnt[WMM_AC_VI];
		sum[WMM_AC_VO] += prPerBssInfo->au4AcqPktCnt[WMM_AC_VO];
	}

	if (pAd->CommonCfg.wifi_cert)
		thr = ONE_SECOND_NON_BE_PACKETS_CERT_THRESHOLD;
	else
		thr = ONE_SECOND_NON_BE_PACKETS_THRESHOLD;

	for (i = 0; i < UNI_EVENT_SDO_NUM_OF_WMM_AC; i++) {
		if (sum[i] > thr)
			pAd->tx_one_second_ac_counter++;
	}

	mt_dynamic_wmm_be_tx_op(pAd, ONE_SECOND_NON_BE_PACKETS_THRESHOLD);
}

VOID UniEventSdoHandler(
	VOID *physical_dev,
	UINT8 *pData,
	UINT32 Length,
	EVENT_RXD *event_rxd)
{
	UINT32 tags_len;
	UINT8 *tag;
	UINT16 offset = 0;
	UINT32 fixed_len = sizeof(struct UNI_EVENT_SDO_T);
	UINT32 event_fail_loop_cnt = 0;
	struct _RTMP_ADAPTER *pAd = physical_device_get_first_mac_adapter(physical_dev);

	if (Length < fixed_len) {
		MTWF_DBG(NULL, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
			"%s: invalid length = %d\n",
			__func__, Length);
		return;
	}

	tags_len = Length - fixed_len;
	tag = pData + fixed_len;

	TAG_FOR_EACH(tag, tags_len, offset) {

		switch (TAG_ID(tag)) {
		case UNI_EVENT_SDO_BSS_ACQ_PKT_NUM:
			if (TAG_LEN(tag) != sizeof(struct UNI_EVENT_SDO_BSS_ACQ_PKT_NUM_T)) {
				MTWF_DBG(NULL, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
					"%s: tag: %d, invalid length = %d",
					__func__, TAG_ID(tag), TAG_LEN(tag));
				break;
			}
			uni_event_bss_acq_pkt_num_handler(pAd, tag, sizeof(struct UNI_EVENT_SDO_BSS_ACQ_PKT_NUM_T));
			break;

		default:
			event_fail_loop_cnt++;
			if (event_fail_loop_cnt > MAX_EVENT_FAIL_LOOP) {
				MTWF_DBG(NULL, DBG_CAT_FW, CATFW_SDO, DBG_LVL_ERROR,
				"%s: [BUG]: invalid event tag = %d, over max loop (%d) fail!\n",
				__func__, TAG_ID(tag), MAX_EVENT_FAIL_LOOP);
				return;
			}
			break;
		}
	}
}



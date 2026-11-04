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
#include "rro_cmd.h"

/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/

/* RRO command Tag */
enum RRO_DEBUG_CMD_TYPE {
	RRO_SET_OP = 9,
	RRO_QUERY_OP = 10,
	RRO_SET_FUNCTION = 11,
};

enum RRO_SET_OPTION {
	RRO_SET_OPTION_RRO_PPS = 0x0,
	RRO_SET_OPTION_NUM
};

enum RRO_QUERY_OPTION {
	RRO_QUERY_OPTION_PPS = 0x0,
	RRO_QUERY_OPTION_MIB = 0x1,
	RRO_QUERY_OPTION_MAX_NUM
};

enum RRO_SET_FUNCTION {
	RRO_SET_FUNC_IND_CMD_EMUL = BIT(0),
	RRO_SET_FUNC_RSS = BIT(1),
	RRO_SET_FUNC_CIDX_DRAM = BIT(2),
	RRO_SET_FUNC_DIDX_DRAM = BIT(3),
};

/* RRO command Tag */
enum UNI_CMD_ID_RRO_TAG_T {
	UNI_CMD_RRO_DELETE_ENTRY = 1,
	UNI_CMD_RRO_SET_PLATFORM_TYPE = 2,
	UNI_CMD_RRO_GET_BA_SESSION_TABLE  = 3,
	UNI_CMD_RRO_SET_BYPASS_MODE = 4,
	UNI_CMD_RRO_SET_TX_FREEDONE_PATH = 5,
	UNI_CMD_RRO_DEL_SESSION = 6,
	UNI_CMD_RRO_SET_FLUSH_TIMEOUT = 7,
	UNI_CMD_RRO_SET_RSS_MAP = 8,
	UNI_CMD_RRO_SET_OP = 9,
	UNI_CMD_RRO_QUERY_OP = 10,
	UIN_CMD_RRO_SET_FUNCTION = 11,
	UNI_CMD_RRO_NUM
};

/*******************************************************************************
 *    PRIVATE TYPES
 ******************************************************************************/
/** FW & DRV sync **/

/* RRO command (0x57) */
struct GNU_PACKED UNI_CMD_RRO_T
{
    /*fixed field*/
	UINT_8 aucPadding[4];
    /* tlv */
	UINT_8 aucTlvBuffer[0];
};

/* UNI_CMD_RRO_SET_FUNCTION_MODE (0x00) */
struct GNU_PACKED UNI_CMD_RRO_SET_FUNCTION_MODE_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8  ucFunctionMode;
	UINT_8  aucPadding[3];
};

/* UNI_CMD_RRO_DELETE_ENTRY (0x01) */
struct GNU_PACKED UNI_CMD_RRO_DELETE_ENTRY_T
{
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_16   u2WlanIdx;
	UINT_8    ucTid;
	UINT_8    aucPadding[1];
};

/**
 * This structure is used for UNI_CMD_RRO_SET_PLATFORM_TYPE (0x2) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag      should be 0x2
 * @param[in] u2Length   the length of this TLV, should be sizeof(UNI_CMD_RRO_SET_PLATFORM_TYPE_T)
 * @param[in] ucType     the ring sku type
 */
struct GNU_PACKED UNI_CMD_RRO_SET_PLATFORM_TYPE_T
{
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucType;
	UINT_8 aucPadding[3];
};

/**
 * This structure is used for UNI_CMD_RRO_SET_BYPASS_MODE (0x4) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag            should be 0x4
 * @param[in] u2Length         the length of this TLV,
 *                             should be sizeof(UNI_CMD_RRO_SET_BYPASS_MODE_T)
 * @param[in] u1RroBypassType  set rro bypass mode
 * @param[in] u1MldDestType    set mld dest
 * @param[in] aucPadding       reserved
 */
/* set rro bypass mode (Tag4) */
struct GNU_PACKED UNI_CMD_RRO_SET_BYPASS_MODE_T {
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 u1RroBypassType;
	UINT_8 u1MldDestType;
	UINT_8 aucPadding[2];
};

struct  GNU_PACKED UNI_CMD_RRO_DEL_SESSION_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_16   u2Seid;
	UINT_8    aucPadding[2];
};

/**
 * This structure is used for UNI_CMD_RRO_SET_TX_FREEDONE_PATH(0x5) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag      should be 0x5
 * @param[in] u2Length   the length of this TLV, should be sizeof(UNI_CMD_RRO_SET_TX_FREEDONE_PATH)
 * @param[in] ucPath     the tx free done event path
 */
 /* rro platform setting (Tag5) */
/**
 * Path definition:
 * Type 0: default: via HIF
 * Type 1: via WA
 */
struct GNU_PACKED UNI_CMD_RRO_SET_TX_FREEDONE_PATH_T {
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_8 ucPath;
	UINT_8 aucPadding[3];
};

/**
 * This structure is used for UNI_CMD_RRO_SET_FLUSH_TIMEOUT (0x7) of UNI_CMD_ID_RRO command (0x57)
 * to delete ba session.
 *
 * @param[in] u2Tag                   should be 0x7
 * @param[in] u2Length                the length of this TLV
 *                                    should be sizeof(UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T)
 * @param[in] ucStepOneTimeout        StepOneTimeout(ms)
 * @param[in] ucFlushAllTimeout       FlushAllTimeout(ms)
 * @param[in] aucPadding              Reserved for future(e.g. timerid, sessionid)
 */
/* rro set flush timeout (Tag7) */
struct GNU_PACKED UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T {
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_16 u2StepOneTimeout;
	UINT_16 u2FlushAllTimeout;
	UINT_8 aucPadding[4];
};

/**
 * This structure is used for UNI_CMD_RRO_SET_RSS_MAP(0x8) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag      should be 0x8
 * @param[in] u2Length   the length of this TLV, should be sizeof(UNI_CMD_RRO_SET_RSS_MAP_T)
 * @param[in] rss_map0   the value of RSS MAP 0
 * @param[in] rss_map1   the value of RSS MAP 1
 */
 /* rro RSS map setting (Tag8) */
struct GNU_PACKED UNI_CMD_RRO_SET_RSS_MAP_T {
	UINT_16 u2Tag;
	UINT_16 u2Length;
	UINT_32 rss_map0;
	UINT_32 rss_map1;
};

/**
 * This structure is used for UNI_CMD_RRO_SET_OP (0x9) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag                   should be 0x9
 * @param[in] u2Length                the length of this TLV, should be sizeof(UNI_CMD_RRO_SET_OP_T)
 * @param[in] u4RroSetArg0            Set parameter 0
 * @param[in] u4RroSetArg1            Set parameter 1
 * @param[in] u4RroSetArg2            Set parameter 2
 */
/* rro set operation (Tag9) */
struct UNI_CMD_RRO_SET_OP_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_32   u4RroSetArg0;
	UINT_32   u4RroSetArg1;
	UINT_32   u4RroSetArg2;
};

/**
 * This structure is used for UNI_CMD_RRO_SET_OP (0xa) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag                   should be 0xa
 * @param[in] u2Length                the length of this TLV, sizeof(UNI_CMD_RRO_QUERY_OP_T)
 * @param[in] u4RroQueryOptionArg0    Query parameter 0
 * @param[in] u4RroQueryOptionArg1    Query parameter 1
 * @param[in] u4RroQueryOptionArg2    Query parameter 2
 */
/* rro query operation (Tag10) */
struct UNI_CMD_RRO_QUERY_OP_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_32   u4RroQueryOptionArg0;
	UINT_32   u4RroQueryOptionArg1;
	UINT_32   u4RroQueryOptionArg2;
};

/**
 * This structure is used for UIN_CMD_RRO_SET_FUNCTION (0xb) of UNI_CMD_ID_RRO command (0x57)
 * to set RRO function mode
 *
 * @param[in] u2Tag                   should be 0xb
 * @param[in] u2Length                the length of this TLV, sizeof(UNI_CMD_RRO_SET_FUNCTION_T)
 * @param[in] u4RroFunctionCap0       Rro Function cap 0
 * @param[in] u4RroFunctionCap1       Rro Function cap 1
 */
/* rro query operation (Tag11) */
struct UNI_CMD_RRO_SET_FUNCTION_T {
	UINT_16   u2Tag;
	UINT_16   u2Length;
	UINT_32   u4RroFunctionCap0;
	UINT_32   u4RroFunctionCap1;
};

/*******************************************************************************
 *    PRIVATE FUNCTION PROTOTYPES
 ******************************************************************************/
/* For Command*/

/*******************************************************************************
 *    FUNCTIONS
 ******************************************************************************/
INT32 uni_cmd_rro_rx_path(
	struct _RTMP_ADAPTER *ad,
	UINT8 rx_path_type)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_PLATFORM_TYPE_T *ring_type;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_PLATFORM_TYPE_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);

	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_PLATFORM_TYPE_T */
	ring_type = (struct UNI_CMD_RRO_SET_PLATFORM_TYPE_T *) head;
	ring_type->u2Tag = UNI_CMD_RRO_SET_PLATFORM_TYPE;
	ring_type->u2Length = sizeof(struct UNI_CMD_RRO_SET_PLATFORM_TYPE_T);
	ring_type->ucType = rx_path_type;

#ifdef CFG_BIG_ENDIAN
	ring_type->u2Tag = cpu2le16(ring_type->u2Tag);
	ring_type->u2Length = cpu2le16(ring_type->u2Length);
#endif /* CFG_BIG_ENDIAN */

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_release_rro_setbl(
	struct _RTMP_ADAPTER *ad,
	UINT16 seid)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_DEL_SESSION_T *reinit_session;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_DEL_SESSION_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_BYPASS_MODE_T */
	reinit_session = (struct UNI_CMD_RRO_DEL_SESSION_T *)head;
	reinit_session->u2Tag = UNI_CMD_RRO_DEL_SESSION;
	reinit_session->u2Length = sizeof(struct UNI_CMD_RRO_DEL_SESSION_T);
	reinit_session->u2Seid = seid;

#ifdef CFG_BIG_ENDIAN
	reinit_session->u2Tag = cpu2le16(reinit_session->u2Tag);
	reinit_session->u2Length = cpu2le16(reinit_session->u2Length);
#endif /* CFG_BIG_ENDIAN */

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_bypass_mode(
	struct _RTMP_ADAPTER *ad,
	UINT8 rro_bypass_type)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_BYPASS_MODE_T *rro_bypass_mode;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_BYPASS_MODE_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_BYPASS_MODE_T */
	rro_bypass_mode = (struct UNI_CMD_RRO_SET_BYPASS_MODE_T *)head;
	rro_bypass_mode->u2Tag = UNI_CMD_RRO_SET_BYPASS_MODE;
	rro_bypass_mode->u2Length = sizeof(struct UNI_CMD_RRO_SET_BYPASS_MODE_T);
	rro_bypass_mode->u1RroBypassType = rro_bypass_type;

#ifdef CFG_BIG_ENDIAN
	rro_bypass_mode->u2Tag = cpu2le16(rro_bypass_mode->u2Tag);
	rro_bypass_mode->u2Length = cpu2le16(rro_bypass_mode->u2Length);
#endif /* CFG_BIG_ENDIAN */

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_tx_free_done_path(
	struct _RTMP_ADAPTER *ad,
	UINT8 tx_free_done_path)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_TX_FREEDONE_PATH_T *tx_free_done_path_cmd;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_TX_FREEDONE_PATH_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_BYPASS_MODE_T */
	tx_free_done_path_cmd = (struct UNI_CMD_RRO_SET_TX_FREEDONE_PATH_T *)head;
	tx_free_done_path_cmd->u2Tag = UNI_CMD_RRO_SET_TX_FREEDONE_PATH;
	tx_free_done_path_cmd->u2Length = sizeof(struct UNI_CMD_RRO_SET_TX_FREEDONE_PATH_T);
	tx_free_done_path_cmd->ucPath = tx_free_done_path;

#ifdef CFG_BIG_ENDIAN
	tx_free_done_path_cmd->u2Tag = cpu2le16(tx_free_done_path_cmd->u2Tag);
	tx_free_done_path_cmd->u2Length = cpu2le16(tx_free_done_path_cmd->u2Length);
#endif /* CFG_BIG_ENDIAN */

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_flush_timeout(
	struct _RTMP_ADAPTER *ad,
	UINT16 u2StepOneTimeout,
	UINT16 u2FlushAllTimeout)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T *rro_flush_timeout;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_BYPASS_MODE_T */
	rro_flush_timeout = (struct UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T *)head;
	rro_flush_timeout->u2Tag = UNI_CMD_RRO_SET_FLUSH_TIMEOUT;
	rro_flush_timeout->u2Tag = cpu2le16(rro_flush_timeout->u2Tag);
	rro_flush_timeout->u2Length = sizeof(struct UNI_CMD_RRO_SET_FLUSH_TIMEOUT_T);
	rro_flush_timeout->u2Length = cpu2le16(rro_flush_timeout->u2Length);
	rro_flush_timeout->u2StepOneTimeout = cpu2le16(u2StepOneTimeout);
	rro_flush_timeout->u2FlushAllTimeout = cpu2le16(u2FlushAllTimeout);

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_rss_map(
	struct _RTMP_ADAPTER *ad,
	UINT32 rss_map0,
	UINT32 rss_map1)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_RSS_MAP_T *rro_rss_map;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_RSS_MAP_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_BYPASS_MODE_T */
	rro_rss_map = (struct UNI_CMD_RRO_SET_RSS_MAP_T *)head;
	rro_rss_map->u2Tag = UNI_CMD_RRO_SET_RSS_MAP;
	rro_rss_map->u2Tag = cpu2le16(rro_rss_map->u2Tag);
	rro_rss_map->u2Length = sizeof(struct UNI_CMD_RRO_SET_RSS_MAP_T);
	rro_rss_map->u2Length = cpu2le16(rro_rss_map->u2Length);
	rro_rss_map->rss_map0 = rss_map0;
	rro_rss_map->rss_map1 = rss_map1;

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(struct UNI_EVENT_CMD_RESULT_T));
	SET_CMD_ATTR_RSP_HANDLER(attr, UniCmdResultRsp);

	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_set_op(
	IN struct _RTMP_ADAPTER *ad,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_OP_T *rro_set_op;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_OP_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_OP_T */
	rro_set_op = (struct UNI_CMD_RRO_SET_OP_T *)head;
	rro_set_op->u2Tag = UNI_CMD_RRO_SET_OP;
	rro_set_op->u2Tag = cpu2le16(rro_set_op->u2Tag);
	rro_set_op->u2Length = sizeof(struct UNI_CMD_RRO_SET_OP_T);
	rro_set_op->u2Length = cpu2le16(rro_set_op->u2Length);
	rro_set_op->u4RroSetArg0 = cpu2le32(arg0);
	rro_set_op->u4RroSetArg1 = cpu2le32(arg1);
	rro_set_op->u4RroSetArg2 = cpu2le32(arg2);
	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_qery_op(
	IN struct _RTMP_ADAPTER *ad,
	IN UINT32 arg0,
	IN UINT32 arg1,
	IN UINT32 arg2)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_QUERY_OP_T *rro_query_op;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_QUERY_OP_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_OP_T */
	rro_query_op = (struct UNI_CMD_RRO_QUERY_OP_T *)head;
	rro_query_op->u2Tag = UNI_CMD_RRO_QUERY_OP;
	rro_query_op->u2Tag = cpu2le16(rro_query_op->u2Tag);
	rro_query_op->u2Length = sizeof(struct UNI_CMD_RRO_QUERY_OP_T);
	rro_query_op->u2Length = cpu2le16(rro_query_op->u2Length);
	rro_query_op->u4RroQueryOptionArg0 = cpu2le32(arg0);
	rro_query_op->u4RroQueryOptionArg1 = cpu2le32(arg1);
	rro_query_op->u4RroQueryOptionArg2 = cpu2le32(arg2);
	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT32 uni_cmd_rro_set_function(
	IN struct _RTMP_ADAPTER *ad,
	IN UINT_32 u4RroFunctionCap0,
	IN UINT_32 u4RroFunctionCap1)
{
	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};
	UINT32 u4CmdNeedMaxBufSize = 0;
	UINT32 u4ComCmdSize = 0;
	UINT8 *tmp_buf = NULL;
	UINT8 *head = NULL;
	struct UNI_CMD_RRO_SET_FUNCTION_T *rro_set_func;

	/* Step 1: Count maximum buffer size from per TLV */
	u4ComCmdSize = sizeof(struct UNI_CMD_RRO_T);
	u4CmdNeedMaxBufSize = u4ComCmdSize + sizeof(struct UNI_CMD_RRO_SET_FUNCTION_T);

	/* Step 2: Allocate tempotary memory space for use later */
	os_alloc_mem(ad, (UCHAR **)&tmp_buf, u4CmdNeedMaxBufSize);
	if (!tmp_buf) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}
	os_zero_mem(tmp_buf, u4CmdNeedMaxBufSize);
	head = tmp_buf + u4ComCmdSize;

	/* Step 3: Filled in parameters of UNI_CMD_RRO_SET_OP_T */
	rro_set_func = (struct UNI_CMD_RRO_SET_FUNCTION_T *)head;
	rro_set_func->u2Tag = UIN_CMD_RRO_SET_FUNCTION;
	rro_set_func->u2Tag = cpu2le16(rro_set_func->u2Tag);
	rro_set_func->u2Length = sizeof(struct UNI_CMD_RRO_SET_FUNCTION_T);
	rro_set_func->u2Length = cpu2le16(rro_set_func->u2Length);
	rro_set_func->u4RroFunctionCap0 = cpu2le32(u4RroFunctionCap0);
	rro_set_func->u4RroFunctionCap1 = cpu2le32(u4RroFunctionCap1);

	/* Step 4: Send data packet */
	msg = AndesAllocUniCmdMsg(ad, u4CmdNeedMaxBufSize);
	if (!msg) {
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, UNI_CMD_ID_RRO);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_LEN_VAR_UNI_CMD_SET_AND_RETRY);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);
	AndesInitCmdMsg(msg, attr);
	msg->seq = AndesGetCmdMsgSeq(ad);

	/* Append this feature */
	AndesAppendCmdMsg(msg, (char *)tmp_buf, u4CmdNeedMaxBufSize);

	ret = chip_cmd_tx(ad, msg);

error:
	if (tmp_buf)
		os_free_mem(tmp_buf);

	MTWF_DBG(ad, DBG_CAT_FW, CATFW_DBGINFO, DBG_LVL_INFO,
			"(ret = %d)\n", ret);
	return ret;
}

INT set_rro_debug_proc(
	struct _RTMP_ADAPTER *pAd,
	RTMP_STRING *arg)
{
	UINT32 rro_debug_arg[10];
	UINT Idx = 0, ret = TRUE;
	RTMP_STRING *thisChar;

	while ((thisChar = strsep((char **)&arg, "-")) != NULL && Idx < 10) {
		rro_debug_arg[Idx] = os_str_tol(thisChar, 0, 10);
		Idx++;
	}

	if (Idx != 0) {
		if (rro_debug_arg[0] == RRO_SET_OP)
			uni_cmd_rro_set_op(pAd,
				rro_debug_arg[1], rro_debug_arg[2], rro_debug_arg[3]);
		else if (rro_debug_arg[0] == RRO_QUERY_OP)
			uni_cmd_rro_qery_op(pAd,
				rro_debug_arg[1], rro_debug_arg[2], rro_debug_arg[3]);
		else if (rro_debug_arg[0] == RRO_SET_FUNCTION)
			uni_cmd_rro_set_function(pAd,
				rro_debug_arg[1], rro_debug_arg[2]);
	} else {
		ret = FALSE;
	}

	MTWF_DBG(pAd, DBG_CAT_CFG, CATCFG_CMD, DBG_LVL_INFO,
						"RRO debug cmd=%d\n", rro_debug_arg[0]);

	return ret;
}



/***************************************************************************
 * MediaTek Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 1997-2025, MediaTek, Inc.
 *
 * All rights reserved. MediaTek source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attempt
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek Technology, Inc. is obtained.
 ***************************************************************************
 */

#ifndef __HWIFI_INFO_H__
#define __HWIFI_INFO_H__

/*
 * enum mtk_mac_info - The information from/to MAC
 *
 * @MAC_INFO_MCU: The MCU info
 * @MAC_INFO_BUS: The BUS info
 */
enum mtk_mac_info {
	MAC_INFO_MCU,
	MAC_INFO_BUS,
	MAC_INFO_END,
};

/* MCU related structure */
/*
 * enum mtk_mac_mcu_info_type - Indicate which query type want to use
 *
 * @MAC_MCU_INFO_FW: To know FW information
 */
enum mtk_mac_mcu_info_type {
	MAC_MCU_INFO_FW,
	MAC_MCU_INFO_END,
};

/*
 * struct mtk_mcu_fw_info - Store the MCU FW's info
 *
 * @mcu_type: Indicate which MCU need to query
 * @fw_ver: The FW version
 * @build_date: The FW build date
 * @fw_ver_long: The FW version
 */
struct mtk_mac_mcu_fw_info {
	int mcu_type;
	char *fw_ver;
	char *build_date;
	char *fw_ver_long;
};

/*
 * struct mtk_mac_bus_info - Get/Set the info from bus
 *
 * @type: Which MCU query type in used
 * @ring_info: Store the overview of fw info
 */
struct mtk_mac_mcu_info {
	enum mtk_mac_mcu_info_type type;
	struct mtk_mac_mcu_fw_info fw_info;
};

/* BUS related structure */

/*
 * enum mtk_mac_bus_info_type - Indicate which query type want to use
 *
 * @MAC_BUS_INFO_TX_DATA_NUM: To know how many Tx Data ring exist,
 *                          may prepare memory needed for query ring info
 * @MAC_BUS_INFO_TX_DATA_BY_WDMA_IDX: Query Tx Data ring info by WDMA idx
 */
enum mtk_mac_bus_info_type {
	MAC_BUS_INFO_TX_DATA_NUM,
	MAC_BUS_INFO_TX_DATA_BY_WDMA_IDX,
	MAC_BUS_INFO_END,
};

/*
 * struct mtk_mac_ring - Store the ring info
 *
 * @base: The DMAD base address
 * @max_cnt: The maximum ring size
 * @magic_cnt: Currently magic_cnt number
 * @cidx: CIDX value
 * @didx: DIDX value
 * @q_cnt: Calculate how many DMAD is unhandled yet
 */
struct mtk_mac_ring {
	dma_addr_t base;
	u32 max_cnt	: 16;
	u32 magic_cnt	: 16;
	u32 cidx;
	u32 didx;
	u32 q_cnt;
};

/*
 * struct mtk_mac_ring_info - Store the overview of ring info
 *
 * @ring: Store the each ring's info
 * @ring_num: Query how many ring exists or
 *            How many ring's info can be written
 */
struct mtk_mac_ring_info {
	struct mtk_mac_ring *ring;
	u32 ring_num;
};

/*
 * struct mtk_mac_bus_info - Get/Set the info from bus
 *
 * @type: Which bus query type in used
 * @ring_info: Store the overview of ring info
 * @wdma_idx: Used by MAC_BUS_INFO_TX_DATA_BY_WDMA_IDX
 */
struct mtk_mac_bus_info {
	enum mtk_mac_bus_info_type type;
	struct mtk_mac_ring_info ring_info;
	u32 wdma_idx;
};

#endif


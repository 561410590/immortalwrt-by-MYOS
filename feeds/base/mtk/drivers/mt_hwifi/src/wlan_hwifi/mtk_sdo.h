/* SPDX-License-Identifier: <SPDX License Expression> */
/*
 * Copyright (c) [2023] MediaTek Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ""AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MTK_SDO_H
#define __MTK_SDO_H

struct mtk_tk_entry;
struct mtk_hw_dev;

#define SDO_OVERRIDE_VALUE_FROM_STAREC_CR 0
#define SDO_OVERRIDE_VALUE_FROM_HIF_TXD 1

#define MAX_SDO_CACHE_ENTRY_CNT 512
#define MAX_SDO_OVERRIDE_CTRL 3
#define MAX_SDO_PATCH_ID 31
#define SDO_PATCH_ID_UNDEFINED 0xff

struct setting_entry {
	u8 bytes_idx;
	u32 mask;
	u32 value;
};

struct patch_entry {
	u8 patch_id;
	u32 condition_bitmap;
	u32 value_from;
	int (*fill_mac_txd)(struct mtk_hw_dev *dev, struct mtk_tk_entry *tk_entry,
		void *tx_pkt_info, u64 *hash);
	struct setting_entry *setting_table;
	u16 setting_table_sz;
	u8 per_cr_ctrl_map;
};

struct override_ctrl_entry {
	u32 value_from;
	u32 patch_bitmap;
};

struct cache_entry {
	u32 magic_cnt;
	u32 patch_bitmap;
	u64 hash_value;
	u16 wcid;
	u8 valid;
};

enum {
	SDO_OVERRIDE_CONDITION_APPLY_ALL = (1 << 0),
	SDO_OVERRIDE_CONDITION_A4_ENTRY = (1 << 1),
	SDO_OVERRIDE_CONDITION_IGMP_NONOFFLOAD = (1 << 2),
};

enum sdo_patch {
	SDO_PATCH_A4_ARP_AUTORATE,
	SDO_PATCH_IGMP_NONOFFLOAD,
	SDO_PATCH_MAX,
};

#define SDO_GLOBAL_OVERRIDE_MAX_DW 8
#define SDO_PERCR_OVERRIDE_MAX_ENTRY 6
#define SDO_PERSTA_OVERRIDE_MAX_ENTRY 5

struct sdo_override_entry {
	u8 bytes_idx;
	u8 mask;
	u8 value;
	u8 ctrl_idx;
	u8 apply_all;
	u8 value_from_hif_txd;
	u32 condition_map;
};

/** struct sdo_override_ge - override information for general sta
 *
 * This structure define override information(general) to pass information to upper driver
 *
 * @param per_cr_override_entry_cnt per_cr_override entry count
 * @param per_sta_override_entry_cnt per_sta_override entry count
 * @param ctrl0_value_from_hif_txd 0: ctrl0 value from starec/cr, 1: ctrl0 value from hif_txd
 * @param ctrl1_value_from_hif_txd 0: ctrl1 value from starec/cr, 1: ctrl1 value from hif_txd
 * @param global_mac_txd_mask legacy override information(mac txd mask)
 * @param per_cr_override per cr override information
 * @param per_sta_override per sta override information
 */
struct sdo_override_ge {
	u8 per_cr_override_entry_cnt;
	u8 per_sta_override_entry_cnt;
	u8 ctrl0_value_from_hif_txd;
	u8 ctrl1_value_from_hif_txd;
	u32 global_mac_txd_mask[SDO_GLOBAL_OVERRIDE_MAX_DW];
	struct sdo_override_entry per_cr_override[SDO_PERCR_OVERRIDE_MAX_ENTRY];
	struct sdo_override_entry per_sta_override[SDO_PERSTA_OVERRIDE_MAX_ENTRY];
};

/** struct sdo_sta_override - override information for specific sta
 *
 * This structure define override information(for specific sta) to pass information to upper driver
 *
 * @param per_sta_override_entry_cnt per_sta_override entry count
 * @param ctrl0_value_from_hif_txd 0: ctrl0 value from starec/cr, 1: ctrl0 value from hif_txd
 * @param ctrl1_value_from_hif_txd 0: ctrl1 value from starec/cr, 1: ctrl1 value from hif_txd
 * @param ctrl0 override ctrl0 bitmap
 * @param ctrl0 override ctrl1 bitmap
 * @param per_sta_override per sta override information
 */
struct sdo_override_specific {
	u8 per_sta_override_entry_cnt;
	u8 ctrl0_value_from_hif_txd;
	u8 ctrl1_value_from_hif_txd;
	u8 ctrl0;
	u8 ctrl1;
	struct sdo_override_entry per_sta_override[SDO_PERSTA_OVERRIDE_MAX_ENTRY];
};

/** struct sdo_cache_indicator - cache indicator
 *
 * This structure define cache indicator to check if cache is idenical
 *
 * @param patch_bitmap patch bitmap from apply override result
 * @param hash hash_value result from apply override result
 */
struct sdo_cache_indicator {
	u32 patch_bitmap;
	u64 hash;
};
#endif

/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_DIAG_H_
#define __MT_OSAL_DIAG_H_

#include "mt_osal_task.h"
#include "mt_osal_timer.h"

#define MT_DIAG_PROC_PATH_DEV0                  "ctcwifi"
#define MT_DIAG_VAR_PATH_DEV0                   "/var/ctcwifi/"
#define MT_DIAG_PROC_PATH_DEV1                  "ctcwifi_dev1"
#define MT_DIAG_VAR_PATH_DEV1                   "/var/ctcwifi_dev1/"
#define MT_DIAG_VAR_PATH_UNKNOWN                "/var/ctcwifi/unknown"
#define MT_DIAG_PROC_FILE_NUM                    5
#define MT_DIAG_VAR_FILE_NUM                     2
#define MT_DIAG_LOG_FILE_SIZE_MAX               (1024 * 1024) /* 1M bytes */
#define MT_DIAG_PROCESS_NUM_MAX			(8)
#define MT_DIAG_FRAME_SIZE_MAX                  (2048)
#define MT_DIAG_LOG_BUFF_SIZE                   (200) /*(1000)*/
#define MT_DIAG_ASSOC_ERROR_BUFF_SIZE           (100)
#define MT_DIAG_WRITE_FILE_PER_SECCOND           50

enum MT_ENUM_DIAG_BAND {
	MT_DIAG_BAND_2G = 0,
	MT_DIAG_BAND_5G,
	MT_DIAG_BAND_6G,
	MT_DIAG_BAND_MAX
};

enum MT_ENUM_DIAG_CONN_ERROR_CODE {
	MT_DIAG_CONN_FRAME_LOST = 0,
	MT_DIAG_CONN_CAP_ERROR,
	MT_DIAG_CONN_AUTH_FAIL,
	MT_DIAG_CONN_ACL_BLK,
	MT_DIAG_CONN_STA_LIM,
	MT_DIAG_CONN_DEAUTH,
	MT_DIAG_CONN_BAND_STE,
	MT_DIAG_CONN_ERROR_MAX,
	MT_DIAG_CONN_DEAUTH_COM
};

enum MT_ENUM_DIAG_ACTION {
	MT_DIAG_DROP_UNWANTED_CTRL_FRAME    = 0x1,
	MT_DIAG_DROP_RTS_CTRL_FRAME         = 0x2,
	MT_DIAG_DROP_CTS_CTRL_FRAME         = 0x4
};

enum mt_diag_cmd_attr_type {
	mt_diag_get_statistic = 1,
	mt_diag_get_ch_occ,
	mt_diag_set_drop_action,
	mt_diag_set_enable,
	mt_diag_get_entry_stat,
};

struct mt_diag_log_entry {
	struct mt_time_info tm_info;
	bool isTX;
	u8 ssid[33];
	u8 ssid_len;
	u8 band;
	u8 frame_type[32];
	u8 sta_addr[6];
	u8 data[MT_DIAG_FRAME_SIZE_MAX];
	u32 data_len;
};

struct mt_diag_frame_info {
	u8 isTX;
	u8 *ssid;
	u8 ssid_len;
	u8 band; /* 0:2G, 1:5G, get from pAd->LatchRfRegs.Channel */
	u8 *pData; /* include 80211 header, refer to pRxBlk (RX)/ pData (TX) */
	u32 dataLen; /* packet length */
};

struct mt_diag_wifi_process_entry {
	u8 name[MT_OS_TASK_NAME_LEN];
	u32 pid;
	u8 is_process;
};

struct mt_diag_wifi_process_info {
	u8 num;
	struct mt_diag_wifi_process_entry entry[MT_DIAG_PROCESS_NUM_MAX];
};

struct mt_diag_assoc_error_entry {
	struct mt_time_info tm_info;
	unsigned char StaAddr[6];
	unsigned char Ssid[32];
	enum MT_ENUM_DIAG_CONN_ERROR_CODE errCode;
	u32 reason;
};

struct mt_diag_ctrl {
	u8 device_idx;
	u8 init_flag;
	u8 channel;
	enum MT_ENUM_DIAG_BAND diag_band;
	u32 diag_duration;
	u32 diag_enable;
	u64 diag_enable_time;
	u32 diag_log_file_offset;
	struct mt_diag_log_entry *diag_log_entry;
	u32 diag_log_read_idx;
	u32 diag_log_write_idx;
	spinlock_t diag_log_lock;
	u32 assoc_error_file_offset;
	struct mt_diag_assoc_error_entry *assoc_error_entry;
	u32 assoc_error_read_idx;
	u32 assoc_error_write_idx;
	spinlock_t assoc_error_lock;
	struct net_device *net_dev;
};

struct mt_diag_statistic_info {
	unsigned long tx_count;
	unsigned long rx_count;
	unsigned long tx_fails;
	unsigned int rx_fcs_error_count;
};

struct mt_diag_ch_occ_info {
	unsigned int ch_occ;
	u8 channel;
};

enum MT_ENUM_DIAG_ENTRY_STATUS {
	MT_DIAG_ENTRY_NORMAL    = 1,
	MT_DIAG_ENTRY_SKIP,
	MT_DIAG_ENTRY_INVALID
};

struct mt_diag_entry_stat_info {
	u8 sta_addr[MT_MAC_ADDR_LEN];
	u32 total_tx_count;
	u32 total_tx_fail_count;
	u32 entry_idx;
	u32 entry_status;
	u8 snr;
	char rssi;
};

struct mt_diag_profile_setting {
	u8 no_profile;
	u8 diag_2G;
	u8 diag_5G;
};

struct mt_diag_ops {
	void (*diag_frame_cache)(
		u8 device_idx, u8 band_idx, struct mt_diag_frame_info *info);
	void (*diag_conn_error)(
		u8 device_idx,
		u8 band_idx,
		u8 *ssid,
		u32 ssid_len,
		u8 *addr,
		enum MT_ENUM_DIAG_CONN_ERROR_CODE DiagCode,
		u32 Reason);
	void (*diag_conn_error_write)(u8 device_idx, u8 band_idx);
	void (*diag_log_file_write)(u8 device_idx, u8 band_idx);
};

void mt_diag_proc_init(
	u8 device_idx,
	u8 band_idx,
	u8 max_band_num,
	struct net_device *net_dev,
	struct mt_diag_ops *ops);
void mt_diag_proc_exit(
	u8 device_idx, u8 band_idx, u8 b_last);

#endif /* __MT_OSAL_DIAG_H_ */

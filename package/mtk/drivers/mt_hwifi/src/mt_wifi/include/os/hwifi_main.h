#ifndef __HWIFI_MAIN_H__
#define __HWIFI_MAIN_H__

#include "os/hwifi_mac.h"

struct hdev_ctrl;
struct wifi_dev;
struct cmd_msg;
struct _CMD_ATTRIBUTE;
struct _MAC_TABLE_ENTRY;
struct radio_res;
struct freq_oper;
struct _MAC_TX_INFO;
struct _TX_BLK;
union _HTTRANSMIT_SETTING;
struct _EDCA_PARM;

#define WED_PAO_NUM_LEN_SET_TO_DEFAULT 0
#define WED_PAO_NUM_LEN_SKIP 1
#define WED_PAO_VLAN_SET_TO_DEFAULT -1
#define WED_PAO_HDRT_SET_TO_DEFAULT -1
#define WED_PAO_VLAN_SKIP 2
#define WED_PAO_HDRT_SKIP 2

struct _RTMP_CHIP_CAP *hwifi_get_chip_cap(struct _RTMP_ADAPTER *ad);
void hwifi_core_ops_register(struct hdev_ctrl *ctrl);
void hwifi_core_ops_unregister(struct hdev_ctrl *ctrl);
int hwifi_tx_data(struct _RTMP_ADAPTER *ad, struct _TX_BLK *tx_blk);
int hwifi_tx_mgmt(struct _RTMP_ADAPTER *pAd, UCHAR *tmac_info,
	struct _MAC_TX_INFO *info, union _HTTRANSMIT_SETTING *pTransmit, struct _TX_BLK *tx_blk);
int hwifi_ser_handler(struct _RTMP_ADAPTER *pAd, u32 action, u32 status);
int hwifi_dbg_info(struct _RTMP_ADAPTER *pAd, RTMP_STRING *arg);
void hwifi_set_edca(struct wifi_dev *wdev);
struct _EDCA_PARM *hwifi_get_edca(struct _RTMP_ADAPTER *ad, struct wifi_dev *wdev);
void hwifi_update_edca(struct _RTMP_ADAPTER *pAd, struct wifi_dev *wdev, struct _EDCA_PARM *pEdca, bool reset);
u16 hwifi_get_inf_num(struct _RTMP_ADAPTER *ad);
void hwifi_get_tx_token_num(struct _RTMP_ADAPTER *ad, u16 tx_token_num[], u8 max_src_num);
void hwifi_get_rro_sp_page_num(struct _RTMP_ADAPTER *ad, u32 *page_num);
void hwifi_get_fw_info(struct _RTMP_ADAPTER *pAd, int mcu_type, char *fw_ver, char *build_date, char *fw_ver_long);
void hwifi_show_info(void);
void hwifi_update_hw_cap(void *ph_dev_obj, unsigned long hw_flags, void *mac_cap_info);
BOOLEAN hwifi_get_wdma_related_tx_bus_info(u32 wdma_idx);

void hwifi_pcie_read32(
	void *ph_dev_obj, u32 reg, u32 *val);
void hwifi_pcie_write32(
	void *ph_dev_obj, u32 reg, u32 val);
VOID hwifi_pcie_map_io_read32(
	void *ph_dev_obj, UINT32 reg, UINT32 *val);
VOID hwifi_pcie_map_io_write32(
	void *ph_dev_obj, UINT32 reg, UINT32 val);
void hwifi_show_mac_cap(struct _RTMP_ADAPTER *ad);
int hwifi_init_hw_ring_setting(struct _RTMP_ADAPTER *ad);
int hwifi_init_sdo_override(struct _RTMP_ADAPTER *ad);
#endif

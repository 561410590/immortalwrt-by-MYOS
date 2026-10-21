/* SPDX-License-Identifier: ISC */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __HWIFI_MULDF_H__
#define __HWIFI_MULDF_H__


#include <linux/soc/mediatek/muldf.h>


#ifndef MULDF_MODULE_NAME
#define MULDF_MODULE_NAME			"hwifi"
#endif

/* MULDF Debug Category for hwifi */
#define MULDF_CAT_FW_CMD_TX		"fw_cmd_tx_path"
#define MULDF_CAT_FW_EVENT_RX		"fw_event_rx_path"
#define MULDF_CAT_MTWF_HWIFI		"mtwf_hwifi"

/* Sub-Category of  MULDF_CAT_FW_CMD_TX */
/* NULL */

/* Sub-Category of  MULDF_CAT_FW_EVENT_RX */
#define MULDF_SCAT_FW_EVENT_ALL		"all"
#define MULDF_SCAT_FW_EVENT_WA		"wa"
#define MULDF_SCAT_FW_EVENT_WM		"wm"
#define MULDF_SCAT_FW_EVENT_TX_FREE	"tx_free"
#define MULDF_SCAT_FW_EVENT_SDO		"sdo"
#define MULDF_SCAT_FW_EVENT_TXS		"txs"

/* Sub-Category of  MULDF_CAT_MTWF_HWIFI */
/* NULL */

void muldf_init(void);

/* muldf print related definitions */
#define MULDF_PRINT(level, ctg, sub_ctg, tag, ...)	\
	muldf_print(level, MULDF_MODULE_NAME, ctg, sub_ctg, tag, __func__, __LINE__, __VA_ARGS__)

#define MULDF_PRINT_HEX(level, ctg, sub_ctg, tag, pkt, len, title)	\
	muldf_print_hex(level, MULDF_MODULE_NAME, ctg, sub_ctg, tag, (pkt), (len), title)

#define MULDF_PRINT_PKT(level, ctg, sub_ctg, tag, pkt, len, ...)	\
	muldf_print_pkt(level, MULDF_MODULE_NAME, ctg, sub_ctg, tag, (pkt), (len), __VA_ARGS__)

#define MULDF_PRINT_ICS(pkt, len)	\
	muldf_xmit_ics((pkt), (len))

#define MULDF_PRINT_FWLOG(pkt, len)	\
	muldf_xmit_fwlog((pkt), (len))

#define MULDF_PRINT_EVENT(id)	\
	muldf_xmit_event((id))

/*redefine dev_emerg*/
#undef dev_emerg
#define dev_emerg(dev, fmt, ...)	\
	do {	\
		_dev_emerg(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_ERROR, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_crit*/
#undef dev_crit
#define dev_crit(dev, fmt, ...)	\
	do {	\
		_dev_crit(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_ERROR, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_alert*/
#undef dev_alert
#define dev_alert(dev, fmt, ...)	\
	do {	\
		_dev_alert(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_ERROR, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_err*/
#undef dev_err
#define dev_err(dev, fmt, ...)	\
	do {	\
		_dev_err(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_ERROR, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_warn*/
#undef dev_warn
#define dev_warn(dev, fmt, ...)	\
	do {	\
		_dev_warn(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_WARNING, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_notice*/
#undef dev_notice
#define dev_notice(dev, fmt, ...)	\
	do {	\
		_dev_notice(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_WARNING, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)

/*redefine dev_info*/
#undef dev_info
#define dev_info(dev, fmt, ...) \
	do {	\
		_dev_info(dev, dev_fmt(fmt), ##__VA_ARGS__);	\
		muldf_print(MTK_MSG_INFO, MULDF_MODULE_NAME, MULDF_CAT_MTWF_HWIFI,	\
			NULL, NULL, NULL, 0, dev_fmt(fmt), ##__VA_ARGS__); \
	} while (0)


#endif /* __HWIFI_MULDF_H__ */

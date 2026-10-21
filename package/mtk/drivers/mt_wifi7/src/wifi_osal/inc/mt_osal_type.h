/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_TYPE_H_
#define __MT_OSAL_TYPE_H_

#ifndef NULL
#define NULL			0
#endif

#define MT_MACSTR "%02x:**:**:%02x:%02x:%02x"
#define MT_MAC2STR(addr) (addr)[0], (addr)[3], (addr)[4], (addr)[5]
#define MT_MAC_ADDR_LEN 6

extern unsigned char mt_zero_mac_addr[];

#endif /* __MT_OSAL_TYPE_H_ */

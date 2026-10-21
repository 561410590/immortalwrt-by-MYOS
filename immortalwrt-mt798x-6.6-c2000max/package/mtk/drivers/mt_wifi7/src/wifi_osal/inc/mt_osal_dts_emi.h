/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#ifndef __MT_OSAL_DTS_EMI_H_
#define __MT_OSAL_DTS_EMI_H_

#define WMCPU_EMI_DEV_NODE "mediatek,wmcpu-reserved"
#define WMCPU_EMI_DRIVER_NAME "wmcpu-reserved"

int mt_os_wm_emi_get_resource(unsigned long *phy_addr, void **vir_addr, size_t *len);

int mt_os_wm_emi_erase(void);

int mt_os_wm_emi_init(bool erase);

void mt_os_wm_emi_deinit(void);

#endif /* __MT_OSAL_DTS_EMI_H_ */

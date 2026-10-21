/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_BUS_H_
#define __MT_OSAL_BUS_H_

void mt_pci_walk_bus(struct pci_bus *top,
	int (*cb)(struct pci_dev *, void *), void *userdata);

#endif /* __MT_OSAL_BUS_H_ */

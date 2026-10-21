/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/pci.h>

#define SUBMODULE "mt_osal_bus"
#include <mt_osal_debug.h>
#include <mt_osal_bus.h>
#include <mt_osal_module.h>

void mt_pci_walk_bus(struct pci_bus *top,
                int (*cb)(struct pci_dev *, void *), void *userdata)
{
	pci_walk_bus(top, cb, userdata);
}
EXPORT_SYMBOL(mt_pci_walk_bus);


DECLARE_SUB_MODULE_INIT(mt_osal_bus_init)
{
	int status = 0;

	MT_DEBUG_TRACE("loaded");

	return status;
}
sub_module_init(mt_osal_bus_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_bus_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_bus_exit);

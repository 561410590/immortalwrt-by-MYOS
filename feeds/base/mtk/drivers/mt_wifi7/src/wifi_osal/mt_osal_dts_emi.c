/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#define SUBMODULE "mt_osal_dts_emi"
#include <mt_osal_debug.h>
#include <mt_osal_dts_emi.h>
#include <mt_osal_module.h>

static void __iomem *rsv_mem_base = NULL;
static struct resource *rsv_mem_res = NULL;

int mt_os_wm_emi_erase(void)
{
	/*  Use memset_io to initialize the reserved ioremap memory */
	if (rsv_mem_base) {
		phys_addr_t phy = rsv_mem_res->start;
		memset_io(rsv_mem_base, 0x0,  (size_t)resource_size(rsv_mem_res));

		pr_info(WMCPU_EMI_DRIVER_NAME ": erase: phy(%pa), vir(%px-%px)(size=0x%zx)\n",
			&phy, rsv_mem_base, rsv_mem_base + (size_t)resource_size(rsv_mem_res),
			(size_t)resource_size(rsv_mem_res));
		return 0;
	} else
	    return -ENOMEM;
}
EXPORT_SYMBOL(mt_os_wm_emi_erase);


int mt_os_wm_emi_get_resource(unsigned long *phy_addr, void **vir_addr, size_t *len)
{
	if (rsv_mem_res) {
		(*phy_addr) = (unsigned long)rsv_mem_res->start;
		(*vir_addr) = rsv_mem_base;
		(*len) = (size_t)resource_size(rsv_mem_res);
		return 0;
	} else
	    return -ENOMEM;
}
EXPORT_SYMBOL(mt_os_wm_emi_get_resource);

int mt_os_wm_emi_init(bool erase)
{
	struct device_node *np = NULL;
	struct resource res;

	/* Find the reserved memory node */
	np = of_find_compatible_node(NULL, NULL, WMCPU_EMI_DEV_NODE);

	if (!np) {
	    pr_err(WMCPU_EMI_DRIVER_NAME ": could not find reserved memory node\n");
	    return -ENODEV;
	}

	/* Get the reserved memory resource */
	if (of_address_to_resource(np, 0, &res)) {
	    pr_err(WMCPU_EMI_DRIVER_NAME ": could not get reserved memory resource\n");
	    return -ENODEV;
	}

	/* Request the reserved memory region */
	rsv_mem_res = request_mem_region(res.start, (size_t)resource_size(&res),
	                      WMCPU_EMI_DRIVER_NAME);
	if (!rsv_mem_res) {
	    pr_err(WMCPU_EMI_DRIVER_NAME ": could not request memory region\n");
	    return -EBUSY;
	}

	/* Map the reserved memory region */
	rsv_mem_base = ioremap(res.start, resource_size(&res));

	if (!rsv_mem_base) {
	    pr_err(WMCPU_EMI_DRIVER_NAME ": could not map reserved memory region\n");
	    release_mem_region(res.start, resource_size(&res));
	    return -ENOMEM;
	}

	if (erase)
		/*  Use memset_io to initialize the reserved ioremap memory */
		mt_os_wm_emi_erase();

	return 0;
}
EXPORT_SYMBOL(mt_os_wm_emi_init);


void mt_os_wm_emi_deinit(void)
{
	if (rsv_mem_base)
	    iounmap(rsv_mem_base);

	if (rsv_mem_res)
	    release_mem_region(rsv_mem_res->start, (size_t)resource_size(rsv_mem_res));
}
EXPORT_SYMBOL(mt_os_wm_emi_deinit);


DECLARE_SUB_MODULE_INIT(mt_osal_dts_emi_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_dts_emi_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_dts_emi_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_dts_emi_exit);

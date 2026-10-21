// SPDX-License-Identifier: <SPDX License Expression>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include "mt7993.h"
#include "mac_if.h"
#include "core.h"
#include "chips/bmac.h"

/*#include "firmware/mt799b_e2p_ePAeLNA.h"*/

/*TBD: not ready*/
static int mac_type = MTK_INTERFACE_CONNAC;
module_param(mac_type, int, 0644);
MODULE_PARM_DESC(mac_type, "mac driver type");

#define MT799B_CHIP_ID 0x799B

static void
dump_chip_info(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	u32 chipid = MT799B_CHIP_ID;

	dev_info(to_device(trans), "%s(): chip id: %x\n",
		__func__, chipid);
}

static void
_mt799b_pdma_msi_int_config(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	struct mtk_hw_dev *dev = to_hw_dev(trans);
	u32 msi_cfg[4];
	int nv = 0;
	u8 ring_id;

	if (dev->irq_type == PCI_IRQ_LEGACY)
		return;

	msi_cfg[0] = bus_read(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR);
	msi_cfg[1] = bus_read(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR);
	msi_cfg[2] = bus_read(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR);
	msi_cfg[3] = bus_read(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR);

	for (; nv < dev->vec_num; nv++) {
		union attr_info info;
		u8 r_num = 0;
		u32 r_id = 0;

		if (pci_hw->vec_data[nv].attr == VEC_ATTR_LUMP)
			continue;
		if (pci_hw->vec_data[nv].attr == VEC_ATTR_RRO_RSS)
			continue;

		r_num = pci_hw->vec_data[nv].ring_num;
		r_id = pci_hw->vec_data[nv].ring_id;
		info = pci_hw->vec_data[nv].info;
		do {
			ring_id = ffs(r_id)-1;
			r_id &= r_id-1;

			if (pci_hw->vec_data[nv].attr == VEC_ATTR_RX_TFD_RING) {
				if (info.band & BIT(BAND0)) {
					msi_cfg[0] &= ~WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN0_TX_FREE_DONE_RING_IDX_MASK;
					msi_cfg[0] |= (ring_id << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN0_TX_FREE_DONE_RING_IDX_SHFT);
				} else {
					msi_cfg[0] &= ~WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN1_TX_FREE_DONE_RING_IDX_MASK;
					msi_cfg[0] |= (ring_id << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_AP_BN1_TX_FREE_DONE_RING_IDX_SHFT);
				}
				info.band &= info.band-1;
				continue;
			}

			if (pci_hw->vec_data[nv].attr == VEC_ATTR_TXD_RING) {
				if (info.band & BIT(BAND0)) {
					msi_cfg[1] &= ~(WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MIN_MASK |
							WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MAX_MASK);
					msi_cfg[1] |= ((ring_id & 0xf) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MIN_SHFT |
							(ring_id & 0xf) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN0_TX_RING_IDX_MAX_SHFT);
					msi_cfg[3] &= ~(WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MIN_MASK |
							WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MAX_MASK);
					msi_cfg[3] |= ((ring_id >> 4) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MIN_SHFT |
							(ring_id >> 4) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_TX_RING_IDX_MAX_SHFT);
				} else {
					msi_cfg[1] &= ~(WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MIN_MASK |
							WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MAX_MASK);
					msi_cfg[1] |= ((ring_id & 0xf) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MIN_SHFT |
							(ring_id & 0xf) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_AP_BN1_TX_RING_IDX_MAX_SHFT);
					msi_cfg[3] &= ~(WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MIN_MASK |
							WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MAX_MASK);
					msi_cfg[3] |= ((ring_id >> 4) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MIN_SHFT |
							(ring_id >> 4) << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_TX_RING_IDX_MAX_SHFT);
				}
				info.band &= info.band-1;
				continue;
			}

			if (pci_hw->vec_data[nv].attr == VEC_ATTR_RXD_RING) {
				if (info.band & BIT(BAND0)) {
					msi_cfg[3] &= ~WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_RX_RING_IDX_MASK;
					msi_cfg[3] |= (ring_id << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN0_RX_RING_IDX_SHFT);
				} else {
					msi_cfg[3] &= ~WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_RX_RING_IDX_MASK;
					msi_cfg[3] |= (ring_id << WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_AP_BN1_RX_RING_IDX_SHFT);
				}
				info.band &= info.band-1;
				continue;
			}
		} while (--r_num);
	}
	bus_write(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG0_ADDR, msi_cfg[0]);
	bus_write(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG1_ADDR, msi_cfg[1]);
	bus_write(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG2_ADDR, msi_cfg[2]);
	bus_write(trans, WF_WFDMA_EXT_WRAP_CSR_MSI_INT_CFG3_ADDR, msi_cfg[3]);
}

static void
_mt799b_pdma_irq_priority_select(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	struct mtk_hw_dev *hw_dev = to_hw_dev(trans);
	u32 tx_pri_sel = 0, rx_pri_sel = 0;
	int nv = 0;

	for (; nv < hw_dev->vec_num; nv++) {
		if (pci_hw->vec_data[nv].attr == VEC_ATTR_LUMP ||
		    pci_hw->vec_data[nv].attr == VEC_ATTR_RRO_RSS) {
			continue;
		}
		if (pci_hw->vec_data[nv].attr == VEC_ATTR_TXD_RING ||
		    pci_hw->vec_data[nv].attr == VEC_ATTR_TX_CMD_RING ||
		    pci_hw->vec_data[nv].attr == VEC_ATTR_TX_FWDL_RING) {
			tx_pri_sel |= pci_hw->vec_data[nv].ring_id;
		}
		if (pci_hw->vec_data[nv].attr == VEC_ATTR_RXD_RING ||
		    pci_hw->vec_data[nv].attr == VEC_ATTR_RX_TFD_RING ||
		    pci_hw->vec_data[nv].attr == VEC_ATTR_RX_EVT_RING) {
			rx_pri_sel |= pci_hw->vec_data[nv].ring_id;
		}
	}

	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_INT_TX_PRI_SEL_ADDR,
		  tx_pri_sel);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_INT_RX_PRI_SEL_ADDR,
		  rx_pri_sel);
}

static void
_mt799b_pdma_irq_clear_int_sta(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	unsigned long flags;

	spin_lock_irqsave(&pci_hw->irq_lock, flags);
	bus_write(trans, pci_hw->mcu2host_sw_int_sta_addr, 0xffffffff);
	bus_write(trans, pci_hw->int_mask_map[0].int_sts_addr, 0xffffffff);
	bus_write(trans, pci_hw->int_mask_map[1].int_sts_addr, 0xffffffff);
	spin_unlock_irqrestore(&pci_hw->irq_lock, flags);
}

static void
_mt799b_pdma_irq_init(struct pci_trans *pci_hw)
{
	_mt799b_pdma_msi_int_config(pci_hw);
	_mt799b_pdma_irq_priority_select(pci_hw);
	_mt799b_pdma_irq_clear_int_sta(pci_hw);
}

static void
_mt799b_pdma_irq_disable(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	unsigned long flags;

	spin_lock_irqsave(&pci_hw->irq_lock, flags);
	bus_write(trans, pci_hw->mcu2host_sw_int_ena_addr, 0);
	bus_write(trans, pci_hw->int_mask_map[0].int_ena_addr, 0);
	bus_write(trans, pci_hw->int_mask_map[1].int_ena_addr, 0);
	spin_unlock_irqrestore(&pci_hw->irq_lock, flags);
	clear_bit(PCI_STATE_ISR_START, &pci_hw->state);
}

static void
_mt799b_pdma_irq_enable(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	unsigned long flags;

	spin_lock_irqsave(&pci_hw->irq_lock, flags);
	bus_write(trans, pci_hw->int_mask_map[0].int_ena_addr,
			pci_hw->int_mask_map[0].int_ena_mask);
	bus_write(trans, pci_hw->int_mask_map[1].int_ena_addr,
			pci_hw->int_mask_map[1].int_ena_mask);
	bus_write(trans, pci_hw->mcu2host_sw_int_ena_addr,
			pci_hw->mcu2host_sw_int_ser_mask);
	spin_unlock_irqrestore(&pci_hw->irq_lock, flags);
	clear_bit(PCI_STATE_ISR_PEDING, &pci_hw->state);
	set_bit(PCI_STATE_ISR_START, &pci_hw->state);
}

static void
_mt799b_pdma_config(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	struct mtk_hw_dev *dev = to_hw_dev(trans);
	u32 host_cfg;

	/* GLO_CFG */
	bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_OMIT_TX_INFO_MASK |
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_PDMA_ADDR_EXT_EN_MASK);
	bus_clear(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_OMIT_RX_INFO_MASK);

	/* GLO_CFG2 */
	/* Clear the bit to allow ring's ATTR mask to work */
	bus_clear(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG2_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG2_TRX_2ND_SRC_BLK_MASK);

	/* GLO_CFG_EXT0 */
	bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_EXT0_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_EXT0_CSR_RX_WB_RXD_MASK |
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_EXT0_CSR_WED_MERGE_MODE_MASK);

	/* GLO_CFG_EXT1 */
	bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_EXT1_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_EXT1_CSR_TX_FCTRL_MODE_MASK);

	/* WRAP_CSR_HOST_CONFIG */
	bus_set(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR,
		WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_pdma_per_band_MASK);
	bus_clear(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR,
		WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_bn0_pcie_sel_MASK |
		WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_bn1_pcie_sel_MASK);

	if (dev->chip_opt == MT7993_CHIP_OPT_PCIE0_5G_PCIE1_2G) {
		/* Set BN0 payload transfer through PCIE1 */
		bus_set(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR,
			WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_bn0_pcie_sel_MASK);
	} else {
		/* Set BN1 payload transfer through PCIE1 */
		bus_set(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR,
			WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_bn1_pcie_sel_MASK);
	}

	if (dev->irq_type == PCI_IRQ_MSI) {
		host_cfg = bus_read(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR);
		host_cfg &= CLEAR_PCIE1_MSI_NUM();
		host_cfg |= SET_PCIE1_MSI_NUM(ffs(dev->vec_num) - 1);

		bus_write(trans, WF_WFDMA_EXT_WRAP_CSR_WFDMA_HOST_CONFIG_ADDR, host_cfg);
	}
}

static void
_mt799b_pdma_interrupt_select(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

#define WF_WFDMA_HOST_DMA0_PCIE1_HOST_RX_INT_PCIE_SEL_ADDR_RX_RING_9_MASK BIT(9)

	if (test_bit(PCI_FLAG_WHNAT_RX, &pci_hw->flags)) {
		/* Redirect RxRing9 interrupt to PCIE0 */
		bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_HOST_RX_INT_PCIE_SEL_ADDR,
			WF_WFDMA_HOST_DMA0_PCIE1_HOST_RX_INT_PCIE_SEL_ADDR_RX_RING_9_MASK);
	}
}

static void
_mt799b_pdma_rx_threshold(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

	/* RRO related, pause PP if lower than DMAD threshold */
	/* WFDMA */
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RX_Q_TH54_ADDR,
		  0xc000c);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RX_Q_TH76_ADDR,
		  0x10008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RX_Q_TH98_ADDR,
		  0x10008);

	/* RRO */
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RRO_Q_ADDR,
		  0x20);

	bus_set_field(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RRO_Q_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_PAUSE_RRO_Q_RRO_PAUSE_RIDX_EN_MASK, 0x330);
}

static void
_mt799b_pdma_prefetch_config(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

	/* Rx */
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING0_EXT_CTRL_ADDR, 0x00000004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING1_EXT_CTRL_ADDR, 0x00400004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING2_EXT_CTRL_ADDR, 0x00800004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING3_EXT_CTRL_ADDR, 0x00C00004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING4_EXT_CTRL_ADDR, 0x01000010);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING5_EXT_CTRL_ADDR, 0x02000010);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING6_EXT_CTRL_ADDR, 0x03000008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING7_EXT_CTRL_ADDR, 0x03800008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING8_EXT_CTRL_ADDR, 0x04000010);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING9_EXT_CTRL_ADDR, 0x05000010);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING10_EXT_CTRL_ADDR, 0x06000008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING11_EXT_CTRL_ADDR, 0x06800008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RX_RING12_EXT_CTRL_ADDR, 0x07000008);

	/* Tx */
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING0_EXT_CTRL_ADDR, 0x07800004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING1_EXT_CTRL_ADDR, 0x07C00004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING2_EXT_CTRL_ADDR, 0x08000004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING3_EXT_CTRL_ADDR, 0x08400004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING4_EXT_CTRL_ADDR, 0x08800004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING5_EXT_CTRL_ADDR, 0x08C00004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING6_EXT_CTRL_ADDR, 0x09000004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING16_EXT_CTRL_ADDR, 0x09400004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING17_EXT_CTRL_ADDR, 0x09800004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING18_EXT_CTRL_ADDR, 0x09C00008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING19_EXT_CTRL_ADDR, 0x0A400008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING20_EXT_CTRL_ADDR, 0x0AC00004);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING21_EXT_CTRL_ADDR, 0x0B000008);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TX_RING22_EXT_CTRL_ADDR, 0x0B800008);
}

static void
_mt799b_pdma_reset_didx(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RST_DTX_PTR_ADDR, 0xffffffff);
	bus_write(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_RST_DRX_PTR_ADDR, 0xffffffff);
}

static void
_mt799b_pdma_dmy(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	struct pci_trans *master_ptrans = to_pci_trans(trans->master);
	struct hw_rro_cfg *rro_cfg = master_ptrans->rro_cfg;

	if (!rro_cfg->dmy_va)
		return;

	/* dmy read configure */
	bus_set_field(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL2_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL2_DMY_RD_BASE_PTR_MASK,
		rro_cfg->dmy_pa);
#ifdef CONFIG_HWIFI_64BIT_SUPPORT
	bus_set_field(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL3_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL3_DMY_RD_BASE_PTR_EXT_MASK,
		rro_cfg->dmy_pa >> DMA_ADDR_H_SHIFT);
#endif

	/* dmy read/write select */
	bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL3_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_TRINFO_WB_DMY_CTRL3_RRO_DMY_RD_WR_SEL_MASK);
}

static void
_mt799b_pdma_init(struct pci_trans *pci_hw)
{
	_mt799b_pdma_config(pci_hw);
	_mt799b_pdma_interrupt_select(pci_hw);
	_mt799b_pdma_rx_threshold(pci_hw);
	_mt799b_pdma_prefetch_config(pci_hw);
	_mt799b_pdma_reset_didx(pci_hw);
	_mt799b_pdma_dmy(pci_hw);
}

static void
_mt799b_pdma_unpause(struct pci_trans *pci_hw)
{
	bus_write(to_bus_trans(pci_hw), pci_hw->pause_trxq_reg_addr, 0);
}

static void
_mt799b_pdma_pause(struct pci_trans *pci_hw)
{
}

static bool
_mt799b_pdma_poll(struct pci_trans *pci_hw, u32 round, u32 wait_us)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);
	u32 val;
	u8 i = 0;
	bool idle = false;

	do {
		val = bus_read(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_ADDR);
		if ((val & (WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_TX_DMA_BUSY_MASK |
			WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_RX_DMA_BUSY_MASK)) == 0) {
			idle = true;
			break;
		}

		udelay(wait_us);
	} while ((i++) < round);

	return !idle;
}

static void
_mt799b_pdma_enable(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

	bus_set(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_ADDR,
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_TX_DMA_EN_MASK |
		WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_RX_DMA_EN_MASK);
}

static void
_mt799b_pdma_disable(struct pci_trans *pci_hw)
{
	struct mtk_bus_trans *trans = to_bus_trans(pci_hw);

	bus_clear(trans, WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_ADDR,
			WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_TX_DMA_EN_MASK |
			WF_WFDMA_HOST_DMA0_PCIE1_WPDMA_GLO_CFG_RX_DMA_EN_MASK);
}

static int
mt799b_pdma_enable(struct pci_trans *trans)
{
	_mt799b_pdma_irq_enable(trans);
	_mt799b_pdma_unpause(trans);
	_mt799b_pdma_enable(trans);

	return 0;
}

static int
mt799b_pdma_disable(struct pci_trans *trans)
{
#define PDMA_POLL_ROUND 100
#define PDMA_POLL_US 1000
	int ret;

	_mt799b_pdma_disable(trans);
	_mt799b_pdma_pause(trans);
	ret = _mt799b_pdma_poll(trans, PDMA_POLL_ROUND, PDMA_POLL_US);
	if (ret)
		dev_err(to_device(trans), "%s(): Polling to idle failed\n", __func__);

	_mt799b_pdma_irq_disable(trans);

	return ret;
}

static int
mt799b_pdma_init(struct pci_trans *trans)
{
	dump_chip_info(trans);
	_mt799b_pdma_init(trans);
	_mt799b_pdma_irq_init(trans);

	return 0;
}

static bool
mt799b_pdma_match(struct pci_trans *master, struct pci_trans *slave)
{
	u32 master_rid = 0;
	u32 slave_rid = 0;
	u32 i;

	if (!master || !slave)
		return false;

	master_rid = get_rid_value(master);

	if (master_rid == DEFAULT_RID) {
		master_rid = master->parent.sid.idx;
		set_rid_value(master, master->parent.sid.idx);
		/* make sure rid value is written before read */
		wmb();
	}

	for (i = 0; i < 5; i++) {
		slave_rid = get_rid_value(slave);

		if (slave_rid != master_rid)
			udelay(1);
		else
			break;
	}

	return slave_rid == master_rid;
}

static int
mt799b_chip_option_check(struct pci_trans *trans)
{
	struct mtk_hw_dev *hw_dev = to_hw_dev(trans);
	struct mtk_chip_drv *chip_drv = hw_dev->chip_drv;
	struct mtk_chip_option_table *chip_opt_desc =
		&chip_drv->chip_opt_table[hw_dev->chip_opt];

	if (!chip_opt_desc->bus_desc_slave.tx_ring_num) {
		dev_err(to_device(trans),
			"%s(): TxRing number in bus descriptor is 0\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int
mt799b_chip_option_set(struct pci_trans *trans)
{
	struct mtk_hw_dev *hw_dev = to_hw_dev(trans);
	struct mtk_chip_drv *chip_drv = hw_dev->chip_drv;
	struct mtk_chip_option_table *chip_opt_desc =
		&chip_drv->chip_opt_table[hw_dev->chip_opt];
	struct mtk_bus_desc *bus_desc = &chip_opt_desc->bus_desc_slave;
	u8 txr_idx = 0, txq_idx = 0;

	for (; txr_idx < bus_desc->tx_ring_num; txr_idx++) {
		struct mtk_bus_tx_ring_desc *tx_ring_desc =
			&bus_desc->tx_ring_desc[txr_idx];
		bool is_used = false;

		while (txq_idx < trans->txq_num) {
			struct pci_queue *q = &trans->txq[txq_idx++].q;

			if (q->q_attr == Q_TX_DATA) {
				q->band_idx_bmp = tx_ring_desc->band_idx_bmp;
				q->q_type_bmp = tx_ring_desc->q_type_bmp;
				is_used = true;
				break;
			}
		}

		if (!is_used)
			dev_err(to_device(trans), "%s(): TxRing descriptor[%u] isn't used\n",
				__func__, txr_idx);
	}

	trans->offload_disable = bus_desc->offload_disable;

	return 0;
}

static int
mt799b_intr_option_check(struct pci_trans *trans)
{
	struct mtk_hw_dev *hw_dev = to_hw_dev(trans);
	struct mtk_chip_drv *chip_drv = hw_dev->chip_drv;
	struct mtk_bus_trans *bus_trans = to_bus_trans(trans);
	struct mtk_bus_cfg *bus_cfg = bus_trans->bus_cfg;
	struct mtk_intr_option_desc *intr_opt_desc;
	u8 intr_opt;

	intr_opt = chip_drv->intr_opt_set[hw_dev->intr_opt].intr_opt_slave;
	intr_opt_desc = (struct mtk_intr_option_desc *)
		bus_cfg->intr_opt_table + intr_opt;

	if (hw_dev->irq_type != intr_opt_desc->irq_type) {
		dev_err(to_device(trans),
			"%s(): Device's irq type (%u) isn't match descriptor's irq type (%u)\n",
			__func__, hw_dev->irq_type, intr_opt_desc->irq_type);
		return -EINVAL;
	}

	if (!intr_opt_desc->vec_num) {
		dev_err(to_device(trans),
			"%s(): Vector number in interrupt descriptor is 0\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int
mt799b_intr_option_set(struct pci_trans *trans)
{
	struct mtk_hw_dev *hw_dev = to_hw_dev(trans);
	struct mtk_chip_drv *chip_drv = hw_dev->chip_drv;
	struct mtk_bus_trans *bus_trans = to_bus_trans(trans);
	struct mtk_bus_cfg *bus_cfg = bus_trans->bus_cfg;
	struct mtk_intr_option_desc *intr_opt_desc;
	struct vector_data *vec_data;
	u8 intr_opt, nr = 0;

	intr_opt = chip_drv->intr_opt_set[chip_drv->intr_opt].intr_opt_slave;
	intr_opt_desc = (struct mtk_intr_option_desc *)
		bus_cfg->intr_opt_table + intr_opt;
	vec_data = intr_opt_desc->vec_data;

	for (; nr < hw_dev->vec_num; nr++) {
		trans->vec_data[nr].bus_trans = to_bus_trans(trans);
		trans->vec_data[nr].attr = vec_data[nr].attr;
		trans->vec_data[nr].int_ena_mask = vec_data[nr].int_ena_mask;

		if (vec_data[nr].attr == VEC_ATTR_LUMP)
			continue;

		trans->vec_data[nr].ring_num = vec_data[nr].ring_num;
		trans->vec_data[nr].ring_id = vec_data[nr].ring_id;
		trans->vec_data[nr].info = vec_data[nr].info;
		trans->vec_data[nr].repeat = vec_data[nr].repeat;

		if (intr_opt_desc->vec_data[nr].attr == VEC_ATTR_RRO_RSS)
			trans->vec_data[nr].isr_map_idx = 1;
	}

	return 0;
}

static int
mt799b_option_init(struct pci_trans *trans)
{
	int ret;

	ret = mt799b_chip_option_check(trans);
	if (ret)
		goto end;

	ret = mt799b_chip_option_set(trans);
	if (ret)
		goto end;

	ret = mt799b_intr_option_check(trans);
	if (ret)
		goto end;

	ret = mt799b_intr_option_set(trans);
end:
	return ret;
}

#ifdef CONFIG_HWIFI_DBG_ISR
static int mt799b_dbg_isr_init(struct pci_trans *ptrans)
{
	struct mtk_bus_trans *trans = to_bus_trans(ptrans);

	trans->bus_dbg.legacy_ring_irq_cnt =
		kzalloc(sizeof(atomic_t) * MT7993_LEGACY_MAX_BIT_NUM, GFP_ATOMIC);

	if (!trans->bus_dbg.legacy_ring_irq_cnt) {
		dev_err(to_device(trans), "%s(): allocate memory error: legacy_ring_irq_cnt\n",
			__func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt799b_dbg_isr_exit(struct pci_trans *ptrans)
{
	struct mtk_bus_trans *trans = to_bus_trans(ptrans);
	void *tmp;

	tmp = trans->bus_dbg.legacy_ring_irq_cnt;
	trans->bus_dbg.legacy_ring_irq_cnt = NULL;
	kfree(tmp);
}
#endif

static int mt799b_dbg_init(struct pci_trans *ptrans)
{
	int ret = 0;

#ifdef CONFIG_HWIFI_DBG_ISR
	ret = mt799b_dbg_isr_init(ptrans);
	if (ret)
		return ret;
#endif

	return ret;
}

static void mt799b_dbg_exit(struct pci_trans *ptrans)
{
#ifdef CONFIG_HWIFI_DBG_ISR
	mt799b_dbg_isr_exit(ptrans);
#endif
}

static int mt799b_dmy_init(struct pci_trans *ptrans)
{
	struct mtk_bus_trans *trans = to_bus_trans(ptrans);
	struct pci_trans *master_ptrans = to_pci_trans(trans->master);
	struct hw_rro_cfg *rro_cfg = master_ptrans->rro_cfg;

	if (!master_ptrans->rro_mode)
		return 0;

	rro_cfg->dmy_va = dma_alloc_coherent(to_device(ptrans),
		MT7993_DMY_BUF_SZ, &rro_cfg->dmy_pa, GFP_KERNEL);

	if (!rro_cfg->dmy_va) {
		dev_err(to_device(trans), "%s(): allocate memory error: dmy_va\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt799b_dmy_exit(struct pci_trans *ptrans)
{
	struct mtk_bus_trans *trans = to_bus_trans(ptrans);
	struct pci_trans *master_ptrans = to_pci_trans(trans->master);
	struct hw_rro_cfg *rro_cfg = master_ptrans->rro_cfg;

	if (rro_cfg->dmy_va)
		dma_free_coherent(to_device(ptrans), MT7993_DMY_BUF_SZ,
			rro_cfg->dmy_va, rro_cfg->dmy_pa);
}

static struct pci_device_id mt799b_pci_device_table[] = {
	{ PCI_DEVICE(0x14c3, MT799B_CHIP_ID) },
	{ },
};

#ifdef CONFIG_HWIFI_REBB_SUPPORT
static struct mtk_mcu_dest_2_path mt799b_rebb_dest_2_path[MCU_DEST_MAX_NUM] = {
	{MCU_DEST_WM,		MCU_PATH_WM},
	{MCU_DEST_RVD,		MCU_PATH_WM},
	{MCU_DEST_WA,		MCU_PATH_WM},
	{MCU_DEST_WA_WM,	MCU_PATH_WM},
	{MCU_DEST_WO,		MCU_PATH_WO},
};

static struct pci_chip_ops mt799b_rebb_pci_ops = {
	.dma_enable = mt799b_pdma_enable,
	.dma_disable = mt799b_pdma_disable,
	.dma_init = mt799b_pdma_init,
	.dma_match = mt799b_pdma_match,
	.dma_schdl_init = NULL,
	.rro_hw_init = NULL,
	.option_init = mt799b_option_init,
	.dbg_init = mt799b_dbg_init,
	.dbg_exit = mt799b_dbg_exit,
	.dmy_init = mt799b_dmy_init,
	.dmy_exit = mt799b_dmy_exit,
};

static struct mtk_chip_drv mt799b_rebb_drv = {
	.bus_cfg = {
		.bus_type = MTK_BUS_PCI,
		.id_table = mt799b_pci_device_table,
		.name = KBUILD_MODNAME,
		.profile = &mt799b_pci_wa_profile,
		.ms_type = CHIP_TYPE_SLAVE,
		.bus_ops = &mt799b_rebb_pci_ops,
		.bus_drv_sz = sizeof(struct pci_driver),
		.intr_opt_table = mt799b_intr_opt_tbl,
		.intr_opt_table_sz = ARRAY_SIZE(mt799b_intr_opt_tbl),
	},
	.ctl_ops = NULL,
	.hw_caps = NULL,
	.device_id = MT799B_CHIP_ID,
	.dest_2_path = mt799b_rebb_dest_2_path,
};
#endif /*CONFIG_HWIFI_REBB_SUPPORT*/

static int __init mtk_chip_mt799b_init(void)
{
	static struct mtk_chip_drv *drv;

#ifdef CONFIG_HWIFI_REBB_SUPPORT
	drv = &mt799b_rebb_drv;
#endif

	drv->interface_type = mac_type;

#ifdef CONFIG_HWIFI_HIF_WED
	drv->bus_cfg.bus_type = MTK_BUS_WED;
	drv->bus_cfg.bus_sub_type = MTK_BUS_PCI;
	drv->bus_cfg.bus_drv_sz = sizeof(struct wed_driver);
#endif
#ifdef CONFIG_HWIFI_HIF_NPU
	drv->bus_cfg.bus_type = MTK_BUS_NPU;
	drv->bus_cfg.bus_sub_type = MTK_BUS_PCI;
#endif

	return mtk_chip_register(drv);
}

static void __exit mtk_chip_mt799b_exit(void)
{
	static struct mtk_chip_drv *drv;

#ifdef CONFIG_HWIFI_REBB_SUPPORT
	drv = &mt799b_rebb_drv;
#endif

	mtk_chip_unregister(drv);
}

module_init(mtk_chip_mt799b_init);
module_exit(mtk_chip_mt799b_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Mediatek Hardware Wi-Fi Module");

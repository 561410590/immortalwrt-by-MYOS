/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef USBNET_RX_BUDGET_H
#define USBNET_RX_BUDGET_H

struct usbnet_rx_budget {
	int packets;
	unsigned int steps;
};

/* NTH, NDP, frame and terminator for one ordinary single-frame NTB. */
static inline struct usbnet_rx_budget usbnet_rx_budget_init(int budget)
{
	return (struct usbnet_rx_budget) {
		.packets = budget,
		.steps = budget * 4U,
	};
}

#endif

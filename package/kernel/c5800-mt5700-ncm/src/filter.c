// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CDC Ethernet based networking peripherals
 * Copyright (C) 2003-2005 by David Brownell
 * Copyright (C) 2006 by Ole Andre Vadla Ravnas (ActiveSync)
 */

// #define	DEBUG			// error path messages, extra info
// #define	VERBOSE			// more; success messages

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include "wt_usbnet.h"


void usbnet_cdc_update_filter(struct usbnet *dev)
{
	struct net_device	*net = dev->net;

	u16 cdc_filter = USB_CDC_PACKET_TYPE_DIRECTED
			| USB_CDC_PACKET_TYPE_BROADCAST;

	/* filtering on the device is an optional feature and not worth
	 * the hassle so we just roughly care about snooping and if any
	 * multicast is requested, we take every multicast
	 */
	if (net->flags & IFF_PROMISC)
		cdc_filter |= USB_CDC_PACKET_TYPE_PROMISCUOUS;
	if (!netdev_mc_empty(net) || (net->flags & IFF_ALLMULTI))
		cdc_filter |= USB_CDC_PACKET_TYPE_ALL_MULTICAST;

	usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			USB_CDC_SET_ETHERNET_PACKET_FILTER,
			USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			cdc_filter,
			dev->intf->cur_altsetting->desc.bInterfaceNumber,
			NULL,
			0,
			USB_CTRL_SET_TIMEOUT
		);
}

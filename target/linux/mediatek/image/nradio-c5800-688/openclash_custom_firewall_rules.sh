#!/bin/sh
. /usr/share/openclash/log.sh
. /lib/functions.sh

# This script is called by /etc/init.d/openclash
# Add your custom firewall rules here, they will be added after the end of the OpenClash iptables rules

LOG_TIP "Start Add Custom Firewall Rules..."

ACTION=openclash /bin/sh /etc/hotplug.d/iface/45-c5800-openclash-cn

exit 0

#
# Copyright (C) 2008-2014 The LuCI Team <luci@lists.subsignal.org>
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=UPnP IGD & PCP/NAT-PMP configuration module | Universal Plug and Play
LUCI_DEPENDS:=+luci-base +miniupnpd +rpcd-mod-ucode

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=Jo-Philipp Wich <jo@mein.io>

PKG_FILE_MODES = $(if $(filter luci-app-upnp,$(1)), \
	/usr/share/luci:root:root:0755 \
	/usr/share/luci/menu.d:root:root:0755 \
	/www/luci-static:root:root:0755 \
	/www/luci-static/resources:root:root:0755 \
	/www/luci-static/resources/view:root:root:0755 \
	/www/luci-static/resources/view/status:root:root:0755 \
	/www/luci-static/resources/view/status/include:root:root:0755 \
	/www/luci-static/resources/view/status/include/80_upnp.js:root:root:0644 \
	/www/luci-static/resources/view/upnp:root:root:0755 \
	/www/luci-static/resources/view/upnp/upnp.js:root:root:0644, \
	/usr/lib/lua/luci:root:root:0755)

include ../../luci.mk

# call BuildPackage - OpenWrt buildroot signature

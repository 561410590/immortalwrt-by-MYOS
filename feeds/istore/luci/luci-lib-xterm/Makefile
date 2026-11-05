#
# Copyright (c) 2017-2019, The xterm.js authors (MIT License)
# Copyright (c) 2014-2017, SourceLair, Private Company (www.sourcelair.com) (MIT License)
# Copyright (c) 2012-2013, Christopher Jeffrey (MIT License)
#
# This is free software, licensed under the MIT License.
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=Xterm.js library
LUCI_DEPENDS:=

PKG_LICENSE:=MIT
PKG_VERSION:=4.18.0
PKG_RELEASE:=
PKG_MAINTAINER:=jjm2473 <jjm2473@gmail.com>

PKG_FILE_MODES:= \
	/usr/lib/lua/luci/view:root:root:0755 \
	/usr/lib/lua/luci/view/xterm:root:root:0755 \
	/www/luci-static:root:root:0755 \
	/www/luci-static/resources:root:root:0755 \
	/www/luci-static/resources/xterm:root:root:0755 \
	/www/luci-static/resources/xterm/xterm.css:root:root:0644 \
	/www/luci-static/resources/xterm/xterm.js:root:root:0644

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature

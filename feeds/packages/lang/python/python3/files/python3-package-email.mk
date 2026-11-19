#
# Copyright (C) 2006-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Package/python3-email
$(call Package/python3/Default)
  TITLE+= email module
  DEPENDS:=+python3-light
endef

define Package/python3-email/description
$(call Package/python3/Default/description)

This package contains the email module.
endef

PYTHON3_EMAIL_DIR:=/usr/lib/python$(PYTHON3_VERSION)/email
$(eval $(call Py3BasePackage,python3-email,$(PYTHON3_EMAIL_DIR)))

define Py3Package/python3-email/filespec
+|$(PYTHON3_EMAIL_DIR)|0644|0755
endef

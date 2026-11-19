#
# Copyright (C) 2019 Alexandru Ardelean <ardeleanalex@gmail.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Package/python3-urllib
$(call Package/python3/Default)
  TITLE+= URL handling modules
  DEPENDS:=+python3-light +python3-email
endef

define Package/python3-urllib/description
$(call Package/python3/Default/description)

This package contains the URL handling modules.
endef

PYTHON3_URLLIB_DIR:=/usr/lib/python$(PYTHON3_VERSION)/urllib
$(eval $(call Py3BasePackage,python3-urllib,$(PYTHON3_URLLIB_DIR)))

define Py3Package/python3-urllib/filespec
+|$(PYTHON3_URLLIB_DIR)|0644|0755
endef

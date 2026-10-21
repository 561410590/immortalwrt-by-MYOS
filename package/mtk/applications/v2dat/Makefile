include $(TOPDIR)/rules.mk

PKG_NAME:=v2dat
PKG_VERSION:=2022.12.15~47b8ee51
PKG_RELEASE:=3
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/urlesistiana/v2dat.git
PKG_SOURCE_DATE:=2022-12-15
PKG_SOURCE_VERSION:=47b8ee51fb528e11e1a83453b7e767a18d20d1f7
PKG_MIRROR_HASH:=skip
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_LICENSE:=GPL-3.0

GO_PKG:=github.com/urlesistiana/v2dat

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/golang/golang-package.mk

define Package/v2dat
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=V2Ray GeoIP and GeoSite data tools
  DEPENDS:=$(GO_ARCH_DEPENDS)
endef

$(eval $(call GoBinPackage,v2dat))
$(eval $(call BuildPackage,v2dat))

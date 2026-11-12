include $(TOPDIR)/rules.mk

PKG_NAME:=netifd
PKG_RELEASE:=2

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=$(PROJECT_GIT)/project/netifd.git
PKG_SOURCE_DATE:=2024-12-17
PKG_SOURCE_VERSION:=ea01ed41f3212ecbe000422f3c122a01b93fe874
PKG_MIRROR_HASH:=dbaad26c1f9b15d0caff6ccdf80d85b34d96bda72cbd2b1dc188a04136d96c28
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=

PKG_BUILD_FLAGS:=lto

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/netifd
  SECTION:=base
  CATEGORY:=Base system
  DEPENDS:=+libuci +libnl-tiny +libubus +ubus +ubusd +jshn +libubox +libudebug +ucode +ucode-mod-fs
  TITLE:=OpenWrt Network Interface Configuration Daemon
endef

define Package/netifd/conffiles
/etc/udhcpc.user
/etc/udhcpc.user.d/
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-I$(STAGING_DIR)/usr/include

CMAKE_OPTIONS += \
	-DLIBNL_LIBS=-lnl-tiny \
	-DDEBUG=1

define Package/netifd/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/netifd $(1)/sbin/
	$(CP) ./files/* $(1)/
	$(INSTALL_BIN) ./files/sbin/devstatus ./files/sbin/ifstatus ./files/sbin/ifup $(1)/sbin/
	$(INSTALL_BIN) ./files/etc/init.d/network $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/lib/netifd/dhcp.script $(1)/lib/netifd/
	$(INSTALL_BIN) ./files/lib/netifd/proto/dhcp.sh $(1)/lib/netifd/proto/
	$(INSTALL_BIN) ./files/usr/share/udhcpc/default.script $(1)/usr/share/udhcpc/
	$(INSTALL_DATA) ./files/lib/network/config.sh $(1)/lib/network/
	$(INSTALL_DIR) $(1)/etc/udhcpc.user.d/
	$(INSTALL_DATA) \
		$(PKG_BUILD_DIR)/scripts/utils.sh \
		$(PKG_BUILD_DIR)/scripts/netifd-proto.sh \
		$(1)/lib/netifd/
endef

$(eval $(call BuildPackage,netifd))

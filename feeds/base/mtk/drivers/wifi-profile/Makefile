#
# Copyright (C) 2016 MediaTek
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=wifi-profile
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

PKG_FILE_MODES = $(if $(filter wifi-dats,$(1)),/lib/upgrade/keep.d/wifi-dats:root:root:0644)

PKG_KCONFIG:= \
	first_card \
	first_card_name \
	second_card \
	second_card_name \
	third_card \
	third_card_name \
	WIFI_NORMAL_SETTING \
	WIFI_QUICK_SETTING \
	MTK_WIFI7_CHIP_MT7990 \
	MTK_WIFI7_CHIP_MT7992 \
	MTK_WIFI7_CHIP_MT7993 \
	MTK_WIFI7_UNIFIED_COMMAND \
	MTK_WIFI7_SKU_TYPE \
	MTK_WIFI7_PROFILING_SUPPORT \
	MTK_WIFI7_2ADIE_TRIBAND

PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))

include $(INCLUDE_DIR)/package.mk

define Package/wifi-profile
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  DEPENDS:=+wifi-dats
  TITLE:=MTK WiFi setup scripts
  VERSION:=$(PKG_RELEASE)
  MENU:=1
endef

define Package/wifi-profile/description
  This package install wifi setup scripts.
endef

define Package/wifi-profile/config
if PACKAGE_wifi-profile
	choice
		prompt "WiFi Setting"
		default WIFI_NORMAL_SETTING
		config WIFI_NORMAL_SETTING
			bool "normal setting"

		config WIFI_QUICK_SETTING
			bool "quick setting"

		help
			This is a way to configure wifi after wifi reload(Save & Apply from UI).
			Normal setting means reloading dat file which will execute "wifi down; wifi up"
			Quick setting means executing iwpriv commands corresponding to your operation on UI.
	endchoice
endif
endef

define Package/wifi-dats
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=MTK WiFi l1 & l2 dat files
  VERSION:=$(PKG_RELEASE)
  MENU:=1
endef

define Package/wifi-dats/description
  This package install wifi l1 and l2 dat files.
endef

define Package/wifi-dats/config
if PACKAGE_wifi-dats
	menuconfig first_card
		bool "1st card"
		default y

	if first_card
	config first_card_name
		string "1st card name"
		default "MT7990" if MTK_WIFI7_CHIP_MT7990
		default "MT7992" if MTK_WIFI7_CHIP_MT7992
		default "MT7993" if MTK_WIFI7_CHIP_MT7993
	endif

	menuconfig second_card
		bool "2nd card"
		default n

	if second_card
	config second_card_name
		string "2nd card name"
		default "MT7990" if MTK_WIFI7_CHIP_MT7990
		default "MT7992" if MTK_WIFI7_CHIP_MT7992
		default "MT7993" if MTK_WIFI7_CHIP_MT7993
	endif

	menuconfig third_card
		bool "3rd card"
		default n

	if third_card
	config third_card_name
		string "3rd card name"
		default "MT7990" if MTK_WIFI7_CHIP_MT7990
	endif

	config MTK_WIFI7_PROFILING_SUPPORT
		bool "wifi7 profiling dat files"
		default n
endif
endef

define Package/wifi-dats/conffiles
/etc/wireless/
endef

define Build/Compile
endef

define Package/wifi-profile/install
	$(INSTALL_DIR) $(1)/lib/wifi/

	$(INSTALL_BIN) ./files/common/mtwifi.lua $(1)/lib/wifi/

	if [ "$$(CONFIG_WIFI_QUICK_SETTING)" = "y" ] ; then \
		$(INSTALL_BIN) ./files/common/quick_setting.lua $(1)/lib/wifi/; \
	fi
endef

define Package/wifi-dats/install
	$(INSTALL_DIR) $(1)/etc/wireless/mediatek/

ifneq ($(CONFIG_MTK_WIFI7_PROFILING_SUPPORT),y)
	if [ "$$(CONFIG_first_card_name)" = "MT7990" -o "$$(CONFIG_second_card_name)" = "MT7990" ] ; then \
		$(INSTALL_DATA) ./files/mt7990/* $(1)/etc/wireless/mediatek/; \
	fi

	if [ "$$(CONFIG_first_card_name)" = "MT7992" -o "$$(CONFIG_second_card_name)" = "MT7992" ] ; then \
		if [ "$$(CONFIG_MTK_WIFI7_SKU_TYPE)" = "BE3600SDB" ] ; then \
			$(INSTALL_DATA) ./files/mt7992/BE3600SDB/* $(1)/etc/wireless/mediatek/; \
		else \
			$(INSTALL_DATA) ./files/mt7992/*.dat $(1)/etc/wireless/mediatek/; \
		fi; \
	fi

	if [ "$$(CONFIG_first_card_name)" = "MT7993" -o "$$(CONFIG_second_card_name)" = "MT7993" ] ; then \
		$(INSTALL_DATA) ./files/mt7993/* $(1)/etc/wireless/mediatek/; \
		if [ "$$(CONFIG_MTK_WIFI7_SKU_TYPE)" = "BE3600_26" ] ; then \
			sed -i "s/mt7993.b1.dat/mt7993.b2.dat/" $(1)/etc/wireless/mediatek/mt7993.1.dat; \
		fi; \
	fi
endif

	if [ "$$(CONFIG_first_card_name)" = "MT7986" -o "$$(CONFIG_second_card_name)" = "MT7986" ] ; then \
		if [ "$$(CONFIG_MTK_WIFI_SKU_TYPE)" = "AX4200" ] ; then \
			$(INSTALL_DATA) ./files/mt7986-ax4200/* $(1)/etc/wireless/mediatek/; \
		else \
			$(INSTALL_DATA) ./files/mt7986/* $(1)/etc/wireless/mediatek/; \
		fi ; \
	fi

	if [ "$$(CONFIG_first_card_name)" = "MT7981" -o "$$(CONFIG_second_card_name)" = "MT7981" ] ; then \
		$(INSTALL_DATA) ./files/mt7981/* $(1)/etc/wireless/mediatek/; \
	fi

	if [ -f "$(1)/etc/wireless/mediatek/l1profile.dat" ]; then \
		mv $(1)/etc/wireless/mediatek/l1profile.dat $(1)/etc/wireless/l1profile.dat ; \
	fi
endef

$(eval $(call BuildPackage,wifi-dats))
$(eval $(call BuildPackage,wifi-profile))

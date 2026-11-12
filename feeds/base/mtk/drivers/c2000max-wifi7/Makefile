include $(TOPDIR)/rules.mk

PKG_NAME:=mt798x-2p5g-phy-firmware-internal
PKG_VERSION:=1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/mt798x-2p5g-phy-firmware-internal
  SECTION:=firmware
  CATEGORY:=Firmware
  TITLE:=MediaTek MT7987 and MT7988 internal 2.5G PHY firmware
endef

define Build/Compile
endef

define Package/mt798x-2p5g-phy-firmware-internal/install
	$(INSTALL_DIR) $(1)/lib/firmware/mediatek/mt7987 $(1)/lib/firmware/mediatek/mt7988
	$(INSTALL_DATA) ./files/firmware/mediatek/mt7987/i2p5ge-phy-pmb.bin \
		./files/firmware/mediatek/mt7987/i2p5ge-phy-DSPBitTb.bin $(1)/lib/firmware/mediatek/mt7987/
	$(INSTALL_DATA) ./files/firmware/mediatek/mt7988/i2p5ge-phy-pmb.bin $(1)/lib/firmware/mediatek/mt7988/
endef

$(eval $(call BuildPackage,mt798x-2p5g-phy-firmware-internal))

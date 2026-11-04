# SPDX-License-Identifier: GPL-2.0-only
# NBCPE-688 / WT9111 NB68 has its own rootfs, device tree and image identity.

NBCPE_VERSION := 3.1.34-202610092000

define Build/nbcpe-688-rootfs-system
	python3 $(TOPDIR)/target/linux/mediatek/image/assemble-nbcpe.py --pack
	rm -f $(IMAGE_ROOTFS)
	cp $(KDIR)/root-nbcpe-myos.squashfs $(IMAGE_ROOTFS)
endef

define Build/nbcpe-688-sysupgrade-tar
	$(call Build/sysupgrade-tar)
	mkdir -p $@.nbcpe-control/sysupgrade-HCMT7981-emmc-NB68
	printf 'BOARD=%s\n' '$(BOARD_NAME)' > \
		$@.nbcpe-control/sysupgrade-HCMT7981-emmc-NB68/CONTROL
	(cd $@.nbcpe-control; tar --owner=0 --group=0 --numeric-owner \
		--mtime=@$(SOURCE_DATE_EPOCH) -rf $@ \
		sysupgrade-HCMT7981-emmc-NB68/CONTROL)
	rm -rf $@.nbcpe-control
endef

define Build/nbcpe-688-append-metadata
	printf '%s\n' \
		'{ "metadata_version": "1.1", "compat_version": "1.0", "supported_devices": ["HCMT7981-emmc-NB68"], "version": { "dist": "MYOS", "version": "$(NBCPE_VERSION)", "revision": "r33578-c88458e977", "target": "mediatek/filogic", "board": "nradio_wt9111_No2-manper_NB68" } }' \
		> $@.metadata
	fwtool -I $@.metadata $@
	rm -f $@.metadata
endef

define Device/nradio_nbcpe-688
  DEVICE_VENDOR := NRadio
  DEVICE_MODEL := NBCPE-688
  DEVICE_DTS := mt7981-nradio-nbcpe-688
  DEVICE_DTS_DIR := ../dts
  BOARD_NAME := nradio_wt9111_No2-manper_NB68
  SUPPORTED_DEVICES := HCMT7981-emmc-NB68
  DEVICE_IMG_PREFIX := MYOS-NBCPE-688-$(NBCPE_VERSION)
  DEVICE_PACKAGES := \
	kmod-gpio-button-hotplug kmod-leds-gpio kmod-mediatek_hnat mtk-smp \
	kmod-mt_wifi kmod-conninfra kmod-warp mtwifi-cfg luci-app-mtwifi-cfg \
	kmod-usb3 kmod-usb-xhci-mtk kmod-usb-acm \
	kmod-usb-net-cdc-ether kmod-usb-net-cdc-ncm kmod-usb-net-cdc-mbim \
	kmod-usb-net-rndis kmod-usb-serial-option kmod-usb-wdm \
	usbutils usbids f2fsck mkf2fs \
	python3-light luci-theme-c2000max-ui luci-theme-mwrt uhttpd uhttpd-mod-ubus \
	luci-app-store luci-app-firewall luci-app-package-manager \
	lua-cjson lua-md5 lua-mosquitto libmosquitto-nossl mosquitto-nossl libcares \
	curl libcurl4 luci-lua-runtime
  KERNEL_LOADADDR := 0x48000000
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb | pad-to 64k
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := nbcpe-688-rootfs-system | nradio-fit-rootfs-metadata | nbcpe-688-sysupgrade-tar | nbcpe-688-append-metadata
endef
TARGET_DEVICES += nradio_nbcpe-688

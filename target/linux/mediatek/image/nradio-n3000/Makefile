TOPDIR ?= /vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6
HERE := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ARCH_PACKAGES := aarch64_cortex-a53
KDIR := $(TOPDIR)/build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic
LINUX := $(KDIR)/linux-6.6.113
STAGING_DIR_HOST := $(TOPDIR)/staging_dir/host
HOST := $(STAGING_DIR_HOST)/bin
export TOPDIR
export PATH := $(LINUX)/scripts/dtc:$(HOST):/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
WORK ?= $(HERE)/build
MWRT ?= /mnt/f/Download/Mwrt-n3000-1164-24-20261117.bin
REFERENCE ?= $(WORK)/mwrt-fs
VERSION ?= 3.1.45-$(shell TZ=Asia/Shanghai date +%Y%m%d%H%M)
export STAGING_DIR := $(TOPDIR)/staging_dir/target-aarch64_cortex-a53_musl
BOARD := NRadio-N3000-emmc
TARGET_FEED := $(TOPDIR)/bin/targets/mediatek/filogic/packages
IPKS := $(wildcard $(TARGET_FEED)/*.ipk $(TOPDIR)/bin/packages/$(ARCH_PACKAGES)/*/*.ipk)
MODE_PACKAGES := base-files luci-app-firewall luci-app-mtwifi-cfg luci-base luci-compat luci-lib-base luci-lib-ip luci-lib-jsonc luci-lib-nixio luci-lua-runtime luci-mod-system
MODE_PACKAGES += qmodem luci-app-qmodem-next luci-i18n-qmodem-next-zh-cn quectel-CM-5G-M sms-forwarder-next tom_modem \
  ca-bundle icu75 libsodium libudns mtwifi-cfg resolveip ucode-mod-html ucode-mod-lua wifi-dats wifi-profile \
  ruby ruby-bigdecimal ruby-date ruby-digest ruby-enc ruby-pstore ruby-psych ruby-stringio ruby-yaml
OPKG = $(call opkg,$(ROOT)) -f $(WORK)/feeds.conf --lists-dir /tmp/opkg-lists
XARGS := xargs -r
export SOURCE_DATE_EPOCH := $(shell stat -c %Y $(LINUX)/arch/arm64/boot/Image)
include $(TOPDIR)/include/rootfs.mk

PACKAGES := base-files busybox ca-bundle dropbear fstools libustream-openssl logd netifd uci \
  uclient-fetch urandom-seed urngd procd-ujail opkg block-mount lsblk blkid f2fsck mkf2fs \
  dnsmasq-full firewall4 nftables odhcp6c odhcpd-ipv6only ppp ppp-mod-pppoe \
  kmod-nft-offload kmod-leds-gpio kmod-gpio-button-hotplug ethtool \
  kmod-usb3 kmod-usb-xhci-mtk kmod-usb-storage kmod-usb-net-rndis kmod-usb-net-cdc-ether \
  kmod-mt_wifi wifi-profile mtwifi-cfg luci-app-mtwifi-cfg luci-i18n-mtwifi-cfg-zh-cn \
  mtk-smp uhttpd uhttpd-mod-ubus luci-base luci-mod-admin-full luci-app-firewall \
  luci-proto-ppp luci-i18n-base-zh-cn luci-i18n-firewall-zh-cn \
  luci-theme-myos-new luci-app-myos-system luci-app-package-manager luci-i18n-package-manager-zh-cn \
  luci-app-openclash luci-app-passwall luci-app-passwall2 luci-i18n-passwall-zh-cn luci-i18n-passwall2-zh-cn \
  sing-box xray-core v2ray-geoip v2ray-geosite geoview hysteria tuic-client naiveproxy \
  shadowsocks-rust-sslocal shadowsocks-rust-ssserver shadowsocksr-libev-ssr-local shadowsocksr-libev-ssr-redir \
  simple-obfs-client shadow-tls v2ray-plugin xray-plugin ipt2socks haproxy \
  kmod-tun kmod-nft-socket kmod-nft-tproxy kmod-nf-tproxy kmod-ipt-tproxy iptables-mod-tproxy ipset \
  luci-app-wireguard luci-proto-wireguard wireguard-tools kmod-wireguard luci-i18n-wireguard-zh-cn \
  luci-app-WTModem qmodem luci-app-qmodem-next luci-i18n-qmodem-next-zh-cn \
  usb-modeswitch usbutils usbids uqmi umbim kmod-usb-net-qmi-wwan kmod-usb-net-cdc-mbim \
  kmod-usb-serial-option kmod-usb-acm kmod-usb2 kmod-usb-ohci kmod-usb-uhci kmod-usb-ehci \
  kmod-n3000-usb-ethernet usbmuxd libimobiledevice-utils kmod-usb-storage-extras kmod-usb-storage-uas \
  kmod-fs-ext4 kmod-fs-vfat kmod-fs-exfat kmod-fs-ntfs3 e2fsprogs

.PHONY: image compile prepare
image:
	mkdir -p $(WORK)
	$(MAKE) -f $(HERE)/Makefile compile ROOT=$$(mktemp -d $(WORK)/root.XXXXXX)

compile: $(WORK)/n3000.dtb prepare
	$(HOST)/mksquashfs4 $(ROOT) $(WORK)/root.squashfs -noappend -no-xattrs -root-owned -comp xz -b 256k -p 'dev/console c 600 0 0 5 1'
	$(HOST)/lzma e $(LINUX)/arch/arm64/boot/Image -lc1 -lp2 -pb2 $(WORK)/kernel.lzma
	sh $(TOPDIR)/scripts/mkits.sh -A arm64 -C lzma -a 0x48000000 -e 0x48000000 -v 6.6.113 -D $(BOARD) -c config-1 -d $(WORK)/n3000.dtb -k $(WORK)/kernel.lzma -o $(WORK)/kernel.its
	$(HOST)/mkimage -f $(WORK)/kernel.its $(WORK)/kernel.itb
	sh $(TOPDIR)/scripts/nradio-fit-rootfs-metadata.sh $(WORK)/kernel.itb $(WORK)/root.squashfs $(BOARD)
	sh $(TOPDIR)/scripts/sysupgrade-tar.sh --board $(BOARD) --kernel $(WORK)/kernel.itb --rootfs $(WORK)/root.squashfs $(WORK)/sysupgrade.bin
	python3 $(HERE)/finalize.py $(TOPDIR) $(WORK) $(ROOT)

$(WORK)/n3000.dtb: $(wildcard $(HERE)/*.dts $(HERE)/*.dtsi) $(LINUX)/arch/arm64/boot/dts/mediatek/mt7981.dtsi
	mkdir -p $(WORK)
	gcc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp -I $(LINUX)/include -I $(LINUX)/scripts/dtc/include-prefixes -I $(LINUX)/arch/arm64/boot/dts/mediatek $(HERE)/mt7981-nradio-n3000-2nd.dts -o $(WORK)/n3000.pp.dts
	$(LINUX)/scripts/dtc/dtc -I dts -O dtb -o $@ $(WORK)/n3000.pp.dts

$(REFERENCE)/usr/lib/opkg/status: $(MWRT)
	mkdir -p $(WORK)
	tar -xOf $(MWRT) sysupgrade-nradio_n3000/root > $(WORK)/mwrt-reference.squashfs
	unsquashfs -f -no-xattrs -no-progress -d $(REFERENCE) $(WORK)/mwrt-reference.squashfs etc usr www lib bin sbin

$(WORK)/repo/Packages.gz: $(IPKS) $(HERE)/package-modes.py $(HERE)/reference-packages.py $(HERE)/wtmodem.lua $(HERE)/wtmodem-status.htm $(HERE)/wtmodem-sendat $(HERE)/Kbuild.usb $(HERE)/build-usb.py $(REFERENCE)/usr/lib/opkg/status
	mkdir -p $(WORK)/repo
	ln -sf -t $(WORK)/repo $(IPKS)
	python3 $(HERE)/reference-packages.py $(TOPDIR) $(WORK) $(REFERENCE)
	python3 $(HERE)/build-usb.py $(TOPDIR) $(LINUX) $(WORK) $(WORK)/repo
	cd $(WORK)/repo && python3 $(HERE)/package-modes.py $(addsuffix _*.ipk,$(MODE_PACKAGES))
	cd $(WORK)/repo && MKHASH=$(HOST)/mkhash bash $(TOPDIR)/scripts/ipkg-make-index.sh . > Packages && gzip -c Packages > Packages.gz

prepare: $(WORK)/repo/Packages.gz
	install -d -m 0755 $(ROOT)
	mkdir -p $(ROOT)/tmp
	echo 'src/gz n3000 file://$(WORK)/repo' > $(WORK)/feeds.conf
	$(OPKG) update
	$(OPKG) install $(wildcard $(TARGET_FEED)/libc_*.ipk $(TOPDIR)/bin/packages/$(ARCH_PACKAGES)/base/libc_*.ipk) $(wildcard $(TARGET_FEED)/kernel_6.6.113~*.ipk)
	$(OPKG) install $(PACKAGES)
	install -m 0755 $(HERE)/files/etc/board.d/02_network $(ROOT)/etc/board.d/02_network
	install -m 0644 $(HERE)/files/lib/upgrade/platform.sh $(ROOT)/lib/upgrade/platform.sh
	install -m 0644 $(HERE)/files/lib/preinit/91_n3000_wifi $(ROOT)/lib/preinit/91_n3000_wifi
	sed -i -E 's@^(\s*option mediaurlbase) .*@\1 /luci-static/myos-new@; /^config internal themes/a\	option MYOS_NEW /luci-static/myos-new' $(ROOT)/etc/config/luci
	sed -i "s/3\.1\.45-202611190030/$(VERSION)/g" $(ROOT)/etc/openwrt_release $(ROOT)/usr/lib/os-release $(ROOT)/etc/banner
	$(call prepare_rootfs,$(ROOT),,myos-nros-app pcie_mhi)

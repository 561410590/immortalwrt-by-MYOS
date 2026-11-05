DTS_DIR := $(DTS_DIR)/mediatek

define Image/Prepare
	# For UBI we want only one extra block
	rm -f $(KDIR)/ubi_mark
	echo -ne '\xde\xad\xc0\xde' > $(KDIR)/ubi_mark
endef

define Build/mt7981-bl2
	cat $(STAGING_DIR_IMAGE)/mt7981-$1-bl2.img >> $@
endef

define Build/mt7981-bl31-uboot
	cat $(STAGING_DIR_IMAGE)/mt7981_$1-u-boot.fip >> $@
endef

define Build/mt7986-bl2
	cat $(STAGING_DIR_IMAGE)/mt7986-$1-bl2.img >> $@
endef

define Build/mt7986-bl31-uboot
	cat $(STAGING_DIR_IMAGE)/mt7986_$1-u-boot.fip >> $@
endef

define Build/mt7988-bl2
	cat $(STAGING_DIR_IMAGE)/mt7988-$1-bl2.img >> $@
endef

define Build/mt7988-bl31-uboot
	cat $(STAGING_DIR_IMAGE)/mt7988_$1-u-boot.fip >> $@
endef

define Build/mt798x-gpt
	cp $@ $@.tmp 2>/dev/null || true
	ptgen -g -o $@.tmp -a 1 -l 1024 \
		$(if $(findstring sdmmc,$1), \
			-H \
			-t 0x83	-N bl2		-r	-p 4079k@17k \
		) \
			-t 0x83	-N ubootenv	-r	-p 512k@4M \
			-t 0x83	-N factory	-r	-p 2M@4608k \
			-t 0xef	-N fip		-r	-p 4M@6656k \
				-N recovery	-r	-p 32M@12M \
		$(if $(findstring sdmmc,$1), \
				-N install	-r	-p 20M@44M \
			-t 0x2e -N production		-p $(CONFIG_TARGET_ROOTFS_PARTSIZE)M@64M \
		) \
		$(if $(findstring emmc,$1), \
			-t 0x2e -N production		-p $(CONFIG_TARGET_ROOTFS_PARTSIZE)M@64M \
		)
	cat $@.tmp >> $@
	rm $@.tmp
endef

metadata_gl_json = \
	'{ $(if $(IMAGE_METADATA),$(IMAGE_METADATA)$(comma)) \
		"metadata_version": "1.1", \
		"compat_version": "$(call json_quote,$(compat_version))", \
		$(if $(DEVICE_COMPAT_MESSAGE),"compat_message": "$(call json_quote,$(DEVICE_COMPAT_MESSAGE))"$(comma)) \
		$(if $(filter-out 1.0,$(compat_version)),"new_supported_devices": \
			[$(call metadata_devices,$(SUPPORTED_DEVICES))]$(comma) \
			"supported_devices": ["$(call json_quote,$(legacy_supported_message))"]$(comma)) \
		$(if $(filter 1.0,$(compat_version)),"supported_devices":[$(call metadata_devices,$(SUPPORTED_DEVICES))]$(comma)) \
		"version": { \
			"release": "$(call json_quote,$(VERSION_NUMBER))", \
			"date": "$(shell TZ='Asia/Chongqing' date '+%Y%m%d%H%M%S')", \
			"dist": "$(call json_quote,$(VERSION_DIST))", \
			"version": "$(call json_quote,$(VERSION_NUMBER))", \
			"revision": "$(call json_quote,$(REVISION))", \
			"target": "$(call json_quote,$(TARGETID))", \
			"board": "$(call json_quote,$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME)))" \
		} \
	}'

define Build/append-gl-metadata
	$(if $(SUPPORTED_DEVICES),-echo $(call metadata_gl_json,$(SUPPORTED_DEVICES)) | fwtool -I - $@)
	sha256sum "$@" | cut -d" " -f1 > "$@.sha256sum"
	[ ! -s "$(BUILD_KEY)" -o ! -s "$(BUILD_KEY).ucert" -o ! -s "$@" ] || { \
		cp "$(BUILD_KEY).ucert" "$@.ucert" ;\
		usign -S -m "$@" -s "$(BUILD_KEY)" -x "$@.sig" ;\
		ucert -A -c "$@.ucert" -x "$@.sig" ;\
		fwtool -S "$@.ucert" "$@" ;\
	}
endef

define Build/zyxel-nwa-fit-filogic
	$(TOPDIR)/scripts/mkits-zyxel-fit-filogic.sh \
		$@.its $@ "80 e1 ff ff ff ff ff ff ff ff"
	PATH=$(LINUX_DIR)/scripts/dtc:$(PATH) mkimage -f $@.its $@.new
	@mv $@.new $@
endef

define Build/nradio-fit-rootfs-metadata
	PATH=$(LINUX_DIR)/scripts/dtc:$(PATH) \
		sh $(TOPDIR)/scripts/nradio-fit-rootfs-metadata.sh \
			$(IMAGE_KERNEL) $(IMAGE_ROOTFS) $(BOARD_NAME)
endef

define Build/c2000max-fit-rootfs-metadata
	cp -f $(IMAGE_KERNEL) $(C2000MAX_SD_KERNEL)
	PATH=$(LINUX_DIR)/scripts/dtc:$(PATH) \
		sh $(TOPDIR)/scripts/nradio-fit-rootfs-metadata.sh \
			$(C2000MAX_SD_KERNEL) $(C2000MAX_MYOS_ROOTFS) $(BOARD_NAME)
endef

define Build/prepare-c2000max-sd-kernel
	cp -f $(IMAGE_KERNEL) $(C2000MAX_SD_KERNEL)
	chmod 0644 $(C2000MAX_SD_KERNEL)
	[ "$$(fdtget -t x $(C2000MAX_SD_KERNEL) /images/kernel-1 load)" = "40080000" ] || \
		{ echo "C2000MAX SD FIT kernel load address must be 0x40080000"; exit 1; }
	[ "$$(fdtget -t x $(C2000MAX_SD_KERNEL) /images/kernel-1 entry)" = "40080000" ] || \
		{ echo "C2000MAX SD FIT kernel entry address must be 0x40080000"; exit 1; }
	[ "$$(fdtget -t s $(C2000MAX_SD_KERNEL) /images/kernel-1 compression)" = "lzma" ] || \
		{ echo "C2000MAX SD FIT kernel compression must be lzma"; exit 1; }
	! fdtget -l $(C2000MAX_SD_KERNEL) / | grep -qx rootfs || \
		{ echo "C2000MAX SD FIT must match the boot-tested V2.1.7 layout without rootfs metadata nodes"; exit 1; }
	$(STAGING_DIR_HOST)/bin/mkimage -l $(C2000MAX_SD_KERNEL) | \
		grep -q "ARM64 OpenWrt Linux-$(LINUX_VERSION)" || \
		{ echo "C2000MAX SD FIT does not contain Linux $(LINUX_VERSION)"; exit 1; }
endef

define Build/c2000max-firmware-permissions
	chmod 0644 $@
endef

define Build/c2000max-upload-size
	[ "$$(stat -c%s $@)" -le "$(C2000MAX_UPLOAD_SIZE_LIMIT)" ] || \
		{ echo "C2000MAX BIN exceeds the 220 MiB official upload budget"; exit 1; }
	[ "$$(dd if=$@ bs=1 skip=257 count=5 2>/dev/null)" = ustar ] || \
		{ echo "C2000MAX official upload requires a sysupgrade tar"; exit 1; }
endef

define Build/c2000max-gpt
	dd if=$(C2000MAX_FACTORY_SDCARD) bs=512 count=34 >> $@
endef

define Build/append-c2000max-bl2
	dd if=$(C2000MAX_FACTORY_SDCARD) bs=512 skip=1024 count=7168 >> $@
endef

define Build/append-c2000max-uboot-env
	dd if=$(C2000MAX_FACTORY_SDCARD) bs=512 skip=8192 count=1024 >> $@
endef

define Build/append-c2000max-factory
	dd if=$(C2000MAX_FACTORY_SDCARD) bs=512 skip=9216 count=8192 >> $@
endef

define Build/append-c2000max-fip
	dd if=$(C2000MAX_FACTORY_SDCARD) bs=512 skip=17408 count=4096 >> $@
endef

C2000MAX_FACTORY_ANALYSIS := $(TOPDIR)/../factory-openwrt/c2000max-analysis
C2000MAX_FACTORY_SDCARD := $(C2000MAX_FACTORY_ANALYSIS)/nradio_c2000-max-SD_0305.img
C2000MAX_FACTORY_ROOTFS_OFFSET := 44564480
C2000MAX_ROOTFS_SIZE_LIMIT := 201326592
C2000MAX_UPLOAD_SIZE_LIMIT := 230686720
C2000MAX_FACTORY_LED_ROOTFS := $(C2000MAX_FACTORY_ANALYSIS)/C2000-788-WT9303_2.1.11.0h-rootfs
C2000MAX_MYOS_ROOTFS_DIR := $(KDIR)/root-c2000max-MYOS
C2000MAX_MYOS_ROOTFS := $(KDIR)/root-c2000max-MYOS.squashfs
C2000MAX_MYOS_ROOTFS_PSEUDO := $(KDIR)/root-c2000max-factory.pseudo
C2000MAX_SD_KERNEL := $(KDIR)/nradio_c2000-max-sd-kernel.bin
C2000MAX_EXTRA_IPK_TMP := $(KDIR)/root-c2000max-ipk-payload
C2000MAX_ROOTFS_ID := $(VERSION_NUMBER)-$(REVISION)
C2000MAX_EXTRA_IPK_PACKAGES := \
	flock ip6tables-nft iptables-mod-socket kmod-ipt-socket kmod-ipt-nat-extra iptables-mod-nat-extra python3-ctypes luci-app-uugamebooster mt5700-web-go luci-app-mt5700-web \
	luci-theme-myos-new luci-app-myos-system luci-light cgi-io bc jq libreadline8 libncurses6 terminfo \
	resize2fs fstools block-mount losetup libsmartcols1 libblkid1 libuuid1 f2fsck mkf2fs libf2fs6 \
	luci-app-store taskd luci-lib-taskd luci-lib-xterm \
	quickfile luci-app-quickfile luci-i18n-quickfile-zh-cn luci-nginx nginx-ssl nginx-mod-luci nginx-mod-ubus nginx-ssl-util uwsgi uwsgi-cgi-plugin uwsgi-syslog-plugin uwsgi-luci-support \
	openvpn-openssl kmod-tun libcap-ng liblzo2 liblz4-1 \
	luci-app-openvpn luci-i18n-openvpn-zh-cn \
	luci-compat luci-lib-jsonc kmod-oaf appfilter luci-app-oaf luci-i18n-oaf-zh-cn \
	kmod-leds-gpio \
	zram-swap kmod-zram \
	script-utils coreutils-stty \
	miniupnpd-nftables luci-app-upnp luci-i18n-upnp-zh-cn \
	iwinfo libiwinfo20230701 libiwinfo-data rpcd-mod-iwinfo fancontrol \
	libpython3-3.11 python3-base python3-light libbz2-1.0 zlib \
	bash \
	coreutils-base64 coreutils-nohup coreutils-timeout lyaml \
	libyaml libudns resolveip libev \
	libsodium ip-full libuci-lua libbpf1 \
	libelf1 iptables-mod-tproxy iptables-mod-extra kmod-ipt-extra libpcap1 ipset \
	libipset13 tc-full conntrack conntrackd \
	libnetfilter-cthelper0 libnetfilter-cttimeout1 libnetfilter-queue1 libltdl7 \
	liblua5.3-5.3 \
	kmod-fuse kmod-sound-core kmod-input-core \
	kmod-ipt-conntrack-extra kmod-nf-conncount kmod-ipt-ipopt kmod-ipt-filter \
	kmod-fs-cifs kmod-fs-smbfs-common kmod-fs-netfs kmod-nls-ucs2-utils kmod-oid-registry kmod-dnsresolver \
	kmod-md-mod kmod-md-raid0 kmod-md-raid1 kmod-md-raid10 \
	qemu-x86_64-softmmu qemu-aarch64-softmmu qemu-img qemu-bridge-helper \
	qemu-firmware-pxe qemu-firmware-seabios qemu-firmware-seavgabios qemu-firmware-efi qemu-keymaps \
	glib2 libffi libattr libpcre2 libstdcpp6 libusb-1.0-0 libudev-zero libevdev \
	libpng libseccomp libfdt libslirp pixman lua-md5

C2000MAX_REMOVE_IPK_PACKAGES := \
	luci-theme-c2000max-ui luci-theme-mwrt luci-theme-argon luci-theme-bootstrap luci-app-argon-config luci-i18n-argon-config-zh-cn \
	luci-app-quickstart luci-i18n-quickstart-zh-cn quickstart \
	luci-app-homeproxy luci-i18n-homeproxy-zh-cn homeproxy sing-box chinadns-ng dns2socks \
	luci-app-ddns-go luci-i18n-ddns-go-zh-cn ddns-go \
	luci-app-ddnsto ddnsto \
	luci-app-wol luci-i18n-wol-zh-cn wol etherwake \
	luci-app-linkease luci-i18n-linkease-zh-cn linkease linkmount

define Build/append-c2000max-kernel
	dd if=$(C2000MAX_SD_KERNEL) >> $@
endef

define Build/prepare-c2000max-rootfs
	rm -rf $(C2000MAX_MYOS_ROOTFS_DIR) $(C2000MAX_MYOS_ROOTFS) $(C2000MAX_MYOS_ROOTFS_PSEUDO)
	$(STAGING_DIR_HOST)/bin/unsquashfs4 -pf $(C2000MAX_MYOS_ROOTFS_PSEUDO) \
		-offset $(C2000MAX_FACTORY_ROOTFS_OFFSET) \
		$(C2000MAX_FACTORY_SDCARD) >/dev/null
	$(STAGING_DIR_HOST)/bin/unsquashfs4 -excludes -d $(C2000MAX_MYOS_ROOTFS_DIR) \
		-offset $(C2000MAX_FACTORY_ROOTFS_OFFSET) \
		$(C2000MAX_FACTORY_SDCARD) dev/console
	sh $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/MYOS-rootfs-patch.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR) "$(VERSION_NUMBER)" "$(REVISION)"
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/opkg $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/is-opkg $(C2000MAX_MYOS_ROOTFS_DIR)/bin/is-opkg
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/99-default-settings-chinese
	for pkg in $(C2000MAX_REMOVE_IPK_PACKAGES); do \
		list="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/$${pkg}.list"; \
		if [ -f "$$list" ]; then \
			while IFS= read -r path; do \
				[ -n "$$path" ] || continue; \
				rm -rf "$(C2000MAX_MYOS_ROOTFS_DIR)$${path}"; \
			done < "$$list"; \
		fi; \
		rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/$${pkg}.*; \
	done
	status="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status"; \
	if [ -f "$$status" ]; then \
		awk 'BEGIN{split("$(C2000MAX_REMOVE_IPK_PACKAGES)", pkgs); for (i in pkgs) del[pkgs[i]]=1; skip=0} /^Package: /{skip=($$2 in del)} skip && /^$$/{skip=0; next} !skip{print}' "$$status" > "$$status.tmp"; \
		mv "$$status.tmp" "$$status"; \
	fi
	sed -i 's/192\.168\.7\.1/192.168.66.1/g' $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit
	printf '%s\n' \
		'src/gz MYOS_core https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/targets/mediatek/filogic/packages' \
		'src/gz MYOS_base https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/base' \
		'src/gz MYOS_luci https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/luci' \
		'src/gz MYOS_packages https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/packages' \
		'src/gz MYOS_routing https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/routing' \
		'src/gz MYOS_telephony https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/telephony' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/opkg/distfeeds.conf
	printf '%s\n' \
		"DISTRIB_ID='MYOS'" \
		"DISTRIB_RELEASE='$(VERSION_NUMBER)'" \
		"DISTRIB_REVISION='$(REVISION)'" \
		"DISTRIB_TARGET='mediatek/filogic'" \
		"DISTRIB_ARCH='aarch64_cortex-a53'" \
		"DISTRIB_DESCRIPTION='MYOS by ImmortalWrt $(VERSION_NUMBER)'" \
		"DISTRIB_TAINTS='no-all busybox'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/openwrt_release
	printf '%s\n' '$(VERSION_NUMBER)' > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/openwrt_version
	printf '%s\n' \
		'NAME="MYOS"' \
		'VERSION="$(VERSION_NUMBER)"' \
		'ID="myos"' \
		'ID_LIKE="lede openwrt"' \
		'PRETTY_NAME="MYOS by ImmortalWrt $(VERSION_NUMBER)"' \
		'VERSION_ID="$(VERSION_NUMBER)"' \
		'BUILD_ID="$(REVISION)"' \
		'OPENWRT_BOARD="mediatek/filogic"' \
		'OPENWRT_ARCH="aarch64_cortex-a53"' \
		'OPENWRT_TAINTS="no-all busybox"' \
		'OPENWRT_DEVICE_MANUFACTURER="NRadio"' \
		'OPENWRT_DEVICE_PRODUCT="C2000-MAX"' \
		'OPENWRT_RELEASE="MYOS by ImmortalWrt $(VERSION_NUMBER)"' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/os-release
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/uhttpd ] || sed -i "s/option commonname .*/option commonname 'MYOS by ImmortalWrt $(VERSION_NUMBER)'/" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/uhttpd
	printf '\nMYOS by ImmortalWrt $(VERSION_NUMBER)\n\n' > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/banner
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua ] || sed -i 's/^luciname[[:space:]]*=.*/luciname    = "LuCI MYOS branch"/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua ] || sed -i 's/^luciversion[[:space:]]*=.*/luciversion = "26.056.11316~a9f2a15 MYOS by ImmortalWrt $(VERSION_NUMBER)"/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua
	printf '%s\n' \
		"config wifi-device 'MT7993_1_1'" \
		"	option type 'mtwifi'" \
		"	option phy 'ra0'" \
		"	option band '2g'" \
		"	option dbdc_main '1'" \
		"	option channel 'auto'" \
		"	option txpower '100'" \
		"	option htmode 'EHT40'" \
		"	option country 'CN'" \
		"	option mu_beamformer '1'" \
		"	option noscan '1'" \
		"	option serialize '1'" \
		'' \
		"config wifi-iface 'default_MT7993_1_1'" \
		"	option device 'MT7993_1_1'" \
		"	option network 'lan'" \
		"	option mode 'ap'" \
		"	option ssid 'ImmortalWrt-2.4G'" \
		"	option encryption 'none'" \
		"	option mlo '0'" \
		'' \
		"config wifi-device 'MT7993_1_2'" \
		"	option type 'mtwifi'" \
		"	option phy 'rai0'" \
		"	option band '5g'" \
		"	option dbdc_main '0'" \
		"	option channel 'auto'" \
		"	option txpower '100'" \
		"	option htmode 'EHT160'" \
		"	option country 'CN'" \
		"	option mu_beamformer '1'" \
		"	option noscan '0'" \
		"	option serialize '1'" \
		'' \
		"config wifi-iface 'default_MT7993_1_2'" \
		"	option device 'MT7993_1_2'" \
		"	option network 'lan'" \
		"	option mode 'ap'" \
		"	option ssid 'ImmortalWrt-5G'" \
		"	option encryption 'none'" \
		"	option mlo '0'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/wireless
	[ -d $(C2000MAX_FACTORY_LED_ROOTFS) ] || { echo "C2000MAX missing LED rootfs: $(C2000MAX_FACTORY_LED_ROOTFS)"; exit 1; }
	for path in \
		sbin/setled \
		sbin/led.sh \
		lib/functions/leds.sh \
		lib/network/switch.sh \
		usr/sbin/nrswitch \
		usr/libexec/rpcd/nrswitch \
		etc/init.d/nrswitch \
		etc/hotplug.d/gmac/02_nrswitch \
		usr/bin/wanchk.sh \
		usr/libexec/rpcd/wanchk \
		etc/config/wanchk \
		etc/init.d/wanchk \
		usr/bin/ledctrl.sh \
		usr/libexec/rpcd/ledctrl \
		etc/config/ledctrl \
		etc/init.d/ledctrl \
		usr/lib/lua/luci/controller/nradio_adv/ledctrl.lua \
		usr/lib/lua/luci/model/cbi/nradio_adv/ledctrl.lua \
		usr/lib/lua/luci/view/nradio_ledctrl/hmvalue.htm \
		etc/uci-defaults/luci-i18n-nradio-ledctrl-en \
		etc/uci-defaults/luci-i18n-nradio-ledctrl-zh-cn \
		usr/lib/lua/luci/i18n/nradio-ledctrl.en.lmo \
		usr/lib/lua/luci/i18n/nradio-ledctrl.zh-cn.lmo \
		usr/lib/opkg/info/ledctrl.conffiles \
		usr/lib/opkg/info/ledctrl.control \
		usr/lib/opkg/info/ledctrl.list \
		usr/lib/opkg/info/ledctrl.prerm \
		usr/lib/opkg/info/nrswitch.control \
		usr/lib/opkg/info/nrswitch.list \
		usr/lib/opkg/info/nrswitch.prerm \
		usr/lib/opkg/info/wanchk.conffiles \
		usr/lib/opkg/info/wanchk.control \
		usr/lib/opkg/info/wanchk.list \
		usr/lib/opkg/info/wanchk.prerm \
		usr/lib/opkg/info/luci-app-nradio-ledctrl.control \
		usr/lib/opkg/info/luci-app-nradio-ledctrl.list \
		usr/lib/opkg/info/luci-app-nradio-ledctrl.prerm \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-en.control \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-en.list \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-en.prerm \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-zh-cn.control \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-zh-cn.list \
		usr/lib/opkg/info/luci-i18n-nradio-ledctrl-zh-cn.prerm; do \
		[ -e $(C2000MAX_FACTORY_LED_ROOTFS)/$${path} ] || { echo "C2000MAX missing LED file: $${path}"; exit 1; }; \
		mkdir -p "$$(dirname $(C2000MAX_MYOS_ROOTFS_DIR)/$${path})"; \
		cp -fpR $(C2000MAX_FACTORY_LED_ROOTFS)/$${path} $(C2000MAX_MYOS_ROOTFS_DIR)/$${path}; \
	done
	grep -q "option schedule '0'" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/ledctrl || \
		sed -i "/option.*disabled.*'0'/a\\\toption schedule '0'\\n\toption starttime '0:0'\\n\toption endtime '0:0'" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/ledctrl
	sed -i 's/schedule:depends("en",not "1")/schedule:depends("disabled", "1")/' \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/model/cbi/nradio_adv/ledctrl.lua
	grep -q "option endtime '0:0'" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/ledctrl
	grep -q 'schedule:depends("disabled", "1")' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/model/cbi/nradio_adv/ledctrl.lua
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303
	rm -rf $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303/config
	cp -fpR $(C2000MAX_FACTORY_LED_ROOTFS)/oem/WT9303/config $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303/config
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d
	ln -sf ../init.d/wanchk $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K10wanchk
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S90wanchk
	ln -sf ../init.d/ledctrl $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K10ledctrl
	ln -sf ../init.d/ledctrl $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S97ledctrl
	ln -sf ../init.d/nrswitch $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99nrswitch
	for pkg in ledctrl nrswitch wanchk luci-app-nradio-ledctrl luci-i18n-nradio-ledctrl-en luci-i18n-nradio-ledctrl-zh-cn; do \
		status="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status"; \
		if [ -f "$$status" ] && grep -q "^Package: $${pkg}$$" "$$status"; then continue; fi; \
		control="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/$${pkg}.control"; \
		[ -f "$$control" ] || { echo "C2000MAX missing LED control: $$pkg"; exit 1; }; \
		awk 'BEGIN{has_status=0; has_time=0} /^Status: /{has_status=1} /^Installed-Time: /{has_time=1} {print} END{if (!has_status) print "Status: install ok installed"; if (!has_time) print "Installed-Time: 1772529460"; print ""}' "$$control" >> "$$status"; \
	done
	[ -d $(TARGET_DIR)/lib/modules/$(LINUX_VERSION) ] || { echo "C2000MAX missing $(LINUX_VERSION) modules"; exit 1; }
	[ -d $(TARGET_DIR)/etc/modules.d ] || { echo "C2000MAX missing modules.d"; exit 1; }
	rm -rf $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d
	cp -fpR $(TARGET_DIR)/lib/modules/$(LINUX_VERSION) $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules/
	cp -fpR $(TARGET_DIR)/etc/modules.d/. $(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d/
	[ ! -d $(TARGET_DIR)/etc/modules-boot.d ] || cp -fpR $(TARGET_DIR)/etc/modules-boot.d/. $(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/
	if [ -d $(TARGET_DIR)/lib/firmware ]; then \
		mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/lib/firmware; \
		cp -fpR $(TARGET_DIR)/lib/firmware/. $(C2000MAX_MYOS_ROOTFS_DIR)/lib/firmware/; \
	fi
	[ -f $(TARGET_DIR)/usr/lib/opkg/status ] || { echo "C2000MAX missing target opkg status"; exit 1; }
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/kernel.* \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/kmod-*
	for path in $(TARGET_DIR)/usr/lib/opkg/info/kernel.* \
		$(TARGET_DIR)/usr/lib/opkg/info/kmod-*; do \
		[ -f "$$path" ] || continue; \
		cp -fp "$$path" $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/; \
	done
	status=$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status; \
	awk '/^Package: / { drop = ($$2 == "kernel" || $$2 ~ /^kmod-/) } !drop { print } drop && /^$$/ { drop = 0 }' \
		"$$status" > "$$status.tmp"; \
	awk '/^Package: / { keep = ($$2 == "kernel" || $$2 ~ /^kmod-/) } keep { print } keep && /^$$/ { keep = 0 }' \
		$(TARGET_DIR)/usr/lib/opkg/status >> "$$status.tmp"; \
	mv "$$status.tmp" "$$status"
	for pkg in $(C2000MAX_EXTRA_IPK_PACKAGES); do \
		status="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status"; \
		refresh=0; \
		case "$$pkg" in luci-theme-myos-new|luci-app-myos-system|luci-light|luci-nginx|luci-lib-xterm|luci-app-upnp|luci-i18n-upnp-zh-cn|cgi-io|fstools|block-mount|losetup|libsmartcols1|libblkid1|libuuid1|f2fsck|mkf2fs|libf2fs6|iwinfo|libiwinfo20230701|libiwinfo-data|fancontrol|mt5700-web-go|luci-app-mt5700-web) refresh=1 ;; esac; \
		if [ "$$refresh" = 0 ] && [ -f "$$status" ] && grep -q "^Package: $${pkg}$$" "$$status"; then continue; fi; \
		ipk=""; \
		for cand in $(TOPDIR)/bin/packages/aarch64_cortex-a53/*/$${pkg}_*.ipk \
			$(TOPDIR)/bin/targets/mediatek/filogic/packages/$${pkg}_*.ipk; do \
			[ -f "$$cand" ] || continue; \
			if [ -z "$$ipk" ] || [ "$$cand" -nt "$$ipk" ]; then ipk="$$cand"; fi; \
			[ "$$refresh" = 1 ] || break; \
		done; \
		[ -n "$$ipk" ] || { echo "C2000MAX missing IPK: $$pkg"; exit 1; }; \
		rm -rf $(C2000MAX_EXTRA_IPK_TMP); \
		mkdir -p $(C2000MAX_EXTRA_IPK_TMP); \
		if tar -tf "$$ipk" >/dev/null; then \
			tar -xf "$$ipk" -C $(C2000MAX_EXTRA_IPK_TMP); \
		else \
			( cd $(C2000MAX_EXTRA_IPK_TMP) && ar x "$$ipk" ); \
		fi; \
		data=""; \
		for cand in $(C2000MAX_EXTRA_IPK_TMP)/data.tar.*; do \
			[ -f "$$cand" ] || continue; \
			data="$$cand"; \
			break; \
		done; \
		[ -n "$$data" ] || { echo "C2000MAX missing IPK payload: $$pkg"; exit 1; }; \
		tar -xf "$$data" -C $(C2000MAX_MYOS_ROOTFS_DIR); \
		info="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info"; \
		mkdir -p "$$info"; \
		tar -tf "$$data" | awk '/\/$$/{next} {sub(/^\.\//, "/"); print}' > "$$info/$${pkg}.list"; \
		control=""; \
		for cand in $(C2000MAX_EXTRA_IPK_TMP)/control.tar.*; do \
			[ -f "$$cand" ] || continue; \
			control="$$cand"; \
			break; \
		done; \
		[ "$$refresh" = 0 ] || [ -n "$$control" ] || { echo "C2000MAX missing IPK control: $$pkg"; exit 1; }; \
		if [ -n "$$control" ]; then \
			rm -rf $(C2000MAX_EXTRA_IPK_TMP)/control; \
			mkdir -p $(C2000MAX_EXTRA_IPK_TMP)/control $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg; \
			tar -xf "$$control" -C $(C2000MAX_EXTRA_IPK_TMP)/control; \
			if [ "$$refresh" = 1 ]; then \
				grep -qx "Package: $$pkg" $(C2000MAX_EXTRA_IPK_TMP)/control/control || { echo "C2000MAX mismatched IPK control: $$pkg"; exit 1; }; \
				grep -q '^Version: .' $(C2000MAX_EXTRA_IPK_TMP)/control/control || { echo "C2000MAX missing IPK version: $$pkg"; exit 1; }; \
				if [ -f "$$status" ]; then \
					awk -v pkg="$$pkg" '/^Package: / { drop = ($$2 == pkg) } !drop { print }' "$$status" > "$$status.tmp" && mv "$$status.tmp" "$$status" || exit 1; \
				fi; \
				for meta in control conffiles preinst postinst postinst-pkg prerm postrm; do rm -f "$$info/$$pkg.$$meta"; done; \
			fi; \
			if [ -f $(C2000MAX_EXTRA_IPK_TMP)/control/control ]; then \
				cp -fp $(C2000MAX_EXTRA_IPK_TMP)/control/control "$$info/$${pkg}.control"; \
				for meta in conffiles preinst postinst postinst-pkg prerm postrm; do \
					[ ! -f $(C2000MAX_EXTRA_IPK_TMP)/control/$$meta ] || cp -fp $(C2000MAX_EXTRA_IPK_TMP)/control/$$meta "$$info/$${pkg}.$$meta"; \
				done; \
				status="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status"; \
				if [ ! -f "$$status" ] || ! grep -q "^Package: $${pkg}$$" "$$status"; then \
					kernelver=""; \
					if [ -f "$$status" ]; then \
						kernelver="$$(awk 'BEGIN{p=0} /^Package: kernel$$/{p=1; next} p && /^Version: /{sub(/^Version: /, ""); print; exit} p && /^$$/{p=0}' "$$status")"; \
					fi; \
					awk -v kernelver="$$kernelver" 'BEGIN{has_status=0; has_time=0} /^Status: /{has_status=1} /^Installed-Time: /{has_time=1} kernelver != "" && /^Depends: /{gsub(/kernel[[:space:]]*\(=[^)]*\)/, "kernel (=" kernelver ")")} {print} END{if (!has_status) print "Status: install ok installed"; if (!has_time) print "Installed-Time: 1772529460"; print ""}' $(C2000MAX_EXTRA_IPK_TMP)/control/control >> "$$status"; \
				fi; \
			fi; \
		fi; \
			done
	rm -rf $(C2000MAX_EXTRA_IPK_TMP)
	[ -s $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/libiwinfo.so.20230701 ] || { echo "C2000MAX missing packaged libiwinfo"; exit 1; }
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/iwinfo ] || { echo "C2000MAX missing packaged iwinfo"; exit 1; }
	[ -s $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/libiwinfo/devices.txt ] || { echo "C2000MAX missing packaged iwinfo devices"; exit 1; }
	grep -q '^0x14c3 0x7993 ' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/libiwinfo/devices.txt || \
		printf '%s\n' '0x14c3 0x7993 0x14c3 0x7993    0      0  "MediaTek" "MT7993"' \
		>> $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/libiwinfo/devices.txt
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules/$(LINUX_VERSION)/oaf.ko ] || { echo "C2000MAX missing OAF kernel module"; exit 1; }
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d/oaf ] || { echo "C2000MAX missing OAF autoload entry"; exit 1; }
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/oafd ] || { echo "C2000MAX missing OAF daemon"; exit 1; }
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/controller/oaf.lua ] || { echo "C2000MAX missing OAF LuCI controller"; exit 1; }
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/i18n/oaf.zh-cn.lmo ] || { echo "C2000MAX missing OAF translation"; exit 1; }
	[ -s $(C2000MAX_MYOS_ROOTFS_DIR)/etc/appfilter/feature_cn.cfg ] || { echo "C2000MAX missing OAF signatures"; exit 1; }
	if [ ! -s $(C2000MAX_MYOS_ROOTFS_DIR)/etc/appfilter/feature.cfg ]; then \
		ln -sfn feature_cn.cfg $(C2000MAX_MYOS_ROOTFS_DIR)/etc/appfilter/feature.cfg; \
	fi
	if ! grep -qE "^[[:space:]]*option[[:space:]]+['\"]?user_mode['\"]?[[:space:]]" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/appfilter; then \
		sed -i -E "/^config[[:space:]]+['\"]?global['\"]?[[:space:]]/a\\\toption user_mode '0'" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/appfilter; \
	fi
	grep -qE "^[[:space:]]*option[[:space:]]+['\"]?user_mode['\"]?[[:space:]]" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/appfilter || { echo "C2000MAX missing OAF user mode"; exit 1; }
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-oaf.lua \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/model/c2000max_oaf.lua
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-oaf-entry.lua \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/controller/appfilter.lua
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-oaf-stats.init \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/c2000max-oaf-stats
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/OpenAppFilter/luci-app-oaf/luasrc/view/oaf/dashboard.htm \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/view/oaf/dashboard.htm
	sed -i 's/require "luci.util"/require "luci.model.c2000max_oaf"/g' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/controller/oaf*.lua
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d
	ln -sf ../init.d/appfilter $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S96appfilter
	ln -sf ../init.d/c2000max-oaf-stats $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99c2000max-oaf-stats
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-upnpd.config \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/upnpd
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/sbin/miniupnpd ]
	grep -q 'ext_allow_private_ipv4' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/miniupnpd
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/nftables.d/table-post/20-miniupnpd.nft ]
	[ -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/luci/menu.d/luci-app-upnp.json ]
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d
	ln -sf ../init.d/miniupnpd $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K15miniupnpd
	ln -sf ../init.d/miniupnpd $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S94miniupnpd
	for pkg in kmod-conninfra kmod-mt_wifi_cmn mdadm; do \
		list="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/$${pkg}.list"; \
		if [ -f "$$list" ]; then \
			while IFS= read -r path; do \
				[ -n "$$path" ] || continue; \
				rm -rf "$(C2000MAX_MYOS_ROOTFS_DIR)$${path}"; \
			done < "$$list"; \
		fi; \
		rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/info/$${pkg}.*; \
	done
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mdadm \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/mdadm \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/mdadm \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/*mdadm* \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d/*mt_wifi_cmn* \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/*mt_wifi_cmn* \
		$(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules/*/mt_wifi_cmn.ko
	status="$(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/opkg/status"; \
	if [ -f "$$status" ]; then \
		awk 'BEGIN{split("kmod-conninfra kmod-mt_wifi_cmn mdadm", pkgs); for (i in pkgs) del[pkgs[i]]=1; skip=0} /^Package: /{skip=($$2 in del)} skip && /^$$/{skip=0; next} !skip{print}' "$$status" > "$$status.tmp"; \
		mv "$$status.tmp" "$$status"; \
	fi
	# OSAL supersedes the legacy common module and exports the same wrapper symbols.
	# Defer the remaining vendor WiFi data path until the normal kmodloader pass.
	rm -f \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/10-conninfra \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/50-mt_wifi_osal \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/55-mt_wifi7 \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/58-mt_hwifi \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/59-connac_if \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/61-mtk_pci \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/63-mtk_wed \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules-boot.d/66-mt7993
	[ ! -e $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules/$(LINUX_VERSION)/mt_wifi_cmn.ko ] || { \
		echo "C2000MAX duplicate WiFi common layer present"; exit 1; \
	}
	for module in $(C2000MAX_MYOS_ROOTFS_DIR)/lib/modules/$(LINUX_VERSION)/*.ko; do \
		vermagic="$$(strings "$$module" | awk -F= '$$1 == "vermagic" { print $$2; exit }')"; \
		case "$$vermagic" in \
			"$(LINUX_VERSION) "*) ;; \
			*) echo "C2000MAX module ABI mismatch: $$module: $$vermagic"; exit 1 ;; \
		esac; \
	done
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/controller/istore_backend.lua ] || sed -i "s/localtime = os.time()/localtime = os.date('%Y-%m-%d %H:%M:%S')/" $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/controller/istore_backend.lua
	sh $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/restore-rootfs-modes.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR) $(C2000MAX_MYOS_ROOTFS_PSEUDO)
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/99-default-settings-chinese
	awk 'BEGIN{done=0} /^\tnradio,c2000-max\)/ && !done { print "\tHCMT7987-EMMC|\\"; print "\thcmt7987-emmc|\\"; print; done=1; next } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/02_network > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/02_network.tmp
	mv $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/02_network.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/02_network
	chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/02_network
	awk 'BEGIN{done=0} /^json_is_a network object && exit 0$$/ && !done { print; print ""; print "case \"$$(board_name)\" in"; print "\tHCMT7987-EMMC|hcmt7987-emmc|nradio,c2000-max)"; print "\t\tucidef_set_interface_lan '\''eth1 hnat'\''"; print "\t\tboard_config_flush"; print "\t\texit 0"; print "\t\t;;"; print "esac"; done=1; next } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/99-default_network > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/99-default_network.tmp
	mv $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/99-default_network.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/99-default_network
	chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/etc/board.d/99-default_network
	sed -i 's/192\.168\.7\.1/192.168.66.1/g' $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	printf '%s\n' \
		'src/gz MYOS_core https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/targets/mediatek/filogic/packages' \
		'src/gz MYOS_base https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/base' \
		'src/gz MYOS_luci https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/luci' \
		'src/gz MYOS_packages https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/packages' \
		'src/gz MYOS_routing https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/routing' \
		'src/gz MYOS_telephony https://mirrors.ustc.edu.cn/openwrt/releases/24.10.5/packages/aarch64_cortex-a53/telephony' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/opkg/distfeeds.conf
	printf '%s\n' \
		"DISTRIB_ID='MYOS'" \
		"DISTRIB_RELEASE='$(VERSION_NUMBER)'" \
		"DISTRIB_REVISION='$(REVISION)'" \
		"DISTRIB_TARGET='mediatek/filogic'" \
		"DISTRIB_ARCH='aarch64_cortex-a53'" \
		"DISTRIB_DESCRIPTION='MYOS by ImmortalWrt $(VERSION_NUMBER)'" \
		"DISTRIB_TAINTS='no-all busybox'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/openwrt_release
	printf '%s\n' '$(VERSION_NUMBER)' > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/openwrt_version
	printf '%s\n' \
		'NAME="MYOS"' \
		'VERSION="$(VERSION_NUMBER)"' \
		'ID="myos"' \
		'ID_LIKE="lede openwrt"' \
		'PRETTY_NAME="MYOS by ImmortalWrt $(VERSION_NUMBER)"' \
		'VERSION_ID="$(VERSION_NUMBER)"' \
		'BUILD_ID="$(REVISION)"' \
		'OPENWRT_BOARD="mediatek/filogic"' \
		'OPENWRT_ARCH="aarch64_cortex-a53"' \
		'OPENWRT_TAINTS="no-all busybox"' \
		'OPENWRT_DEVICE_MANUFACTURER="NRadio"' \
		'OPENWRT_DEVICE_PRODUCT="C2000-MAX"' \
		'OPENWRT_RELEASE="MYOS by ImmortalWrt $(VERSION_NUMBER)"' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/os-release
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/uhttpd ] || sed -i "s/option commonname .*/option commonname 'MYOS by ImmortalWrt $(VERSION_NUMBER)'/" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/uhttpd
	printf '\nMYOS by ImmortalWrt $(VERSION_NUMBER)\n\n' > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/banner
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua ] || sed -i 's/^luciname[[:space:]]*=.*/luciname    = "LuCI MYOS branch"/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua ] || sed -i 's/^luciversion[[:space:]]*=.*/luciversion = "26.056.11316~a9f2a15 MYOS by ImmortalWrt $(VERSION_NUMBER)"/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/lib/lua/luci/version.lua
	printf '%s\n' \
		"config wifi-device 'MT7993_1_1'" \
		"	option type 'mtwifi'" \
		"	option phy 'ra0'" \
		"	option band '2g'" \
		"	option dbdc_main '1'" \
		"	option channel 'auto'" \
		"	option txpower '100'" \
		"	option htmode 'EHT40'" \
		"	option country 'CN'" \
		"	option mu_beamformer '1'" \
		"	option noscan '1'" \
		"	option serialize '1'" \
		'' \
		"config wifi-iface 'default_MT7993_1_1'" \
		"	option device 'MT7993_1_1'" \
		"	option network 'lan'" \
		"	option mode 'ap'" \
		"	option ssid 'ImmortalWrt-2.4G'" \
		"	option encryption 'none'" \
		"	option mlo '0'" \
		'' \
		"config wifi-device 'MT7993_1_2'" \
		"	option type 'mtwifi'" \
		"	option phy 'rai0'" \
		"	option band '5g'" \
		"	option dbdc_main '0'" \
		"	option channel 'auto'" \
		"	option txpower '100'" \
		"	option htmode 'EHT160'" \
		"	option country 'CN'" \
		"	option mu_beamformer '1'" \
		"	option noscan '0'" \
		"	option serialize '1'" \
		'' \
		"config wifi-iface 'default_MT7993_1_2'" \
		"	option device 'MT7993_1_2'" \
		"	option network 'lan'" \
		"	option mode 'ap'" \
		"	option ssid 'ImmortalWrt-5G'" \
		"	option encryption 'none'" \
		"	option mlo '0'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/wireless
	for dat in $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993*.dat; do \
		[ -f "$$dat" ] || continue; \
		sed -i \
			-e 's/^Channel=.*/Channel=0/' \
			-e 's/^CountryCode=.*/CountryCode=CN/' \
			-e 's/^CountryRegion=.*/CountryRegion=1/' \
			-e 's/^CountryRegionABand=.*/CountryRegionABand=0/' \
			-e 's/^MldGroup=.*/MldGroup=0/' \
			"$$dat"; \
	done
	for dat in \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b0.dat \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b1.dat; do \
		grep -a -q '^MldGroup=0' "$$dat" || { echo "C2000MAX MLO profile still enabled: $$dat"; exit 1; }; \
	done
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat ] || { \
		grep -q '^INDEX0_debug_level=' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat \
			&& sed -i 's/^INDEX0_debug_level=.*/INDEX0_debug_level=0/' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat \
			|| printf '\nINDEX0_debug_level=0\n' >> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat; \
		grep -q '^INDEX0_debug_option=' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat \
			&& sed -i 's/^INDEX0_debug_option=.*/INDEX0_debug_option=0:0:0:0/' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat \
			|| printf 'INDEX0_debug_option=0:0:0:0\n' >> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/l1profile.dat; \
	}
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303
	rm -rf $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303/config
	cp -fpR $(C2000MAX_FACTORY_LED_ROOTFS)/oem/WT9303/config $(C2000MAX_MYOS_ROOTFS_DIR)/oem/WT9303/config
	printf '%s\n' \
		"config system 'board'" \
		" option name 'WT9303'" \
		" option ptype 'rt'" \
		" option sig_mode 'only'" \
		'' \
		"config system 'feature'" \
		" option cpe '1'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/oem
	printf '%s\n' \
		"config cpesel 'sim'" \
		" option max '4'" \
		" option gval '0-2,1-1,1-2'" \
		" option stype '0,0,4'" \
		" option mode '1'" \
		" option cur '1'" \
		" option default '1'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/cpesel
	install -m 0755 $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-sim-defaults.uci-default \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/33-c2000max-sim-selection-$(VERSION_NUMBER)
	printf '%s\n' \
		"config cpecfg 'config'" \
		'' \
		"config cpesim 'cpesim1'" \
		'' \
		"config cpesim 'cpesim2'" \
		'' \
		"config cpesim 'cpesim3'" \
		'' \
		"config cpesim 'cpesim4'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/cpecfg
	printf '%s\n' \
		"config mode 'mode'" \
		" option en '0'" \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/auto_adapt
	sed -i "/^uci commit/i uci -q set network.lan.ipaddr='192.168.66.1'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.lan.def_ipaddr='192.168.66.1'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch='nrswitch'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch.nvlan='NMNNNNC'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch.ovlan='NMNNNNC'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch.dsa='1'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch.no_switch='1'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set network.nrswitch.disable_wan='1'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].timezone='CST-8'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].zonename='Asia/Shanghai'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].zram_size_mb='256'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].zram_comp_algo='lzo-rle'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].zram_priority='100'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	sed -i "/^uci commit/i uci -q set system.@system[0].swapfile_size_mb='512'" $(C2000MAX_MYOS_ROOTFS_DIR)/bin/config_generate
	printf '%s\n' \
		'#!/bin/sh' \
		'uci -q get system.@system[0] >/dev/null || uci -q add system system >/dev/null' \
		"uci -q set system.@system[0].timezone='CST-8'" \
		"uci -q set system.@system[0].zonename='Asia/Shanghai'" \
		'uci -q commit system' \
		'/etc/init.d/system reload >/dev/null 2>&1 || true' \
		'exit 0' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/10-c2000max-timezone
	chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/10-c2000max-timezone
	printf '%s\n' \
		'#!/bin/sh' \
		'uci -q get system.@system[0] >/dev/null || uci -q add system system >/dev/null' \
		"uci -q set system.@system[0].zram_size_mb='256'" \
		"uci -q set system.@system[0].zram_comp_algo='lzo-rle'" \
		"uci -q set system.@system[0].zram_priority='100'" \
		"uci -q set system.@system[0].swapfile_size_mb='512'" \
		'uci -q commit system' \
		'exit 0' \
		> $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/11-c2000max-zram
	chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/11-c2000max-zram
	install -m 0755 -D $(TOPDIR)/package/system/zram-swap/files/zram.init \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/zram
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_reset_overlay
	printf '%s\n' '$(C2000MAX_ROOTFS_ID)' > \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/c2000max-rootfs-id
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/12-c2000max-format-data $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/13-c2000max-reset-data-*
	install -m 0755 -D $(TOPDIR)/package/base-files/files/sbin/firstboot \
		$(C2000MAX_MYOS_ROOTFS_DIR)/sbin/firstboot
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-upgrade.platform \
		$(C2000MAX_MYOS_ROOTFS_DIR)/lib/upgrade/platform.sh
	install -m 0644 -D $(TOPDIR)/package/base-files/files/lib/upgrade/emmc.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR)/lib/upgrade/emmc.sh
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-sd-extroot.preinit \
		$(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-bootlog.init \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/c2000max-bootlog
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-bootlog \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-bootlog
	ln -sf ../init.d/c2000max-bootlog $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S12c2000max-bootlog
	# Use the live-verified resolvers in the image itself, before cellular DHCP.
	grep -q '^config dnsmasq' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/dhcp
	sed -i -e '/^[[:space:]]*option noresolv /d' \
		-e "/^[[:space:]]*list server '223[.]5[.]5[.]5'$$/d" \
		-e "/^[[:space:]]*list server '114[.]114[.]114[.]114'$$/d" \
		-e "/^config dnsmasq/a\\\toption noresolv '1'\n\tlist server '223.5.5.5'\n\tlist server '114.114.114.114'" \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/dhcp
	chmod 0755 $(C2000MAX_MYOS_ROOTFS_DIR)/usr/sbin/resize2fs
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/myos
	chmod 0755 $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/myos
	$(TARGET_CC) $(TARGET_CFLAGS) -Os -Wall -Wextra -Werror -static \
		$(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-gpt.c \
		-o $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-gpt
	chmod 0755 $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-gpt
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/ubenv-snsd \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-ubenv
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-boot \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-boot
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/return-myos.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd/return-myos.sh
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/recovery-README.txt \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd/recovery-README.txt
	cp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-gpt \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd/
	cp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/myos-max-ubenv \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd/
	tar -C $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-max-sd -cf \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/myos-max-recovery.tar \
		myos-max-gpt myos-max-ubenv return-myos.sh recovery-README.txt
	chmod 0644 $(C2000MAX_MYOS_ROOTFS_DIR)/www/myos-max-recovery.tar
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-sd.uc \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/rpcd/ucode/myos-max-sd
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-sd-acl.json \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/rpcd/acl.d/myos-max-sd.json
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/myos-max-sd-menu.json \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/luci/menu.d/myos-max-sd.json
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/sd-tools/max_sd.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/myos/max_sd.js
	grep -q '\.c2000max-reset-on-boot' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/firstboot
	grep -q 'p7 will be preserved' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/firstboot
	grep -qxF '$(C2000MAX_ROOTFS_ID)' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/c2000max-rootfs-id
	grep -q 'resetting system overlay without formatting p7' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	grep -q 'initializing existing unformatted p7' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	grep -q 'blkid -p -D "' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	grep -q 'desired=.*disk_sectors - p7_start - 33' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	grep -q 'Commit the new ID only after the clean overlay is durable' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	[ "$$(grep -c '^[[:space:]]*mkfs\.ext4 -F -L data' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot)" -eq 1 ]
	! grep -q 'formatting p7 extroot' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	! grep -q 'C2000MAX_SD_RESET_GENERATION' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/preinit/79_c2000max_sd_extroot
	for path in usr/sbin/fdisk sbin/partprobe usr/sbin/blkid usr/sbin/mkfs.ext4 usr/sbin/resize2fs usr/libexec/myos-max-gpt bin/mknod sbin/block etc/config/fstab; do \
		[ -e $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} ] || { echo "C2000MAX extroot missing runtime file: $${path}"; exit 1; }; \
	done
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/luci-app-mtwifi-cfg/root/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/luci-app-mtwifi-cfg/root/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/luci-app-mtwifi-cfg/root/etc/uci-defaults/luci-app-mtwifi-cfg.default \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/luci-app-mtwifi-cfg.default
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/mtwifi-cfg/files/mtwifi-cfg/mtwifi_cfg \
		$(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/31-c2000max-mtwifi-wpa3
	install -m 0755 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-mtwifi-security.uci-default \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/32-c2000max-mtwifi-security
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/99-at-webserver-enable
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S??at-webserver
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/5g-modem/luci-app-5700webui/root/etc/config/at-webserver \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/at-webserver
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/5g-modem/luci-app-5700webui/root/etc/init.d/at-webserver \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/at-webserver
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/5g-modem/luci-app-5700webui/root/etc/init.d/ubus-at-daemon \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/ubus-at-daemon
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/5g-modem/luci-app-5700webui/root/www/luci-static/resources/view/embed/htmlview.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/embed/htmlview.js
	rm -rf $(C2000MAX_MYOS_ROOTFS_DIR)/www/5700
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/www/scripts/loading.js
	ln -sf ../init.d/mt5700-web $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S97mt5700-web
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/5g-modem/luci-app-5700webui/root/www/cgi-bin/at-ws-info \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/cgi-bin/at-ws-info
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/at-webserver ] || { echo "C2000MAX missing at-webserver init script"; exit 1; }
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/www/cgi-bin/at-ws-info ] || { echo "C2000MAX missing at-webserver CGI"; exit 1; }
	python3 $(TOPDIR)/target/linux/mediatek/image/finalize-dual-theme.py \
		$(C2000MAX_MYOS_ROOTFS_DIR) $(TOPDIR)
	cmp -s $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q 'CBIWifiFrequencyValue' $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q "'be', 'BE'" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q "'EHT160'" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q 'var fallback = (bandval' $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q 'name = name.replace(/ unknown' $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -A 10 "else if (hwtype == 'mtwifi')" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js | grep -q "\['psk2',[[:space:]]*'WPA2-PSK'"
	! grep -A 10 "else if (hwtype == 'mtwifi')" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js | grep -q "\['psk',[[:space:]]*'WPA-PSK'"
	grep -A 10 "else if (hwtype == 'mtwifi')" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js | grep -q "\['sae',[[:space:]]*'WPA3-SAE'"
	grep -A 10 "else if (hwtype == 'mtwifi')" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js | grep -q "\['sae-mixed',[[:space:]]*'WPA2-PSK/WPA3-SAE Mixed Mode'"
	grep -q "uci.set('wireless', section_id, 'key', value)" $(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/network/wireless.js
	grep -q 'config_foreach migrate_legacy_ap_psk wifi-iface' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/32-c2000max-mtwifi-security
	if ! grep -q '^find_qmodem_wan_iface()' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh; then \
		sed -i 's/gLanPorts=$$(get_ports '\''L'\'')/gLanPorts=$$(get_ports "$$SWITCH_VALID_LAN_PORT_TYPES")/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{done=0} /^get_sig_info\(\)\{/ && !done { print "find_qmodem_wan_iface() {"; print "\tlocal _section=\"$$1\""; print "\tlocal _modem="; print "\tlocal _up="; print "\t[ -n \"$$gQmodemWan\" ] && return"; print "\tconfig_get _modem \"$$_section\" modem_config"; print "\t[ -n \"$$_modem\" ] || return"; print "\t_up=$$(ifstatus \"$$_section\" 2>/dev/null | jsonfilter -e \"@.up\" 2>/dev/null)"; print "\t[ \"$$_up\" = \"true\" ] || return"; print "\tgQmodemWan=\"$$_section\""; print "\tgQmodemCfg=\"$$_modem\""; print "}"; print ""; print "find_qmodem_wan() {"; print "\tgQmodemWan="; print "\tgQmodemCfg="; print "\tconfig_load network"; print "\tconfig_foreach find_qmodem_wan_iface interface"; print "\t[ -n \"$$gQmodemWan\" ]"; print "}"; print ""; print "get_qmodem_rsrp() {"; print "\tlocal _rsrp="; print "\tfind_qmodem_wan || return 1"; print "\tfor _rsrp in $$(/usr/share/qmodem/modem_ctrl.sh cell_info \"$$gQmodemCfg\" 2>/dev/null | jq -r \".modem_info[]? | select(.key==\\\"RSRP\\\") | .value\" 2>/dev/null); do"; print "\t\tcase \"$$_rsrp\" in"; print "\t\t-[0-9]*|[0-9]*)"; print "\t\t\techo \"$${_rsrp%.*}\""; print "\t\t\treturn 0"; print "\t\t\t;;"; print "\t\tesac"; print "\tdone"; print "\treturn 1"; print "}"; print ""; done=1 } { print }' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{skip=0} /^get_sig_info\(\)\{/ { print "get_sig_info(){"; print "\tlocal _rsrp="; print "\tif [ -n \"$$1\" ]; then"; print "\t\t_rsrp=$$(ubus call infocd cpestatus 2>/dev/null | jsonfilter -e \"@.*[@.status.name=\\\"$${1}\\\"]\" 2>/dev/null | jsonfilter -e \"@.status\" 2>/dev/null | jsonfilter -e \"@.rsrp\" 2>/dev/null)"; print "\tfi"; print "\t[ -n \"$$_rsrp\" ] && { echo \"$$_rsrp\"; return; }"; print "\tget_qmodem_rsrp"; print "}"; skip=1; next } skip && /^}/ { skip=0; next } !skip { print }' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{skip=0} /^get_qmodem_rsrp\(\) \{/ { print "get_qmodem_rsrp() {"; print "\tlocal _rsrp="; print "\t_rsrp=$$(jsonfilter -i /tmp/myos-nradio-data/base.json -e @.value.rsrp 2>/dev/null)"; print "\tcase \"$$_rsrp\" in"; print "\t\t-[0-9]*|[0-9]*) echo \"$${_rsrp%.*}\"; return 0 ;;"; print "\tesac"; print "\treturn 1"; print "}"; skip=1; next } skip && /^}/ {skip=0; next} !skip {print}' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{done=0} /^check_cell_stat\(\) \{/ && !done { print "find_qmodem_config_iface() {"; print "\tlocal _section=\"$$1\""; print "\tlocal _modem="; print "\t[ -n \"$$gQmodemConfigWan\" ] && return"; print "\tconfig_get _modem \"$$_section\" modem_config"; print "\t[ -n \"$$_modem\" ] || return"; print "\tgQmodemConfigWan=\"$$_section\""; print "}"; print ""; print "find_qmodem_config() {"; print "\tgQmodemConfigWan="; print "\tconfig_load network"; print "\tconfig_foreach find_qmodem_config_iface interface"; print "\t[ -n \"$$gQmodemConfigWan\" ]"; print "}"; print ""; print "set_cell_sig_blink() {"; print "\t/sbin/setled timer blue sig1 500 500"; print "\t/sbin/setled timer blue sig2 500 500"; print "\t/sbin/setled timer blue sig3 500 500"; print "}"; print ""; print "stop_cell_sig_blink() {"; print "\t/sbin/setled off blue sig1"; print "\t/sbin/setled off blue sig2"; print "\t/sbin/setled off blue sig3"; print "}"; print ""; done=1 } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{in_func=0; done=0} /^check_wan\(\) \{/ { in_func=1 } in_func && /^[[:space:]]*if \[ -n "\$$_dev" \]; then/ && !done { print "\tif find_qmodem_wan; then"; print "\t\t[ -n \"$$_dev\" ] && onoff_led \"$$_dev\" 1"; print "\t\tgOnlineType=\"cpe\""; print "\t\treturn 0"; print "\tfi"; print ""; done=1 } { print } in_func && /^}/ { in_func=0 }' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		awk 'BEGIN{in_func=0; done=0} /^check_cell_stat\(\) \{/ { in_func=1 } in_func && /^[[:space:]]*\[ -z "\$$gLedSig" \]/ && !done { print "\tif [ -z \"$$STAT$$MODE$$SIM$$name\" ]; then"; print "\t\tif find_qmodem_wan; then"; print "\t\t\tstop_cell_sig_blink"; print "\t\t\tgCpeStat=\"register\""; print "\t\t\tcheck_cell_sig \"\""; print "\t\t\treturn 0"; print "\t\telif find_qmodem_config; then"; print "\t\t\tset_cell_sig_blink"; print "\t\t\treturn 0"; print "\t\tfi"; print "\tfi"; print ""; done=1 } { print } in_func && /^}/ { in_func=0 } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		grep -q '^set_cell_sig_blink()' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		grep -q '/sbin/setled timer blue sig1 500 500' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh && \
		grep -q 'elif find_qmodem_config; then' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ledctrl.sh; \
	fi
	if ! grep -q '^c2000max_eth1_wan_enabled()' $(C2000MAX_MYOS_ROOTFS_DIR)/lib/network/switch.sh; then \
		awk -f $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/patch-c2000max-switch-eth1-wan.awk $(C2000MAX_MYOS_ROOTFS_DIR)/lib/network/switch.sh > $(C2000MAX_MYOS_ROOTFS_DIR)/lib/network/switch.sh.tmp && \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/lib/network/switch.sh.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/lib/network/switch.sh; \
	fi
	mkdir -p $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share
	cp -fpR $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/share/qmodem $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/
	cp -fpR $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/share/rpcd $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/
	install -m 0755 -D $(TOPDIR)/package/mtk/applications/c2000max-5g/files/luci-app-qmodem-next/usr/share/rpcd/ucode/qmodem_sms \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/rpcd/ucode/qmodem_sms
	install -m 0644 -D $(TOPDIR)/package/mtk/applications/c2000max-5g/files/luci-app-qmodem-next/www/luci-static/resources/qmodem/sms.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/qmodem/sms.js
	cp -fpR $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/libexec/rpcd $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/config/qmodem $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/qmodem
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/init.d/pcie_mhi $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/pcie_mhi
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/init.d/qmodem_init $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/qmodem_init
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/init.d/qmodem_network $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/qmodem_network
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/init.d/qmodem_reboot $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/qmodem_reboot
	cp -fp $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-sim-select.init $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/c2000max-sim-select
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-simcard.js \
		$(C2000MAX_MYOS_ROOTFS_DIR)/www/luci-static/resources/view/embed/simcard.js
	cp -fp $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/c2000max-mac.init $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/c2000max-mac
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/uci-defaults/99-add-5g-handler $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/99-add-5g-handler
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/etc/uci-defaults/luci-app-qmodem $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/luci-app-qmodem
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/share/qmodem/modem_ctrl.sh $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem/modem_ctrl.sh
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/share/qmodem/modem_dial.sh $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem/modem_dial.sh
	cp -fp $(TOPDIR)/package/mtk/applications/c2000max-5g/files/rootfs/usr/share/qmodem/vendor/huawei.sh $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem/vendor/huawei.sh
	find $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/rpcd $(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/rpcd -type d -exec chmod 0755 {} +
	chmod 0755 $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem/*.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/rpcd/qmodem \
		$(C2000MAX_MYOS_ROOTFS_DIR)/usr/libexec/rpcd/modem_ctrl \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/qmodem_init \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/qmodem_network \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/c2000max-sim-select
	sh $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/MYOS-nros-app-patch.sh \
		$(C2000MAX_MYOS_ROOTFS_DIR) $(C2000MAX_FACTORY_LED_ROOTFS)
	# Match MWRT's bounded first-boot scan instead of reading the whole SD data partition.
	sed -i 's#strings -n 6 "$$rootfs_dev" > "$$dump"#dd if="$$rootfs_dev" bs=1M count=256 2>/dev/null | strings -n 6 > "$$dump"#' \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/29-myos-nros-app
	# Boot recovery: old ABI cloud payload makes rootfs exceed fixed p6 rootfs_data boundary.
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/turboacc ] || sed -i 's/"turboacc.config.fullcone"="1"/"turboacc.config.fullcone"="0"/' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/turboacc
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b0.dat ] || sed -i 's/^CCKTxStream=4/CCKTxStream=2/' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b0.dat
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b1.dat ] || sed -i 's/^CCKTxStream=4/CCKTxStream=2/' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/wireless/mediatek/mt7993.b1.dat
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg ] || grep -q 'keep C2000MAX MT7993 split module stack loaded' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg || { awk 'BEGIN{done=0} /^function mtwifi_reinstall\(\)/ && !done { print; print "    if utils.exists(\"/sys/module/mt7993\") and utils.exists(\"/sys/module/mtk_wed\") then"; print "        nixio.syslog(\"info\", \"mtwifi-cfg: keep C2000MAX MT7993 split module stack loaded\")"; print "        return"; print "    end"; print ""; done=1; next } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg > $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp; mv $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; }
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg ] || grep -q '^function c2000max_ser_recover' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg || { awk 'BEGIN{done=0} /^function ifup\(ifname\)/ && !done { print "function c2000max_ser_recover(ifname)"; print "    if ifname ~= \"rai0\" then return end"; print "    if utils.trim(utils.read_pipe(\"cat /tmp/sysinfo/board_name 2>/dev/null\")) ~= \"nradio,c2000-max\" then return end"; print "    if not utils.exists(\"/sys/module/mt7993\") or not utils.exists(\"/sys/module/mtk_wed\") then return end"; print ""; print "    nixio.syslog(\"info\", \"mtwifi-cfg: trigger C2000MAX MT7993 L1 SER recovery\")"; print "    os.execute(\"iwpriv rai0 set ser=995 >/dev/null 2>&1\")"; print "end"; print ""; done=1 } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg > $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp; mv $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; }
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg ] || grep -q 'c2000max_ser_recover(dev.main_ifname)' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg || { awk 'BEGIN{done=0} /^[[:space:]]*mtwifi_cfg_iwpriv_hook\(cfg\)/ && !done { print; print "    c2000max_ser_recover(dev.main_ifname)"; done=1; next } { print } END{if (!done) exit 1}' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg > $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp; mv $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; chmod 755 $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg; }
	cmp -s $(TOPDIR)/package/mtk/applications/mtwifi-cfg/files/mtwifi-cfg/mtwifi_cfg $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg
	grep -q 'MT7993_1_1 = { band = "2g", htmode = "EHT40", channel = "auto" }' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/mtwifi_cfg
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/modules.d/59-mtk_pse_shim
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall ] || awk 'BEGIN{done=0} /^[[:space:]]*config[[:space:]]*defaults/ { print; print "\toption flow_offloading 0"; print "\toption flow_offloading_hw 0"; print "\toption fullcone 0"; print "\toption fullcone6 0"; done=1; next } /^[[:space:]]*option[[:space:]]+(flow_offloading|flow_offloading_hw|fullcone|fullcone6)[[:space:]]/ { next } { print } END{ if (!done) exit 1 }' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall.tmp
	mv $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall ] || for section in homeproxy_forward homeproxy_input homeproxy_post; do \
		awk -v section="$${section}" 'BEGIN{insec=0; done=0} /^[[:space:]]*config[[:space:]]+/ { if (insec && !done) print "\toption enabled '\''0'\''"; insec=($$0 ~ "config[[:space:]]+include[[:space:]]+'" section "'"); done=0 } insec && /^[[:space:]]*option[[:space:]]+enabled[[:space:]]/ { print "\toption enabled '\''0'\''"; done=1; next } { print } END{ if (insec && !done) print "\toption enabled '\''0'\''" }' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall > $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall.tmp; \
		mv $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall.tmp $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall; \
	done
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/luci-homeproxy ] || for section in homeproxy_forward homeproxy_input homeproxy_post; do \
		grep -q "set firewall.$${section}.enabled" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/luci-homeproxy || sed -i "/set firewall.$${section}=include/a\\\tset firewall.$${section}.enabled=\"0\"" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/luci-homeproxy; \
	done
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/turboacc ] || sed -i "s/^[[:space:]]*option[[:space:]]*fullcone[[:space:]].*/\toption fullcone '0'/" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/turboacc
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/bootcount ] || sed -i 's#^[[:space:]]*/usr/sbin/telnetd -l /bin/login#[ ! -x /usr/sbin/telnetd ] || /usr/sbin/telnetd -l /bin/login#' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/bootcount
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/fancontrol ] || { echo "C2000MAX missing packaged fancontrol"; exit 1; }
	install -m 0755 $(TOPDIR)/package/fancontrol/files/fancontrol.init $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/fancontrol
	install -m 0644 $(TOPDIR)/package/fancontrol/files/fancontrol.config $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/fancontrol
	ln -sf ../init.d/fancontrol $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99fancontrol
	cmp -s $(TOPDIR)/package/fancontrol/files/fancontrol.init $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/fancontrol
	cmp -s $(TOPDIR)/package/fancontrol/files/fancontrol.config $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/fancontrol
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/smp.sh ] || sed -i 's/ethtool -k "$$iface" |/ethtool -k "$$iface" 2>\/dev\/null |/' $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/smp.sh
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/ucode/fw4.uc ] || sed -i 's/if (!nft_try_fullcone()) {/if ((defs.fullcone || defs.fullcone6) \&\& !nft_try_fullcone()) {/' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/ucode/fw4.uc
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/ucode/fw4.uc ] || sed -i '/delete defs.fullcone;/a\            delete defs.fullcone6;' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/ucode/fw4.uc
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/firewall4/templates/ruleset.uc ] || sed -i '/^[[:space:]]*flags offload;[[:space:]]*$$/d' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/firewall4/templates/ruleset.uc
	grep -q 'option flow_offloading 0' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall
	grep -q 'option flow_offloading_hw 0' $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/firewall
	! grep -q '^[[:space:]]*flags offload;[[:space:]]*$$' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/firewall4/templates/ruleset.uc
	grep -q '^[[:space:]]*return \[\];[[:space:]]*$$' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/ucode/fw4.uc
	ln -sf ../init.d/c2000max-mac $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S18c2000max-mac
	ln -sf ../init.d/myos-factory-wifi $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S19myos-factory-wifi
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/web/luci.locations \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/nginx/conf.d/luci.locations
	install -m 0644 -D $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/web/luci-cgi_io.ini \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/uwsgi/vassals/luci-cgi_io.ini
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/*uhttpd*
	for service in uwsgi nginx quickfile; do \
		init=$(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/$${service}; \
		[ -x "$$init" ] || { echo "C2000MAX missing init service: $$service"; exit 1; }; \
		start="$$(sed -n 's/^START=//p' "$$init" | head -n 1)"; \
		[ -n "$$start" ] || { echo "C2000MAX missing START for service: $$service"; exit 1; }; \
		ln -sf ../init.d/$${service} $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S$${start}$${service}; \
	done
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S25switch_ctl
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S95embed_simcard
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S??c2000max-sim-select
	ln -sf ../init.d/c2000max-sim-select $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S97c2000max-sim-select
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S??qmodem_network
	ln -sf ../init.d/qmodem_network $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S98qmodem_network
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S??ubus-at-daemon
	ln -sf ../init.d/ubus-at-daemon $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S18ubus-at-daemon
	[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/ubus-at-daemon ] && [ -x $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/tom_modem ]
	grep -q "option use_ubus '1'" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/qmodem
	grep -q 'parse_pdp_addresses' $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/qmodem/modem_dial.sh
	ln -sf ../init.d/wanchk $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K10wanchk
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S90wanchk
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/wanchk ] || sed -i "s/option 'enable' '1'/option 'enable' '0'/" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/wanchk
	[ ! -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/linkease ] || sed -i "s/option enabled '1'/option enabled '0'/" $(C2000MAX_MYOS_ROOTFS_DIR)/etc/config/linkease
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S19zram
	ln -sf ../init.d/zram $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S15zram
	ln -sf ../usr/libexec/stty-coreutils $(C2000MAX_MYOS_ROOTFS_DIR)/bin/stty
	# Native-rootfs IPK extraction does not run the alternatives post-install step.
	for path in usr/libexec/ip-full usr/libexec/tc-full usr/libexec/base64-coreutils usr/libexec/nohup-coreutils usr/libexec/timeout-coreutils; do \
		[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/$$path ] || { echo "C2000MAX missing dependency command: $$path"; exit 1; }; \
	done
	ln -sf ../usr/libexec/ip-full $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/ip
	ln -sf ../usr/libexec/tc-full $(C2000MAX_MYOS_ROOTFS_DIR)/sbin/tc
	ln -sf ../usr/libexec/base64-coreutils $(C2000MAX_MYOS_ROOTFS_DIR)/bin/base64
	ln -sf ../libexec/nohup-coreutils $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/nohup
	ln -sf ../libexec/timeout-coreutils $(C2000MAX_MYOS_ROOTFS_DIR)/usr/bin/timeout
	for path in etc/init.d usr/bin usr/sbin; do \
		[ -e $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} ] || continue; \
		find $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} -type f -exec chmod 755 {} +; \
	done
	for path in www/luci-static usr/lib/lua/luci/view usr/lib/lua/luci/controller usr/lib/lua/luci/model usr/share/luci/menu.d; do \
		[ -e $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} ] || continue; \
		find $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} -type d -exec chmod 755 {} +; \
		find $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} -type f -exec chmod 644 {} +; \
	done
	# IPK directory modes must not make ubus, rpcd, or dnsmasq lose access to /usr.
	for path in . etc usr usr/bin usr/sbin usr/lib usr/lib/lua usr/lib/lua/luci usr/share usr/share/rpcd usr/share/rpcd/acl.d www; do \
		[ -d $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} ] || { echo "C2000MAX missing public directory: $$path"; exit 1; }; \
		chmod 0755 $(C2000MAX_MYOS_ROOTFS_DIR)/$${path} || exit 1; \
	done
	chmod 0400 $(C2000MAX_MYOS_ROOTFS_DIR)/usr/share/myos-nros/id_rsa.db
	python3 $(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max/install-mwrt-packages.py $(C2000MAX_MYOS_ROOTFS_DIR) $(TOPDIR) kmod-usb-net-huawei-cdc-ncm kmod-qmi_wwan_s kmod-fs-msdos kmod-nls-cp932 kmod-nls-cp936 kmod-nls-cp950 kmod-bonding quectel-CM-5G tar mount-utils luci-app-openclash luci-app-passwall luci-i18n-passwall-zh-cn cpetools mwrt-openclash-core ipt2socks nftables kmod-nft-socket kmod-nft-tproxy kmod-nft-nat kmod-nf-reject kmod-nf-reject6 geoview haproxy shadowsocks-rust-sslocal shadowsocks-rust-ssserver shadowsocksr-libev-ssr-local shadowsocksr-libev-ssr-redir simple-obfs-client sing-box v2ray-plugin xray-core unzip
	for service in openclash passwall passwall_server; do \
		[ -x $(C2000MAX_MYOS_ROOTFS_DIR)/etc/init.d/$${service} ] || { echo "C2000MAX missing proxy init: $$service"; exit 1; }; \
		ln -sf ../init.d/$${service} $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99$${service}; \
	done
	for service in openclash passwall; do \
		ln -sf ../init.d/$${service} $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K15$${service}; \
	done
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/uci-defaults/30_luci-theme-argon
	rm -f $(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S92quickstart \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S60etherwake \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99ddns-go \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/S99homeproxy \
		$(C2000MAX_MYOS_ROOTFS_DIR)/etc/rc.d/K10homeproxy
	python3 $(TOPDIR)/scripts/prepare-public-rootfs.py $(C2000MAX_MYOS_ROOTFS_DIR)
	$(STAGING_DIR_HOST)/bin/mksquashfs4 $(C2000MAX_MYOS_ROOTFS_DIR) $(C2000MAX_MYOS_ROOTFS) \
		-nopad -noappend -root-owned -comp xz -Xpreset 9 -Xe -Xlc 0 -Xlp 2 -Xpb 2 \
		-b 1048576 -no-xattrs \
		-pf $(C2000MAX_MYOS_ROOTFS_PSEUDO)
	chmod 0644 $(C2000MAX_MYOS_ROOTFS)
endef

define Build/append-c2000max-rootfs
	size_before=$$(stat -c%s $@); \
	[ "$$size_before" -eq "$(C2000MAX_FACTORY_ROOTFS_OFFSET)" ] || { echo "C2000MAX unexpected rootfs offset: $$size_before"; exit 1; }
	rootfs_size=$$(stat -c%s $(C2000MAX_MYOS_ROOTFS)); \
	[ "$$rootfs_size" -lt "$(C2000MAX_ROOTFS_SIZE_LIMIT)" ] || { echo "C2000MAX rootfs exceeds size limit: $$rootfs_size"; exit 1; }
	dd if=$(C2000MAX_MYOS_ROOTFS) >> $@
endef

define Build/reset-c2000max-rootfs-data
	rootfs_size=$$(stat -c%s $(C2000MAX_MYOS_ROOTFS)); \
	image_size=$$(stat -c%s $@); \
	expected_size=$$(( $(C2000MAX_FACTORY_ROOTFS_OFFSET) + rootfs_size )); \
	[ "$$image_size" -eq "$$expected_size" ] || { echo "C2000MAX unexpected image size before rootfs_data reset: $$image_size"; exit 1; }; \
	rootfs_data_offset=$$(( (rootfs_size + 65535) / 65536 * 65536 )); \
	rootfs_data_start=$$(( $(C2000MAX_FACTORY_ROOTFS_OFFSET) + rootfs_data_offset )); \
	truncate -s $$(( $(C2000MAX_FACTORY_ROOTFS_OFFSET) + $(C2000MAX_ROOTFS_SIZE_LIMIT) + 8192 )) $@; \
	echo "C2000MAX rootfs_data reset bytes: offset=$$rootfs_data_offset absolute=$$rootfs_data_start"
endef

define Build/c5800-688-factory-kernel
	rm -f $@
	cp $(TOPDIR)/target/linux/mediatek/image/nradio-c5800-688/kernel.itb $@
endef

define Build/c5800-688-rootfs-system
	rm -f $(IMAGE_ROOTFS)
	cp $(TOPDIR)/target/linux/mediatek/image/nradio-c5800-688/root.squashfs $(IMAGE_ROOTFS)
endef

define Build/c5800-688-sysupgrade-tar
	rm -rf $@.sysupgrade
	mkdir -p $@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))
	echo "BOARD=$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))" > \
		$@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/CONTROL
	dd if=$(IMAGE_ROOTFS) \
		of=$@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/root \
		bs=1024 conv=sync
	cp $(IMAGE_KERNEL) \
		$@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/kernel
	chmod 755 $@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))
	chmod 644 $@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/CONTROL \
		$@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/kernel \
		$@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/root
	(cd $@.sysupgrade; tar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime=@1772464398 -cf sysupgrade.tar \
		sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME)))
	mkdir -p $@.sysupgrade/sysupgrade-HCMT7981-emmc
	cp $@.sysupgrade/sysupgrade-$(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME))/CONTROL \
		$@.sysupgrade/sysupgrade-HCMT7981-emmc/CONTROL
	(cd $@.sysupgrade; tar --owner=0 --group=0 --numeric-owner \
		--mtime=@1772464398 -rf sysupgrade.tar \
		sysupgrade-HCMT7981-emmc/CONTROL)
	cp $@.sysupgrade/sysupgrade.tar $@
	rm -rf $@.sysupgrade
endef

define Build/c5800-688-append-metadata
	printf '%s\n' \
		'{  "metadata_version": "1.1", "compat_version": "1.0",   "supported_devices":["nradio,c5800-688","HCMT7981-emmc"], "version": { "dist": "MYOS V1.0.0 Designed by ImmortalWrt", "version": "V1.0.0", "revision": "r33512-4c657c9354", "target": "mediatek/filogic", "board": "nradio_c5800-688" } }' \
		> $@.metadata
	fwtool -I $@.metadata $@
	rm -f $@.metadata
endef

define Build/cetron-header
	$(eval magic=$(word 1,$(1)))
	$(eval model=$(word 2,$(1)))
	( \
		dd if=/dev/zero bs=856 count=1 2>/dev/null; \
		printf "$(model)," | dd bs=128 count=1 conv=sync 2>/dev/null; \
		md5sum $@ | cut -f1 -d" " | dd bs=32 count=1 2>/dev/null; \
		printf "$(magic)" | dd bs=4 count=1 conv=sync 2>/dev/null; \
		cat $@; \
	) > $@.tmp
	fw_crc=$$(gzip -c $@.tmp | tail -c 8 | od -An -N4 -tx4 --endian little | tr -d ' \n'); \
	printf "$$(echo $$fw_crc | sed 's/../\\x&/g')" | cat - $@.tmp > $@
	rm $@.tmp
endef

define Device/abt_asr3000
  DEVICE_VENDOR := ABT
  DEVICE_MODEL := ASR3000
  DEVICE_DTS := mt7981b-abt-asr3000
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 113152k
  KERNEL_IN_UBI := 1  
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += abt_asr3000

define Device/acelink_ew-7886cax
  DEVICE_VENDOR := Acelink
  DEVICE_MODEL := EW-7886CAX
  DEVICE_DTS := mt7986a-acelink-ew-7886cax
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += acelink_ew-7886cax

define Device/acer_predator-w6
  DEVICE_VENDOR := Acer
  DEVICE_MODEL := Predator Connect W6
  DEVICE_DTS := mt7986a-acer-predator-w6
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7986-firmware kmod-mt7916-firmware \
	mt7986-wo-firmware f2fsck mkf2fs automount
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += acer_predator-w6

define Device/acer_predator-w6d
  DEVICE_VENDOR := Acer
  DEVICE_MODEL := Predator Connect W6d
  DEVICE_DTS := mt7986a-acer-predator-w6d
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7916-firmware kmod-mt7986-firmware mt7986-wo-firmware e2fsprogs f2fsck mkf2fs
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += acer_predator-w6d

define Device/acer_vero-w6m
  DEVICE_VENDOR := Acer
  DEVICE_MODEL := Connect Vero W6m
  DEVICE_DTS := mt7986a-acer-vero-w6m
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES := kmod-leds-ktd202x kmod-mt7915e kmod-mt7916-firmware kmod-mt7986-firmware mt7986-wo-firmware e2fsprogs f2fsck mkf2fs
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += acer_vero-w6m

define Device/adtran_smartrg
  DEVICE_VENDOR := Adtran
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := e2fsprogs f2fsck mkf2fs kmod-hwmon-pwmfan
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef

define Device/smartrg_sdg-8612
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8612
  DEVICE_DTS := mt7986a-smartrg-SDG-8612
  DEVICE_PACKAGES += kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8612

define Device/smartrg_sdg-8614
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8614
  DEVICE_DTS := mt7986a-smartrg-SDG-8614
  DEVICE_PACKAGES += kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8614

define Device/smartrg_sdg-8622
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8622
  DEVICE_DTS := mt7986a-smartrg-SDG-8622
  DEVICE_PACKAGES += kmod-mt7915e kmod-mt7915-firmware kmod-mt7986-firmware mt7986-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8622

define Device/smartrg_sdg-8632
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8632
  DEVICE_DTS := mt7986a-smartrg-SDG-8632
  DEVICE_PACKAGES += kmod-mt7915e kmod-mt7915-firmware kmod-mt7986-firmware mt7986-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8632

define Device/smartrg_sdg-8733
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8733
  DEVICE_DTS := mt7988a-smartrg-SDG-8733
  DEVICE_PACKAGES += kmod-mt7996-firmware kmod-phy-aquantia kmod-usb3 mt7988-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8733

define Device/smartrg_sdg-8733a
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8733A
  DEVICE_DTS := mt7988d-smartrg-SDG-8733A
  DEVICE_PACKAGES += mt7988-2p5g-phy-firmware kmod-mt7996-233-firmware kmod-phy-aquantia mt7988-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8733a

define Device/smartrg_sdg-8734
$(call Device/adtran_smartrg)
  DEVICE_MODEL := SDG-8734
  DEVICE_DTS := mt7988a-smartrg-SDG-8734
  DEVICE_PACKAGES += kmod-mt7996-firmware kmod-phy-aquantia kmod-sfp kmod-usb3 mt7988-wo-firmware
endef
TARGET_DEVICES += smartrg_sdg-8734

define Device/asus_rt-ax52
  DEVICE_VENDOR := ASUS
  DEVICE_MODEL := RT-AX52
  DEVICE_DTS := mt7981b-asus-rt-ax52
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += asus_rt-ax52

define Device/asus_rt-ax59u
  DEVICE_VENDOR := ASUS
  DEVICE_MODEL := RT-AX59U
  DEVICE_DTS := mt7986a-asus-rt-ax59u
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware automount
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += asus_rt-ax59u

define Device/asus_tuf-ax4200
  DEVICE_VENDOR := ASUS
  DEVICE_MODEL := TUF-AX4200
  DEVICE_DTS := mt7986a-asus-tuf-ax4200
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware automount
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += asus_tuf-ax4200

define Device/arcadyan_mozart
  DEVICE_VENDOR := Arcadyan
  DEVICE_MODEL := Mozart
  DEVICE_DTS := mt7988a-arcadyan-mozart
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x45f00000
  DEVICE_PACKAGES := kmod-hwmon-pwmfan e2fsprogs f2fsck mkf2fs kmod-mt7996-firmware
  KERNEL_LOADADDR := 0x46000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := .itb
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  IMAGES := sysupgrade.itb
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | pad-rootfs | append-metadata
  ARTIFACTS := emmc-preloader.bin emmc-bl31-uboot.fip emmc-gpt.bin
  ARTIFACT/emmc-gpt.bin := mt798x-gpt emmc
  ARTIFACT/emmc-preloader.bin	:= mt7988-bl2 emmc-comb
  ARTIFACT/emmc-bl31-uboot.fip	:= mt7988-bl31-uboot arcadyan_mozart
  SUPPORTED_DEVICES += arcadyan,mozart
endef
TARGET_DEVICES += arcadyan_mozart

define Device/asus_tuf-ax6000
  DEVICE_VENDOR := ASUS
  DEVICE_MODEL := TUF-AX6000
  DEVICE_DTS := mt7986a-asus-tuf-ax6000
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware automount
  IMAGES := sysupgrade.bin
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += asus_tuf-ax6000

define Device/bananapi_bpi-r3
  DEVICE_VENDOR := Bananapi
  DEVICE_MODEL := BPi-R3
  DEVICE_DTS := mt7986a-bananapi-bpi-r3
  DEVICE_DTS_CONFIG := config-mt7986a-bananapi-bpi-r3
  DEVICE_DTS_OVERLAY:= mt7986a-bananapi-bpi-r3-emmc mt7986a-bananapi-bpi-r3-nand \
		       mt7986a-bananapi-bpi-r3-nor mt7986a-bananapi-bpi-r3-sd \
		       mt7986a-bananapi-bpi-r3-respeaker-2mics
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-hwmon-pwmfan kmod-i2c-gpio kmod-mt7915e kmod-mt7986-firmware kmod-sfp \
		     kmod-usb3 f2fsck mkf2fs mt7986-wo-firmware automount
  IMAGES := sysupgrade.itb
  KERNEL_LOADADDR := 0x44000000
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  ARTIFACTS := \
	       emmc-preloader.bin emmc-bl31-uboot.fip \
	       nor-preloader.bin nor-bl31-uboot.fip \
	       sdcard.img.gz \
	       snand-preloader.bin snand-bl31-uboot.fip
  ARTIFACT/emmc-preloader.bin	:= mt7986-bl2 emmc-ddr4
  ARTIFACT/emmc-bl31-uboot.fip	:= mt7986-bl31-uboot bananapi_bpi-r3-emmc
  ARTIFACT/nor-preloader.bin	:= mt7986-bl2 nor-ddr4
  ARTIFACT/nor-bl31-uboot.fip	:= mt7986-bl31-uboot bananapi_bpi-r3-nor
  ARTIFACT/snand-preloader.bin	:= mt7986-bl2 spim-nand-ubi-ddr4
  ARTIFACT/snand-bl31-uboot.fip	:= mt7986-bl31-uboot bananapi_bpi-r3-snand
  ARTIFACT/sdcard.img.gz	:= mt798x-gpt sdmmc |\
				   pad-to 17k | mt7986-bl2 sdmmc-ddr4 |\
				   pad-to 6656k | mt7986-bl31-uboot bananapi_bpi-r3-sdmmc |\
				$(if $(CONFIG_TARGET_ROOTFS_INITRAMFS),\
				   pad-to 12M | append-image-stage initramfs-recovery.itb | check-size 44m |\
				) \
				   pad-to 44M | mt7986-bl2 spim-nand-ubi-ddr4 |\
				   pad-to 45M | mt7986-bl31-uboot bananapi_bpi-r3-snand |\
				   pad-to 49M | mt7986-bl2 nor-ddr4 |\
				   pad-to 50M | mt7986-bl31-uboot bananapi_bpi-r3-nor |\
				   pad-to 51M | mt7986-bl2 emmc-ddr4 |\
				   pad-to 52M | mt7986-bl31-uboot bananapi_bpi-r3-emmc |\
				   pad-to 56M | mt798x-gpt emmc |\
				$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS),\
				   pad-to 64M | append-image squashfs-sysupgrade.itb | check-size |\
				) \
				  gzip
ifeq ($(DUMP),)
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
endif
  KERNEL			:= kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | pad-rootfs | append-metadata
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_COMPAT_VERSION := 1.2
  DEVICE_COMPAT_MESSAGE := SPI-NAND flash layout changes require bootloader update
endef
TARGET_DEVICES += bananapi_bpi-r3

define Device/bananapi_bpi-r3-mini
  DEVICE_VENDOR := Bananapi
  DEVICE_MODEL := BPi-R3 Mini
  DEVICE_DTS := mt7986a-bananapi-bpi-r3-mini
  DEVICE_DTS_CONFIG := config-mt7986a-bananapi-bpi-r3-mini
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-hwmon-pwmfan kmod-mt7915e kmod-mt7986-firmware kmod-phy-airoha-en8811h \
		     kmod-usb3 f2fsck mkf2fs mt7986-wo-firmware automount
  KERNEL_LOADADDR := 0x44000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
    fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := snand-factory.bin sysupgrade.itb
ifeq ($(DUMP),)
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
endif
  IMAGE/sysupgrade.itb := append-kernel | \
    fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | \
    pad-rootfs | append-metadata
  ARTIFACTS := \
       emmc-gpt.bin emmc-preloader.bin emmc-bl31-uboot.fip \
       snand-factory.bin snand-preloader.bin snand-bl31-uboot.fip
  ARTIFACT/emmc-gpt.bin := mt798x-gpt emmc
  ARTIFACT/emmc-preloader.bin := mt7986-bl2 emmc-ddr4
  ARTIFACT/emmc-bl31-uboot.fip := mt7986-bl31-uboot bananapi_bpi-r3-mini-emmc
  ARTIFACT/snand-factory.bin := mt7986-bl2 spim-nand-ubi-ddr4 | pad-to 256k | \
				mt7986-bl2 spim-nand-ubi-ddr4 | pad-to 512k | \
				mt7986-bl2 spim-nand-ubi-ddr4 | pad-to 768k | \
				mt7986-bl2 spim-nand-ubi-ddr4 | pad-to 2048k | \
				ubinize-image fit squashfs-sysupgrade.itb
  ARTIFACT/snand-preloader.bin := mt7986-bl2 spim-nand-ubi-ddr4
  ARTIFACT/snand-bl31-uboot.fip := mt7986-bl31-uboot bananapi_bpi-r3-mini-snand
  UBINIZE_PARTS := fip=:$(STAGING_DIR_IMAGE)/mt7986_bananapi_bpi-r3-mini-snand-u-boot.fip
ifneq ($(CONFIG_PACKAGE_airoha-en8811h-firmware),)
  UBINIZE_PARTS += en8811h-fw=:$(STAGING_DIR_IMAGE)/EthMD32.bin
endif
  DEVICE_COMPAT_VERSION := 1.2
  DEVICE_COMPAT_MESSAGE := Flash layout changes require bootloader update
endef
TARGET_DEVICES += bananapi_bpi-r3-mini

define Device/bananapi_bpi-r4-common
  DEVICE_VENDOR := Bananapi
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_DTS_LOADADDR := 0x45f00000
  DEVICE_DTS_OVERLAY:= mt7988a-bananapi-bpi-r4-emmc mt7988a-bananapi-bpi-r4-rtc mt7988a-bananapi-bpi-r4-sd mt7988a-bananapi-bpi-r4-wifi-mt7996a
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_PACKAGES := kmod-hwmon-pwmfan kmod-i2c-mux-pca954x kmod-eeprom-at24 kmod-mt7996-firmware kmod-mt7996-233-firmware \
		     kmod-rtc-pcf8563 kmod-sfp kmod-usb3 e2fsprogs f2fsck mkf2fs mt7988-wo-firmware
  IMAGES := sysupgrade.itb
  KERNEL_LOADADDR := 0x46000000
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  ARTIFACTS := \
	       emmc-preloader.bin emmc-bl31-uboot.fip \
	       sdcard.img.gz \
	       snand-preloader.bin snand-bl31-uboot.fip
  ARTIFACT/emmc-preloader.bin	:= mt7988-bl2 emmc-comb
  ARTIFACT/emmc-bl31-uboot.fip	:= mt7988-bl31-uboot $$(DEVICE_NAME)-emmc
  ARTIFACT/snand-preloader.bin	:= mt7988-bl2 spim-nand-ubi-comb
  ARTIFACT/snand-bl31-uboot.fip	:= mt7988-bl31-uboot $$(DEVICE_NAME)-snand
  ARTIFACT/sdcard.img.gz	:= mt798x-gpt sdmmc |\
				   pad-to 17k | mt7988-bl2 sdmmc-comb |\
				   pad-to 6656k | mt7988-bl31-uboot $$(DEVICE_NAME)-sdmmc |\
				$(if $(CONFIG_TARGET_ROOTFS_INITRAMFS),\
				   pad-to 12M | append-image-stage initramfs-recovery.itb | check-size 44m |\
				) \
				   pad-to 44M | mt7988-bl2 spim-nand-ubi-comb |\
				   pad-to 45M | mt7988-bl31-uboot $$(DEVICE_NAME)-snand |\
				   pad-to 51M | mt7988-bl2 emmc-comb |\
				   pad-to 52M | mt7988-bl31-uboot $$(DEVICE_NAME)-emmc |\
				   pad-to 56M | mt798x-gpt emmc |\
				$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS),\
				   pad-to 64M | append-image squashfs-sysupgrade.itb | check-size |\
				) \
				  gzip
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  KERNEL			:= kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | pad-rootfs | append-metadata
endef

define Device/bananapi_bpi-r4
  DEVICE_MODEL := BPi-R4
  DEVICE_DTS := mt7988a-bananapi-bpi-r4
  DEVICE_DTS_CONFIG := config-mt7988a-bananapi-bpi-r4
  $(call Device/bananapi_bpi-r4-common)
endef
TARGET_DEVICES += bananapi_bpi-r4

define Device/bananapi_bpi-r4-poe
  DEVICE_MODEL := BPi-R4 2.5GE
  DEVICE_DTS := mt7988a-bananapi-bpi-r4-poe
  DEVICE_DTS_CONFIG := config-mt7988a-bananapi-bpi-r4-poe
  $(call Device/bananapi_bpi-r4-common)
  DEVICE_PACKAGES += mt7988-2p5g-phy-firmware
endef
TARGET_DEVICES += bananapi_bpi-r4-poe

define Device/cetron_ct3003
  DEVICE_VENDOR := Cetron
  DEVICE_MODEL := CT3003
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7981b-cetron-ct3003
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += cetron,ct3003-stock mediatek,mt7981-spim-snand-rfb
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 32768k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGES += factory.bin
  IMAGE/factory.bin := $$(IMAGE/sysupgrade.bin) | cetron-header rd30 CT3003
endef
TARGET_DEVICES += cetron_ct3003

define Device/cetron_ct3003-ubootmod
  DEVICE_VENDOR := Cetron
  DEVICE_MODEL := CT3003
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-cetron-ct3003-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot cetron_ct3003
  DEVICE_COMPAT_VERSION := 1.2
  DEVICE_COMPAT_MESSAGE := Flash layout changes require bootloader update
endef
TARGET_DEVICES += cetron_ct3003-ubootmod

define Device/cmcc_a10-stock
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := A10
  DEVICE_VARIANT := (stock layout)
  DEVICE_ALT0_VENDOR := SuperElectron
  DEVICE_ALT0_MODEL := ZN-M5
  DEVICE_ALT0_VARIANT := (stock layout)
  DEVICE_ALT1_VENDOR := SuperElectron
  DEVICE_ALT1_MODEL := ZN-M8
  DEVICE_ALT1_VARIANT := (stock layout)
  DEVICE_DTS := mt7981b-cmcc-a10-stock
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += mediatek,mt7981-spim-snand-rfb
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  KERNEL = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
endef
TARGET_DEVICES += cmcc_a10-stock

define Device/cmcc_a10-ubootmod
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := A10
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_ALT0_VENDOR := SuperElectron
  DEVICE_ALT0_MODEL := ZN-M5
  DEVICE_ALT0_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_ALT1_VENDOR := SuperElectron
  DEVICE_ALT1_MODEL := ZN-M8
  DEVICE_ALT1_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-cmcc-a10-ubootmod
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += cmcc,a10
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot cmcc_a10
  DEVICE_COMPAT_VERSION := 1.2
  DEVICE_COMPAT_MESSAGE := Flash layout changes require bootloader update (for ImmortalWrt 23.05 users)
endef
TARGET_DEVICES += cmcc_a10-ubootmod

define Device/clx_s20p
  DEVICE_VENDOR := CLX
  DEVICE_MODEL := S20P
  DEVICE_DTS := mt7986a-clx-s20p
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware kmod-usb3 automount
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += clx_s20p

define Device/philips_hy3000
  DEVICE_VENDOR := Philips
  DEVICE_MODEL := HY3000
  DEVICE_DTS := mt7981b-philips-hy3000
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 f2fsck mkf2fs
  SUPPORTED_DEVICES += philips,hy3000
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += philips_hy3000

define Device/cmcc_rax3000m_common
  DEVICE_DTS_OVERLAY := mt7981b-cmcc-rax3000m-nand
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 \
	automount f2fsck mkf2fs
  KERNEL_LOADADDR := 0x44000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | \
	pad-rootfs | append-metadata
endef

define Device/cmcc_rax3000m-emmc-mtk
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := RAX3000M EMMC
  DEVICE_VARIANT := (MTK layout)
  DEVICE_DTS := mt7981b-cmcc-rax3000m-emmc-mtk
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 f2fsck mkf2fs
  SUPPORTED_DEVICES += cmcc,rax3000m-emmc
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += cmcc_rax3000m-emmc-mtk

define Device/cmcc_rax3000m-nand-mtk
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := RAX3000M NAND
  DEVICE_VARIANT := (MTK layout)
  DEVICE_DTS := mt7981b-cmcc-rax3000m-nand-mtk
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 f2fsck mkf2fs
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 116736k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += cmcc_rax3000m-nand-mtk

define Device/cmcc_rax3000m
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := RAX3000M NAND
  DEVICE_DTS := mt7981b-cmcc-rax3000m
  $(call Device/cmcc_rax3000m_common)
  ARTIFACTS += nand-preloader.bin nand-bl31-uboot.fip
  ARTIFACT/nand-preloader.bin := mt7981-bl2 spim-nand-ddr4
  ARTIFACT/nand-bl31-uboot.fip := mt7981-bl31-uboot cmcc_rax3000m-nand
endef
TARGET_DEVICES += cmcc_rax3000m

define Device/cmcc_rax3000me
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := RAX3000Me
  DEVICE_DTS := mt7981b-cmcc-rax3000me
  $(call Device/cmcc_rax3000m_common)
  DEVICE_DTS_OVERLAY += mt7981b-cmcc-rax3000me-nousb
  ARTIFACTS += nand-ddr3-preloader.bin nand-ddr3-bl31-uboot.fip \
	nand-ddr4-preloader.bin nand-ddr4-bl31-uboot.fip
  ARTIFACT/nand-ddr3-preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/nand-ddr3-bl31-uboot.fip := mt7981-bl31-uboot cmcc_rax3000me-nand-ddr3
  ARTIFACT/nand-ddr4-preloader.bin := mt7981-bl2 spim-nand-ddr4
  ARTIFACT/nand-ddr4-bl31-uboot.fip := mt7981-bl31-uboot cmcc_rax3000me-nand-ddr4
endef
TARGET_DEVICES += cmcc_rax3000me

define Device/cmcc_xr30-nand
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := XR30 NAND
  DEVICE_VARIANT := (U-Boot mod)
  DEVICE_DTS := mt7981b-cmcc-xr30-nand
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 116736k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += cmcc_xr30-nand

define Device/cmcc_xr30-stock
  DEVICE_VENDOR := CMCC
  DEVICE_MODEL := XR30 NAND
  DEVICE_VARIANT := (H layout)
  DEVICE_DTS := mt7981b-cmcc-xr30-stock
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 \
	automount f2fsck mkf2fs
  SUPPORTED_DEVICES := cmcc,xr30
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 116736k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += cmcc_xr30-stock

define Device/comfast_cf-e393ax
  DEVICE_VENDOR := COMFAST
  DEVICE_MODEL := CF-E393AX
  DEVICE_ALT0_VENDOR := COMFAST
  DEVICE_ALT0_MODEL := CF-E395AX
  DEVICE_DTS := mt7981a-comfast-cf-e393ax
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  KERNEL_LOADADDR := 0x44000000
  KERNEL = kernel-bin | lzma | \
       fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
       fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES := sysupgrade.bin factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += comfast_cf-e393ax

define Device/confiabits_mt7981
  DEVICE_VENDOR := Confiabits
  DEVICE_MODEL := MT7981
  DEVICE_DTS := mt7981b-confiabits-mt7981
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += mediatek,mt7981-spim-snand-2500wan-gmac2-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware \
	kmod-usb3 automount
endef
TARGET_DEVICES += confiabits_mt7981

define Device/cudy_ap3000outdoor-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := AP3000 Outdoor
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-ap3000outdoor-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R51
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_ap3000outdoor-v1

define Device/cudy_ap3000-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := AP3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-ap3000-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R49
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_ap3000-v1

define Device/cudy_m3000-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := M3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-m3000-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R37
  DEVICE_DTS_LOADADDR := 0x44000000
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGES := sysupgrade.bin
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_m3000-v1

define Device/cudy_re3000-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := RE3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-re3000-v1
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  IMAGES := sysupgrade.bin
  IMAGE_SIZE := 15424k
  SUPPORTED_DEVICES += R36
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | pad-rootfs | check-size | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_re3000-v1

define Device/cudy_tr3000-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := TR3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-tr3000-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R47
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware automount
endef
TARGET_DEVICES += cudy_tr3000-v1

define Device/cudy_tr3000-v1-ubootmod
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := TR3000
  DEVICE_VARIANT := v1 MOD
  DEVICE_DTS := mt7981b-cudy-tr3000-v1-ubootmod
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R47
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 114688k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware automount
endef
TARGET_DEVICES += cudy_tr3000-v1-ubootmod

define Device/cudy_tr3000-v1-256mb
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := TR3000
  DEVICE_VARIANT := v1 256mb
  DEVICE_DTS := mt7981b-cudy-tr3000-v1-256mb
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R103
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 235520k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware automount
endef
TARGET_DEVICES += cudy_tr3000-v1-256mb

define Device/cudy_wr3000-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := WR3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-wr3000-v1
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  IMAGES := sysupgrade.bin
  IMAGE_SIZE := 15424k
  SUPPORTED_DEVICES += R31
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | pad-rootfs | check-size | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_wr3000-v1

define Device/cudy_wr3000s-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := WR3000S
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-wr3000s-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R59
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_wr3000s-v1

define Device/cudy_wbr3000uax-v1
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := WBR3000UAX
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-cudy-wbr3000uax-v1
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += R120
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += cudy_wbr3000uax-v1

define Device/cudy_wbr3000uax-v1-ubootmod
  DEVICE_VENDOR := Cudy
  DEVICE_MODEL := WBR3000UAX
  DEVICE_VARIANT := v1 (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-cudy-wbr3000uax-v1-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 cudy-wbr3000uax-v1
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot cudy_wbr3000uax-v1
endef
TARGET_DEVICES += cudy_wbr3000uax-v1-ubootmod

define Device/dlink_aquila-pro-ai-m30-a1
  DEVICE_VENDOR := D-Link
  DEVICE_MODEL := AQUILA PRO AI M30
  DEVICE_VARIANT := A1
  DEVICE_DTS := mt7981b-dlink-aquila-pro-ai-m30-a1
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-leds-gca230718 kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  KERNEL_IN_UBI := 1
  IMAGES += recovery.bin
  IMAGE_SIZE := 51200k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGE/recovery.bin := sysupgrade-tar | pad-to $$(IMAGE_SIZE) | dlink-ai-recovery-header DLK6E6110001 \x6A\x28\xEE\x0B \x00\x00\x2C\x00 \x00\x00\x20\x03 \x61\x6E
endef
TARGET_DEVICES += dlink_aquila-pro-ai-m30-a1

define Device/dlink_aquila-pro-ai-m60-a1
  DEVICE_VENDOR := D-Link
  DEVICE_MODEL := AQUILA PRO AI M60
  DEVICE_VARIANT := A1
  DEVICE_DTS := mt7986a-dlink-aquila-pro-ai-m60-a1
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-leds-gca230718 kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  IMAGES += recovery.bin
  IMAGE_SIZE := 51200k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGE/recovery.bin := sysupgrade-tar | pad-to $$(IMAGE_SIZE) | dlink-ai-recovery-header DLK6E8202001 \x30\x6C\x19\x0C \x00\x00\x2C\x00 \x00\x00\x20\x03 \x82\x6E
endef
TARGET_DEVICES += dlink_aquila-pro-ai-m60-a1

define Device/edgecore_eap111
  DEVICE_VENDOR := Edgecore
  DEVICE_MODEL := EAP111
  DEVICE_DTS := mt7981a-edgecore-eap111
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  UBINIZE_OPTS := -E 5
  KERNEL_IN_UBI := 1
  IMAGE_SIZE := 65536k
  IMAGES := sysupgrade.bin factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += edgecore_eap111

define Device/gatonetworks_gdsp
  DEVICE_VENDOR := GatoNetworks
  DEVICE_MODEL := gdsp
  DEVICE_DTS := mt7981b-gatonetworks-gdsp
  DEVICE_DTS_DIR := ../dts
  IMAGES := sysupgrade.itb
  IMAGE_SIZE := 32768k
  DEVICE_PACKAGES := fitblk kmod-mt7915e kmod-mt7981-firmware \
    kmod-usb-net-qmi-wwan kmod-usb-serial-option kmod-usb3 \
    mt7981-wo-firmware -kmod-phy-aquantia
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 nor-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot gatonetworks_gdsp
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += gatonetworks_gdsp

define Device/glinet_gl-mt2500
  DEVICE_VENDOR := GL.iNet
  DEVICE_MODEL := GL-MT2500
  DEVICE_DTS := mt7981b-glinet-gl-mt2500
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  DEVICE_PACKAGES :=-wpad-openssl f2fsck mkf2fs kmod-usb3 automount
  SUPPORTED_DEVICES += glinet,mt2500-emmc
  IMAGES := sysupgrade.bin
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-gl-metadata
endef
TARGET_DEVICES += glinet_gl-mt2500

define Device/glinet_gl-mt3000
  DEVICE_VENDOR := GL.iNet
  DEVICE_MODEL := GL-MT3000
  DEVICE_DTS := mt7981b-glinet-gl-mt3000
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += glinet,mt3000-snand
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware \
	kmod-hwmon-pwmfan kmod-usb3 automount
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 246272k
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-gl-metadata
endef
TARGET_DEVICES += glinet_gl-mt3000

define Device/glinet_gl-mt6000
  DEVICE_VENDOR := GL.iNet
  DEVICE_MODEL := GL-MT6000
  DEVICE_DTS := mt7986a-glinet-gl-mt6000
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware \
	automount f2fsck mkf2fs kmod-mmc
  IMAGES += factory.bin
  IMAGE/factory.bin := append-kernel | pad-to 32M | append-rootfs
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-gl-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7986-bl2 emmc-ddr4
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot glinet_gl-mt6000
endef
TARGET_DEVICES += glinet_gl-mt6000

define Device/glinet_gl-x3000-xe3000-common
  DEVICE_VENDOR := GL.iNet
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware mkf2fs \
    kmod-fs-f2fs kmod-hwmon-pwmfan kmod-usb3 kmod-usb-serial-option \
    kmod-usb-storage kmod-usb-net-qmi-wwan uqmi automount
  IMAGES += factory.bin
  IMAGE/factory.bin := append-kernel | pad-to 32M | append-rootfs
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 emmc-ddr4
endef

define Device/glinet_gl-x3000
  DEVICE_MODEL := GL-X3000
  DEVICE_DTS := mt7981a-glinet-gl-x3000
  SUPPORTED_DEVICES := glinet,gl-x3000
  $(call Device/glinet_gl-x3000-xe3000-common)
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot glinet_gl-x3000
endef
TARGET_DEVICES += glinet_gl-x3000

define Device/glinet_gl-xe3000
  DEVICE_MODEL := GL-XE3000
  DEVICE_DTS := mt7981a-glinet-gl-xe3000
  SUPPORTED_DEVICES := glinet,gl-xe3000
  $(call Device/glinet_gl-x3000-xe3000-common)
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot glinet_gl-xe3000
endef
TARGET_DEVICES += glinet_gl-xe3000

define Device/openfi_6c
  DEVICE_VENDOR := OpenFi
  DEVICE_MODEL := 6C (with 256MB+1GB+M.2 for 5G+U3)
  DEVICE_DTS := mt7981b-openfi-6c
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += openfi,6c
  DEVICE_PACKAGES := automount  kmod-hwmon-pwmfan kmod-usb-net-rndis kmod-usb-serial-option f2fsck losetup mkf2fs kmod-fs-f2fs kmod-mmc luci-app-openfi 
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += openfi_6c

define Device/h3c_magic-nx30-pro
  DEVICE_VENDOR := H3C
  DEVICE_MODEL := Magic NX30 Pro
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-h3c-magic-nx30-pro
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGE_SIZE := 65536k
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot h3c_magic-nx30-pro
endef
TARGET_DEVICES += h3c_magic-nx30-pro

define Device/h3c_magic-nx30-pro-nmbm
  DEVICE_VENDOR := H3C
  DEVICE_MODEL := Magic NX30 Pro
  DEVICE_VARIANT := (NMBM layout)
  DEVICE_DTS := mt7981b-h3c-magic-nx30-pro-nmbm
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  KERNEL = kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
endef
TARGET_DEVICES += h3c_magic-nx30-pro-nmbm

define Device/huasifei_wh3000-pro
  DEVICE_VENDOR := Huasifei
  DEVICE_MODEL := WH3000 Pro
  DEVICE_DTS := mt7981b-huasifei-wh3000-pro
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES :=  kmod-hwmon-pwmfan kmod-usb3 f2fsck mkf2fs
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += huasifei_wh3000-pro

define Device/Airpi
  DEVICE_VENDOR := Airpi
  DEVICE_MODEL := emmc
  DEVICE_DTS := mt7981b-Airpi-emmc16G
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb-net-cdc-mbim  kmod-hwmon-pwmfan kmod-usb-net-qmi-wwan \
		     kmod-usb-serial-option kmod-usb3 automount kmod-Airpi-gpio-fan \
 		f2fsck mkf2fs uqmi luci-app-Airpifanctrl
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += Airpi

define Device/airpi_ap3000m
  DEVICE_VENDOR := Airpi
  DEVICE_MODEL := AP3000M
  DEVICE_DTS := mt7981-airpi-ap3000m
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3 kmod-usb-net-cdc-mbim kmod-hwmon-pwmfan kmod-usb-net-qmi-wwan \
	kmod-usb-serial-option automount f2fsck mkf2fs uqmi
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += airpi_ap3000m

define Device/huasifei_wh3000-emmc
  DEVICE_VENDOR := Huasifei
  DEVICE_MODEL := WH3000 eMMC
  DEVICE_ALT0_VENDOR := Fudy
  DEVICE_ALT0_MODEL := MT3000
  DEVICE_DTS := mt7981b-huasifei-wh3000-emmc
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES :=  \
	kmod-usb-net-cdc-mbim kmod-usb-net-qmi-wwan kmod-usb-serial-option \
	kmod-usb3 automount f2fsck mkf2fs uqmi
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += huasifei_wh3000-emmc

define Device/imou_lc-hx3001
  DEVICE_VENDOR := Imou
  DEVICE_MODEL := LC-HX3001
  DEVICE_DTS := mt7981b-imou-lc-hx3001
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot imou_lc-hx3001
endef
TARGET_DEVICES += imou_lc-hx3001

define Device/jcg_q30-pro
  DEVICE_VENDOR := JCG
  DEVICE_MODEL := Q30 PRO
  DEVICE_DTS := mt7981b-jcg-q30-pro
  DEVICE_DTS_DIR := ../dts
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += jcg_q30-pro

define Device/jdcloud_re-cp-03
  DEVICE_VENDOR := JDCloud
  DEVICE_MODEL := RE-CP-03
  DEVICE_DTS := mt7986a-jdcloud-re-cp-03
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES :=  \
	e2fsprogs f2fsck mkf2fs
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += jdcloud_re-cp-03

define Device/keenetic_kn-3811
  DEVICE_VENDOR := Keenetic
  DEVICE_MODEL := KN-3811
  DEVICE_DTS := mt7981b-keenetic-kn-3811
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-usb3
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_SIZE := 6144k
  IMAGE_SIZE := 233984k
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb | \
	append-squashfs4-fakeroot
  IMAGES += factory.bin
  IMAGE/factory.bin := append-kernel | pad-to $$(KERNEL_SIZE) | \
	append-ubi | check-size | zyimage -d 0x803811 -v "KN-3811"
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += keenetic_kn-3811

define Device/keenetic_kn-3911
  DEVICE_VENDOR := Keenetic
  DEVICE_MODEL := KN-3911
  DEVICE_DTS := mt7981b-keenetic-kn-3911
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-phy-airoha-en8811h
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_SIZE := 6144k
  IMAGE_SIZE := 108544k
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb | \
	append-squashfs4-fakeroot
  IMAGES += factory.bin
  IMAGE/factory.bin := append-kernel | pad-to $$(KERNEL_SIZE) | \
	append-ubi | check-size | zyimage -d 0x803911 -v "KN-3911"
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += keenetic_kn-3911

define Device/konka_komi-a31
  DEVICE_VENDOR := Konka
  DEVICE_MODEL := KOMI A31
  DEVICE_ALT0_VENDOR := E-Life
  DEVICE_ALT0_MODEL := ETR631-T
  DEVICE_ALT1_VENDOR := E-Life
  DEVICE_ALT1_MODEL := ETR635-U
  DEVICE_DTS := mt7981b-konka-komi-a31
  DEVICE_DTS_OVERLAY := mt7981b-konka-komi-a31-emmc mt7981b-konka-komi-a31-nor
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  KERNEL_LOADADDR := 0x44000000
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip \
	emmc-gpt.bin emmc-preloader.bin emmc-bl31-uboot.fip \
	nor-preloader.bin nor-bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot konka_komi-a31
  ARTIFACT/emmc-gpt.bin := mt798x-gpt emmc
  ARTIFACT/emmc-preloader.bin := mt7981-bl2 emmc-ddr3
  ARTIFACT/emmc-bl31-uboot.fip := mt7981-bl31-uboot konka_komi-a31-emmc
  ARTIFACT/nor-preloader.bin := mt7981-bl2 nor-ddr3
  ARTIFACT/nor-bl31-uboot.fip := mt7981-bl31-uboot konka_komi-a31-nor
endef
TARGET_DEVICES += konka_komi-a31

define Device/livinet_zr-3020
  DEVICE_VENDOR := Livinet
  DEVICE_MODEL := ZR-3020
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7981b-livinet-zr-3020
  DEVICE_DTS_DIR := ../dts
  SUPPORTED_DEVICES += mediatek,mt7981-spim-snand-gsw-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  KERNEL = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
endef
TARGET_DEVICES += livinet_zr-3020

define Device/livinet_zr-3020-ubootmod
  DEVICE_VENDOR := Livinet
  DEVICE_MODEL := ZR-3020
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-livinet-zr-3020-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot livinet_zr-3020
  DEVICE_COMPAT_VERSION := 1.2
  DEVICE_COMPAT_MESSAGE := Flash layout changes require bootloader update
endef
TARGET_DEVICES += livinet_zr-3020-ubootmod

define Device/mediatek_7981r128
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := 7981r128
  DEVICE_DTS := mt7981b-mediatek-7981r128
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  SUPPORTED_DEVICES += mediatek,7981r128
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 114688k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mediatek_7981r128

define Device/mediatek_mt7981-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MT7981 rfb
  DEVICE_DTS := mt7981-rfb
  DEVICE_DTS_OVERLAY:= \
	mt7981-rfb-spim-nand \
	mt7981-rfb-mxl-2p5g-phy-eth1 \
	mt7981-rfb-mxl-2p5g-phy-swp5
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware kmod-usb3 e2fsprogs f2fsck mkf2fs mt7981-wo-firmware
  KERNEL_LOADADDR := 0x44000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := .itb
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | pad-rootfs | append-metadata
  ARTIFACTS := \
	emmc-preloader.bin emmc-bl31-uboot.fip \
	nor-preloader.bin nor-bl31-uboot.fip \
	sdcard.img.gz \
	snfi-nand-preloader.bin snfi-nand-bl31-uboot.fip \
	spim-nand-preloader.bin spim-nand-bl31-uboot.fip
  ARTIFACT/emmc-preloader.bin		:= mt7981-bl2 emmc-ddr3
  ARTIFACT/emmc-bl31-uboot.fip		:= mt7981-bl31-uboot rfb-emmc
  ARTIFACT/nor-preloader.bin		:= mt7981-bl2 nor-ddr3
  ARTIFACT/nor-bl31-uboot.fip		:= mt7981-bl31-uboot rfb-emmc
  ARTIFACT/snfi-nand-preloader.bin	:= mt7981-bl2 snand-ddr3
  ARTIFACT/snfi-nand-bl31-uboot.fip	:= mt7981-bl31-uboot rfb-snfi
  ARTIFACT/spim-nand-preloader.bin	:= mt7981-bl2 spim-nand-ddr3
  ARTIFACT/spim-nand-bl31-uboot.fip	:= mt7981-bl31-uboot rfb-spim-nand
  ARTIFACT/sdcard.img.gz	:= mt798x-gpt sdmmc |\
				   pad-to 17k | mt7981-bl2 sdmmc-ddr3 |\
				   pad-to 6656k | mt7981-bl31-uboot rfb-sd |\
				$(if $(CONFIG_TARGET_ROOTFS_INITRAMFS),\
				   pad-to 12M | append-image-stage initramfs.itb | check-size 44m |\
				) \
				   pad-to 44M | mt7981-bl2 spim-nand-ddr3 |\
				   pad-to 45M | mt7981-bl31-uboot rfb-spim-nand |\
				   pad-to 49M | mt7981-bl2 nor-ddr3 |\
				   pad-to 50M | mt7981-bl31-uboot rfb-nor |\
				   pad-to 51M | mt7981-bl2 snand-ddr3 |\
				   pad-to 53M | mt7981-bl31-uboot rfb-snfi |\
				$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS),\
				   pad-to 64M | append-image squashfs-sysupgrade.itb | check-size |\
				) \
				  gzip
endef
TARGET_DEVICES += mediatek_mt7981-rfb

define Device/mediatek_mt7986a-rfb-nand
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MT7986 rfba AP (NAND)
  DEVICE_DTS := mt7986a-rfb-spim-nand
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  SUPPORTED_DEVICES := mediatek,mt7986a-rfb-snand
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  KERNEL = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
endef
TARGET_DEVICES += mediatek_mt7986a-rfb-nand

define Device/mediatek_mt7986b-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MTK7986 rfbb AP
  DEVICE_DTS := mt7986b-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  SUPPORTED_DEVICES := mediatek,mt7986b-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mediatek_mt7986b-rfb

define Device/mediatek_mt7988a-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MT7988A rfb
  DEVICE_DTS := mt7988a-rfb
  DEVICE_DTS_OVERLAY:= \
	mt7988a-rfb-emmc \
	mt7988a-rfb-sd \
	mt7988a-rfb-snfi-nand \
	mt7988a-rfb-spim-nand \
	mt7988a-rfb-spim-nand-factory \
	mt7988a-rfb-spim-nor \
	mt7988a-rfb-eth1-aqr \
	mt7988a-rfb-eth1-i2p5g-phy \
	mt7988a-rfb-eth1-mxl \
	mt7988a-rfb-eth1-sfp \
	mt7988a-rfb-eth2-aqr \
	mt7988a-rfb-eth2-mxl \
	mt7988a-rfb-eth2-sfp
  DEVICE_DTS_DIR := $(DTS_DIR)/
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x45f00000
  DEVICE_PACKAGES := mt7988-2p5g-phy-firmware kmod-sfp
  KERNEL_LOADADDR := 0x46000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := .itb
  KERNEL_IN_UBI := 1
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  IMAGES := sysupgrade.itb
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | pad-rootfs | append-metadata
  ARTIFACTS := \
	       emmc-gpt.bin emmc-preloader.bin emmc-bl31-uboot.fip \
	       nor-preloader.bin nor-bl31-uboot.fip \
	       sdcard.img.gz \
	       snand-preloader.bin snand-bl31-uboot.fip
  ARTIFACT/emmc-gpt.bin		:= mt798x-gpt emmc
  ARTIFACT/emmc-preloader.bin	:= mt7988-bl2 emmc-comb
  ARTIFACT/emmc-bl31-uboot.fip	:= mt7988-bl31-uboot rfb-emmc
  ARTIFACT/nor-preloader.bin	:= mt7988-bl2 nor-comb
  ARTIFACT/nor-bl31-uboot.fip	:= mt7988-bl31-uboot rfb-nor
  ARTIFACT/snand-preloader.bin	:= mt7988-bl2 spim-nand-ubi-comb
  ARTIFACT/snand-bl31-uboot.fip	:= mt7988-bl31-uboot rfb-snand
  ARTIFACT/sdcard.img.gz	:= mt798x-gpt sdmmc |\
				   pad-to 17k | mt7988-bl2 sdmmc-comb |\
				   pad-to 6656k | mt7988-bl31-uboot rfb-sd |\
				$(if $(CONFIG_TARGET_ROOTFS_INITRAMFS),\
				   pad-to 12M | append-image-stage initramfs.itb | check-size 44m |\
				) \
				   pad-to 44M | mt7988-bl2 spim-nand-comb |\
				   pad-to 45M | mt7988-bl31-uboot rfb-snand |\
				   pad-to 51M | mt7988-bl2 nor-comb |\
				   pad-to 51M | mt7988-bl31-uboot rfb-nor |\
				   pad-to 55M | mt7988-bl2 emmc-comb |\
				   pad-to 56M | mt7988-bl31-uboot rfb-emmc |\
				   pad-to 62M | mt798x-gpt emmc |\
				$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS),\
				   pad-to 64M | append-image squashfs-sysupgrade.itb | check-size |\
				) \
				  gzip
endef
TARGET_DEVICES += mediatek_mt7988a-rfb

define Device/mercusys_mr80x-v3
  DEVICE_VENDOR := MERCUSYS
  DEVICE_MODEL := MR80X
  DEVICE_VARIANT := v3
  DEVICE_DTS := mt7981b-mercusys-mr80x-v3
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mercusys_mr80x-v3

define Device/mercusys_mr90x-v1
  DEVICE_VENDOR := MERCUSYS
  DEVICE_MODEL := MR90X v1
  DEVICE_DTS := mt7986b-mercusys-mr90x-v1
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 51200k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mercusys_mr90x-v1

define Device/mercusys_mr90x-v1-ubi
  DEVICE_VENDOR := MERCUSYS
  DEVICE_MODEL := MR90X v1 (UBI)
  DEVICE_DTS := mt7986b-mercusys-mr90x-v1-ubi
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | \
	pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | \
	append-metadata
  ARTIFACTS := bl31-uboot.fip preloader.bin
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot mercusys_mr90x-v1
  ARTIFACT/preloader.bin := mt7986-bl2 spim-nand-ubi-ddr3
endef
TARGET_DEVICES += mercusys_mr90x-v1-ubi

define Device/netcore_n60
  DEVICE_VENDOR := Netcore
  DEVICE_MODEL := N60
  DEVICE_DTS := mt7986a-netcore-n60
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7986-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot netcore_n60
endef
TARGET_DEVICES += netcore_n60

define Device/netcore_n60-pro
  DEVICE_VENDOR := Netcore
  DEVICE_MODEL := N60 Pro
  DEVICE_DTS := mt7986a-netcore-n60-pro
  DEVICE_DTS_DIR := ../dts
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware kmod-usb3 automount
endef
TARGET_DEVICES += netcore_n60-pro

define Device/netgear_wax220
  DEVICE_VENDOR := NETGEAR
  DEVICE_MODEL := WAX220
  DEVICE_DTS := mt7986b-netgear-wax220
  DEVICE_DTS_DIR := ../dts
  NETGEAR_ENC_MODEL := WAX220
  NETGEAR_ENC_REGION := US
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  IMAGE_SIZE := 32768k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  IMAGES += factory.img
  # Padding to 10M seems to be required by OEM web interface
  IMAGE/factory.img := sysupgrade-tar | \
	  pad-to 10M | check-size | netgear-encrypted-factory
endef
TARGET_DEVICES += netgear_wax220

define Device/netis_nx31
  DEVICE_VENDOR := netis
  DEVICE_MODEL := NX31
  DEVICE_DTS := mt7981b-netis-nx31
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | \
	append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot netis_nx31
endef
TARGET_DEVICES += netis_nx31

define Device/netis_nx32u
  DEVICE_VENDOR := Netis
  DEVICE_MODEL := NX32U
  DEVICE_DTS := mt7981b-netis-nx32u
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | \
	append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot netis_nx32u
endef
TARGET_DEVICES += netis_nx32u

define Device/nokia_ea0326gmp
  DEVICE_VENDOR := Nokia
  DEVICE_MODEL := EA0326GMP
  DEVICE_DTS := mt7981b-nokia-ea0326gmp
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 112640k
  KERNEL_IN_UBI := 1  
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += nokia_ea0326gmp

define Device/openembed_som7981
  DEVICE_VENDOR := OpenEmbed
  DEVICE_MODEL := SOM7981
  DEVICE_DTS := mt7981b-openembed-som7981
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware \
	kmod-crypto-hw-atmel kmod-eeprom-at24 kmod-gpio-beeper kmod-rtc-pcf8563 \
	kmod-usb-net-cdc-mbim kmod-usb-net-qmi-wwan kmod-usb-serial-option \
	kmod-usb3 automount uqmi
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 244224k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += openembed_som7981

define Device/openwrt_one
  DEVICE_VENDOR := OpenWrt
  DEVICE_MODEL := One
  DEVICE_DTS := mt7981b-openwrt-one
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTC_FLAGS := --pad 4096
  DEVICE_DTS_LOADADDR := 0x43f00000
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-rtc-pcf8563 kmod-usb3 kmod-nvme kmod-phy-airoha-en8811h
  KERNEL_LOADADDR := 0x44000000
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  KERNEL_INITRAMFS_SUFFIX := .itb
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  IMAGE_SIZE := $$(shell expr 64 + $$(CONFIG_TARGET_ROOTFS_PARTSIZE))m
  IMAGE/sysupgrade.itb := append-kernel | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-with-rootfs | pad-rootfs | append-metadata
  ARTIFACTS := \
	nor-preloader.bin nor-bl31-uboot.fip \
	snand-preloader.bin snand-bl31-uboot.fip \
	factory.ubi snand-factory.bin nor-factory.bin
  ARTIFACT/nor-preloader.bin		:= mt7981-bl2 nor-ddr4
  ARTIFACT/nor-bl31-uboot.fip		:= mt7981-bl31-uboot openwrt_one-nor
  ARTIFACT/snand-preloader.bin		:= mt7981-bl2 spim-nand-ubi-ddr4
  ARTIFACT/snand-bl31-uboot.fip		:= mt7981-bl31-uboot openwrt_one-snand
  ARTIFACT/factory.ubi			:= ubinize-image fit squashfs-sysupgrade.itb
  ARTIFACT/snand-factory.bin		:= mt7981-bl2 spim-nand-ubi-ddr4 | pad-to 256k | \
					   mt7981-bl2 spim-nand-ubi-ddr4 | pad-to 512k | \
					   mt7981-bl2 spim-nand-ubi-ddr4 | pad-to 768k | \
					   mt7981-bl2 spim-nand-ubi-ddr4 | pad-to 1024k | \
					   ubinize-image fit squashfs-sysupgrade.itb
  ARTIFACT/nor-factory.bin		:= mt7981-bl2 nor-ddr4 | pad-to 256k | \
					   mt7981-bl31-uboot openwrt_one-nor | pad-to 512k | \
					   append-image-stage initramfs.itb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  UBINIZE_PARTS := fip=:$(STAGING_DIR_IMAGE)/mt7981_openwrt_one-snand-u-boot.fip \
		   $(if $(IB),recovery=:$(STAGING_DIR_IMAGE)/mediatek-filogic-openwrt_one-initramfs.itb,\
			      recovery=:$(KDIR)/tmp/$$(KERNEL_INITRAMFS_IMAGE)) \
		   $(if $(wildcard $(TOPDIR)/openwrt-mediatek-filogic-openwrt_one-calibration.itb), calibration=:$(TOPDIR)/openwrt-mediatek-filogic-openwrt_one-calibration.itb)
endef
TARGET_DEVICES += openwrt_one

define Device/qihoo_360t7
  DEVICE_VENDOR := Qihoo
  DEVICE_MODEL := 360T7
  DEVICE_DTS := mt7981b-qihoo-360t7
  DEVICE_DTS_DIR := ../dts
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += qihoo_360t7

define Device/routerich_ax3000
  DEVICE_VENDOR := Routerich
  DEVICE_MODEL := AX3000
  DEVICE_DTS := mt7981b-routerich-ax3000
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 automount
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  SUPPORTED_DEVICES += mediatek,mt7981-spim-snand-rfb
  DEVICE_COMPAT_VERSION := 1.1
  DEVICE_COMPAT_MESSAGE := Partition layout has been changed. Bootloader MUST be \
	upgraded to avoid data corruption and getting bricked. \
	Please, contact your vendor and follow the guide: \
	https://openwrt.org/toh/routerich/ax3000#web_ui_method
endef
TARGET_DEVICES += routerich_ax3000

define Device/routerich_ax3000-ubootmod
  DEVICE_VENDOR := Routerich
  DEVICE_MODEL := AX3000 (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-routerich-ax3000-ubootmod
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
	fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | \
	append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware kmod-usb3 mt7981-wo-firmware
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot routerich_ax3000
endef
TARGET_DEVICES += routerich_ax3000-ubootmod

define Device/routerich_ax3000-v1
  DEVICE_VENDOR := Routerich
  DEVICE_MODEL := AX3000
  DEVICE_VARIANT := v1
  DEVICE_DTS := mt7981b-routerich-ax3000-v1
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware kmod-usb3 mt7981-wo-firmware
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  SUPPORTED_DEVICES += mediatek,mt7981-spim-snand-rfb
endef
TARGET_DEVICES += routerich_ax3000-v1

define Device/yvr_x6
  DEVICE_VENDOR := DaZoo
  DEVICE_MODEL := yvr-x6 (ax6000)
  DEVICE_DTS := mt7986a-yvr-x6
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware kmod-usb3 automount
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  BLOCKSIZE := 256k
  PAGESIZE := 4096
  KERNEL_IN_UBI := 1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += yvr_x6

define Device/ruijie_rg-x60-pro
  DEVICE_VENDOR := Ruijie
  DEVICE_MODEL := RG-X60 Pro
  DEVICE_DTS := mt7986a-ruijie-rg-x60-pro
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += ruijie_rg-x60-pro

define Device/sl_3000-emmc
  DEVICE_VENDOR := SL
  DEVICE_MODEL := 3000 eMMC
  DEVICE_DTS := mt7981b-sl-3000-emmc
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware automount coremark blkid blockdev fdisk f2fsck mkf2fs kmod-mmc
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += sl_3000-emmc

define Device/bt_r320
  DEVICE_VENDOR := Globitel
  DEVICE_MODEL := BT-R320
  DEVICE_DTS := mt7981b-bt-r320
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 automount e2fsprogs f2fsck mkf2fs
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += bt_r320

define Device/tplink_re6000xd
  DEVICE_VENDOR := TP-Link
  DEVICE_MODEL := RE6000XD
  DEVICE_DTS := mt7986b-tplink-re6000xd
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 51200k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += tplink_re6000xd

define Device/tplink_tl-xdr-common
  DEVICE_VENDOR := TP-Link
  DEVICE_DTS_DIR := ../dts
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  DEVICE_PACKAGES := kmod-usb3 automount
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef

define Device/tplink_tl-xdr4288
  DEVICE_MODEL := TL-XDR4288
  DEVICE_DTS := mt7986a-tplink-tl-xdr4288
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot tplink_tl-xdr4288
  $(call Device/tplink_tl-xdr-common)
endef
TARGET_DEVICES += tplink_tl-xdr4288

define Device/tplink_tl-xdr6086
  DEVICE_MODEL := TL-XDR6086
  DEVICE_DTS := mt7986a-tplink-tl-xdr6086
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot tplink_tl-xdr6086
  $(call Device/tplink_tl-xdr-common)
endef
TARGET_DEVICES += tplink_tl-xdr6086

define Device/tplink_tl-xdr6088
  DEVICE_MODEL := TL-XDR6088
  DEVICE_DTS := mt7986a-tplink-tl-xdr6088
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot tplink_tl-xdr6088
  $(call Device/tplink_tl-xdr-common)
endef
TARGET_DEVICES += tplink_tl-xdr6088

define Device/tplink_tl-xtr8488
  DEVICE_MODEL := TL-XTR8488
  DEVICE_DTS := mt7986a-tplink-tl-xtr8488
  $(call Device/tplink_tl-xdr-common)
  DEVICE_PACKAGES += kmod-mt7915-firmware
  ARTIFACT/preloader.bin := mt7986-bl2 spim-nand-ddr4
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot tplink_tl-xtr8488
endef
TARGET_DEVICES += tplink_tl-xtr8488

define Device/ubnt_unifi-6-plus
  DEVICE_VENDOR := Ubiquiti
  DEVICE_MODEL := UniFi U6+
  DEVICE_DTS := mt7981a-ubnt-unifi-6-plus
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware e2fsprogs f2fsck mkf2fs fdisk partx-utils
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += ubnt_unifi-6-plus

define Device/umi_uax3000e
  DEVICE_VENDOR := UMI
  DEVICE_MODEL := UAX3000E
  DEVICE_DTS := mt7981b-umi-uax3000e
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware automount coremark blkid blockdev fdisk f2fsck mkf2fs kmod-mmc mmc-utils
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += umi_uax3000e

define Device/unielec_u7981-01
  DEVICE_VENDOR := Unielec
  DEVICE_MODEL := U7981-01
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 f2fsck mkf2fs fdisk partx-utils automount
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef

define Device/unielec_u7981-01-emmc
  DEVICE_DTS := mt7981b-unielec-u7981-01-emmc
  DEVICE_VARIANT := (EMMC)
  $(call Device/unielec_u7981-01)
endef
TARGET_DEVICES += unielec_u7981-01-emmc

define Device/unielec_u7981-01-nand
  DEVICE_DTS := mt7981b-unielec-u7981-01-nand
  DEVICE_VARIANT := (NAND)
  $(call Device/unielec_u7981-01)
endef
TARGET_DEVICES += unielec_u7981-01-nand

define Device/wavlink_wl-wn586x3
  DEVICE_VENDOR := WAVLINK
  DEVICE_MODEL := WL-WN586X3
  DEVICE_DTS := mt7981b-wavlink-wl-wn586x3
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  IMAGE_SIZE := 15424k
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += wavlink_wl-wn586x3

define Device/wavlink_wl-wn573hx3
  DEVICE_VENDOR := WAVLINK
  DEVICE_MODEL := WL-WN573HX3
  DEVICE_ALT0_VENDOR := 7Links
  DEVICE_ALT0_MODEL := WLR-1300
  DEVICE_DTS := mt7981b-wavlink-wl-wn573hx3
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  IMAGE_SIZE := 14336k
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  SUPPORTED_DEVICES += mediatek,mt7981-spim-nor-rfb
  IMAGES = WN573HX3-sysupgrade.bin
  IMAGE/WN573HX3-sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | pad-rootfs | check-size | append-metadata
endef
TARGET_DEVICES += wavlink_wl-wn573hx3

define Device/xiaomi_mi-router-ax3000t
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Mi Router AX3000T
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7981b-xiaomi-mi-router-ax3000t
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS := initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-kernel.bin | ubinize-kernel
endif
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_mi-router-ax3000t

define Device/xiaomi_mi-router-ax3000t-mtkuboot
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Mi Router AX3000T
  DEVICE_VARIANT := (MTK U-Boot layout)
  DEVICE_DTS := mt7981b-xiaomi-mi-router-ax3000t-mtkuboot
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 114688k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_mi-router-ax3000t-mtkuboot

define Device/xiaomi_mi-router-ax3000t-ubootmod
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Mi Router AX3000T
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-xiaomi-mi-router-ax3000t-ubootmod
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  IMAGES := sysupgrade.itb
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  KERNEL := kernel-bin | gzip
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot xiaomi_mi-router-ax3000t
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS += initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-recovery.itb | ubinize-kernel
endif
endef
TARGET_DEVICES += xiaomi_mi-router-ax3000t-ubootmod

define Device/xiaomi_mi-router-wr30u-stock
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Mi Router WR30U
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7981b-xiaomi-mi-router-wr30u-stock
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS := initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-kernel.bin | ubinize-kernel
endif
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_mi-router-wr30u-stock

define Device/xiaomi_mi-router-wr30u-ubootmod
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Mi Router WR30U
  DEVICE_DTS := mt7981b-xiaomi-mi-router-wr30u-ubootmod
  DEVICE_DTS_DIR := ../dts
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS := initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-kernel.bin | ubinize-kernel
endif
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_mi-router-wr30u-ubootmod

define Device/xiaomi_redmi-router-ax6000-stock
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Redmi Router AX6000
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7986a-xiaomi-redmi-router-ax6000-stock
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-leds-ws2812b kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS := initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-kernel.bin | ubinize-kernel
endif
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_redmi-router-ax6000-stock

define Device/xiaomi_redmi-router-ax6000-ubootmod
  DEVICE_VENDOR := Xiaomi
  DEVICE_MODEL := Redmi Router AX6000
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7986a-xiaomi-redmi-router-ax6000-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-leds-ws2812b kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  ARTIFACTS := initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-recovery.itb | ubinize-kernel
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += xiaomi_redmi-router-ax6000-ubootmod

define Device/yuncore_ax835
  DEVICE_VENDOR := YunCore
  DEVICE_MODEL := AX835
  DEVICE_DTS := mt7981b-yuncore-ax835
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS_LOADADDR := 0x47000000
  IMAGES := sysupgrade.bin
  IMAGE_SIZE := 14336k
  SUPPORTED_DEVICES += mediatek,mt7981-spim-nor-rfb
  KERNEL := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | pad-rootfs | check-size | append-metadata
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
endef
TARGET_DEVICES += yuncore_ax835

define Device/zbtlink_zbt-z8102ax
  DEVICE_VENDOR := Zbtlink
  DEVICE_MODEL := ZBT-Z8102AX
  DEVICE_DTS := mt7981b-zbtlink-zbt-z8102ax
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3 \
	kmod-usb-net-qmi-wwan kmod-usb-serial-option automount
  KERNEL_IN_UBI := 1
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += zbtlink_zbt-z8102ax

define Device/zbtlink_zbt-z8103ax
  DEVICE_VENDOR := Zbtlink
  DEVICE_MODEL := ZBT-Z8103AX
  DEVICE_DTS := mt7981b-zbtlink-zbt-z8103ax
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware
  KERNEL_IN_UBI := 1
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += zbtlink_zbt-z8103ax

define Device/zyxel_ex5601-t0-stock
  DEVICE_VENDOR := Zyxel
  DEVICE_MODEL := EX5601-T0
  DEVICE_ALT0_VENDOR := Zyxel
  DEVICE_ALT0_MODEL := EX5601-T1
  DEVICE_ALT1_VENDOR := Zyxel
  DEVICE_ALT1_MODEL := T-56
  DEVICE_VARIANT := (stock layout)
  DEVICE_DTS := mt7986a-zyxel-ex5601-t0-stock
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware kmod-usb3 automount
  SUPPORTED_DEVICES := mediatek,mt7986a-rfb-snand
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 256k
  PAGESIZE := 4096
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
  KERNEL = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
endef
TARGET_DEVICES += zyxel_ex5601-t0-stock

define Device/zyxel_ex5601-t0-ubootmod
  DEVICE_VENDOR := Zyxel
  DEVICE_MODEL := EX5601-T0
  DEVICE_ALT0_VENDOR := Zyxel
  DEVICE_ALT0_MODEL := EX5601-T1
  DEVICE_ALT1_VENDOR := Zyxel
  DEVICE_ALT1_MODEL := T-56
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7986a-zyxel-ex5601-t0-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7986-firmware mt7986-wo-firmware kmod-usb3 automount
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  IMAGES := sysupgrade.itb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 256k
  PAGESIZE := 4096
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  KERNEL := kernel-bin | lzma
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd
  IMAGE/sysupgrade.itb := append-kernel | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7986-bl2 spim-nand-4k-ddr4
  ARTIFACT/bl31-uboot.fip := mt7986-bl31-uboot zyxel_ex5601-t0
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS += initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-recovery.itb | ubinize-kernel
endif
endef
TARGET_DEVICES += zyxel_ex5601-t0-ubootmod

define Device/zyxel_ex5700-telenor
  DEVICE_VENDOR := Zyxel
  DEVICE_MODEL := EX5700 (Telenor)
  DEVICE_DTS := mt7986a-zyxel-ex5700-telenor
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7916-firmware kmod-ubootenv-nvram \
	kmod-usb3 kmod-mt7986-firmware mt7986-wo-firmware automount
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += zyxel_ex5700-telenor

define Device/zyxel_nwa50ax-pro
  DEVICE_VENDOR := Zyxel
  DEVICE_MODEL := NWA50AX Pro
  DEVICE_DTS := mt7981b-zyxel-nwa50ax-pro
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware zyxel-bootconfig
  DEVICE_DTS_LOADADDR := 0x44000000
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 51200k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE) | zyxel-nwa-fit-filogic
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += zyxel_nwa50ax-pro

define Device/wirelesstag_zx7981pg
  DEVICE_VENDOR := Wireless-Tag
  DEVICE_MODEL := ZX7981PG
  DEVICE_DTS := mt7981b-wirelesstag-zx7981pg
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware kmod-usb3 mt7981-wo-firmware
  DEVICE_DTS_LOADADDR := 0x44000000
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 51200k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += wirelesstag_zx7981pg

define Device/wirelesstag_zx7981pd
  DEVICE_VENDOR := Wireless-Tag
  DEVICE_MODEL := ZX7981PD
  DEVICE_DTS := mt7981b-wirelesstag-zx7981pd
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware kmod-usb3 mt7981-wo-firmware
  DEVICE_DTS_LOADADDR := 0x44000000
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 51200k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += wirelesstag_zx7981pd

define Device/wirelesstag_zx7981pd-ubootmod
  DEVICE_VENDOR := Wireless-Tag
  DEVICE_MODEL := ZX7981PD
  DEVICE_VARIANT := (OpenWrt U-Boot layout)
  DEVICE_DTS := mt7981b-wirelesstag-zx7981pd-ubootmod
  DEVICE_DTS_DIR := ../dts
  DEVICE_PACKAGES := kmod-mt7915e kmod-mt7981-firmware mt7981-wo-firmware kmod-usb3
  KERNEL_INITRAMFS_SUFFIX := -recovery.itb
  IMAGES := sysupgrade.itb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_IN_UBI := 1
  UBOOTENV_IN_UBI := 1
  KERNEL := kernel-bin | lzma
  KERNEL_INITRAMFS := kernel-bin | lzma | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.itb := append-kernel | \
        fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb external-static-with-rootfs | append-metadata
  ARTIFACTS := preloader.bin bl31-uboot.fip
  ARTIFACT/preloader.bin := mt7981-bl2 spim-nand-ddr3
  ARTIFACT/bl31-uboot.fip := mt7981-bl31-uboot wirelesstag_zx7981pd
ifneq ($(CONFIG_TARGET_ROOTFS_INITRAMFS),)
  ARTIFACTS += initramfs-factory.ubi
  ARTIFACT/initramfs-factory.ubi := append-image-stage initramfs-recovery.itb | ubinize-kernel
endif
endef
TARGET_DEVICES += wirelesstag_zx7981pd-ubootmod

define Device/nradio_c5800-688
  DEVICE_VENDOR := NRadio
  DEVICE_MODEL := C5800-688
  DEVICE_DTS := mt7981-nradio-c5800-688
  DEVICE_DTS_DIR := ../dts
  BOARD_NAME := nradio_c5800-688
  SUPPORTED_DEVICES += HCMT7981-emmc
  DEVICE_PACKAGES := \
	kmod-conninfra kmod-mediatek_hnat kmod-mt_wifi kmod-warp \
	wifi-dats wifi-profile mtwifi-cfg luci-app-mtwifi-cfg \
	mtk-smp luci-app-c5800-688-wtmodem \
	kmod-wwan kmod-mhi-bus kmod-mhi-net kmod-mhi-pci-generic \
	kmod-mhi-wwan-ctrl kmod-mhi-wwan-mbim \
	kmod-usb-net-qmi-wwan kmod-usb-net-qmi-wwan-fibocom \
	kmod-usb-net-qmi-wwan-quectel kmod-usb-net-huawei-cdc-ncm \
	fibocom-dial quectel-CM-5G quectel-cm modemband sms-tool umbim \
	adb f2fsck mkf2fs \
	usbutils usbids uqmi \
	python3-light luci-theme-argon uhttpd uhttpd-mod-ubus \
	kmod-usb3 kmod-usb-core kmod-usb-xhci-hcd kmod-usb-xhci-mtk \
	kmod-usb-acm kmod-usb-ehci kmod-usb-ohci kmod-usb-uhci \
	kmod-usb-storage \
	kmod-usb-net kmod-usb-net-cdc-ether \
	kmod-usb-net-cdc-mbim kmod-usb-net-cdc-ncm \
	kmod-usb-net-rndis kmod-usb-net-sierrawireless \
	kmod-usb-serial kmod-usb-serial-option \
	kmod-usb-serial-qualcomm kmod-usb-serial-wwan \
	kmod-usb-wdm
  KERNEL_LOADADDR := 0x48000000
  KERNEL := c5800-688-factory-kernel
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := c5800-688-rootfs-system | c5800-688-sysupgrade-tar | c5800-688-append-metadata
endef
TARGET_DEVICES += nradio_c5800-688

define Device/nradio_c8-688
  DEVICE_VENDOR := NRadio
  DEVICE_MODEL := C8-688
  DEVICE_DTS := mt7981-nradio-c8-688
  DEVICE_DTS_DIR := ../dts
  BOARD_NAME := nradio_wt9104_No2-manper_New688
  SUPPORTED_DEVICES += HCMT7981-emmc nradio_wt9104_No2-manper_New688
  DEVICE_PACKAGES := \
	kmod-conninfra kmod-mediatek_hnat kmod-mt_wifi kmod-warp \
	wifi-dats wifi-profile mtwifi-cfg luci-app-mtwifi-cfg \
	mtk-smp \
	luci-app-turboacc-mtk luci-i18n-turboacc-mtk-zh-cn mtkhqos_util \
	adb usbutils usbids pciutils pciids usb-modeswitch \
	uqmi umbim chat comgt comgt-directip comgt-ncm wwan \
	python3-light luci-theme-argon uhttpd uhttpd-mod-ubus \
	f2fsck mkf2fs kmod-hwmon-core kmod-hwmon-pwmfan \
	kmod-usb2 kmod-usb3 kmod-usb-core kmod-usb-xhci-hcd kmod-usb-xhci-mtk \
	kmod-usb-acm kmod-usb-ehci kmod-usb-ohci kmod-usb-uhci \
	kmod-usb-storage kmod-usb-net kmod-usb-net-cdc-ether \
	kmod-usb-net-cdc-mbim kmod-usb-net-cdc-ncm \
	kmod-usb-net-huawei-cdc-ncm kmod-usb-net-qmi-wwan \
	kmod-usb-net-qmi-wwan-fibocom kmod-usb-net-qmi-wwan-quectel \
	kmod-usb-net-rndis kmod-usb-net-sierrawireless \
	kmod-usb-serial kmod-usb-serial-option \
	kmod-usb-serial-qualcomm kmod-usb-serial-sierrawireless kmod-usb-serial-wwan \
	kmod-usb-wdm kmod-mhi-bus kmod-mhi-pci-generic \
	kmod-mhi-net kmod-mhi-wwan-ctrl kmod-mhi-wwan-mbim
  KERNEL_LOADADDR := 0x48080000
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k
  IMAGE/sysupgrade.bin := nradio-fit-rootfs-metadata | sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += nradio_c8-688

define Device/nradio_c2000-max
  DEVICE_VENDOR := NRadio
  DEVICE_MODEL := C2000-MAX
  DEVICE_DTS := mt7987-nradio-wt9303-sd
  DEVICE_DTS_DIR := ../dts
  BOARD_NAME := HCMT7987-EMMC
  SUPPORTED_DEVICES += nradio,c2000-max HCMT7987-EMMC HCMT7987-SNSD
  DEVICE_PACKAGES := \
	luci-theme-myos-new luci-app-myos-system flock ip6tables-nft iptables-mod-socket kmod-ipt-socket kmod-ipt-nat-extra iptables-mod-nat-extra python3-ctypes luci-app-uugamebooster mt5700-web-go luci-app-mt5700-web \
	kmod-mediatek_hnat \
	kmod-oaf appfilter luci-app-oaf luci-i18n-oaf-zh-cn \
	kmod-hwmon-pwmfan \
	kmod-mt798x-2p5g-phy \
	kmod-ifb kmod-sfp kmod-ebtables \
	kmod-mtk_wed kmod-mtk_pci kmod-warp \
	kmod-tun openvpn-openssl libcap-ng liblzo2 liblz4-1 \
	luci-app-openvpn luci-i18n-openvpn-zh-cn \
	kmod-mt_wifi_osal kmod-mt_wifi7 kmod-mt_hwifi kmod-connac_if \
	kmod-mt7993 kmod-mt7993_dbg kmod-mt799a \
	wifi-dats wifi-scripts wifi-profile mtwifi-cfg luci-app-mtwifi-cfg \
	luci-app-store miniupnpd-nftables luci-app-upnp luci-i18n-upnp-zh-cn \
	python3-light uhttpd uhttpd-mod-ubus \
	f2fsck mkf2fs e2fsprogs losetup \
	zram-swap kmod-zram \
	kmod-usb3 kmod-usb-core kmod-usb-xhci-hcd kmod-usb-xhci-mtk \
	kmod-usb-acm kmod-usb-ehci kmod-usb-ohci kmod-usb-uhci \
	kmod-usb-storage kmod-usb-net kmod-usb-net-cdc-ether \
	kmod-usb-net-cdc-mbim kmod-usb-net-cdc-ncm \
	kmod-usb-net-huawei-cdc-ncm kmod-usb-net-qmi-wwan \
	kmod-usb-net-rndis kmod-usb-net-sierrawireless \
	kmod-usb-serial kmod-usb-serial-option \
	kmod-usb-serial-qualcomm kmod-usb-serial-wwan \
	kmod-usb-wdm kmod-mhi-bus kmod-mhi-pci-generic \
	kmod-mhi-net kmod-mhi-wwan-ctrl kmod-mhi-wwan-mbim \
	kmod-pcie_mhi kmod-qmi_wwan_q kmod-qmi_wwan_f \
	quectel-CM-5G quectel-CM-5G-M qmodem \
	coreutils-base64 coreutils-nohup coreutils-timeout lyaml \
	libyaml libudns resolveip libev \
	libsodium kmod-netlink-diag ip-full libuci-lua \
	libbpf libelf iptables-mod-tproxy kmod-ipt-tproxy \
	libpcap kmod-ipt-ipset ipset libipset \
	tc-full kmod-netem conntrack conntrackd \
	kmod-sched kmod-sched-core libnetfilter-cthelper libnetfilter-cttimeout \
	libnetfilter-queue libltdl liblua5.3
  KERNEL_LOADADDR := 0x40080000
  KERNEL := kernel-bin | lzma | fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb | \
	c2000max-firmware-permissions
  KERNEL_INITRAMFS := kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb with-initrd | pad-to 64k | \
	c2000max-firmware-permissions
  KERNEL_INITRAMFS_SUFFIX := .itb
  IMAGES := sysupgrade.bin
  IMAGE/sysupgrade.bin := prepare-c2000max-rootfs | prepare-c2000max-sd-kernel | \
	sysupgrade-tar kernel=$(C2000MAX_SD_KERNEL) rootfs=$(C2000MAX_MYOS_ROOTFS) | \
	append-metadata | c2000max-upload-size | c2000max-firmware-permissions
  ARTIFACTS :=
endef
TARGET_DEVICES += nradio_c2000-max

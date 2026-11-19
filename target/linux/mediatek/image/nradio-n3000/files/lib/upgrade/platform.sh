REQUIRE_IMAGE_METADATA=1

platform_check_image() {
	nand_do_platform_check 'NRadio-N3000-emmc' "$1"
}

platform_do_upgrade() {
	local CI_KERNPART=kernel_2nd CI_ROOTPART=rootfs_2nd CI_DATAPART=
	EMMC_KERN_DEV="$(find_mmc_part "$CI_KERNPART")"
	EMMC_ROOT_DEV="$(find_mmc_part "$CI_ROOTPART")"
	EMMC_DATA_DEV=
	if [ ! -b "$EMMC_KERN_DEV" ] || [ ! -b "$EMMC_ROOT_DEV" ]; then
		echo 'N3000: missing kernel_2nd or rootfs_2nd on eMMC' >&2
		return 1
	fi
	emmc_do_upgrade "$1"
}

platform_copy_config() {
	emmc_copy_config
}

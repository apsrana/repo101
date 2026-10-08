#!/usr/bin/env bash
# Install postmarketOS for the generic qcom-msm8953 device and flash the
# rootfs to userdata through lk2nd's fastboot.
#
# Usage: scripts/pmos-install.sh [--rootfs-only] [--ui console|phosh|plasma-mobile|...]
#                                [--kernel-src work/linux] [--no-flash]
#   --rootfs-only  console UI, for kernel bring-up with kernel-quick-boot.sh
#   --kernel-src   build linux-postmarketos-qcom-msm8953 from this tree (one that
#                  has the blueberry DTS, e.g. after kernel-quick-boot.sh --fetch).
#                  Needed until the DTS is in the packaged kernel; without it lk2nd
#                  finds no blueberry DTB when booting from eMMC.
#
# Before flashing, boot into lk2nd and select "Fastboot".
set -euo pipefail
. "$(dirname "$0")/common.sh"
need pmbootstrap fastboot

ui="phosh"
flash=1
kernel_src=""
while [ $# -gt 0 ]; do
	case "$1" in
	--rootfs-only) ui="console"; shift ;;
	--ui) ui="$2"; shift 2 ;;
	--kernel-src) kernel_src="$(realpath "$2")"; shift 2 ;;
	--no-flash) flash=0; shift ;;
	-h|--help) sed -n 2,12p "$0"; exit 0 ;;
	*) die "unknown option $1" ;;
	esac
done

if [ "$(pmbootstrap config device 2>/dev/null)" != "qcom-msm8953" ]; then
	log "Running 'pmbootstrap init'. Choose: vendor 'qcom', device 'msm8953', UI '$ui'."
	pmbootstrap init
fi
[ "$(pmbootstrap config device)" = "qcom-msm8953" ] || die "pmbootstrap device must be qcom-msm8953"
pmbootstrap config ui "$ui"

if [ -n "$kernel_src" ]; then
	[ -f "$kernel_src/arch/arm64/boot/dts/qcom/$SD10_DTS_NAME.dts" ] ||
		die "$kernel_src has no $SD10_DTS_NAME.dts (run kernel-quick-boot.sh --fetch --no-boot)"
	log "Building linux-postmarketos-qcom-msm8953 from $kernel_src"
	pmbootstrap build --force --src="$kernel_src" linux-postmarketos-qcom-msm8953
elif [ "$flash" = 1 ]; then
	warn "No --kernel-src: the packaged kernel has no blueberry DTB yet, so only"
	warn "kernel-quick-boot.sh will boot this rootfs."
fi

log "Building the image"
pmbootstrap install --password "${PMOS_PASSWORD:-147147}"

if [ "$flash" = 1 ]; then
	fastboot devices | grep -q . || die "no fastboot device. Boot lk2nd and select 'Fastboot'."
	# Only accept lk2nd's fastboot. Stock fastboot would flash userdata too,
	# but the stock bootloader can't boot it.
	fastboot getvar lk2nd:version 2>&1 | grep -q "^lk2nd:version:" ||
		confirm "This fastboot does not report lk2nd. Flash userdata anyway?"
	log "Flashing the rootfs to userdata"
	pmbootstrap flasher flash_rootfs --partition userdata
	log "Done. Run 'fastboot reboot'. lk2nd boots pmOS from extlinux.conf on userdata."
	[ -n "${PMOS_PASSWORD:-}" ] || warn "default password 147147: change it with 'passwd' after first login"
fi

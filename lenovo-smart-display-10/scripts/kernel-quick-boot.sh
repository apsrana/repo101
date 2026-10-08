#!/usr/bin/env bash
# Fast kernel/DTS loop: build msm8953-mainline Linux with the blueberry
# DTS and `fastboot boot` it through lk2nd. Nothing is flashed.
#
# Usage:
#   scripts/kernel-quick-boot.sh --fetch     clone the kernel at the pmOS tag and add the DTS
#   scripts/kernel-quick-boot.sh [--no-boot] [--cmdline "extra args"]
#
# The ramdisk and root= arguments are taken from a pmbootstrap install
# (scripts/pmos-install.sh --rootfs-only). Without one, the image still
# boots far enough to test display/touch over the serial console.
set -euo pipefail
. "$(dirname "$0")/common.sh"
need git make clang ld.lld python3

src="$SD10_WORK/linux"
dts_dir="$src/arch/arm64/boot/dts/qcom"
cfg_url="https://gitlab.postmarketos.org/postmarketOS/pmaports/-/raw/main/device/community/linux-postmarketos-qcom-msm8953/config-postmarketos-qcom-msm8953.aarch64"

fetch=0
boot=1
extra=""
while [ $# -gt 0 ]; do
	case "$1" in
	--fetch) fetch=1; shift ;;
	--no-boot) boot=0; shift ;;
	--cmdline) extra="$2"; shift 2 ;;
	-h|--help) sed -n 2,12p "$0"; exit 0 ;;
	*) die "unknown option $1" ;;
	esac
done

if [ "$fetch" = 1 ] || [ ! -d "$src/.git" ]; then
	if [ ! -d "$src/.git" ]; then
		log "Cloning $SD10_KERNEL_REPO at $SD10_KERNEL_TAG"
		git clone --depth 1 --branch "$SD10_KERNEL_TAG" "$SD10_KERNEL_REPO" "$src"
	fi
	log "Fetching the pmOS kernel config"
	curl -fsSL "$cfg_url" -o "$src/arch/arm64/configs/pmos_msm8953_defconfig"
fi

# Keep the DTS in this repo as the source of truth; copy it in every time.
cp "$SD10_ROOT/dts/linux/$SD10_DTS_NAME.dts" "$dts_dir/"
grep -q "$SD10_DTS_NAME.dtb" "$dts_dir/Makefile" ||
	sed -i "s|^dtb-\$(CONFIG_ARCH_QCOM)\s*+= apq8053-lenovo-cd-18781y.dtb|&\ndtb-\$(CONFIG_ARCH_QCOM)\t+= $SD10_DTS_NAME.dtb|" \
		"$dts_dir/Makefile"
grep -q "$SD10_DTS_NAME.dtb" "$dts_dir/Makefile" || die "could not add the DTB to $dts_dir/Makefile"

mk() { make -C "$src" ARCH=arm64 LLVM=1 -j"$(nproc)" "$@"; }

if [ ! -f "$src/.config" ]; then
	mk pmos_msm8953_defconfig
	# The pmOS rootfs ships modules for the packaged kernel, not this one,
	# so build the bring-up drivers in.
	"$src/scripts/config" --file "$src/.config" \
		--enable DRM_MSM \
		--enable DRM_PANEL_LENOVO_CD_18781Y_HX83100A \
		--enable TOUCHSCREEN_HIMAX_HX83112B \
		--enable USB_LIBCOMPOSITE --enable USB_CONFIGFS \
		--enable USB_F_NCM --enable USB_F_RNDIS --enable USB_F_ECM \
		--enable SERIAL_MSM --enable SERIAL_MSM_CONSOLE
	mk olddefconfig
fi

log "Building Image.gz and $SD10_DTS_NAME.dtb"
mk Image.gz "qcom/$SD10_DTS_NAME.dtb"

out="$SD10_OUT/kernel"
mkdir -p "$out"
cat "$src/arch/arm64/boot/Image.gz" "$dts_dir/$SD10_DTS_NAME.dtb" > "$out/Image.gz-dtb"
cp "$dts_dir/$SD10_DTS_NAME.dtb" "$out/"

# Reuse the initramfs and root arguments from a pmbootstrap install, if there is one.
ramdisk_args=()
cmdline="earlycon console=ttyMSM0,115200 console=tty0 loglevel=7 PMOS_NOSPLASH"
if command -v pmbootstrap >/dev/null; then
	pmwork="$(pmbootstrap config work 2>/dev/null || true)"
	chroot="$pmwork/chroot_rootfs_qcom-msm8953"
	if [ -f "$chroot/boot/initramfs" ]; then
		ramdisk_args=(--ramdisk "$chroot/boot/initramfs")
		append="$(grep -m1 -E '^\s*append' "$chroot/boot/extlinux/extlinux.conf" 2>/dev/null | sed 's/^\s*append\s*//')"
		cmdline="$append $cmdline"
		log "Using pmOS initramfs from $chroot"
	fi
fi
[ ${#ramdisk_args[@]} -gt 0 ] || warn "no pmbootstrap rootfs found: booting without an initramfs"
cmdline="$cmdline $extra"

$(mkbootimg_cmd) \
	--kernel "$out/Image.gz-dtb" "${ramdisk_args[@]}" \
	--base 0x80000000 --pagesize 2048 \
	--cmdline "$cmdline" \
	-o "$out/boot.img"
log "Built $out/boot.img"
log "cmdline: $cmdline"

if [ "$boot" = 1 ]; then
	need fastboot
	fastboot devices | grep -q . || die "no fastboot device. Select 'Fastboot' in the lk2nd menu."
	fastboot boot "$out/boot.img"
	log "Booting. Over USB: ssh user@172.16.42.1   Serial: picocom -b 115200 /dev/ttyUSB0"
fi

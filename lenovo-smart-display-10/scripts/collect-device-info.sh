#!/usr/bin/env bash
# Fingerprint the connected Smart Display over adb and/or fastboot.
# Read-only: nothing is written to the device.
#
# Usage: scripts/collect-device-info.sh [outdir]
set -euo pipefail
. "$(dirname "$0")/common.sh"
need adb fastboot

out="${1:-$SD10_OUT/info-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$out"
log "Writing to $out"

lsusb 2>/dev/null | grep -Ei '05c6|18d1' > "$out/lsusb.txt" || true
cat "$out/lsusb.txt"

if adb get-state >/dev/null 2>&1; then
	log "adb: device found"
	adb devices -l                    > "$out/adb-devices.txt"
	adb shell getprop                 > "$out/getprop.txt"
	adb shell cat /proc/cmdline       > "$out/cmdline.txt"         2>/dev/null || true
	adb shell cat /proc/cpuinfo       > "$out/cpuinfo.txt"         2>/dev/null || true
	adb shell cat /proc/partitions    > "$out/partitions.txt"      2>/dev/null || true
	adb shell ls -l /dev/block/bootdevice/by-name/ > "$out/by-name.txt" 2>/dev/null || true
	adb shell cat /proc/device-tree/model > "$out/dt-model.txt"    2>/dev/null || true
	# msm-id and board-id are what lk2nd's and Linux's DTS headers need.
	for p in qcom,msm-id qcom,board-id compatible; do
		adb shell "od -An -tx4 /proc/device-tree/$p 2>/dev/null || xxd -p /proc/device-tree/$p" \
			> "$out/dt-${p//,/_}.txt" 2>/dev/null || true
	done
	adb shell 'cat /sys/class/graphics/fb0/msm_fb_panel_info /sys/class/graphics/fb0/name 2>/dev/null; dmesg 2>/dev/null | grep -iE "panel|mdss_dsi|himax|touch" | head -50' \
		> "$out/panel.txt" 2>/dev/null || true
	adb shell 'zcat /proc/config.gz 2>/dev/null' > "$out/kernel-config.txt" || true
	adb shell dmesg > "$out/dmesg.txt" 2>/dev/null || true
	adb shell 'ls -lR /vendor/firmware /firmware /bt_firmware /dsp /oem/firmware 2>/dev/null' \
		> "$out/firmware-listing.txt" || true
	adb shell 'ls -l /sys/bus/i2c/devices/; for d in /sys/bus/i2c/devices/*; do echo "$d $(cat $d/name 2>/dev/null)"; done' \
		> "$out/i2c.txt" 2>/dev/null || true
	grep -E 'ro.build.fingerprint|ro.boot.slot_suffix|ro.boot.verifiedbootstate|ro.boot.flash.locked|ro.product.(model|device)|ro.oem.product.model' \
		"$out/getprop.txt" || true
else
	warn "adb: no device (that's fine if it's in fastboot)"
fi

if fastboot devices | grep -q .; then
	log "fastboot: device found"
	fastboot getvar all > "$out/fastboot-getvar-all.txt" 2>&1 || true
	grep -Ei 'unlocked|secure|current-slot|product|variant|version-bootloader|slot-count' \
		"$out/fastboot-getvar-all.txt" || true
	fastboot oem device-info > "$out/fastboot-device-info.txt" 2>&1 || true
else
	warn "fastboot: no device. To check, run: adb reboot bootloader"
fi

log "Done. Copy the board-id into docs/HARDWARE.md and the DTS drafts."

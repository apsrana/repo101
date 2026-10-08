#!/usr/bin/env bash
# Install the host tools for unlocking and porting the Smart Display 10:
# adb/fastboot, bkerler/edl, pmbootstrap, dtc, the arm-none-eabi toolchain
# (lk2nd), clang/lld (kernel) and mkbootimg.
#
# Usage: scripts/setup-host.sh [--no-udev]
set -euo pipefail
. "$(dirname "$0")/common.sh"

install_udev=1
[ "${1:-}" = "--no-udev" ] && install_udev=0

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

log "Installing distro packages"
if command -v apt-get >/dev/null; then
	$SUDO apt-get update
	$SUDO apt-get install -y \
		adb fastboot git make python3 python3-pip python3-venv pipx \
		python3-dev liblzma-dev libusb-1.0-0 \
		gcc-arm-none-eabi device-tree-compiler libfdt-dev \
		python3-pyasn1-modules python3-pycryptodome \
		clang lld llvm bc bison flex libssl-dev libelf-dev cpio kmod \
		mkbootimg usbutils
elif command -v dnf >/dev/null; then
	$SUDO dnf install -y \
		android-tools git make python3 python3-pip pipx python3-devel \
		xz-devel libusb1 \
		arm-none-eabi-gcc-cs dtc libfdt-devel \
		python3-pyasn1-modules python3-pycryptodomex \
		clang lld llvm bc bison flex openssl-devel elfutils-libelf-devel cpio kmod \
		usbutils
elif command -v pacman >/dev/null; then
	$SUDO pacman -S --needed --noconfirm \
		android-tools git make python python-pip python-pipx xz libusb \
		arm-none-eabi-gcc arm-none-eabi-newlib dtc \
		python-pyasn1-modules python-pycryptodome \
		clang lld llvm bc bison flex openssl libelf cpio kmod \
		usbutils
else
	die "unsupported distro: install the packages listed in this script by hand"
fi

log "Installing edl and pmbootstrap with pipx"
pipx ensurepath >/dev/null || true
pipx install --force "git+https://github.com/bkerler/edl.git"
pipx install --force pmbootstrap
# Some distros don't ship mkbootimg/unpack_bootimg; use AOSP's scripts.
if ! mkbootimg --help >/dev/null 2>&1 || ! command -v unpack_bootimg >/dev/null; then
	log "Fetching AOSP mkbootimg/unpack_bootimg into $SD10_WORK/tools"
	mkdir -p "$SD10_WORK/tools" "$HOME/.local/bin"
	[ -d "$SD10_WORK/tools/mkbootimg" ] ||
		git clone --depth 1 https://android.googlesource.com/platform/system/tools/mkbootimg \
			"$SD10_WORK/tools/mkbootimg"
	ln -sf "$SD10_WORK/tools/mkbootimg/mkbootimg.py" "$HOME/.local/bin/mkbootimg"
	ln -sf "$SD10_WORK/tools/mkbootimg/unpack_bootimg.py" "$HOME/.local/bin/unpack_bootimg"
fi

if [ "$install_udev" = 1 ]; then
	log "Installing udev rules for EDL (05c6:9008/900e/901d) and fastboot (18d1)"
	$SUDO tee /etc/udev/rules.d/51-sd10-qcom.rules >/dev/null <<'RULES'
# Qualcomm EDL / diag / debug composite
SUBSYSTEM=="usb", ATTR{idVendor}=="05c6", MODE="0666", TAG+="uaccess"
# Google fastboot / adb
SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", TAG+="uaccess"
RULES
	$SUDO udevadm control --reload-rules
	$SUDO udevadm trigger
	# ModemManager probes 05c6 devices and can break the Sahara handshake.
	if systemctl is-active --quiet ModemManager 2>/dev/null; then
		warn "ModemManager is running and may grab the device in EDL mode."
		warn "Stop it while flashing: sudo systemctl stop ModemManager"
	fi
fi

log "Checking tools"
for c in adb fastboot edl pmbootstrap dtc arm-none-eabi-gcc clang mkbootimg unpack_bootimg; do
	if command -v "$c" >/dev/null; then
		printf '  %-18s %s\n' "$c" "$(command -v "$c")"
	else
		warn "$c not on PATH (open a new shell after 'pipx ensurepath')"
	fi
done

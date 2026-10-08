#!/usr/bin/env bash
# Build lk2nd for msm8953 with the blueberry board file added.
#
# Usage: scripts/build-lk2nd.sh [--sign] [--fbcon] [--ref <git ref>]
#   --sign   sign with the AOSP AVB1 test key (Path B: locked bootloader)
#   --fbcon  print lk2nd logs on the display (DEBUG_FBCON=1)
set -euo pipefail
. "$(dirname "$0")/common.sh"
need git make arm-none-eabi-gcc dtc

sign=0
fbcon=0
ref=""
while [ $# -gt 0 ]; do
	case "$1" in
	--sign) sign=1; shift ;;
	--fbcon) fbcon=1; shift ;;
	--ref) ref="$2"; shift 2 ;;
	-h|--help) sed -n 2,7p "$0"; exit 0 ;;
	*) die "unknown option $1" ;;
	esac
done

src="$SD10_WORK/lk2nd"
if [ ! -d "$src/.git" ]; then
	log "Cloning lk2nd"
	git clone "$SD10_LK2ND_REPO" "$src"
fi
if [ -n "$ref" ]; then
	git -C "$src" fetch origin "$ref" && git -C "$src" checkout -q FETCH_HEAD
fi

dtsdir="$src/lk2nd/device/dts/msm8953"
cp "$SD10_ROOT/dts/lk2nd/$SD10_DTS_NAME.dts" "$dtsdir/"
if ! grep -q "$SD10_DTS_NAME.dtb" "$dtsdir/rules.mk"; then
	log "Registering $SD10_DTS_NAME in rules.mk"
	# Insert next to the ThinkSmart View entry.
	sed -i "s|^\(\s*\)\$(LOCAL_DIR)/apq8053-lenovo-cd-18781y.dtb \\\\|&\n\1\$(LOCAL_DIR)/$SD10_DTS_NAME.dtb \\\\|" \
		"$dtsdir/rules.mk"
	grep -q "$SD10_DTS_NAME.dtb" "$dtsdir/rules.mk" || die "could not patch rules.mk; add the DTB by hand"
fi

if [ "$sign" = 1 ] && ! python3 -c 'import Crypto, pyasn1_modules' 2>/dev/null; then
	# lk2nd's bootsignature.py imports "Crypto"; Debian's python3-pycryptodome
	# only provides "Cryptodome". Use a private venv instead.
	venv="$SD10_WORK/venv-lk2nd"
	if [ ! -x "$venv/bin/python3" ]; then
		log "Creating $venv with pycryptodome for image signing"
		python3 -m venv "$venv"
		"$venv/bin/pip" install -q pycryptodome pyasn1 pyasn1-modules
	fi
	export PATH="$venv/bin:$PATH"
fi

log "Building (SIGN_BOOTIMG=$sign, DEBUG_FBCON=$fbcon)"
make -C "$src" -j"$(nproc)" \
	TOOLCHAIN_PREFIX=arm-none-eabi- \
	LK2ND_QCDTBS="" \
	SIGN_BOOTIMG="$sign" \
	DEBUG_FBCON="$fbcon" \
	lk2nd-msm8953

mkdir -p "$SD10_OUT"
cp "$src/build-lk2nd-msm8953/lk2nd.img" "$SD10_OUT/lk2nd.img"
sha256sum "$SD10_OUT/lk2nd.img"
log "Built $SD10_OUT/lk2nd.img. Flash it with scripts/flash-lk2nd.sh"

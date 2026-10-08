#!/usr/bin/env bash
# Extract the stock (downstream) device trees from a boot image backup,
# decompile them, find the one matching this board and print the facts
# the mainline/lk2nd DTS need (panel, touch, keys, Wi-Fi/BT, audio).
#
# Usage: scripts/extract-downstream-dtb.sh <boot_a.bin> [board-id hex, default 0x01010020]
set -euo pipefail
. "$(dirname "$0")/common.sh"
need python3 dtc

img="${1:?usage: $0 <boot_a.bin> [board-id]}"
board_id="${2:-0x01010020}"
out="$SD10_OUT/downstream-dtb"
rm -rf "$out"; mkdir -p "$out/dtb" "$out/dts"

log "Splitting device trees out of $img"
# split-dtb.py scans for FDT magic, so it handles both appended DTBs
# (zImage-dtb) and a QCDT blob stored in the boot image.
python3 -I "$SD10_ROOT/scripts/split-dtb.py" "$img" "$out/dtb"
for f in "$out"/dtb/*.dtb; do
	dtc -q -I dtb -O dts -o "$out/dts/$(basename "${f%.dtb}").dts" "$f" 2>/dev/null || true
done
log "$(ls "$out/dts" | wc -l) device trees decompiled into $out/dts"

# Match "qcom,board-id = <0x1010020 0x00>" in any formatting.
want="$(printf '0x%x' "$board_id")"
match=""
for f in "$out"/dts/*.dts; do
	if grep -Eiq "qcom,board-id = <${want}[ >]" "$f"; then
		match="$f"
		printf '  %s: %s\n' "$(basename "$f")" "$(grep -m1 -E '^\s*model = ' "$f" | sed 's/^\s*//')"
	fi
done
[ -n "$match" ] || {
	warn "no DTS has board-id $want. All board-ids found:"
	grep -h "qcom,board-id" "$out"/dts/*.dts | sort | uniq -c
	exit 1
}
log "Using $(basename "$match")"
cp "$match" "$out/blueberry-downstream.dts"
f="$out/blueberry-downstream.dts"

section() { printf '\n\033[1m--- %s ---\033[0m\n' "$1"; }

section "IDs"
grep -E 'qcom,(msm-id|board-id|pmic-id) =|^\s*model =' "$f" | head

section "Panels (look for hx83100a)"
grep -nE 'qcom,mdss_dsi_[a-z0-9_]+ \{|qcom,mdss-dsi-panel-name' "$f" || true

section "DSI GPIOs / supplies"
grep -nE 'qcom,platform-(reset|enable|bklight-en|te)-gpio|qcom,panel-supply-entries|qcom,dsi-pref-prim-pan' "$f" || true

section "Touch / I2C devices"
grep -nE 'himax|focaltech|goodix|synaptics|,irq-gpio|,reset-gpio' "$f" | head -40 || true

section "Keys"
awk '/gpio_keys|gpio-keys/,/^\t\};/' "$f" | grep -E 'label|gpios|linux,code' || true

section "Wi-Fi / BT"
grep -nE 'cnss|qca9379|qca6174|wlan|bt_qca|bluetooth|naples' "$f" | head -30 || true

section "Audio"
grep -nE 'qcom,model =|tas57|tas25|max98|aw8|amp|mi2s' "$f" | head -40 || true

section "Panel on-command (compare with panel-lenovo-cd-18781y-hx83100a.c)"
awk '/qcom,mdss_dsi_hx83100a_800p_video/,/^\t\t\};/' "$f" | grep -A3 -E 'qcom,mdss-dsi-on-command =' | head -20 || true

cat <<MSG

Full DTS: $f
To generate a panel driver from it:
  git clone https://github.com/msm8916-mainline/linux-mdss-dsi-panel-driver-generator
  python3 linux-mdss-dsi-panel-driver-generator/lmdpdg.py $out/dtb/<matching>.dtb
Record the results in docs/HARDWARE.md and remove the TODO(verify) markers in dts/.
MSG

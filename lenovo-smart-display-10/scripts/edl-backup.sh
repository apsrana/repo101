#!/usr/bin/env bash
# Full eMMC backup over Qualcomm EDL (9008). Read-only.
#
# Usage: scripts/edl-backup.sh [--loader prog_emmc_firehose_8953_ddr.mbn] [--skip-userdata] [outdir]
#
# Start this first, then power the device into EDL (Vol+ and Vol- held while
# plugging in power, or `adb reboot edl`). The Sahara window is short.
set -euo pipefail
. "$(dirname "$0")/common.sh"
need edl sha256sum

loader=()
skip=()
out=""
while [ $# -gt 0 ]; do
	case "$1" in
	--loader) loader=(--loader="$(realpath "$2")"); shift 2 ;;
	--skip-userdata) skip=(--skip=userdata); shift ;;
	-h|--help) sed -n 2,8p "$0"; exit 0 ;;
	*) out="$1"; shift ;;
	esac
done
out="${out:-$SD10_BACKUPS/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$out/dump" "$out/gpt"
out="$(realpath "$out")"
log "Backing up to $out"

if lsusb 2>/dev/null | grep -qi '05c6:900e'; then
	die "device is in 900E (diag), which is not writable/readable over firehose. Force 9008 (keys or EDL cable)."
fi

# One edl session per command. edl reconnects to the loaded firehose
# programmer after the first upload.
log "Partition table"
edl printgpt --memory=emmc "${loader[@]}" | tee "$out/printgpt.txt"

log "Primary/backup GPT + rawprogram0.xml"
( cd "$out/gpt" && edl gpt . --genxml --memory=emmc "${loader[@]}" )

log "All partitions${skip:+ (except userdata)}. This takes a while."
edl rl "$out/dump" --genxml --memory=emmc "${skip[@]}" "${loader[@]}"

log "Re-reading boot_a/boot_b to check the dump is stable"
mkdir -p "$out/verify"
edl r boot_a,boot_b "$out/verify/boot_a.bin,$out/verify/boot_b.bin" --memory=emmc "${loader[@]}" || true
for s in a b; do
	if [ -f "$out/verify/boot_$s.bin" ] && [ -f "$out/dump/boot_$s.bin" ]; then
		if cmp -s "$out/verify/boot_$s.bin" "$out/dump/boot_$s.bin"; then
			log "boot_$s: stable"
		else
			warn "boot_$s differs between reads, so the dump is not trustworthy"
		fi
	fi
done

log "Hashing"
( cd "$out" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS )

cat <<MSG

Backup complete: $out
  - Copy this whole directory to a second disk now.
  - Keep gpt/ and dump/persist.bin, modemst*, fsg, devinfo, keystore safe.
    They are unique to this unit.
  - Next step: scripts/extract-downstream-dtb.sh $out/dump/boot_a.bin
MSG

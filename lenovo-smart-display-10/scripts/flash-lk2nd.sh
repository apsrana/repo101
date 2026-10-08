#!/usr/bin/env bash
# Write lk2nd.img to ONE boot slot. The other slot keeps the stock boot
# image as a fallback.
#
# Usage: scripts/flash-lk2nd.sh --slot a|b --via fastboot|edl [--image out/lk2nd.img]
#                               [--loader firehose.mbn] [--set-active]
#   --via fastboot  unlocked bootloader (Path A)
#   --via edl       locked + test-key signed image (Path B), or fastboot refuses
#   --set-active    fastboot only: make that slot active afterwards
set -euo pipefail
. "$(dirname "$0")/common.sh"

slot=""
via=""
image="$SD10_OUT/lk2nd.img"
loader=()
set_active=0
while [ $# -gt 0 ]; do
	case "$1" in
	--slot) slot="$2"; shift 2 ;;
	--via) via="$2"; shift 2 ;;
	--image) image="$2"; shift 2 ;;
	--loader) loader=(--loader="$(realpath "$2")"); shift 2 ;;
	--set-active) set_active=1; shift ;;
	-h|--help) sed -n 2,10p "$0"; exit 0 ;;
	*) die "unknown option $1" ;;
	esac
done
case "$slot" in a|b) ;; *) die "--slot a|b is required" ;; esac
case "$via" in fastboot|edl) ;; *) die "--via fastboot|edl is required" ;; esac
[ -f "$image" ] || die "$image not found. Run scripts/build-lk2nd.sh first."

# Refuse to write anything until there's a backup with a hash manifest.
if ! compgen -G "$SD10_BACKUPS/*/SHA256SUMS" >/dev/null; then
	die "no backup found in $SD10_BACKUPS. Run scripts/edl-backup.sh first."
fi
backup="$(dirname "$(ls -1t "$SD10_BACKUPS"/*/SHA256SUMS | head -1)")"
[ -f "$backup/dump/boot_$slot.bin" ] || die "backup $backup has no boot_$slot.bin"
log "Latest backup: $backup"
log "To restore this slot: edl w boot_$slot $backup/dump/boot_$slot.bin"

confirm "About to write $(basename "$image") to boot_$slot via $via."

if [ "$via" = fastboot ]; then
	need fastboot
	fastboot devices | grep -q . || die "no fastboot device (adb reboot bootloader)"
	fastboot flash "boot_$slot" "$image"
	if [ "$set_active" = 1 ]; then
		fastboot --set-active="$slot"
	fi
	log "Done. Reboot with: fastboot reboot"
else
	need edl
	edl w "boot_$slot" "$image" --memory=emmc "${loader[@]}"
	log "Written. If boot_$slot is not the active slot, boot to stock fastboot"
	log "and run 'fastboot --set-active=$slot', or write the other slot as well."
	edl reset "${loader[@]}" || true
fi

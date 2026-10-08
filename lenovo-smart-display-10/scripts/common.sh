# Shared helpers, sourced by the other scripts.
# shellcheck shell=bash disable=SC2034  # variables are used by the sourcing scripts

SD10_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SD10_WORK="${SD10_WORK:-$SD10_ROOT/work}"
SD10_BACKUPS="${SD10_BACKUPS:-$SD10_ROOT/backups}"
SD10_OUT="${SD10_OUT:-$SD10_ROOT/out}"

# Kernel tag used by pmaports' linux-postmarketos-qcom-msm8953.
SD10_KERNEL_REPO="${SD10_KERNEL_REPO:-https://github.com/msm8953-mainline/linux.git}"
SD10_KERNEL_TAG="${SD10_KERNEL_TAG:-v7.1.3-r0}"
SD10_LK2ND_REPO="${SD10_LK2ND_REPO:-https://github.com/msm8916-mainline/lk2nd.git}"

SD10_DTS_NAME="apq8053-lenovo-blueberry"

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarn:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

need() {
	local c
	for c in "$@"; do
		command -v "$c" >/dev/null 2>&1 || die "'$c' not found. Run scripts/setup-host.sh first."
	done
}

# Ask for an explicit "yes" before anything that writes to the device.
confirm() {
	local reply
	printf '\033[1;31m%s\033[0m\nType "yes" to continue: ' "$*"
	read -r reply
	[ "$reply" = "yes" ] || die "aborted"
}

# Debian's packaged mkbootimg can fail with "No module named 'gki'".
# Use it only if it actually runs; otherwise use AOSP's script.
mkbootimg_cmd() {
	if command -v mkbootimg >/dev/null && mkbootimg --help >/dev/null 2>&1; then
		echo mkbootimg
		return
	fi
	local aosp="$SD10_WORK/tools/mkbootimg"
	if [ ! -f "$aosp/mkbootimg.py" ]; then
		git clone -q --depth 1 https://android.googlesource.com/platform/system/tools/mkbootimg "$aosp" >&2 ||
			die "could not fetch AOSP mkbootimg"
	fi
	echo "python3 $aosp/mkbootimg.py"
}

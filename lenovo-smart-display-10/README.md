# Lenovo Smart Display 10: unlock and postmarketOS port

This directory holds the tooling and notes to:

1. back up and unlock the **Lenovo Smart Display 10"** (model `SD-X701B` /
   `SD-X501F` / `SD-X701F`, codename **`blueberry`**), then
2. boot **postmarketOS** on it with a close-to-mainline kernel, through `lk2nd`.

It is independent of the Furby firmware in the rest of this repository.

> **Brick risk.** Everything here writes to the eMMC of a device that has no
> official unlock and no official firmware downloads any more. Take the full
> EDL backup in [docs/UNLOCK.md](docs/UNLOCK.md) **before anything else**, and
> keep it somewhere other than your laptop. One person who skipped the exact
> partition table had to rebuild a bricked unit from a factory GPT image
> (see the recovery report linked in [Sources](#sources)).

## Why this port is realistic

The Smart Display 10 runs Android Things on the Qualcomm **"Home Hub"
SDA624**, which is an **APQ8053** (the modem-less MSM8953 / Snapdragon 625).
Its sibling, the **Lenovo ThinkSmart View (`lenovo-cd-18781y`)**, is
already supported by postmarketOS through the generic `qcom-msm8953` port,
and much of the hardware is shared:

| Block | Smart Display 10 (`blueberry`), stock | ThinkSmart View (`cd-18781y`), mainline | Status for this port |
|---|---|---|---|
| SoC | APQ8053, msm-id 304 | APQ8053, msm-id 304 | same |
| Panel | Himax **HX83100A** 800×1280 DSI video, 4-lane | one variant uses the **HX83100A** 800p panel | driver `panel-lenovo-cd-18781y-hx83100a` exists, but the init sequence still has to be compared |
| Touch | Himax HX83100A, I²C | `himax,hx83100a` node at i2c_3 0x48 | upstream `himax_hx83112b` driver supports it |
| Backlight | WLED (`lcd-backlight`) | PMI8950 WLED | likely the same |
| Wi-Fi / BT | QCA (CLD driver), BT `naples_uart` | QCA9379 SDIO + UART BT | probably the same, still to confirm |
| Audio | `msm8953-openq624-snd-card` | openq624 + TAS5782M amplifier | amplifier and wiring still to confirm |
| Board-id | `0x01010020` (from the extracted DTB) | `0x1010520` | **different**: needs its own DTS |
| Display rotation | panel is portrait, Android rotates it 270° | portrait | handle in the DTS or in userspace |
| Storage | eMMC, A/B slots, AVB (`vbmeta_a/b`) | eMMC, A/B | same layout family |

So the port mostly means **writing a new board DTS** (for both lk2nd and
Linux), checked against the stock device tree, rather than writing new
drivers.

## Phases

| # | Phase | Doc / script | Done when |
|---|---|---|---|
| 0 | Host setup | `scripts/setup-host.sh` | `edl`, `adb`, `fastboot`, `pmbootstrap`, `dtc`, `arm-none-eabi-gcc` are on `PATH` |
| 1 | Get USB access and identify the unit | [docs/UNLOCK.md §1–2](docs/UNLOCK.md) and `scripts/collect-device-info.sh` | `adb`/`fastboot`/EDL each see the device; the info bundle is saved |
| 2 | **Full backup** | `scripts/edl-backup.sh` | every partition plus the GPT is dumped, hashed and copied off-machine |
| 3 | Unlock / AVB | [docs/UNLOCK.md §4](docs/UNLOCK.md) | a custom-signed or unsigned boot image boots |
| 4 | Read the stock hardware description | `scripts/extract-downstream-dtb.sh` | panel, touch, GPIO and regulator facts are recorded in `docs/HARDWARE.md` |
| 5 | lk2nd | `dts/lk2nd/`, `scripts/build-lk2nd.sh`, `scripts/flash-lk2nd.sh` | lk2nd menu on screen, `fastboot devices` shows lk2nd |
| 6 | First mainline boot | `dts/linux/`, `scripts/kernel-quick-boot.sh` | USB networking / SSH into a pmOS rootfs |
| 7 | Full pmOS install | `scripts/pmos-install.sh` | a desktop UI (Phosh, Plasma Mobile…) runs from eMMC |
| 8 | Bring-up and upstreaming | [docs/PORTING.md](docs/PORTING.md) | DTS sent to `msm8953-mainline`, lk2nd and `pmaports` |

## Layout

```
lenovo-smart-display-10/
├── README.md                 this file
├── docs/
│   ├── UNLOCK.md             USB access, EDL, backup, unlock, un-brick
│   ├── PORTING.md            pmbootstrap workflow, DTS bring-up, upstreaming
│   └── HARDWARE.md           facts table, filled in from your own unit
├── dts/
│   ├── lk2nd/apq8053-lenovo-blueberry.dts   draft lk2nd board file
│   └── linux/apq8053-lenovo-blueberry.dts   draft mainline board file (minimal bring-up)
└── scripts/
    ├── common.sh                  shared paths/helpers (kernel tag, repos)
    ├── setup-host.sh              install the host tools (Debian/Ubuntu/Fedora/Arch)
    ├── collect-device-info.sh     adb + fastboot fingerprinting
    ├── edl-backup.sh              full eMMC backup over EDL, with SHA-256 manifest
    ├── extract-downstream-dtb.sh  pull the stock DTBs out of boot.img and grep the facts
    ├── split-dtb.py               dependency-free DTB splitter used by the above
    ├── build-lk2nd.sh             build lk2nd with the blueberry DTS
    ├── flash-lk2nd.sh             write lk2nd to one boot slot (EDL or fastboot)
    ├── kernel-quick-boot.sh       build the msm8953-mainline kernel and `fastboot boot` it via lk2nd
    └── pmos-install.sh            pmbootstrap init/install/flash for qcom-msm8953
```

Backups, logs and build trees go to `work/`, `backups/` and `out/`, which are
git-ignored. **Never commit partition dumps:** they contain per-device
keys, serials and calibration data.

## Quick start

```bash
cd lenovo-smart-display-10
scripts/setup-host.sh                         # 0. tools
scripts/collect-device-info.sh                # 1. fingerprint (adb and/or fastboot)
scripts/edl-backup.sh --loader <firehose.mbn> # 2. FULL BACKUP, then copy it off-machine
#                                               3. unlock: follow docs/UNLOCK.md §4
scripts/extract-downstream-dtb.sh backups/<date>/dump/boot_a.bin   # 4. facts → docs/HARDWARE.md
scripts/build-lk2nd.sh [--sign]               # 5. lk2nd
scripts/flash-lk2nd.sh --slot a --via fastboot|edl
scripts/pmos-install.sh --rootfs-only         # 6. pmOS rootfs on userdata
scripts/kernel-quick-boot.sh --fetch          #    then iterate on dts/linux/*.dts
scripts/pmos-install.sh --ui phosh --kernel-src work/linux   # 7. full install
```

What has been checked without hardware: both lk2nd variants (plain and
AVB1 test-key signed) build with the blueberry board file, the draft Linux
DTS compiles against the `msm8953-mainline` tag that pmOS uses (with the
same dtc warnings as the ThinkSmart View board), and the scripts pass
`shellcheck`. **None of it has run on a real Smart Display yet.** Every
`TODO(verify)` value comes from the sibling device.

## Sources

- XDA: [Lenovo Smart Display can run Android apps once you unlock the bootloader](https://www.xda-developers.com/lenovo-smart-display-bootloader-unlock-android-apps/) (codenames, USB-C location, unlock overview)
- XDA thread by deadman96385: [(Lenovo Smart Display 8" & 10") (Amber & Blueberry) AVB/Bootloader Unlock, Firmware](https://xdaforums.com/t/lenovo-smart-display-8-10-amber-blueberry-avb-bootloader-unlock-firmware.4472049/) (the unlock tool, firehose and firmware packages)
- [Matthmusic/lineage16-lenovo-smartdisplay](https://github.com/Matthmusic/lineage16-lenovo-smartdisplay): hardware extraction (`extraction/HARDWARE_REPORT.md`), stock kernel config, and an EDL un-brick report for `blueberry`
- [msm8953-mainline/linux](https://github.com/msm8953-mainline/linux): `arch/arm64/boot/dts/qcom/apq8053-lenovo-cd-18781y.dts`, the sibling board this port starts from
- [msm8916-mainline/lk2nd](https://github.com/msm8916-mainline/lk2nd): `lk2nd/device/dts/msm8953/apq8053-lenovo-cd-18781y.dts`
- [pmaports `device-qcom-msm8953`](https://gitlab.postmarketos.org/postmarketOS/pmaports/-/tree/main/device/community/device-qcom-msm8953)
- [bkerler/edl](https://github.com/bkerler/edl)
- [Tao of Mac: The Lenovo ThinkSmart View, Rebooted](https://taoofmac.com/space/blog/2023/04/22/1330) (EDL key combination on the sibling device)

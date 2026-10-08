# Porting postmarketOS to the Smart Display 10

Boot flow once the port is done:

```
PBL → SBL1 → TZ/RPM → stock LK (aboot) ──boot_x──▶ lk2nd ──extlinux.conf──▶ mainline Linux + DTB ──▶ pmOS rootfs (userdata)
```

The generic **`qcom-msm8953`** pmOS device package already ships lk2nd, the
`linux-postmarketos-qcom-msm8953` kernel, firmware packages and an
extlinux-based boot. It includes **all** `qcom/apq8053-*.dtb` files. Once
blueberry has a DTS in the kernel and in lk2nd, it works without a
device-specific pmOS package. lk2nd picks the DTB through
`lk2nd,dtb-files`.

## 1. Facts to collect from the stock firmware

Run `scripts/extract-downstream-dtb.sh backups/<date>/dump/boot_a.bin`. It
unpacks the stock boot image, splits out every appended DTB, decompiles
them, and picks the one whose `qcom,board-id` matches the value the running
device reported (`collect-device-info.sh` saves
`/proc/device-tree/qcom,board-id`). From that DTB, record the following in
[HARDWARE.md](HARDWARE.md):

| What | Where in the downstream DTS | Used for |
|---|---|---|
| msm-id / board-id | `/qcom,msm-id`, `/qcom,board-id` | lk2nd and Linux DTS headers (the extraction says `0x01010020`) |
| Panel name, init/off commands, timings, lanes | `qcom,mdss_dsi_hx83100a_800p_video` | compare with the TSV panel driver, or generate one with [lmdpdg](https://github.com/msm8916-mainline/linux-mdss-dsi-panel-driver-generator) |
| Panel reset / enable / backlight GPIOs, LAB/IBB, WLED | `qcom,platform-reset-gpio`, `qcom,platform-enable-gpio`, the `mdss_dsi` node, `qcom,qpnp-wled` | `&mdss_dsi0`/panel node, `&pmi8950_wled` |
| Touch bus, address, IRQ/reset GPIOs, supplies | `himax` node under an `i2c@78b…` bus | `hx83100a_ts` node |
| Keys | `gpio_keys` | `gpio-keys` node, lk2nd menu keys |
| Wi-Fi SDIO power/reset GPIOs, BT enable GPIO | `sdhc_2`, `qcom,cnss-sdio`, `bt_qca*` | `wlan_pwrseq`, `wlan_3v3`, `&uart_5` |
| Audio amplifier(s), I²S routing, mics | `sound`, `*_mi2s*`, amplifier I²C nodes | `&sound_card` |
| Camera privacy shutter, mic-mute switch | `gpio_keys`, or a vendor `hall`/`switch` node | `SW_CAMERA_LENS_COVER` / mic mute |

The panel compatible in the stock DTB is a good sign. lk2nd reads the
panel name that the stock LK chose and puts it in the DTB's panel
`compatible`. Because blueberry reports `hx83100a 800p video mode dsi panel`,
the existing `lenovo,cd-18781y-hx83100a` panel driver may work unchanged.
Check this by comparing `qcom,mdss-dsi-on-command` byte for byte with
`drivers/gpu/drm/panel/msm8953-generated/panel-lenovo-cd-18781y-hx83100a.c`.

## 2. lk2nd

`dts/lk2nd/apq8053-lenovo-blueberry.dts` is a draft board file. Build it
with:

```bash
scripts/build-lk2nd.sh            # Path A: unlocked bootloader
scripts/build-lk2nd.sh --sign     # Path B: AVB1 test-key signed, written via EDL
```

The script clones lk2nd into `work/lk2nd`, copies the DTS into
`lk2nd/device/dts/msm8953/`, registers it in `rules.mk`, and builds with
`LK2ND_QCDTBS=""`. The stock LK on the sibling device rejects images with
an embedded QCDT, so that variable is required.

The device is ready for the next step when the lk2nd menu shows up
(Vol− moves, Vol+ selects) and `fastboot getvar product` from the host
answers.

## 3. First mainline boot (fast loop)

`dts/linux/apq8053-lenovo-blueberry.dts` is a **minimal bring-up** DTS. It
starts from the ThinkSmart View board file and turns off everything that has
not been verified. On first boot, only eMMC, USB (gadget networking),
display and touch are enabled.

```bash
scripts/kernel-quick-boot.sh --fetch     # clone msm8953-mainline/linux at the pmOS tag, add the DTS
scripts/kernel-quick-boot.sh             # build Image.gz + DTB, mkbootimg, `fastboot boot` through lk2nd
```

For a rootfs, run `scripts/pmos-install.sh --rootfs-only` once. It flashes
a pmOS image to `userdata`. The kernel booted with `fastboot boot` then
mounts that rootfs, and you can reach it with `ssh user@172.16.42.1` over
USB networking.

If nothing shows up on USB: use the serial console (UNLOCK.md §1), add
`earlycon` to the command line (the script already does), and check
`pstore`/ramoops after a reboot into lk2nd.

Until the blueberry DTS is in the packaged `linux-postmarketos-qcom-msm8953`,
picking the normal boot entry in lk2nd will not work for this rootfs:
lk2nd looks for `apq8053-lenovo-blueberry.dtb` in the installed kernel and
won't find it. Use the fast loop above, or §4 with `--kernel-src`.

## 4. Full pmOS install

```bash
scripts/pmos-install.sh --kernel-src work/linux   # build the kernel from your tree, init, install, flash
```

At `pmbootstrap init`, choose vendor **`qcom`**, device **`msm8953`**, and a
UI (`phosh`, `plasma-mobile`, `gnome-mobile`, or `console` for bring-up).
`--kernel-src` runs the following steps, which you can also run by hand:

```bash
pmbootstrap build --force --src=work/linux linux-postmarketos-qcom-msm8953
pmbootstrap install && pmbootstrap flasher flash_rootfs --partition userdata
```

## 5. Bring-up order after first boot

1. **Display and touch.** Confirm the panel driver. Set
   `rotation = <270>` on the panel node if the compositor should see it in
   landscape (or use a udev/hwdb rule plus `WL_OUTPUT_TRANSFORM`).
   Calibrate the touch axes with `touchscreen-inverted-*` /
   `touchscreen-swapped-x-y`.
2. **Wi-Fi.** Enable `&sdhc_2` with the `ath10k` SDIO node. Firmware goes in
   `/lib/firmware/ath10k/QCA9379/hw1.0/` (`firmware-qcom-msm8953` already
   ships the TSV's `board-2.bin`). If the calibration variant differs, take
   the blueberry board data from the `bluetooth`/`modem`/`persist`
   partitions in your backup.
3. **GPU.** Adreno 506 needs a zap shader signed for *this* device. Copy
   `a506_zap.mdt/.b0*` from the stock `vendor`/`firmware` partition, then
   point `&gpu_zap_shader` at `qcom/msm8953/lenovo/blueberry/`.
4. **Bluetooth.** Enable `&uart_5` (`qcom,qca6174-bt` family).
5. **Audio.** Find the speaker amplifier(s) in the downstream DTS, then
   describe the `sound_card` routing and DMICs.
6. **Keys and switches.** Volume, mic mute, camera shutter. Add an hwdb file
   like `61-gmobile-wakeup-lenovo-cd18781y.hwdb` so the volume keys can wake
   the screen (there is no power key).
7. **Camera.** Last. Mainline CAMSS on msm8953 works, but sensor drivers vary.

## 6. Upstreaming

- **msm8953-mainline/linux:** add `arch/arm64/boot/dts/qcom/apq8053-lenovo-blueberry.dts`
  plus a Makefile line. Share common nodes with `cd-18781y` in an
  `apq8053-lenovo-common.dtsi` if the maintainers want that. Then upstream
  through linux-arm-msm.
- **lk2nd:** add the board file and the `rules.mk` entry.
- **pmaports:** add firmware entries to `firmware-qcom-msm8953` (zap shader,
  Wi-Fi board file), an hwdb file in `device-qcom-msm8953`, and a wiki page
  `Lenovo Smart Display 10 (lenovo-blueberry)`.
- Use `blueberry` (or the model number) consistently in compatibles, for
  example `lenovo,blueberry` or `lenovo,sd-x701b`. Agree on the name with the
  msm8953-mainline maintainers before sending patches.

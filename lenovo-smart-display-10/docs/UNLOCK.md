# Unlocking the Smart Display 10 (`blueberry`)

Steps that are confirmed on this device are marked **(blueberry)**. Steps
that come from the sibling ThinkSmart View and have not been checked on
blueberry yet are marked **(TSV, verify)**. If a step behaves differently on
your unit, write down what you saw in [HARDWARE.md](HARDWARE.md).

## 0. Ground rules

- Work from Linux. Qualcomm EDL on Windows needs driver swaps (QDLoader,
  WinUSB, UsbDk), and that is where the published blueberry brick
  happened. `edl` on Linux needs only libusb and a udev rule.
- **Back up first (§3), and do nothing that writes to the device before
  it is done.** That includes `fastboot flashing unlock`, which wipes
  `userdata`.
- Only flash A/B partitions to **one slot** at a time, and keep the other
  slot stock. The published brick came from writing an untested image to
  `boot_b` and then switching the active slot to `b`.
- Never write a `rawprogram*.xml` from a different device (the ThinkSmart
  View's included). Its partition map does not match blueberry.

## 1. Physical access

- **USB-C (blueberry):** under the rubber/fabric cover at the **bottom
  right corner** of the back. It carries data only; keep the barrel power
  adapter connected.
- Use a USB-C ↔ USB-A cable on a USB 2.0 port or hub if EDL enumeration is
  flaky. Qualcomm boot ROMs are often unreliable on some USB 3 controllers.
- **Serial console (TSV, verify):** on the ThinkSmart View, `uart_0` (GPIO4
  RX, GPIO5 TX, 1.8 V) is wired to the USB-C connector, with RX on pins
  A5/B5 and TX on pins A8/B8. If blueberry does the same, a USB-C breakout
  plus a **1.8 V** USB-UART adapter gives you lk2nd and kernel logs, which
  makes Phase 6 much easier. Measure the voltage before connecting
  anything.

## 2. The device's USB modes

| Mode | USB ID | How to get there | What it's for |
|---|---|---|---|
| Android (adb) | `05c6:901d` (diag,adb) on debug builds | normal boot | `collect-device-info.sh`, `adb reboot bootloader` / `adb reboot edl` |
| Fastboot (stock LK) | `18d1:d00d` | `adb reboot bootloader`, or a key combination at power-on | `getvar`, `flashing unlock`, the AVB tool |
| **EDL / 9008** | `05c6:9008` | Vol+ **and** Vol− held while plugging in power **(TSV, verify)**, `adb reboot edl`, or an EDL ("deep flash") cable | full backup, writing any partition, un-brick |
| Diag / 900E | `05c6:900e` | the boot chain is broken | not writable. Force 9008 with the key combination or an EDL cable |

Check which mode the device is in with `lsusb | grep -Ei '05c6|18d1'`.

On EDL timing **(TSV)**: the boot ROM's Sahara window is short. Start the
`edl` command first, then power the device on into EDL, so the loader
uploads right away. A `Sahara` error usually means you missed the window.
Power-cycle and try again.

## 3. Full backup (do this first)

```bash
scripts/setup-host.sh                 # once
scripts/collect-device-info.sh        # while it still boots Android (adb) and/or in fastboot
scripts/edl-backup.sh --loader /path/to/prog_emmc_firehose_8953_ddr.mbn
```

`edl-backup.sh` does the following:

1. runs `edl printgpt` and saves the partition table,
2. runs `edl gpt <dir> --genxml` to get the primary and backup GPT plus `rawprogram0.xml`,
3. runs `edl rl <dir> --genxml` to dump **every** partition (`userdata` too, unless you pass `--skip-userdata`),
4. writes `SHA256SUMS` and reads `boot_a`/`boot_b` a second time to check that the dumps are stable.

**The firehose loader:** `edl` can auto-detect loaders from its loader
collection, but these devices are signed with an OEM/Google key hash.
Use the `prog_emmc_firehose_8953_ddr.mbn` (or similarly named) programmer
from the **blueberry firmware package** in deadman96385's XDA thread, and
pass it with `--loader`. If auto-detection works, you can omit `--loader`.

The partitions that cannot be replaced are `persist`, `modemst1`,
`modemst2`, `fsg`, `fsc`, `devinfo`, `keystore`, `frp`, `misc`,
`sec`/`ssd` (whichever exist), the GPT, and the whole `*_a`/`*_b` set of
your current build. Copy `backups/` to a second disk.

## 4. Unlock

There are two known approaches. Find out which one applies first: the
`fastboot getvar all` output from `collect-device-info.sh` shows
`unlocked`, `secure`, `current-slot` and whether `flashing unlock` is
allowed.

### Path A: bootloader unlock plus AVB bypass (blueberry, deadman96385)

This is the published method for the 8" and 10" Smart Displays (tested on
the 10"):

1. `adb reboot bootloader` (or the fastboot key combination).
2. `fastboot flashing unlock` and confirm. **This wipes userdata.**
3. Run deadman96385's **AVB unlocker** CLI script from that thread while
   the device is in bootloader mode. Plain `fastboot flashing unlock` is
   not enough, because AVB still refuses unsigned images.
4. Optionally flash his **debug firmware** for `adb root`. Note his
   warning: with AVB off, the *user* (retail) firmware cannot reach its
   TEE keys and never finishes setup. That doesn't matter for postmarketOS,
   but it means a retail restore needs AVB back on, so keep the §3 backup.

I could not fetch the thread's attachments from here, so the exact
filenames are not listed. Download the tool and firmware from the XDA thread
and add the filenames and SHA-256 hashes to [HARDWARE.md](HARDWARE.md).

### Path B: stay locked and sign with the AOSP test key (TSV, verify)

The ThinkSmart View's LK **crashes while unlocked**, but it boots **any
image signed with the AOSP AVB1 test (verity) key**, because Android
Things builds use `test-keys`. lk2nd builds such an image with
`SIGN_BOOTIMG=1`, and it is written over EDL rather than fastboot.
Blueberry debug builds are also `test-keys`
(`iot_msm8x53_som-userdebug … test-keys`), so this path might work too.
The catch: blueberry has `vbmeta_a/b` partitions (AVB 2.0), so its LK may
check vbmeta instead of an AVB1 signature.

To test Path B safely, write the signed lk2nd to the **inactive** slot's
`boot_x` over EDL, then switch to that slot with `fastboot --set-active=x`.
A locked bootloader may refuse to switch. In that case, write it to the
**active** slot instead; the backup lets you undo it. If the device doesn't
come up, force EDL and restore that `boot_x` from the backup.
`scripts/flash-lk2nd.sh` handles both cases.

## 5. After unlocking: lk2nd

lk2nd is a second-stage bootloader that is packed as an Android boot image.
It gives you a standard `fastboot`, picks the right mainline DTB, and
boots postmarketOS from `extlinux.conf`. See [PORTING.md](PORTING.md) §2:

```bash
scripts/build-lk2nd.sh               # add --sign for Path B
scripts/flash-lk2nd.sh --slot a --via fastboot   # Path A, unlocked
scripts/flash-lk2nd.sh --slot a --via edl        # Path B, or if fastboot is refused
```

## 6. Un-brick

If you get a black screen, no fastboot and no adb:

1. Run `lsusb`. **9008** means flash right away. **900E** means force
   9008 with Vol+ and Vol− at power-on, or with an EDL cable (shorting
   D+ to GND briefly while plugging in). That is what the published
   blueberry recovery needed.
2. Restore only the partitions you changed: `edl w boot_a backups/<date>/boot_a.bin`.
3. If the GPT is damaged, restore **your own** `gpt_main0.bin` /
   `gpt_backup0.bin` first (`edl w gpt …` or `edl qfil rawprogram0.xml patch0.xml <dir>`),
   then the partitions.
4. Last resort: write the whole set back with `edl wl backups/<date>/dump`.

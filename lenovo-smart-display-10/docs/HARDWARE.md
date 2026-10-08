# Smart Display 10 (`blueberry`): hardware facts

Fill this in from **your own unit** (`collect-device-info.sh`,
`extract-downstream-dtb.sh`). The **Source** column says where each value
came from. "extraction" means
[Matthmusic/lineage16-lenovo-smartdisplay `extraction/HARDWARE_REPORT.md`](https://github.com/Matthmusic/lineage16-lenovo-smartdisplay),
taken from an `SD-X701F`.

## Identity

| Item | Value | Source |
|---|---|---|
| Model on label | | your unit |
| Codename | `blueberry` | XDA |
| Stock build / fingerprint | | `collect-device-info.sh` |
| `ro.product.device` / `ro.boot.hardware` | `msm8x53_som` / `msm8x53` | extraction |
| msm-id | `0x130` (304, APQ8053) | extraction |
| board-id | `0x01010020 0x00000000` | extraction (**confirm** from `/proc/device-tree/qcom,board-id`) |
| Stock DTB model | `Qualcomm Technologies, Inc. APQ8053 Lite DragonBoard V2.1` | extraction |
| Bootloader state (`unlocked`, `secure`) | | `fastboot getvar all` |
| Current slot | | `fastboot getvar current-slot` |

## Peripherals

| Block | Value | Source |
|---|---|---|
| Panel | Himax HX83100A, 800×1280, DSI video mode, rotated 270° by Android, 240 dpi | extraction |
| Panel reset / enable GPIO | | downstream DTB |
| Backlight | WLED, `lcd-backlight`, max 255 | extraction |
| Touch | Himax HX83100A, I²C, bus/addr: | extraction / downstream DTB |
| Touch IRQ / reset GPIO | | downstream DTB |
| Wi-Fi | QCA (CLD driver), `WCNSS_qcom_cfg.ini` | extraction |
| Bluetooth | `qcom.bluetooth.soc=naples_uart` | extraction |
| Sound card | `msm8953-openq624-snd-card` | extraction |
| Speaker amp(s) | | downstream DTB |
| Mics | | downstream DTB |
| Keys / switches | Vol+, Vol−, mic mute, camera shutter | |
| UART on USB-C? | | multimeter / scope |

## Partition map

Paste `backups/<date>/printgpt.txt` here (names and sizes only, no dumps).

## Unlock tools used

| File | SHA-256 | From |
|---|---|---|
| firehose loader | | XDA blueberry firmware package |
| AVB unlocker | | XDA thread |
| debug firmware | | XDA thread |

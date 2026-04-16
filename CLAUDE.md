# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a custom Yocto layer (`meta-access-control`) for an access control system built on the Toradex Verdin i.MX8M Plus module. It extends the Toradex BSP without modifying upstream sources, integrating the following hardware peripherals:

- OV5640 camera (MIPI CSI, I2C3 @ 0x3c)
- WM8904 audio codec (SAI1, I2C4 @ 0x1a)
- ATECC608A cryptographic coprocessor (I2C4 @ 0x60)
- VL53L5CX ToF sensor (I2C4 @ 0x29, out-of-tree kernel module)
- OPT3001 light sensor (I2C4 @ 0x44)
- HD3SS3220 USB Type-C controller (I2C4 @ 0x47)
- SR602 PIR motion detector (GPIO1_IO11)
- RCWL1670 ultrasonic sensor (UART2)
- PN532 NFC reader (UART1, userspace via libnfc)
- WS2812 addressable LEDs (GPIO1_IO08)
- Lock relay control (GPIO4_IO07)
- Tamper switch (GPIO4_IO15)
- Panic input/output signals (GPIO4_IO13, GPIO4_IO14)

Target machine: `verdin-imx8mp`, DISTRO: `tdx-xwayland`, layer compatibility: `scarthgap`.

## Layer Structure

```
meta-access-control/
├── conf/layer.conf                              # Layer priority 6, overlay definitions
├── recipes-bsp/device-tree/
│   ├── device-tree-overlays_%.bbappend          # Integrates overlays into build
│   └── device-tree-overlays/
│       ├── verdin-imx8mp_ov5640_overlay.dts     # Camera (MIPI CSI + I2C3 + GPIO4_IO08/09)
│       ├── verdin-imx8mp_wm8904_overlay.dts     # Audio codec (SAI1 + I2C4)
│       ├── verdin-imx8mp_atecc608a_overlay.dts   # Crypto chip (I2C4)
│       ├── verdin-imx8mp_vl53l5x_overlay.dts     # ToF sensor (I2C4)
│       ├── verdin-imx8mp_opt3001_overlay.dts      # Light sensor (I2C4)
│       ├── verdin-imx8mp_hd3ss3220_overlay.dts    # USB-C controller (I2C4 + USB3)
│       ├── verdin-imx8mp_sr602_overlay.dts        # PIR motion (GPIO1_IO11)
│       ├── verdin-imx8mp_rcwl1670_overlay.dts     # Ultrasonic (UART2)
│       ├── verdin-imx8mp_pn532_overlay.dts        # NFC reader (UART1)
│       ├── verdin-imx8mp_ws2812_overlay.dts       # LED strip (GPIO1_IO08)
│       ├── verdin-imx8mp_lock_ctrl_overlay.dts    # Lock relay (GPIO4_IO07)
│       ├── verdin-imx8mp_tamper_switch_overlay.dts # Tamper detect (GPIO4_IO15)
│       └── verdin-imx8mp_panic_overlay.dts        # Panic I/O (GPIO4_IO13/14)
├── recipes-kernel/
│   ├── linux/
│   │   ├── linux-toradex_%.bbappend             # Adds kernel config fragments
│   │   └── linux-toradex/
│   │       ├── ov5640.cfg                       # Camera + ISI + MIPI CSI
│   │       ├── wm8904.cfg                       # Audio codec
│   │       ├── opt3001.cfg                      # Light sensor
│   │       └── pn532.cfg                        # Disables kernel NFC (userspace libnfc used instead)
│   └── stmvl53l5cx/
│       ├── stmvl53l5cx_1.0.bb                   # Out-of-tree kernel module (GPL-2.0)
│       └── files/kernel/                        # Module sources (I2C + ioctl + interrupt)
├── recipes-apps/
│   ├── atecc-test/                              # ATECC608A test app (I2C4, cryptoauthlib)
│   ├── stmvl53l5cx-user/                        # VL53L5CX test app (ULD driver + examples)
│   ├── rcwl1670-user/                           # RCWL1670 UART test (9600 baud, /dev/ttymxc1)
│   ├── ws2812-user/                             # WS2812 LED control (libgpiod bit-bang)
│   └── usb-c-test/                              # HD3SS3220 status monitor (I2C)
├── recipes-images/images/
│   └── access-control.bb                        # Root filesystem image
├── recipes-support/
│   ├── cryptoauthlib/cryptoauthlib_git.bb       # Microchip CryptoAuthLib v3.3.3 (CMake)
│   └── libnfc/
│       ├── libnfc_%.bbappend                    # UART transport config
│       └── libnfc/libnfc.conf
├── DEVELOPMENT_GUIDE.md
├── README
└── COPYING.MIT
```

## Building

This layer is designed to be used within the Toradex BSP build system.

### Prerequisites

Clone the Toradex BSP and place this layer at `layers/meta-access-control/`. Ensure it is listed in `build/conf/bblayers.conf`.

### Setup and Build

```bash
cd toradex-bsp
source export
bitbake access-control          # Full image
bitbake <recipe-name>           # Individual recipe (e.g., bitbake stmvl53l5cx)
```

### Clean and Rebuild

```bash
bitbake -c clean <recipe>       # Clean build artifacts
bitbake -c cleansstate <recipe> # Remove shared state (forces full rebuild)
```

### Kernel Rebuild (after config or overlay changes)

```bash
bitbake -c cleansstate virtual/kernel
bitbake virtual/kernel
```

### Inspection

```bash
bitbake-layers show-layers
bitbake-layers show-recipes
bitbake -e <recipe> | grep <VARIABLE>
```

## Hardware Pin Mapping

### I2C Buses
| Bus | Controller | Devices |
|-----|-----------|---------|
| `/dev/i2c-2` | I2C3 (SODIMM 93/95) | OV5640 @ 0x3c |
| `/dev/i2c-3` | I2C4 (SODIMM 89/91) | WM8904 @ 0x1a, VL53L5CX @ 0x29, OPT3001 @ 0x44, HD3SS3220 @ 0x47, ATECC608A @ 0x60 |

### UARTs
| Device | Controller | Config |
|--------|-----------|--------|
| `/dev/ttymxc0` | UART1 | PN532 NFC (2-wire, no flow control) |
| `/dev/ttymxc1` | UART2 | RCWL1670 ultrasonic (9600 baud, 2-wire) |

### GPIOs
| Pin | SODIMM | Function | Type |
|-----|--------|----------|------|
| GPIO1_IO08 | 218 | WS2812 LED data | Output (gpio-leds) |
| GPIO1_IO11 | 16 | SR602 motion detect | Input (gpio-keys, wakeup) |
| GPIO4_IO07 | 211 | Lock relay control | Output (gpio-leds, default ON) |
| GPIO4_IO08 | 205 | OV5640 PWDN | Output (active high) |
| GPIO4_IO09 | 207 | OV5640 RESET | Output (active low) |
| GPIO4_IO13 | 219 | Panic input | Input (gpio-keys, KEY_RESTART) |
| GPIO4_IO14 | 217 | Panic output | Output (gpio-leds) |
| GPIO4_IO15 | 215 | Tamper switch | Input (gpio-keys, wakeup) |

### Pin Conflicts (handled via overlay disables)
- OV5640 + Tamper switch: both disable FEC (Ethernet 2) for GPIO4 pins
- SR602: disables PWM2 and backlight_mezzanine for GPIO1_IO11

## Image Contents (access-control.bb)

Base: `tdx-reference-minimal-image` with OpenSSH (replaces Dropbear).

Packages: `v4l-utils`, `media-ctl`, `fbgrab`, GStreamer stack (`base`, `good`, `bad`, `bad-kms`), `imx-gpu-viv-tools`, `imx-gst1.0-plugin`, `i2c-tools`, `libgpiod`, `libgpiod-tools`, `alsa-utils`, `alsa-plugins`, `libnfc`, `cryptoauthlib`, `stmvl53l5cx`, `stmvl53l5cx-user`, `atecc-test`, `usb-c-test`, `rcwl1670-user`, `ws2812-user`.

## Important Notes

- Layer priority 6 ensures overrides take effect over upstream layers.
- `ACCEPT_FSL_EULA = "1"` must be set in `local.conf` for NXP proprietary components.
- Device tree overlays are modular — each peripheral can be enabled/disabled independently.
- `pn532.cfg` explicitly disables kernel NFC stack because PN532 is driven from userspace via libnfc over UART.
- I2C4 carries 5 devices — if reliability issues occur, check pull-up values and bus capacitance.
- WS2812 uses GPIO bit-banging from userspace (timing-sensitive on non-RT kernel).

## References

- DEVELOPMENT_GUIDE.md — Detailed hardware mapping, device tree theory, verification procedures
- [Yocto Project](https://www.yoctoproject.org/)
- [BitBake Manual](https://docs.yoctoproject.org/bitbake/)
- [Toradex Developer Center](https://developer.toradex.com/)
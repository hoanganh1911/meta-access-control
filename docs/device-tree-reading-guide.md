# Device Tree reading guide (Toradex Verdin i.MX8MP + meta-access-control)

This document is a practical guide to **read** and **author** Linux Device Tree overlays in this Yocto layer, using the layer’s real overlay sources as examples.

Target platform (examples in this doc): **Toradex Verdin iMX8M Plus on Verdin Development Board**.

Relevant overlay sources live in:
- [`toradex-bsp/layers/meta-access-control/recipes-bsp/device-tree/device-tree-overlays/`](../recipes-bsp/device-tree/device-tree-overlays/)


## 1) What you are looking at when you open a `.dts` overlay

A Device Tree **overlay** is a small DTS that is merged on top of a base DT (the board `.dtb`) at boot time (or runtime via configfs). In this layer, overlays typically start like:

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "toradex,verdin-imx8mp";
    /* fragments... */
};
```

Example: [`verdin-imx8mp_sr602_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts#L1)

Key ideas:
- `/plugin/;` tells `dtc` this file is an overlay (produces `.dtbo`).
- The overlay usually has one or more `fragment@N` blocks.
- Each fragment selects a target node and applies changes inside `__overlay__ { ... }`.


## 2) Fragment targeting: `target = <&label>` vs `target-path = "/..."`

Overlays can attach changes in two common ways:

### 2.1 `target = <&label>` (phandle label)

This targets a node that has a label somewhere in the **base DTS/DTSI** include tree.

Example from [`verdin-imx8mp_sr602_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts#L12): (Note: `fragment@0` starts here, targets `&iomuxc`)

```dts
fragment@0 {
    target = <&iomuxc>;
    __overlay__ {
        pinctrl_sr602: sr602grp {
            fsl,pins = <
                MX8MP_IOMUXC_GPIO1_IO11__GPIO1_IO11 0x1c4
            >;
        };
    };
};
```

Meaning:
- The base tree must define a node labelled `iomuxc`.
- The overlay adds a new pinctrl group (`pinctrl_sr602`) under that node.

### 2.2 `target-path = "/"` (absolute path)

This targets by path instead of label.

Example from [`verdin-imx8mp_ws2812_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_ws2812_overlay.dts#L22):

```dts
fragment@1 {
    target-path = "/";
    __overlay__ {
        ws2812 {
            compatible = "custom,ws2812-bitbang";
            /* ... */
        };
    };
};
```

Meaning:
- This adds a brand new node at the root `/`.
- This does *not* rely on a label existing in the base DTS.


## 3) Common overlay operations you’ll see in this layer

### 3.1 Add a new node at `/` (new device)

- Create a node under `target-path = "/"`.
- Usually include `compatible = "...";` and some properties.

Examples:
- SR602 module node in [`verdin-imx8mp_sr602_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts#L23)
- USB VBUS regulator node in [`verdin-imx8mp_hd3ss3220_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_hd3ss3220_overlay.dts#L11)


### 3.2 Enable a bus and add I2C devices under it

Typical pattern:

```dts
&i2c4 {
    status = "okay";

    device@XX {
        compatible = "vendor,part";
        reg = <0xXX>;
    };
};
```

Examples:
- ATECC608A on I2C4: [`verdin-imx8mp_atecc608a_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_atecc608a_overlay.dts#L8)
- OPT3001 on I2C4: [`verdin-imx8mp_opt3001_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_opt3001_overlay.dts#L8)
- VL53L5CX on I2C4: [`verdin-imx8mp_vl53l5x_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_vl53l5x_overlay.dts#L8)


### 3.3 Add pinmux / pinctrl groups under `&iomuxc`

On i.MX8MP, pinmux groups are described in `&iomuxc` using `fsl,pins = < ... >;` entries.

Examples:
- LED strip GPIO pinmux: [`verdin-imx8mp_ws2812_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_ws2812_overlay.dts#L10)
- OV5640 reset/powerdown GPIOs + MCLK: [`verdin-imx8mp_ov5640_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_ov5640_overlay.dts#L102)


### 3.4 Override properties and remove things (`/delete-property/`, `/delete-node/`)

Overlays can also **remove** or **replace** existing parts of the base tree.

Examples:
- Remove base `pinctrl-0` and replace it with a new list:
  - [`verdin-imx8mp_wm8904_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_wm8904_overlay.dts#L88)
- Remove a base `port` block and rebuild endpoints:
  - [`verdin-imx8mp_ov5640_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_ov5640_overlay.dts#L34)

Notes:
- Use deletions sparingly; they are powerful but can break assumptions other DT parts make.
- Prefer targeted changes (override only what you must).


### 3.5 Resolve pin/resource conflicts by disabling base nodes

This layer uses overlays to disable default consumers when pins clash.

Example: SR602 uses the same pin as PWM_2, so it disables both PWM_2 and the mezzanine backlight:

- [`verdin-imx8mp_sr602_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts#L35)

```dts
fragment@2 { target = <&pwm2>; __overlay__ { status = "disabled"; }; };
fragment@3 { target = <&backlight_mezzanine>; __overlay__ { status = "disabled"; }; };
```


## 4) How to trace the base DTS/DTSI that your overlay applies to

When an overlay references `&i2c4`, `&fec`, `&pwm2`, `&usb3_1`, `&iomuxc`, etc., those labels are defined in the **base board DT include chain**.

For the Verdin i.MX8MP dev board in this build tree, the entry point is:
- [`toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-nonwifi-dev.dts`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-nonwifi-dev.dts#L1)

It includes:

```dts
#include "imx8mp-verdin.dtsi"
#include "imx8mp-verdin-nonwifi.dtsi"
#include "imx8mp-verdin-dev.dtsi"
```

(Links: [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L1), [`imx8mp-verdin-nonwifi.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-nonwifi.dtsi#L1), [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi#L1))

So, when you want to find where `&i2c4` or `backlight_mezzanine` comes from:

1) Start at the board `.dts` entry point.
2) Follow the `#include` chain into the `.dtsi` files.
3) Search those files for:
   - `&i2c4 { ... }` (node modifications by label)
   - `label: node-name { ... }` (label definitions)

Concrete examples (definitions in the base include tree):
- `backlight_mezzanine: backlight-mezzanine { ... pwms = <&pwm2 ...>; }` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L37)
- `&i2c4 { ... }` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L693)
- `&fec { ... }` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L311)
- `&usb3_1 { ... }` and `&usb_dwc3_1 { ... }` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L887)
- Development-board enablement (status flips, extra children) in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi#L83)

Important subtlety:
- Some labels used by overlays are defined in SoC-level DTSI (e.g. `mipi_csi_0` is defined in `imx8mp.dtsi`). See [`imx8mp.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp.dtsi#L1690).


## 5) How to write overlays that override/extend the base DTS

### 5.1 Rule of thumb: extend where possible, override where necessary

- **Extend**: add new child nodes/properties without removing base content.
  - Example: adding `opt3001@44` under `&i2c4` in [`verdin-imx8mp_opt3001_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_opt3001_overlay.dts#L8)

- **Override**: redefine properties of an existing node.
  - Example: change `&usb_dwc3_1` from host to OTG in [`verdin-imx8mp_hd3ss3220_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_hd3ss3220_overlay.dts#L71)

- **Replace/remove**: use `/delete-property/` or `/delete-node/` when the base definition is incompatible.
  - Example: camera endpoint rebuild in [`verdin-imx8mp_ov5640_overlay.dts`](../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_ov5640_overlay.dts#L34)


### 5.2 Practical workflow: from “what overlay changes” to “where base defines it”

For each overlay fragment:

1) Identify the target label/path.
   - Example: `target = <&i2c4>`.
2) Locate that label in the base DTS/DTSI include chain.
   - Example: `&i2c4 { ... }` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L693)
3) Compare:
   - base node properties (often default `status = "disabled"`)
   - dev-board overrides (often `status = "okay"` + extra children)
   - overlay modifications (add/override/delete)

This is the fastest way to build confidence that your overlay is safe and minimal.


## 6) Mapping table: overlay → base DTS → impacted nodes

Base board DT include chain (this build):
- entry: [`imx8mp-verdin-nonwifi-dev.dts`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-nonwifi-dev.dts#L1)
- includes: [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi#L1), `imx8mp-verdin-nonwifi.dtsi`, [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi#L1)
- SoC: [`imx8mp.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp.dtsi#L1)

| Overlay | Overlay targets / impacted nodes | Where those nodes are defined (base) |
|---|---|---|
| `verdin-imx8mp_sr602_overlay.dts` | `&iomuxc` add `pinctrl_sr602`; `/` add `sr602`; disable `&pwm2`; disable `&backlight_mezzanine` | `&iomuxc`, `&pwm2`, `backlight_mezzanine` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:37) and [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950); PWM2 enabled on dev board in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi:159) |
| `verdin-imx8mp_ws2812_overlay.dts` | `&iomuxc` add `pinctrl_led_strip`; `/` add `ws2812` | `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950) |
| `verdin-imx8mp_atecc608a_overlay.dts` | `&i2c4` enable + add `atecc608a@60` | `&i2c4` defined in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:693); enabled on dev board in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi:131) |
| `verdin-imx8mp_opt3001_overlay.dts` | `&i2c4` enable + add `opt3001@44` | `&i2c4` defined in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:693); enabled on dev board in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi:131) |
| `verdin-imx8mp_vl53l5x_overlay.dts` | `&i2c4` enable + add `vl53l5x@29` | `&i2c4` defined in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:693); enabled on dev board in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi:131) |
| `verdin-imx8mp_wm8904_overlay.dts` | `/` add `reg_wm8904_pa_en`, add `sound_wm8904`; `&i2c4` add `wm8904@1a`; `&sai1` clock assignments + enable; `&iomuxc` delete+replace `pinctrl-0` | `&i2c4` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:693); `&sai1` defined (SoC) in [`imx8mp.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp.dtsi:1748); dev board also enables `&sai1` in [`imx8mp-verdin-dev.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin-dev.dtsi:173); `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950) |
| `verdin-imx8mp_ov5640_overlay.dts` | `/` add 3 fixed regulators; `&mipi_csi_0` enable + delete/rebuild `port`; `&i2c3` enable + add `ov5640@3c` + endpoints; enable media nodes; `&iomuxc` delete/replace `pinctrl-0`, delete `gpiohog3grp`, add `pinctrl_ov5640_*`; disable `&fec` | `&i2c3` defined in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:682); `mipi_csi_0` defined in [`imx8mp.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp.dtsi:1690); `&fec` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:311); `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950) |
| `verdin-imx8mp_hd3ss3220_overlay.dts` | `/` add `reg_usb2_vbus`; `&i2c4` add `typec@47` + `connector` endpoints; `&usb3_1` enable + add `usb-role-switch` + `port` endpoint; `&usb_dwc3_1` set `dr_mode = "otg"` + `usb-role-switch` + `vbus-supply` | `&i2c4` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:693); `&usb3_1`/`&usb_dwc3_1` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:887) |
| `verdin-imx8mp_rcwl1670_overlay.dts` | `&uart2` enable + delete `uart-has-rtscts` | `&uart2` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:838) |
| `verdin-imx8mp_pn532_overlay.dts` | `&iomuxc` add `pinctrl_uart1_pn532`; `&uart1` enable + delete `uart-has-rtscts` | `&uart1` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:831) |
| `verdin-imx8mp_lock_ctrl_overlay.dts` | `&iomuxc` add `pinctrl_lock_ctrl`; `/` add `lock_ctrl` | `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950) |
| `verdin-imx8mp_tamper_switch_overlay.dts` | `&iomuxc` add `pinctrl_tamper_switch`; `/` add `tamper_switch`; disable `&fec` | `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950); `&fec` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:311) |
| `verdin-imx8mp_panic_overlay.dts` | `&iomuxc` add `pinctrl_panic`; `/` add `panic_signals` and `panic_output` | `&iomuxc` in [`imx8mp-verdin.dtsi`](../../../build/tmp/work-shared/verdin-imx8mp/kernel-source/arch/arm64/boot/dts/freescale/imx8mp-verdin.dtsi:950) |



## References in this layer

- Layer overview and notes (pin conflicts, interfaces): [`toradex-bsp/layers/meta-access-control/CLAUDE.md`](../CLAUDE.md#L1)
- Build/test workflows for overlays and modules: [`toradex-bsp/layers/meta-access-control/plans/test-dtbo-ko-workflow.md`](./test-dtbo-ko-workflow.md#L1)

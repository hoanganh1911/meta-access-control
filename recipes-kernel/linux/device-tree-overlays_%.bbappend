FILESEXTRAPATHS:prepend := "${THISDIR}/device-tree-overlays:"

CUSTOM_OVERLAYS_SOURCE = " \
    verdin-imx8mp_ov5640_overlay.dts \
    verdin-imx8mp_wm8904_overlay.dts \
    verdin-imx8mp_rcwl1670_overlay.dts \
    verdin-imx8mp_opt3001_overlay.dts \
    verdin-imx8mp_vl53l5x_overlay.dts \
    verdin-imx8mp_sr602_overlay.dts \
    verdin-imx8mp_tamper_switch_overlay.dts \
    verdin-imx8mp_pn532_overlay.dts \
    verdin-imx8mp_lock_ctrl_overlay.dts \
    verdin-imx8mp_ws2812_overlay.dts \
    verdin-imx8mp_panic_overlay.dts \
    verdin-imx8mp_atecc608a_overlay.dts \
    verdin-imx8mp_hd3ss3220_overlay.dts \
"
CUSTOM_OVERLAYS_BINARY = " \
    verdin-imx8mp_ov5640_overlay.dtbo \
    verdin-imx8mp_wm8904_overlay.dtbo \
    verdin-imx8mp_rcwl1670_overlay.dtbo \
    verdin-imx8mp_opt3001_overlay.dtbo \
    verdin-imx8mp_vl53l5x_overlay.dtbo \
    verdin-imx8mp_sr602_overlay.dtbo \
    verdin-imx8mp_tamper_switch_overlay.dtbo \
    verdin-imx8mp_pn532_overlay.dtbo \
    verdin-imx8mp_lock_ctrl_overlay.dtbo \
    verdin-imx8mp_ws2812_overlay.dtbo \
    verdin-imx8mp_panic_overlay.dtbo \
    verdin-imx8mp_atecc608a_overlay.dtbo \
    verdin-imx8mp_hd3ss3220_overlay.dtbo \
"
SRC_URI += "\
    file://verdin-imx8mp_ov5640_overlay.dts \
    file://verdin-imx8mp_wm8904_overlay.dts \
    file://verdin-imx8mp_rcwl1670_overlay.dts \
    file://verdin-imx8mp_opt3001_overlay.dts \
    file://verdin-imx8mp_vl53l5x_overlay.dts \
    file://verdin-imx8mp_sr602_overlay.dts \
    file://verdin-imx8mp_tamper_switch_overlay.dts \
    file://verdin-imx8mp_pn532_overlay.dts \
    file://verdin-imx8mp_lock_ctrl_overlay.dts \
    file://verdin-imx8mp_ws2812_overlay.dts \
    file://verdin-imx8mp_panic_overlay.dts \
    file://verdin-imx8mp_atecc608a_overlay.dts \
    file://verdin-imx8mp_hd3ss3220_overlay.dts \
"

TEZI_EXTERNAL_KERNEL_DEVICETREE += " \
    ${CUSTOM_OVERLAYS_BINARY} \
"

TEZI_EXTERNAL_KERNEL_DEVICETREE_BOOT = " \
    ${CUSTOM_OVERLAYS_BINARY} \
"

do_collect_overlays:prepend() {
    for DTS in ${CUSTOM_OVERLAYS_SOURCE}; do
        cp ${WORKDIR}/${DTS} ${S}
    done
}

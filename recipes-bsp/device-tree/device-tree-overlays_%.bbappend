FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "\
    file://verdin-imx8mp_pinmux_overlay.dts \
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

do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_pinmux_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_ov5640_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_wm8904_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_rcwl1670_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_opt3001_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_vl53l5x_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_sr602_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_tamper_switch_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_pn532_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_lock_ctrl_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_ws2812_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_panic_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_atecc608a_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_hd3ss3220_overlay.dts ${S}/
}

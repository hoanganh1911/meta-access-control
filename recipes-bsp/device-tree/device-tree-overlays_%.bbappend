FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "\
    file://verdin-imx8mp_ov5640_overlay.dts \
    file://verdin-imx8mp_wm8904_overlay.dts \
    file://verdin-imx8mp_rcwl1670_overlay.dts \
    file://verdin-imx8mp_opt3001_overlay.dts \
    file://verdin-imx8mp_vl53l5x_overlay.dts \
    file://verdin-imx8mp_sr602_overlay.dts \
    file://verdin-imx8mp_tamper_switch_overlay.dts \
"

do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_ov5640_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_wm8904_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_rcwl1670_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_opt3001_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_vl53l5x_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_sr602_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_tamper_switch_overlay.dts ${S}/
}

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "\
    file://verdin-imx8mp_ov5640_overlay.dts \
    file://verdin-imx8mp_wm8904_overlay.dts \
"

do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_ov5640_overlay.dts ${S}/
    cp ${WORKDIR}/verdin-imx8mp_wm8904_overlay.dts ${S}/
}

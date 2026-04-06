FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "\
    file://verdin-imx8mp_ov5640_overlay.dts \
"

do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_ov5640_overlay.dts ${S}/
}

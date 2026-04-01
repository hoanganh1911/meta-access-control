FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://verdin-imx8mp_ov5648_overlay.dts"



do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_ov5648_overlay.dts ${S}/
}

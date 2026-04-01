FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://verdin-imx8mp_ov5648_overlay.dts"

TEZI_EXTERNAL_KERNEL_DEVICETREE += "verdin-imx8mp_ov5648_overlay.dtbo"

do_collect_overlays:prepend() {
    cp ${WORKDIR}/verdin-imx8mp_ov5648_overlay.dts ${S}/
}

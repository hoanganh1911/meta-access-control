FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += " \
    file://ov5648.cfg \
    file://0001-media-imx8-fix-sensor-csi-link-setup-enoioctlcmd.patch \
    file://0001-media-i2c-ov5648-Add-debug-poweron-off.patch \
"

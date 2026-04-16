SUMMARY = "WS2812 Kernel Module bit-bang driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module

SRC_URI = " \
    file://tdx_ws2812.c \
    file://Makefile \
"

S = "${WORKDIR}"

# The inherit of module.bbclass will take care of most things
KERNEL_MODULE_AUTOLOAD += "tdx_ws2812"
FILES_${PN} += "${sysconfdir}/modules-load.d"


SUMMARY = "SR602 PIR Sensor test kernel module"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module

SRC_URI = " \
    file://sr602_mod.c \
    file://Makefile \
"

S = "${WORKDIR}"

KERNEL_MODULE_AUTOLOAD += "sr602_mod"
FILES_${PN} += "${sysconfdir}/modules-load.d"

SUMMARY = "Tự động cấu hình WiFi ConnMan cho dự án Access Control"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://wifi.config"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${localstatedir}/lib/connman
    install -m 0600 ${WORKDIR}/wifi.config ${D}${localstatedir}/lib/connman/
}

FILES:${PN} = "${localstatedir}/lib/connman/wifi.config"

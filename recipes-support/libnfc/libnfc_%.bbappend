FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://libnfc.conf"

PACKAGECONFIG:append = " uart"

do_install:append() {
    install -d ${D}${sysconfdir}/nfc
    install -m 0644 ${WORKDIR}/libnfc.conf ${D}${sysconfdir}/nfc/
}

SUMMARY = "User-space test application for ATECC608A via I2C"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "cryptoauthlib"

SRC_URI = " \
    file://user \
"

S = "${WORKDIR}/user/src"

EXTRA_OEMAKE = "CFLAGS='${CFLAGS} -I${STAGING_INCDIR}/cryptoauthlib'"

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 atecc-test ${D}${bindir}/atecc-test
}

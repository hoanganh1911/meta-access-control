SUMMARY = "Microchip CryptoAuthentication Library"
DESCRIPTION = "Library for interacting with Microchip CryptoAuthentication devices"
HOMEPAGE = "https://github.com/MicrochipTech/cryptoauthlib"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://license.txt;md5=2b56c229b6ececc3a14d533a1d70029b"

SRC_URI = "git://github.com/MicrochipTech/cryptoauthlib.git;protocol=https;nobranch=1"
SRCREV = "v3.3.3"
PV = "3.3.3"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DATCA_HAL_I2C=ON \
    -DATCA_PKCS11=OFF \
    -DATCA_TEDS=OFF \
    -DATCA_WIFI=OFF \
"

# The library produces libcryptoauth.so (unversioned)
do_install:append() {
    # Ensure headers are installed
    install -d ${D}${includedir}/cryptoauthlib
    cp -r ${S}/lib/*.h ${D}${includedir}/cryptoauthlib/
    cp -r ${S}/lib/atcacert/*.h ${D}${includedir}/cryptoauthlib/
    cp -r ${S}/lib/hal/*.h ${D}${includedir}/cryptoauthlib/
    install -m 0644 ${B}/lib/atca_config.h ${D}${includedir}/cryptoauthlib/
}

# Fix for "contains non-symlink .so" and SPDX issue
FILES:${PN} += "${libdir}/libcryptoauth.so"
FILES_SOLIBSDEV = ""
INSANE_SKIP:${PN} += "dev-so"

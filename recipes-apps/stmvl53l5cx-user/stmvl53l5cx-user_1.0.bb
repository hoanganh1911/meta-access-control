SUMMARY = "User-space test application for VL53L5CX"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://menu.c;beginline=1;endline=30;md5=fe089395f6b90bf1e62dbd7b57febc62"

SRC_URI = " \
    file://user \
"

S = "${WORKDIR}/user/test"

# The Makefile is in the test directory, and it references sources in ../platform, ../uld-driver, etc.
# Since we copied the whole user dir into ${WORKDIR}/user, relative paths should work.

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 menu ${D}${bindir}/vl53l5cx-test
}

FILESEXTRAPATHS:prepend := "${THISDIR}/linux-toradex:"

# Prevent the use of in-tree defconfig
unset KBUILD_DEFCONFIG

SRC_URI += " \
    file://defconfig \
    file://0001-typec-hd3ss3220-add-logging.patch \
"
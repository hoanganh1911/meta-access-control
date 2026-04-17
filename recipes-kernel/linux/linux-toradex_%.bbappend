FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += " \
    file://ov5640.cfg \
    file://wm8904.cfg \
    file://opt3001.cfg \
    file://pn532.cfg \
    file://hd3ss3220.cfg \
    file://0001-typec-hd3ss3220-add-logging.patch \
"
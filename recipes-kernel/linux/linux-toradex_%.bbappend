FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += " \
    file://ov5640.cfg \
    file://wm8904.cfg \
    file://opt3001.cfg \
    file://tof.cfg \
"
SUMMARY = "STMicroelectronics VL53L5CX Time-of-Flight sensor driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://stmvl53l5cx_module.c;beginline=1;endline=30;md5=98b4b739ad0821468cf4f4fbeb3876f4"

inherit module

SRC_URI = " \
    file://kernel/Makefile \
    file://kernel/stmvl53l5cx_module.c \
    file://kernel/stmvl53l5cx_i2c.c \
    file://kernel/stmvl53l5cx_i2c.h \
"

S = "${WORKDIR}/kernel"

# The inherit of module.bbclass will automatically do module_do_compile and module_do_install.
# We just need to ensure the Makefile is correct.

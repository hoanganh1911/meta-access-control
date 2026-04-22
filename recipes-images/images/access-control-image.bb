SUMMARY = "Custom Image for Access Control Project"
DESCRIPTION = "Full system image and environment for Access Control with WiFi, Ethernet, UART and SSH support"
LICENSE = "MIT"

inherit core-image

export IMAGE_BASENAME = "access-control-image"
MACHINE_NAME ?= "${MACHINE}"
IMAGE_NAME = "${MACHINE_NAME}_${IMAGE_BASENAME}"

IMAGE_LINGUAS = "en-us"

SYSTEMD_DEFAULT_TARGET = "graphical.target"

ROOTFS_PKGMANAGE_PKGS ?= '${@oe.utils.conditional("ONLINE_PACKAGE_MANAGEMENT", "none", "", "${ROOTFS_PKGMANAGE}", d)}'

IMAGE_FEATURES += "ssh-server-openssh debug-tweaks"

# List of packages to add to the system
IMAGE_INSTALL:append = " \
    packagegroup-boot \
    udev-extra-rules \
    ${ROOTFS_PKGMANAGE_PKGS} \
    v4l-utils \
    imx-gpu-viv-tools \
    imx-gst1.0-plugin \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-bad-kms \
    gstreamer1.0 \
    media-ctl \
    fbgrab \
    i2c-tools \
    alsa-utils \
    alsa-plugins \
    stmvl53l5cx \
    stmvl53l5cx-user \
    libnfc \
    libgpiod \
    libgpiod-tools \
    ws2812-mod \
    cryptoauthlib \
    atecc-test \
    usb-c-test \
    rcwl1670-user \
    sr602-mod \
    evtest \
    systemd-conf \
    canutils \
    iproute2 \
"
IMAGE_DEV_MANAGER   = "udev"
IMAGE_INIT_MANAGER  = "systemd"
IMAGE_INITSCRIPTS   = " "
IMAGE_LOGIN_MANAGER = "busybox shadow"

require recipes-images/images/tdx-reference-minimal-image.bb

SUMMARY = "Custom Image for Access Control Project"
DESCRIPTION = "Full system image and environment for Access Control with WiFi, Ethernet, UART and SSH support"

IMAGE_BASENAME = "access-control"
IMAGE_NAME = "${MACHINE}_${IMAGE_BASENAME}"

# List of packages to add to the system
IMAGE_INSTALL:append = " \
    packagegroup-base-wifi \
    packagegroup-wifi-tdx-cli \
    packagegroup-wifi-fw-tdx-cli \
    connman \
    connman-client \
    connman-plugin-ethernet \
    connman-plugin-wifi \
    openssh \
    openssh-sftp-server \
    v4l-utils \
    gstreamer1.0 \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    connman-wifi-config \
"

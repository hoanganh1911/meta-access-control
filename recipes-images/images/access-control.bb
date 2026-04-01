require recipes-images/images/tdx-reference-multimedia-image.bb

SUMMARY = "Custom Image for Access Control Project"
DESCRIPTION = "Full system image and environment for Access Control with WiFi, Ethernet, UART and SSH support"

# Rename the output tarball file
export IMAGE_BASENAME = "access-control"

# List of packages to add to the system
IMAGE_INSTALL:append = " \
    packagegroup-base-wifi \
    packagegroup-base-ethernet \
    packagegroup-wifi-tdx-cli \
    packagegroup-wifi-fw-tdx-cli \
    openssh \
    openssh-sftp-server \
    v4l-utils \
    gstreamer1.0 \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    connman-wifi-config \
"

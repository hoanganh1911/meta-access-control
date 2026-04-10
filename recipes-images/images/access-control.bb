require recipes-images/images/tdx-reference-minimal-image.bb

SUMMARY = "Custom Image for Access Control Project"
DESCRIPTION = "Full system image and environment for Access Control with WiFi, Ethernet, UART and SSH support"

IMAGE_BASENAME = "access-control"
IMAGE_NAME = "${MACHINE}_${IMAGE_BASENAME}"

# Use OpenSSH instead of Dropbear (resolves conflicts)
IMAGE_FEATURES += "ssh-server-openssh"

# Remove packagegroup-basic because it has a hard dependency on dropbear 
# which conflicts with openssh. We will let IMAGE_FEATURES handle SSH.
IMAGE_INSTALL:remove = "packagegroup-basic"

# Explicitly exclude dropbear just to be safe
PACKAGE_EXCLUDE = "dropbear"

# List of packages to add to the system
IMAGE_INSTALL:append = " \
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
"

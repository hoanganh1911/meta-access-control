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

# Automatically add our custom overlay to overlays.txt in the boot partition
TEZI_EXTERNAL_KERNEL_DEVICETREE:append:verdin-imx8mp = " verdin-imx8mp_ov5648_overlay.dtbo"
TEZI_EXTERNAL_KERNEL_DEVICETREE_BOOT:append:verdin-imx8mp = " verdin-imx8mp_ov5648_overlay.dtbo"

# List of packages to add to the system
IMAGE_INSTALL:append = " \
    packagegroup-base-wifi \
    packagegroup-wifi-tdx-cli \
    packagegroup-wifi-fw-tdx-cli \
    connman \
    connman-client \
    connman-plugin-ethernet \
    connman-plugin-wifi \
    v4l-utils \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    connman-wifi-config \
    fbgrab \
    gstreamer1.0-plugins-bad-kms \
"

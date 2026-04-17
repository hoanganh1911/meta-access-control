# Hướng dẫn: Build và kiểm thử Device Tree Overlay (.dtbo) & Kernel Module (.ko)

**Target board:** Toradex Verdin iMX8M Plus  
**Distro:** `tdx-xwayland` (scarthgap)  
**Kiến trúc:** `aarch64` (arm64)

---

## Tổng quan: 2 Workflow

Có **2 cách** để build và deploy `.dtbo` / `.ko` — mỗi cách phù hợp với mục đích khác nhau:

| | Workflow A — Yocto (bitbake) | Workflow B — Out-of-tree thủ công |
|---|---|---|
| Lệnh | `bitbake ws2812-mod` | `make ARCH=arm64 CROSS_COMPILE=...` |
| Thời gian | 30–60 phút | 10–30 giây |
| Output | Đóng gói vào rootfs image | File `.ko` / `.dtbo` trực tiếp |
| Deploy | Flash toàn bộ image lên board | `scp` file lên board đang chạy |
| Cần reboot | Có (sau khi flash) | Không (`insmod` + configfs) |
| Phù hợp | Release / CI / production | Phát triển / debug nhanh |

> **Quy tắc thực tế:**  
> - Đang **phát triển / sửa code** → dùng **Workflow B** (nhanh, không cần flash)  
> - Chuẩn bị **release / deploy chính thức** → dùng **Workflow A** (bitbake)

---

## Workflow A — Yocto (bitbake)

### A.1 Build bằng bitbake

```bash
cd ~/Toradex_iMX8M-/toradex-bsp
source export

# Build kernel module (Out-of-tree: các driver tự thêm bên ngoài)
bitbake ws2812-mod
bitbake sr602-mod
bitbake stmvl53l5cx

# Build virtual/kernel (In-tree: các driver có sẵn trong nhân như HD3SS3220, WM8904)
# Dùng khi bạn thay đổi cấu hình .cfg hoặc áp dụng .patch cho nhân
# QUAN TRỌNG: Phải clean state trước khi build lại
bitbake virtual/kernel -c cleansstate
bitbake virtual/kernel

# Build device tree overlays
bitbake device-tree-overlays

# Hoặc build toàn bộ image (bao gồm tất cả)
bitbake access-control
```

Yocto thực hiện bên trong:
1. Cross-compile `.c` → `.ko` (dùng `aarch64-tdx-linux-gcc`)
2. Preprocess `.dts` + `dtc -@` → `.dtbo`
3. Đóng gói vào rootfs

### A.2 Tìm output sau khi bitbake xong

**Kernel modules (.ko):**

```bash
# Ví dụ: ws2812-mod
find toradex-bsp/build/tmp/work/cortexa53*-tdx-linux/ws2812-mod/ \
     -name "*.ko" 2>/dev/null

# Đường dẫn điển hình:
# build/tmp/work/cortexa53_crypto-tdx-linux/ws2812-mod/1.0-r0/
#   image/lib/modules/<kernel-version>/extra/tdx_ws2812.ko
```

**Device tree overlays (.dtbo):**

```bash
find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/device-tree-overlays/ \
     -name "*.dtbo" 2>/dev/null

# Đường dẫn điển hình:
# build/tmp/work/verdin_imx8mp-tdx-linux/device-tree-overlays/<ver>/
#   image/boot/overlays/verdin-imx8mp_sr602_overlay.dtbo
```

### A.3 SCP output từ bitbake lên board (không cần flash lại image)

```bash
BOARD_IP=192.168.1.100
KVER=$(ssh root@${BOARD_IP} "uname -r")

# Lấy .ko từ build tree và gửi lên board
KO_PATH=$(find toradex-bsp/build/tmp/work/cortexa53*-tdx-linux/ws2812-mod/ -name "*.ko" | head -1)
scp ${KO_PATH} root@${BOARD_IP}:/tmp/

# Lấy .dtbo từ build tree và gửi lên board
DTBO_PATH=$(find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/device-tree-overlays/ \
            -name "verdin-imx8mp_ws2812_overlay.dtbo" | head -1)
scp ${DTBO_PATH} root@${BOARD_IP}:/tmp/
```

### A.4 Deploy In-Tree Kernel Modules (HD3SS3220, WM8904, OV5640, etc.)

Sau khi build `virtual/kernel`, các module in-tree (được enable bằng `.cfg`) sẽ nằm trong kernel build tree. Có 3 cách deploy:

**Cách 1: SCP module trực tiếp (nhanh nhất - ~30 giây)**

```bash
BOARD_IP=192.168.1.100
KVER=$(ssh root@${BOARD_IP} "uname -r")

# Tìm module trong kernel build tree
# Ví dụ: HD3SS3220 USB Type-C driver
# Đường dẫn thực tế sau khi build: image/usr/lib/modules/<KVER>/kernel/drivers/usb/typec/
MODULE_PATH=$(find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/ \
              -path "*/image/usr/lib/modules/${KVER}/kernel/drivers/usb/typec/hd3ss3220.ko" | head -1)

# Hoặc dùng đường dẫn trực tiếp (thay <version> bằng version thực tế)
# MODULE_PATH="toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/6.6.94+git/image/usr/lib/modules/${KVER}/kernel/drivers/usb/typec/hd3ss3220.ko"

# Gửi lên board
scp ${MODULE_PATH} root@${BOARD_IP}:~

# Load module trên board
ssh root@${BOARD_IP} "insmod ~/hd3ss3220.ko"
ssh root@${BOARD_IP} "dmesg | tail -20"
```

**Cách 2: Update IPK package (trung bình - ~3 phút)**

```bash
# Tạo IPK package cho kernel modules
bitbake virtual/kernel -c package

# Tìm IPK file (đường dẫn thực tế trong work directory)
IPK_PATH=$(find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/ \
           -path "*/deploy-ipks/verdin_imx8mp/kernel-module-hd3ss3220*.ipk" | head -1)

# Hoặc dùng đường dẫn trực tiếp
# IPK_PATH="toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/6.6.94+git/deploy-ipks/verdin_imx8mp/kernel-module-hd3ss3220-*.ipk"

# Gửi lên board và cài đặt
scp ${IPK_PATH} root@${BOARD_IP}:/tmp/
ssh root@${BOARD_IP} "opkg install --force-reinstall /tmp/$(basename ${IPK_PATH})"
ssh root@${BOARD_IP} "modprobe hd3ss3220"
```

**Cách 3: Flash toàn bộ image (chậm nhất - ~10 phút)**

```bash
# Build image hoàn chỉnh
bitbake access-control

# Flash lên board (xem CLAUDE.md để biết cách flash)
```

**So sánh:**

| Phương pháp | Thời gian | Cần reboot | Phù hợp |
|---|---|---|---|
| SCP module | ~30s | Không | Development/debug nhanh |
| Update IPK | ~3 phút | Không | Test trước khi release |
| Flash image | ~10 phút | Có | Production deployment |

**Ví dụ với các module khác:**

```bash
# WM8904 audio codec
MODULE_PATH=$(find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/ \
              -path "*/image/usr/lib/modules/${KVER}/kernel/sound/soc/codecs/snd-soc-wm8904.ko" | head -1)
scp ${MODULE_PATH} root@${BOARD_IP}:/tmp/
ssh root@${BOARD_IP} "insmod /tmp/snd-soc-wm8904.ko"

# OV5640 camera sensor
MODULE_PATH=$(find toradex-bsp/build/tmp/work/verdin_imx8mp-tdx-linux/linux-toradex/ \
              -path "*/image/usr/lib/modules/${KVER}/kernel/drivers/media/i2c/ov5640.ko" | head -1)
scp ${MODULE_PATH} root@${BOARD_IP}:/tmp/
ssh root@${BOARD_IP} "insmod /tmp/ov5640.ko"
```

---

## Workflow B — Out-of-tree thủ công (phát triển nhanh)

Dùng khi bạn **sửa code và muốn test ngay** mà không cần chờ bitbake hay flash lại board.

```
Host (x86_64)                              Board (aarch64)
─────────────────────────────────          ──────────────────────────────
 .dts  ──[gcc -E + dtc]──►  .dtbo ──┐
                                     ├──[scp]──►  /tmp/
 .c    ──[cross-gcc]──►      .ko  ──┘
                                          ├── apply overlay (configfs)
                                          ├── insmod .ko
                                          └── verify (dmesg / sysfs / /dev)
```

### B.1 Chuẩn bị môi trường trên Host

```bash
# Ubuntu/Debian
sudo apt install device-tree-compiler gcc-aarch64-linux-gnu make

# Kiểm tra phiên bản dtc (cần >= 1.5.0 để hỗ trợ /plugin/)
dtc --version
```

**Xác định KERNEL_SRC** — phải khớp với kernel đang chạy trên board:

| Nguồn | Đường dẫn |
|---|---|
| Yocto build tree | `toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source` |
| Toradex SDK (sau `source`) | `$SDKTARGETSYSROOT/usr/src/kernel` |

```bash
# Kiểm tra kernel version trên board
ssh root@192.168.1.100 "uname -r"
# → 6.6.x-...-verdin-imx8mp

# Đường dẫn kernel source tương ứng
KERNEL_SRC=~/Toradex_iMX8M-/toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source
```

### B.2 Build Device Tree Overlay (.dtbo)

Các overlay trong layer này dùng `#include` (ví dụ [`verdin-imx8mp_sr602_overlay.dts`](../../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts) include `imx8mp-pinfunc.h`), nên cần chạy C preprocessor trước `dtc`.

**Biên dịch thủ công:**

```bash
KERNEL_SRC=~/Toradex_iMX8M-/toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source
DTS_DIR=~/Toradex_iMX8M-/toradex-bsp/layers/meta-access-control/recipes-bsp/device-tree/device-tree-overlays

# Bước 1: C preprocessor xử lý #include
gcc -E -nostdinc \
    -I ${KERNEL_SRC}/arch/arm64/boot/dts/freescale \
    -I ${KERNEL_SRC}/include \
    -undef -D__DTS__ \
    -x assembler-with-cpp \
    ${DTS_DIR}/verdin-imx8mp_sr602_overlay.dts \
    -o /tmp/sr602.dts.pp

# Bước 2: Compile sang .dtbo
dtc -@ -I dts -O dtb \
    -o /tmp/verdin-imx8mp_sr602_overlay.dtbo \
    /tmp/sr602.dts.pp
```

**Makefile tự động — build tất cả overlay:**

Tạo file `scripts/build-overlays.mk` (chưa tồn tại - cần tạo):

```makefile
# scripts/build-overlays.mk
# Sử dụng: make -f scripts/build-overlays.mk [KERNEL_SRC=/path/to/kernel]

LAYER_DIR  := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
DTS_DIR    := $(LAYER_DIR)/recipes-bsp/device-tree/device-tree-overlays
OUT_DIR    := $(LAYER_DIR)/build/overlays

KERNEL_SRC      ?= $(HOME)/Toradex_iMX8M-/toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source
KERNEL_ARCH_INC := $(KERNEL_SRC)/arch/arm64/boot/dts/freescale
KERNEL_INC      := $(KERNEL_SRC)/include

DTS_FILES  := $(wildcard $(DTS_DIR)/*.dts)
DTBO_FILES := $(patsubst $(DTS_DIR)/%.dts, $(OUT_DIR)/%.dtbo, $(DTS_FILES))

.PHONY: all clean

all: $(OUT_DIR) $(DTBO_FILES)
	@echo "==> Overlays built in $(OUT_DIR)"

$(OUT_DIR):
	mkdir -p $@

$(OUT_DIR)/%.dtbo: $(DTS_DIR)/%.dts
	@echo "  DTC  $(notdir $<)"
	gcc -E -nostdinc \
	    -I $(KERNEL_ARCH_INC) \
	    -I $(KERNEL_INC) \
	    -undef -D__DTS__ \
	    -x assembler-with-cpp $< -o $<.pp
	dtc -@ -I dts -O dtb -o $@ $<.pp
	@rm -f $<.pp

clean:
	rm -rf $(OUT_DIR)
```

```bash
cd ~/Toradex_iMX8M-/toradex-bsp/layers/meta-access-control
make -f scripts/build-overlays.mk
```

### B.3 Build Kernel Module (.ko)

**Biên dịch thủ công:**

```bash
KERNEL_SRC=~/Toradex_iMX8M-/toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source

# sr602-mod
cd ~/Toradex_iMX8M-/toradex-bsp/layers/meta-access-control/recipes-kernel/sr602-mod/files
make -C ${KERNEL_SRC} M=$(pwd) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
# → sr602_mod.ko

# stmvl53l5cx
cd ~/Toradex_iMX8M-/toradex-bsp/layers/meta-access-control/recipes-kernel/stmvl53l5cx/files/kernel
make -C ${KERNEL_SRC} M=$(pwd) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
# → stmvl53l5cx.ko

# ws2812-mod
cd ~/Toradex_iMX8M-/toradex-bsp/layers/meta-access-control/recipes-kernel/ws2812-mod/files
make -C ${KERNEL_SRC} M=$(pwd) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
# → tdx_ws2812.ko
```

**Makefile tự động — build tất cả module:**

Tạo file `scripts/build-modules.mk` (chưa tồn tại - cần tạo):

```makefile
# scripts/build-modules.mk
# Sử dụng: make -f scripts/build-modules.mk [KERNEL_SRC=/path/to/kernel]

LAYER_DIR  := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))/..
OUT_DIR    := $(LAYER_DIR)/build/modules

KERNEL_SRC    ?= $(HOME)/Toradex_iMX8M-/toradex-bsp/build/tmp/work-shared/verdin-imx8mp/kernel-source
ARCH          := arm64
CROSS_COMPILE := aarch64-linux-gnu-

MODULE_DIRS := \
    $(LAYER_DIR)/recipes-kernel/sr602-mod/files \
    $(LAYER_DIR)/recipes-kernel/stmvl53l5cx/files/kernel \
    $(LAYER_DIR)/recipes-kernel/ws2812-mod/files

.PHONY: all clean $(MODULE_DIRS)

all: $(OUT_DIR) $(MODULE_DIRS)
	@echo "==> Modules built in $(OUT_DIR)"

$(OUT_DIR):
	mkdir -p $@

$(MODULE_DIRS): $(OUT_DIR)
	@echo "  KO   $(notdir $@)"
	$(MAKE) -C $(KERNEL_SRC) M=$@ ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	find $@ -name "*.ko" -exec cp {} $(OUT_DIR)/ \;

clean:
	rm -rf $(OUT_DIR)
	for d in $(MODULE_DIRS); do \
	    $(MAKE) -C $(KERNEL_SRC) M=$$d ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean; \
	done
```

```bash
make -f scripts/build-modules.mk
```

### B.4 Deploy lên board qua SCP

Tạo file `scripts/deploy.sh` (chưa tồn tại - cần tạo):

```bash
#!/bin/bash
# scripts/deploy.sh — Gửi .dtbo và .ko lên board qua SCP
# Sử dụng: ./scripts/deploy.sh <BOARD_IP>

set -e

BOARD_IP="${1:-192.168.1.100}"
BOARD_USER="root"
LAYER_DIR="$(dirname "$(realpath "$0")")/.."
OVERLAY_DIR="${LAYER_DIR}/build/overlays"
MODULE_DIR="${LAYER_DIR}/build/modules"

echo "==> Deploying to ${BOARD_USER}@${BOARD_IP}"

ssh ${BOARD_USER}@${BOARD_IP} "mkdir -p /tmp/overlays /tmp/modules"

if ls ${OVERLAY_DIR}/*.dtbo 2>/dev/null | grep -q .; then
    echo "  --> Sending overlays..."
    scp ${OVERLAY_DIR}/*.dtbo ${BOARD_USER}@${BOARD_IP}:/tmp/overlays/
fi

if ls ${MODULE_DIR}/*.ko 2>/dev/null | grep -q .; then
    echo "  --> Sending kernel modules..."
    scp ${MODULE_DIR}/*.ko ${BOARD_USER}@${BOARD_IP}:/tmp/modules/
fi

echo "==> Files on board:"
ssh ${BOARD_USER}@${BOARD_IP} "ls -lh /tmp/overlays/ /tmp/modules/"
```

```bash
chmod +x scripts/deploy.sh
./scripts/deploy.sh 192.168.1.100
```

### B.5 Kiểm thử trên Board

**Apply Device Tree Overlay (dùng configfs):**

```bash
ssh root@192.168.1.100

# Mount configfs nếu chưa có
mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config

# Apply overlay sr602
mkdir /sys/kernel/config/device-tree/overlays/sr602
cat /tmp/overlays/verdin-imx8mp_sr602_overlay.dtbo \
    > /sys/kernel/config/device-tree/overlays/sr602/dtbo

# Kiểm tra trạng thái
cat /sys/kernel/config/device-tree/overlays/sr602/status
# → applied

# Xác nhận node trong device tree
find /proc/device-tree -name "sr602" 2>/dev/null

# Gỡ overlay
rmdir /sys/kernel/config/device-tree/overlays/sr602
```

**Load Kernel Module:**

```bash
# sr602 (cần overlay đã applied trước)
insmod /tmp/modules/sr602_mod.ko
dmesg | tail -10
# → "sr602-mod: Probing device..."
lsmod | grep sr602

# stmvl53l5cx (cần overlay vl53l5x đã applied trước)
mkdir /sys/kernel/config/device-tree/overlays/vl53l5x
cat /tmp/overlays/verdin-imx8mp_vl53l5x_overlay.dtbo \
    > /sys/kernel/config/device-tree/overlays/vl53l5x/dtbo

insmod /tmp/modules/stmvl53l5cx.ko
dmesg | tail -10
ls /sys/bus/i2c/devices/3-0029/

# Gỡ module
rmmod sr602_mod
rmmod stmvl53l5cx
```

**Script test tự động trên board:**

Tạo file `scripts/board-test.sh` (chưa tồn tại - cần tạo, sau đó copy lên board):

```bash
#!/bin/bash
# board-test.sh — Chạy trên board
# Sử dụng: bash /tmp/board-test.sh [sr602|vl53l5x|ws2812]

set -e
TARGET="${1:-sr602}"

declare -A DTBO_MAP=(
    [sr602]="verdin-imx8mp_sr602_overlay.dtbo"
    [vl53l5x]="verdin-imx8mp_vl53l5x_overlay.dtbo"
    [ws2812]="verdin-imx8mp_ws2812_overlay.dtbo"
)
declare -A KO_MAP=(
    [sr602]="sr602_mod.ko"
    [vl53l5x]="stmvl53l5cx.ko"
    [ws2812]="tdx_ws2812.ko"
)

DTBO="${DTBO_MAP[$TARGET]}"
KO="${KO_MAP[$TARGET]}"

[ -z "$DTBO" ] && { echo "Unknown target: $TARGET"; exit 1; }

echo "=== Testing: $TARGET ==="

mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config

echo "--> Applying overlay: $DTBO"
mkdir -p /sys/kernel/config/device-tree/overlays/${TARGET}
cat /tmp/overlays/${DTBO} > /sys/kernel/config/device-tree/overlays/${TARGET}/dtbo
STATUS=$(cat /sys/kernel/config/device-tree/overlays/${TARGET}/status)
echo "    Status: $STATUS"
[ "$STATUS" = "applied" ] || { echo "ERROR: overlay not applied"; exit 1; }

echo "--> Loading module: $KO"
insmod /tmp/modules/${KO}
sleep 1

echo "--> dmesg (last 10 lines):"
dmesg | tail -10

echo "--> lsmod:"
lsmod | grep -E "sr602|stmvl53l5cx|ws2812" || echo "  (no match)"

echo "=== PASS: $TARGET ==="
```

```bash
scp scripts/board-test.sh root@192.168.1.100:/tmp/
ssh root@192.168.1.100 "bash /tmp/board-test.sh sr602"
ssh root@192.168.1.100 "bash /tmp/board-test.sh vl53l5x"
```

---

## Quy trình đầy đủ — Workflow B (tóm tắt 4 lệnh)

```bash
# 1. Build overlays
make -f scripts/build-overlays.mk

# 2. Build modules
make -f scripts/build-modules.mk

# 3. Deploy lên board
./scripts/deploy.sh 192.168.1.100

# 4. Test trên board
ssh root@192.168.1.100 "bash /tmp/board-test.sh sr602"
```

---

## Lưu ý quan trọng

| Vấn đề | Giải pháp |
|---|---|
| `dtc: unrecognized option '-@'` | Nâng cấp `dtc` lên >= 1.5.0 |
| `#include` không tìm thấy | Thêm `-I` đúng đường dẫn `arch/arm64/boot/dts/freescale` và `include/` |
| Module không probe | Kiểm tra overlay đã `applied` trước khi `insmod` |
| `vermagic mismatch` | Phải dùng đúng kernel source khớp với kernel đang chạy trên board (`uname -r`) |
| I2C4 có 5 thiết bị | Nếu probe lỗi, kiểm tra pull-up resistor và bus capacitance |
| SR602 xung đột PWM2 | Overlay [`verdin-imx8mp_sr602_overlay.dts`](../../recipes-bsp/device-tree/device-tree-overlays/verdin-imx8mp_sr602_overlay.dts) đã disable `pwm2` tự động |

---

## Tham khảo

- [`recipes-kernel/sr602-mod/files/sr602_mod.c`](../../recipes-kernel/sr602-mod/files/sr602_mod.c) — Driver SR602 PIR
- [`recipes-kernel/stmvl53l5cx/files/kernel/`](../../recipes-kernel/stmvl53l5cx/files/kernel/) — Driver VL53L5CX
- [`recipes-bsp/device-tree/device-tree-overlays/`](../../recipes-bsp/device-tree/device-tree-overlays/) — Tất cả DTS overlay
- [`CLAUDE.md`](../../CLAUDE.md) — Tổng quan layer và pin mapping
- [Toradex Device Tree Overlays](https://developer.toradex.com/linux-bsp/application-development/device-tree-overlays-on-toradex-modules/)
- [Kernel configfs overlays](https://www.kernel.org/doc/html/latest/devicetree/configfs-overlays.html)

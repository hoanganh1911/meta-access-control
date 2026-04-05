# Tài liệu Kỹ thuật Dự án Access Control (Verdin i.MX8M Plus)

Tài liệu này cung cấp cái nhìn tổng quan về kiến trúc hệ thống, quy trình phát triển lớp (layer) `meta-access-control` và các phương pháp xác thực phần cứng thực tế.

---

## 1. Kiến trúc Layer: `meta-access-control`
Dự án tuân thủ triết lý thiết kế module của Yocto Project, đảm bảo tính tách biệt giữa BSP gốc và tùy chỉnh đặc thù.

### Cấu trúc thư mục (Layer Hierarchy)
```text
layers/meta-access-control/
├── conf/layer.conf                      # Metadata & Priority
├── recipes-bsp/                         # Device Tree Overlays
│   └── device-tree/
│       └── device-tree-overlays/
│           └── verdin-imx8mp_ov5648_overlay.dts
├── recipes-kernel/                      # Linux Drivers & Patches
│   └── linux/
│       ├── linux-toradex_%.bbappend     
│       └── linux-toradex/
│           └── ov5648.cfg               # Bật driver OV5648
└── recipes-images/                      # Rootfs Definition
    └── images/
        └── access-control.bb            # Recipe chính tạo Image
```

---

## 2. Ma trận Ánh xạ Phần cứng (Hardware Mapping Matrix)
Sự chính xác giữa Schematic vật lý và định danh Linux là ưu tiên hàng đầu.

### 2.1 Giao tiếp I2C (Camera Control)
- **Chuẩn Verdin**: I2C_4_CSI (SODIMM 93, 95).
- **SoC Controller**: **I2C3** (Địa chỉ: `0x30A40000`).
- **Xác minh gốc**: `imx8mp-verdin.dtsi:L1182` xác nhận SODIMM 93/95 nối vào Pad `I2C3_SCL/SDA`.
- **Linux Node**: **`/dev/i2c-2`**.
    *   *Lý thuyết Alias*: Trong `imx8mp.dtsi`, NXP gán `alias i2c2 = &i2c3;`, do đó Linux khởi tạo nó tại bus số 2.
- **Đính chính**: Tránh nhầm lẫn với `/dev/i2c-4` (bus HDMI - i2c5). Mọi giao tiếp với sensor OV5648 **bắt buộc dùng bus 2**.

### 2.2 Giao tiếp GPIO (Power & Reset)
Hệ thống mượn chân từ Ethernet 2 để điều khiển Camera:
- **Powerdown**: **SODIMM 205** (GPIO4_IO08).
- **Reset**: **SODIMM 207** (GPIO4_IO09).
- **Xác minh gốc**: `imx8mp-verdin.dtsi:L1004-1005` xác nhận các chân này vốn là của Pad `SAI1_RXD6/7`.

### 2.3 Cơ chế xuất xung nhịp (Clocking Strategy) cho SODIMM 91
Để điều khiển Camera, SoC cần phát xung 24MHz ổn định tại chân **SODIMM 91** (`CSI_1_MCLK`). Hệ thống sử dụng cơ chế sau:

#### Phương pháp: Sử dụng Hệ thống Xung nhịp (CCM_CLKO2)
Đây là cách chuyên dụng nhất, cho phép Kernel quản lý bật/tắt xung đồng bộ với Driver Camera.
- **Chế độ (Mode)**: **ALT 6** (`CCM_CLKO2`).
- **Nguồn xung**: `CLKOUT2` (có thể cấu hình lấy từ OSC 24M qua Device Tree).
- **SoC Pad**: **GPIO1_IO15** (Dẫn chứng: `imx8mp-verdin.dtsi:L1130`).
- **ALT Code**: `0x6` (Dẫn chứng: `imx8mp-pinfunc.h`).

| Thành phần | Tệp tham chiếu | Nội dung xác thực (Grep) | Ý nghĩa kỹ thuật |
| :--- | :--- | :--- | :--- |
| **SODIMM 91** | `imx8mp-verdin.dtsi` | `/* CSI_1_MCLK */ <...GPIO1_IO15...>; /* SODIMM 91 */` | Khẳng định thực thể vật lý. |
| **MCLK ALT** | `imx8mp-pinfunc.h` | `MX8MP_IOMUXC_GPIO1_IO15__CCM_CLKO2 ... 0x6` | Khẳng định phương pháp ALT 6. |

---

## 3. Lý thuyết và Cơ chế Sửa đổi (Implementation Theory)

Để vận hành một thành phần phần cứng mới (như Camera OV5648), hệ thống cần sự phối hợp giữa hai tầng: **Bản đồ phần cứng (Device Tree)** và **Trình điều khiển (Kernel Driver)**.

### 3.1 Lý thuyết Device Tree (Phần cứng mô tả bằng Dữ liệu)
Device Tree (DT) là một cấu trúc dữ liệu dùng để mô tả các thành phần phần cứng của máy tính/nhúng. Nó giúp tách rời cấu hình phần cứng ra khỏi mã nguồn của nhân Linux.

- **DTB (Device Tree Blob)**: File nhị phân được Kernel nạp lúc khởi động để biết hệ thống có bao nhiêu I2C, GPIO, v.v.
- **DTBO (Device Tree Overlay)**: Một "lớp đè" kỹ thuật. Thay vì sửa file DTB gốc của Toradex, chúng ta tạo file `.dtbo` để **cập nhật hoặc ghi đè** các thuộc tính (ví dụ: chuyển chân Ethernet sang GPIO cho Camera).
- **Cơ chế `compatible`**: Mỗi thiết bị trong DT phải có một chuỗi `compatible` (Vd: `"ovti,ov5648"`). Chuỗi này đóng vai trò là **khóa định danh duy nhất (Unique Identifier)** để hệ điều hành thực hiện việc liên kết (Binding) Node thiết bị với đúng Trình điều khiển (Driver) tương ứng trong nhân Linux.

### 3.2 Cơ chế và Triển khai Driver Kernel (The Driver Layer)
Việc vận hành driver camera yêu cầu sự phối hợp từ cấu cấu hình phần mềm đến điều kiện vật lý trên board mạch.

#### A. Cơ chế Liên kết (Theory of Binding)
1.  **Matching**: Khi Kernel đọc tệp DTB, nó sử dụng chuỗi **`compatible`** (Vd: `"ovti,ov5648"`) làm **khóa định danh duy nhất (Unique Identifier)** để liên kết (Bind) Node thiết bị với đúng Trình điều khiển tương ứng.
    *   *Dẫn chứng*: Driver `ov5648.c` chứa cấu trúc `ov5648_of_match[]` để thực hiện việc so khớp này.
2.  **Probe (Khởi động)**: Tại bước này, Driver yêu cầu Kernel cấp các tài nguyên phần cứng (I2C Address 0x36, Reset/Powerdown GPIOs, Clocks) đã khai báo trong Device Tree.

#### B. Kích hoạt và Điều kiện Vật lý (Implementation & Prerequisite)
- **Kích hoạt Driver bằng Kconfig Fragments (`.cfg`)**:
    *   *Tại sao dùng .cfg?*: Thay vì sửa trực tiếp file cấu hình nhân khổng lồ, chúng ta sử dụng tệp `.cfg` để khai báo các biến cần thiết (Vd: `CONFIG_VIDEO_OV5648=y`).
    *   *Cơ chế Merge*: Trong quá trình biên dịch (Bitbake), hệ thống sẽ tự động hợp nhất các dòng trong `.cfg` vào cấu hình gốc (`defconfig`) của Toradex. 
    *   *Kết quả*: Điều này giúp bật Driver `ov5648.c` để nhân Linux tạo ra các thiết bị truyền dẫn như `/dev/video0` và `/dev/media0`.
- **Yêu cầu Xung nhịp (MCLK)**: Camera chỉ có thể bắt đầu giao tiếp I2C khi nhận được xung nhịp **MCLK 24MHz** tại chân **SODIMM 91 (GPIO1_IO15)**. Hệ điều hành cần nạp đúng chế độ **ALT 6** để chân này phát xung thay vì giữ mức GPIO tĩnh.

#### C. Lệnh xác thực "Hàng rào Driver" (Hardware Proof)
```bash
# 1. Kiểm tra xem Kernel đã nạp thông tin node từ DTBO chưa
grep -r "ovti,ov5648" /proc/device-tree/

# 2. Kiểm tra xem Driver đã thực hiện Liên kết (Binding) thành công với địa chỉ 0x36 chưa
# Nếu thấy i2c-2-0036 là Matching thành công.
find /sys/bus/i2c/devices/ -name "*0036*"

# 3. Kiểm tra xem Module Driver đã được nạp hay chưa
lsmod | grep ov5648
```

### 3.3 Giải quyết xung đột (Conflict Resolution)
Do SODIMM 205/207 mặc định được dùng cho Ethernet RGMII, chúng ta phải thực hiện cơ chế ghi đè trong Device Tree:
- **Hành động**: Vô hiệu hóa bộ điều khiển Ethernet (`&fec { status = "disabled"; };`).
- **Kết quả**: Giải phóng các Pad SoC khỏi driver mạng, cho phép driver GPIO chiếm quyền điều khiển.

### 3.4 Giải phẫu PAD_CTL (Tầng vật lý)
Việc cấu hình chân (như **`0x6`** hay **`0x10`**) dựa trên việc giải mã các Bit trong thanh ghi **Pad Control Register** (Tham chiếu: `/verdin_imx8m_plus_datasheet.pdf` - **Table 4-2**).

Hệ thống trích xuất thông tin từ bảng này để người đọc không cần tra cứu lại tài liệu gốc:

| Bit | Field | Description | Remarks (Trích dẫn Datasheet) |
| :--- | :--- | :--- | :--- |
| **8** | **PE** | 0: Pull disabled, 1: Pull enable | Vô hiệu hóa điện trở kéo nội. |
| **7** | **HYS** | 0: CMOS input, 1: Schmitt trigger | Chế độ ngõ vào chuẩn CMOS. |
| **6** | **PUE** | 0: Select pull-down, 1: Select pull-up | TYP: 22kΩ Pull-up / 23kΩ Pull-down. |
| **5** | **ODE** | 0: Output is CMOS, 1: Open-drain | Chế độ đẩy-kéo (Push-Pull). |
| **4-3** | **FSE** | **0x: Slow Slew Rate**, 1x: Fast | **Ưu tiên dùng Slow** để giảm nhiễu (EMC). |
| **2-0** | **DSE** | 11x: X6, 10x: X4, 01x: X2, 00x: X1 | Chọn 110 (X6) để thắng trở kéo ngoài board. |

#### Giải mã giá trị `0x6` (Cấu hình Dự án):
- Chuyển đổi nhị phân: `0x6` = `0000 0110`.
- **DSE [2:0] = 110 (X6)**: Lực phát tín hiệu mạnh nhất.
- **FSE [4:3] = 00 (Slow)**: Tối ưu hoá chống nhiễu EMC.
- **PE [8] = 0**: Tắt trở nội (Vì Schematic đã có trở kéo ngoài board).

---


## 4. Khởi tạo Môi trường (Environment Setup)
```bash
# Tại thư mục gốc toradex-bsp
source export   # (Có thể dùng lệnh tương đương là ". export")
```
*Giải thích*: Lệnh `source` thực thi script trong Shell hiện tại để giữ lại các biến môi trường (PATH, MACHINE). Nếu chỉ chạy `./export`, các biến này sẽ mất ngay sau khi script kết thúc.

---

## 5. Quy trình Prototyping & Triển khai Nhanh
Dùng quy trình này để test thay đổi trong vài phút thay vì build lại toàn bộ Image.

### 5.1 Bí quyết Đóng dấu Phiên bản (DTBO Tagging)
Để chắc chắn file DTBO trên Board là bản mới nhất, hãy thêm một thuộc tính "giả" vào node thiết bị trong file `.dts`:
```dts
ov5648: camera@36 {
    status = "okay";
    build-version = "v1.2_2026_04_05_Hogan"; // Dấu vân tay phiên bản
    ...
};
```

### 5.2 Biên dịch và Triển khai
- **Biên dịch DTBO**:
  `bitbake device-tree-overlays -c compile -f && bitbake device-tree-overlays -c deploy`
  *SCP*: `scp build/deploy/images/verdin-imx8mp/verdin-imx8mp_ov5648_overlay.dtbo root@<board_ip>:/boot/overlays/`

- **Biên dịch Kernel**:
  `bitbake virtual/kernel -c compile -f && bitbake virtual/kernel -c deploy`
  *SCP*: `scp build/deploy/images/verdin-imx8mp/Image root@<board_ip>:/boot/`

---

## 6. Quy trình Xác thực Toàn diện (Comprehensive Verification)
Đây là chương quan trọng nhất để đảm bảo hệ thống vận hành đúng như thiết kế.

### 6.1 Xác thực Phiên bản (Versioning Check)
Trước khi test tính năng, hãy xác nhận xem code mới đã "chạy" chưa:
1. **Kiểm tra Kernel**: `uname -a` (So khớp ngày giờ build).
2. **Kiểm tra DTBO (Dấu vân tay)**:
   - Lệnh: `cat /proc/device-tree/soc@0/bus@30800000/i2c@30a40000/ov5648@36/build-version`
   - Nếu trả về đúng chuỗi bạn đã đặt ở Bước 5.1, file DTBO đã nạp thành công.

### 6.2 Xác thực Tầng Driver (Driver Binding)
1. **Check Matching**: `find /sys/bus/i2c/devices/ -name "*0036*"` (Nếu thấy `i2c-2-0036` là Driver đã nhận HW).
2. **Check GPIO**: `cat /sys/kernel/debug/gpio | grep ov5648` (Driver phải chiếm quyền 2 chân Powerdown/Reset).

### 6.3 Xác thực Tầng Vật lý (Deep Probe)
Sử dụng khi I2C không trả về địa chỉ 0x36 dù đã nạp DTBO:

1. **Kiểm tra trạng thái Chân điều khiển (libgpiod)**:
   Nếu driver không thể tự điều khiển chân, hãy kiểm tra mức logic vật lý:
   - **Powerdown (SODIMM 205 - GPIO4_IO08)**: Phải là **0 (Low)** để camera hoạt động.
     + Kiểm tra: `gpioget 3 8`
   - **Reset (SODIMM 207 - GPIO4_IO09)**: Phải là **1 (High)** để camera hoạt động.
     + Kiểm tra: `gpioget 3 9`
   - **Manual Force (Cưỡng bức)**: Thử ép mức logic để "đánh thức" camera:
     `gpioset 3 8=0 && gpioset 3 9=1`
     Sau đó chạy lại lệnh `i2cdetect -y -r 2`. Nếu thấy 0x36 hiện ra, lỗi nằm ở Driver/Device Tree quản lý chân chưa đúng.
   - **Reversed Verification (Kiểm tra phản chứng)**: Để chắc chắn 100% bạn điều khiển đúng chân vật lý:
     1. Chạy: `gpioset 3 8=1` (Ép camera vào trạng thái Power-down).
     2. Chạy: `i2cdetect -y -r 2`.
     3. **Kết quả**: Địa chỉ 0x36 **phải biến mất**. Nếu nó vẫn còn, nghĩa là bạn đang điều khiển nhầm chân GPIO hoặc Schematic có sai sót.

2. **Đọc vân tay Sensor (Chip ID)**:
   - Lệnh: `i2cget -y 2 0x36 0x300a w` (Phải trả về `0x5648`).

3. **Kiểm tra Cây xung nhịp (Clock Tree)**:
   - Lệnh: `cat /sys/kernel/debug/clk/clk_summary | grep -i clkout2`
   - **Giải thích cú pháp**: 
     * `clk_summary`: Tệp ảo trong `debugfs` chứa toàn bộ sơ đồ phân phối xung nhịp của SoC.
     * `clkout2`: Tên logic của bộ tạo xung được gán cho chân 91 (CCM_CLKO2).
   - **Kết quả kỳ vọng**:
     ```text
     clk_node             enable_count  prepare_count  rate
     ----------------------------------------------------------
     clkout2              1             1              24000000
     ```
     * `rate = 24000000`: Xung nhịp đúng tần số 24MHz.
     * `enable_count = 1`: Xung đang được phát (đã bật).

4. **Kiểm tra Backlight**: `echo 10 > /sys/class/backlight/backlight/brightness` (PWM check).

---

## 7. Cấu hình Pipeline & Thu hình (Media Logic)
```bash
# Thiết lập Topology
media-ctl -d /dev/media0 -V "'ov5648 2-0036':0 [fmt:YUYV8_1X16/1280x720]"

# Thu hình test
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,format=YUY2,width=1280,height=720 ! autovideosink
```

---

## 8. Phương pháp Tham chiếu (Reference Methodology)
Để xác định chân mới trong tương lai:
1.  **Lớp Cứng**: Tra cứu SODIMM trong Datasheet.
2.  **Lớp BSP**: Tìm chú thích SODIMM trong `imx8mp-verdin.dtsi` để biết SoC Pad.
3.  **Lớp Software**: Tra cứu `imx8mp-pinfunc.h` để biết chức năng (ALT mode).

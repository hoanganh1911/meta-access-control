# Tài liệu Kỹ thuật Phát triển Camera OV5640 (Verdin i.MX8M Plus)

Tài liệu này cung cấp hướng dẫn chi tiết về cấu hình, triển khai và xác thực hệ thống Camera OV5640 trên nền tảng Toradex Verdin i.MX8M Plus sử dụng Yocto Project.

---

## 1. Kiến trúc Layer: `meta-access-control`

Dự án sử dụng Layer tùy chỉnh để quản lý cấu hình camera mà không ảnh hưởng đến BSP gốc của Toradex.

### Cấu trúc thư mục (Layer Hierarchy)
```text
layers/meta-access-control/
├── conf/layer.conf                      # Cấu hình Layer & Ưu tiên (Priority)
├── recipes-bsp/                         # Quản lý Device Tree Overlays
│   └── device-tree/
│       └── device-tree-overlays/
│           └── verdin-imx8mp_ov5640_overlay.dts
├── recipes-kernel/                      # Linux Drivers & Patches
│   └── linux/
│       ├── linux-toradex_%.bbappend     
│       └── linux-toradex/
│           └── ov5640.cfg               # Kích hoạt CONFIG_VIDEO_OV5640
└── recipes-images/                      # Định nghĩa Rootfs
    └── images/
        └── access-control.bb            # Recipe tạo Image hệ thống
```

---

## 2. Ma trận Ánh xạ Phần cứng (Hardware Mapping Matrix)

Việc ánh xạ chính xác giữa chân vật lý (SODIMM) và định danh trong phần mềm là điều kiện tiên quyết.

### 2.1 Giao tiếp I2C (Điều khiển Camera)
- **Bus vật lý**: I2C_4_CSI (SODIMM 93, 95).
- **SoC Controller**: **I2C3** (Địa chỉ gốc: `0x30A40000`).
- **Linux Device**: **`/dev/i2c-2`** (Do định danh Alias trong `imx8mp.dtsi`).
- **Địa chỉ Slave (OV5640)**: **`0x3c`**.

### 2.2 Giao tiếp GPIO (Điều khiển Nguồn)
Sử dụng các chân từ cụm Ethernet 2 (đã bị vô hiệu hóa trong DT) để điều khiển Camera:
- **Powerdown (PWDN)**: **SODIMM 205** (GPIO4_IO08) - Mặc định: `Active High`.
- **Reset (RESET)**: **SODIMM 207** (GPIO4_IO09) - Mặc định: `Active Low`.

### 2.3 Hệ thống Xung nhịp (Clocking)
Camera yêu cầu xung nhịp **24MHz** để khởi tạo bộ điều khiển I2C/SCCB.
- **Chân vật lý**: **SODIMM 91** (CSI_1_MCLK).
- **SoC Function**: **CCM_CLKO2** (Xung nhịp ngõ ra số 2).
- **Mã Pad**: `GPIO1_IO15` (Chế độ `ALT 1`).

---

## 3. Lý thuyết Triển khai (Implementation Theory)

### 3.1 Device Tree Overlay (DTBO)
Thay vì sửa file gốc của Toradex, chúng ta sử dụng cơ chế Overlay (`.dtbo`) để:
1.  **Vô hiệu hóa (Disable)**: Tắt bộ điều khiển Ethernet (`fec`) để giải phóng chân GPIO4_IO08/09.
2.  **Kích hoạt (Enable)**: Bật `mipi_csi_0`, `isi_0`, và `isp_0`.
3.  **Liên kết (Link)**: Kết nối Endpoint của camera tới bộ thu MIPI của SoC.

### 3.2 Cơ chế Binding của Driver
Khi Kernel nạp DTBO, nó tìm chuỗi `compatible = "ovti,ov5640"`.
1.  **Matching**: Kernel đối chiếu chuỗi này với danh sách ID trong driver `ov5640.c`.
2.  **Probing**: Khi khớp, hàm `ov5640_probe()` được gọi để cấp phát tài nguyên, bật xung nhịp 24MHz và kiểm tra Chip ID qua I2C.

---

## 4. Giải phẫu PAD_CTL (Tầng vật lý)

Cấu hình các giá trị như `0x106` hoặc `0x156` dựa trên thanh ghi **Pad Control Register**.

| Bit | Field | Ý nghĩa kỹ thuật | Cấu hình khuyên dùng |
| :--- | :--- | :--- | :--- |
| **8** | **PE** | Pull Enable (Bật điện trở kéo) | 1 (Bật để ổn định mức logic) |
| **6** | **PUE** | Pull Up Select (Chọn kéo lên) | 0 (Pull-down) hoặc 1 (Pull-up) |
| **4-3** | **FSE** | Slew Rate (Tốc độ chuyển mức) | 0 (Slow) để giảm nhiễu điện từ (EMI) |
| **2-0** | **DSE** | Drive Strength (Độ mạnh dòng) | 110 (X6) để đảm bảo tín hiệu đi xa |

---

## 5. Quy trình Xác thực (Verification Workflow)

Sau khi nạp Image mới, thực hiện các bước sau để xác nhận hệ thống:

### Bước 1: Kiểm tra phần cứng I2C
```bash
# Quét bus i2c-2 để tìm địa chỉ 0x3c
i2cdetect -y -r 2
```
*Kết quả kỳ vọng*: Thấy số `3c` hoặc chữ `UU` tại vị trí 0x3c. Nếu thấy `UU`, driver đã nhận diện thành công.

### Bước 2: Kiểm tra Xung nhịp (Heartbeat)
```bash
# Kiểm tra xem xung nhịp 24MHz đã được bật chưa
cat /sys/kernel/debug/clk/clk_summary | grep -i clkout2
```

### Bước 3: Kiểm tra luồng dữ liệu (Media Pipeline)
```bash
# Xem sơ đồ kết nối các node video
media-ctl -p -d /dev/media0
```

### Bước 4: Stream thử nghiệm lên màn hình
Sử dụng GStreamer để lấy dữ liệu từ ISI (thường là `/dev/video3`) và hiển thị:
```bash
gst-launch-1.0 v4l2src device=/dev/video3 ! \
    video/x-raw,format=YUY2,width=640,height=480 ! \
    autovideosink
```

---

## 6. Xử lý sự cố (Troubleshooting)

| Hiện tượng | Nguyên nhân phổ biến | Cách khắc phục |
| :--- | :--- | :--- |
| `failed to get xclk` | Tên xung nhịp trong DT không khớp driver | Kiểm tra `clock-names = "xclk"` |
| `I2C write error` | Camera chưa có điện hoặc chưa có MCLK | Kiểm tra chân PWDN và `clkout2` |
| `Device busy` | Chân GPIO đang bị Ethernet (fec) chiếm | Đảm bảo `&fec { status = "disabled"; };` |
| `No remote pad found` | Sai nhãn `remote-endpoint` | Kiểm tra tính đối xứng của nhãn trong DT |

---
*Tài liệu được biên soạn cho dự án Access Control - Verdin i.MX8M Plus.*

# Access Control Board - Verdin iMX8M Plus

This README contains build instructions and hardware functional test procedures for the Access Control project.

---

## Host Machine Setup for Building with Yocto

### Install Repo Dependencies

```bash
$ sudo apt install curl git python3
```

After installing Git, configure your user name and e-mail address:

```bash
$ git config --global user.name "John Doe"
$ git config --global user.email johndoe@example.com
```

### Install Repo

```bash
$ mkdir ~/bin
$ export PATH=~/bin:$PATH
$ curl https://commondatastorage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
$ chmod a+x ~/bin/repo
```

### Install the Yocto Project Repositories

```bash
$ mkdir Acess_control
$ cd Acess_control/
$ repo init -u git://git.toradex.com/toradex-manifest.git -b refs/tags/7.3.0 -m tdxref/default.xml
$ repo sync
```

Source the file `export` to setup the environment. On the first invocation, this also creates a sample configuration in `build/conf/`.

```bash
$ . export
```

### Install Custom Layer meta-access-control

```bash
$ cd layers/
$ git clone https://github.com/hoanganh1911/meta-access-control.git meta-access-control
```

### Add Layer to the Yocto Build

```bash
$ cd ../build/
$ bitbake-layers add-layer ../layers/meta-access-control
```

### Edit the Build Configuration Files

Edit `conf/local.conf`:

```
MACHINE ?= "verdin-imx8mp"
ACCEPT_FSL_EULA = "1"
```

### Build the Image

```bash
$ bitbake access-control
```

The first build will typically take several hours.

### Deploy the Built Image

After the build finishes, the output image is located at `build/deploy/images/verdin-imx8mp/`.

Deploy the Tezi image to your board using Toradex Easy Installer (Tezi).

Pre-built Tezi image download: [Access Control Tezi Image](https://vidisvn-my.sharepoint.com/:w:/g/personal/hogan_tran_devbrix_com/IQCd6gfewE2ZTrcb14ss_nivAUn0vP7K1nxFY4v0AtC9y20?e=bsalDg)

---

## Hardware Functional Test Procedures

After flashing the image and booting the board, use the following commands to test each hardware component.

### TC-001: Camera & Display

Stream real-time camera data to the display using GStreamer:

```bash
$ gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! autovideosink
```

**Expected:** Camera image is streamed in real-time and displayed on the screen.

---

### TC-002: Mic & Audio (WM8904)

Record 5 seconds of audio from the microphone:

```bash
$ arecord -D hw:0,0 -f S16_LE -r 48000 -c 2 -d 5 /tmp/test.wav
```

Play back the recorded clip:

```bash
$ aplay -D hw:0,0 /tmp/test.wav
```

**Expected:** Playback audio matches the audio captured by the microphone.

---

### TC-003: OPT3001 Light Sensor

The OPT3001 is on I2C4 at address 0x44. Read the lux value via sysfs:

```bash
$ cat /sys/bus/iio/devices/iio:device*/in_illuminance_input
```

Cover the sensor with your hand and read again to observe the value decrease.

**Expected:** Light values change according to lighting conditions.

---

### TC-004: Panic Button

The Panic button is registered as a gpio-keys input device (KEY_F1). Monitor input events:

```bash
$ evtest /dev/input/event0
```

Press the Panic button and observe the event output.

To control the Panic LED output:

```bash
# Turn LED on
$ echo 1 > /sys/class/leds/SOM_PANIC_OUTPUT/brightness

# Turn LED off
$ echo 0 > /sys/class/leds/SOM_PANIC_OUTPUT/brightness
```

**Expected:** The LED turns off when the Panic button is pressed; evtest shows KEY_F1 events.

---

### TC-005: PIR Sensor (SR602)

The SR602 PIR sensor uses a kernel module that logs motion events to dmesg. Monitor the kernel log:

```bash
$ dmesg -w | grep sr602
```

Wave your hand in front of the PIR sensor.

**Expected:** Kernel log displays "Motion detected!" when motion is sensed, "Motion ended." when idle.

---

### TC-006: PN532 NFC

The PN532 is connected via UART at `/dev/ttymxc0`. Use libnfc to poll for NFC tags:

```bash
$ nfc-poll
```

Place an NFC tag near the PN532 module.

**Expected:** Tag UID and type information are displayed upon successful scan.

---

### TC-007: VL53L5CX ToF Sensor

Run the VL53L5CX userspace test application:

```bash
$ stmvl53l5cx-user
```

Select example 1 (basic ranging) from the menu. Place the sensor face down on the table, then flip it upward.

**Expected:** Distance value is approx. 0 mm when face down; value increases when flipped upward.

---

### TC-008: Tamper Button

The Tamper button is registered as a gpio-keys input device (KEY_WAKEUP). Monitor input events:

```bash
$ evtest /dev/input/event1
```

Press the Tamper button.

**Expected:** evtest shows KEY_WAKEUP events when the button is pressed.

---

### TC-009: SD Card

Write test data to the SD card:

```bash
$ echo "access-control-test" > /mnt/sd/test.txt
$ sync
```

Remove the SD card and read the file on a laptop to verify data integrity.

**Expected:** Data read on the laptop is identical to the data written.

---

### TC-010: RCWL-1670 Ultrasonic Sensor

Run the RCWL-1670 userspace application:

```bash
$ rcwl1670-user
```

Point the sensor toward the table surface, then gradually move it away.

**Expected:** Measured distance values change corresponding to the distance from the surface.

---

### TC-011: WS2812 LED Strip

Control the LED strip using the userspace application:

```bash
# Set color to Red
$ ws2812-user 255 0 0

# Set color to Green
$ ws2812-user 0 255 0

# Set color to Blue
$ ws2812-user 0 0 255
```

Or write directly to sysfs:

```bash
$ echo "255 0 0" > /sys/bus/platform/devices/ws2812/colors
```

**Expected:** LED strip lights up and displays the correct RGB color as commanded.

---

### TC-012: Security ATECC608A

Scan the I2C bus to detect the ATECC608A (address 0x60):

```bash
$ i2cdetect -y 3
```

Run the ATECC608A test application to read the device serial number:

```bash
$ atecc-test
```

**Expected:** Device is detected on I2C bus 3; serial number is read and displayed.

---

### TC-013: USB 3.0 + USB 2.0 (HD3SS3220)

Verify the HD3SS3220 USB Type-C controller on I2C bus 3 (address 0x47):

```bash
$ i2cdetect -y 3
```

Run the USB-C status monitor:

```bash
$ usb-c-test
```

Plug in USB devices to USB 3.0 and USB 2.0 ports and observe the connection state.

**Expected:** Controller is identified via I2C; USB ports function correctly with connected devices.

---

## Test Summary

| TC ID  | Component            | Test Command                     | Status |
|--------|----------------------|----------------------------------|--------|
| TC-001 | Camera & Display     | `gst-launch-1.0 ...`            | PASS   |
| TC-002 | Mic & Audio          | `arecord` / `aplay`             | PASS   |
| TC-003 | OPT3001 Light Sensor | `cat .../in_illuminance_input`   | PASS   |
| TC-004 | Panic Button         | `evtest`                         | PASS   |
| TC-005 | PIR Sensor           | `dmesg -w \| grep sr602`        | PASS   |
| TC-006 | PN532 NFC            | `nfc-poll`                       | PASS   |
| TC-007 | VL53L5CX ToF        | `stmvl53l5cx-user`              | PASS   |
| TC-008 | Tamper Button        | `evtest`                         | PASS   |
| TC-009 | SD Card              | `echo ... > /mnt/sd/test.txt`   | PASS   |
| TC-010 | RCWL-1670            | `rcwl1670-user`                  | PASS   |
| TC-011 | WS2812 LED Strip     | `ws2812-user R G B`             | DEFER  |
| TC-012 | Security ATECC608A   | `atecc-test`                     | PASS   |
| TC-013 | USB 3.0 + USB 2.0    | `usb-c-test`                     | DEFER  |

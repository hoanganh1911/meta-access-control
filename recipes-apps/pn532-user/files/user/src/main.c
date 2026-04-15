#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>

#define UART_PORT "/dev/ttymxc0"

// PN532 Constants
#define PN532_PREAMBLE                (0x00)
#define PN532_STARTCODE1              (0x00)
#define PN532_STARTCODE2              (0xFF)
#define PN532_POSTAMBLE               (0x00)
#define PN532_HOSTTOPN532             (0xD4)
#define PN532_PN532TOHOST             (0xD5)

#define PN532_COMMAND_GETFIRMWAREVERSION (0x02)
#define PN532_COMMAND_SAMCONFIGURATION   (0x14)

// Helper: In ra chuỗi Hex để debug
void print_hex(const char* label, const uint8_t *buf, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

int setup_uart(int fd) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -1;

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         // 8-bit chars
    tty.c_cflag &= ~PARENB;     // no parity
    tty.c_cflag &= ~CSTOPB;     // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;    // no flow control

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;            
    tty.c_cc[VTIME] = 10; // Timeout 1s

    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -1;
    return 0;
}

// Hàm gửi lệnh chuẩn PN532 (tự tính checksum)
int8_t write_command(int fd, const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen) {
    uint8_t length = hlen + blen + 1; // +1 cho TFI (D4)
    uint8_t checksum;
    uint8_t sum = PN532_HOSTTOPN532;

    uint8_t packet[32];
    int p = 0;

    packet[p++] = PN532_PREAMBLE;
    packet[p++] = PN532_STARTCODE1;
    packet[p++] = PN532_STARTCODE2;
    packet[p++] = length;
    packet[p++] = ~length + 1;
    packet[p++] = PN532_HOSTTOPN532;

    for (int i = 0; i < hlen; i++) {
        packet[p++] = header[i];
        sum += header[i];
    }
    for (int i = 0; i < blen; i++) {
        packet[p++] = body[i];
        sum += body[i];
    }

    checksum = ~sum + 1;
    packet[p++] = checksum;
    packet[p++] = PN532_POSTAMBLE;

    print_hex("Write", packet, p);
    if (write(fd, packet, p) < 0) return -1;
    return 0;
}

// Đọc khung ACK (00 00 FF 00 FF 00)
int8_t read_ack(int fd) {
    uint8_t ack[6];
    uint8_t expected_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    
    int n = read(fd, ack, 6);
    if (n < 6) {
        printf("ACK Timeout or incomplete (%d bytes)\n", n);
        return -1;
    }
    
    print_hex("ACK", ack, 6);
    if (memcmp(ack, expected_ack, 6) != 0) {
        printf("Invalid ACK frame!\n");
        return -2;
    }
    return 0;
}

void wakeup(int fd) {
    printf("Sending wakeup sequence...\n");
    uint8_t dummy[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    write(fd, dummy, sizeof(dummy));
    usleep(100000); // 100ms - đủ thời gian cho PN532 thức dậy

    // Drain buffer có kiểm soát giống Arduino: đọc bỏ dữ liệu rác
    // KHÔNG dùng tcflush vì nó xóa cả ACK mà PN532 gửi về!
    uint8_t discard;
    int drained = 0;
    while (read(fd, &discard, 1) > 0) {
        drained++;
    }
    if (drained > 0) {
        printf("Wakeup: drained %d junk bytes.\n", drained);
    }
}

int main() {
    int fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Open UART");
        return 1;
    }

    if (setup_uart(fd) != 0) {
        printf("Setup UART failed\n");
        return 1;
    }

    wakeup(fd);

    // Bước 1: Get Firmware Version
    printf("\n--- Test 1: Get Firmware Version ---\n");
    uint8_t cmd_ver = PN532_COMMAND_GETFIRMWAREVERSION;
    write_command(fd, &cmd_ver, 1, NULL, 0);

    if (read_ack(fd) == 0) {
        uint8_t resp[32];
        int n = read(fd, resp, sizeof(resp));
        if (n > 0) {
            print_hex("Response", resp, n);
            // Tìm kiếm D5 03 trong response
            for (int i = 0; i < n - 1; i++) {
                if (resp[i] == 0xD5 && resp[i+1] == 0x03) {
                    printf("Success! Found PN532. IC: 0x%02X, Ver: %d.%d\n", 
                           resp[i+2], resp[i+3], resp[i+4]);
                    break;
                }
            }
        }
    }

    // Bước 2: SAM Configuration (Cấu hình chế độ hoạt động bình thường)
    printf("\n--- Test 2: SAM Configuration ---\n");
    uint8_t sam_cmd[] = {PN532_COMMAND_SAMCONFIGURATION, 0x01, 0x14, 0x01};
    write_command(fd, sam_cmd, 4, NULL, 0);
    if (read_ack(fd) == 0) {
        printf("SAM Configured successfully (ACK received).\n");
    }

    close(fd);
    printf("\nTest finished.\n");
    return 0;
}

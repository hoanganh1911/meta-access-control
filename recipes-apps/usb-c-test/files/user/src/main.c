#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define I2C_BUS "/dev/i2c-3"
#define DEVICE_ADDR 0x47

int main() {
    int file;
    char *bus = I2C_BUS;

    if ((file = open(bus, O_RDWR)) < 0) {
        perror("Failed to open the bus");
        return 1;
    }

    if (ioctl(file, I2C_SLAVE, DEVICE_ADDR) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        return 1;
    }

    printf("--- USB-C Status Monitor (HD3SS3220) ---\n");
    printf("Press Ctrl+C to stop.\n\n");

    while (1) {
        // Đọc thanh ghi 0x08 (Connection Status)
        unsigned char reg = 0x08;
        unsigned char status;
        write(file, &reg, 1);
        read(file, &status, 1);

        int conn_state = (status >> 2) & 0x07; // Các bit [4:2]
        
        // Đọc thanh ghi 0x09 (General Status)
        reg = 0x09;
        unsigned char status2;
        write(file, &reg, 1);
        read(file, &status2, 1);
        
        int orientation = status2 & 0x01; // Bit 0

        // Xóa màn hình cũ để cập nhật (ANSI escape code)
        printf("\033[H\033[J");
        printf("=== USB Type-C Real-time Status ===\n");
        
        printf("Cable Direction: %s\n", orientation ? "REVERSED (Side B)" : "NORMAL (Side A)");
        
        printf("Current Mode: ");
        switch (conn_state) {
            case 0: printf("Nothing Attached\n"); break;
            case 1: printf("Attached as SINK (Device mode)\n"); break;
            case 2: printf("Attached as SOURCE (Host mode)\n"); break;
            case 3: printf("Audio Accessory attached\n"); break;
            case 4: printf("Debug Accessory attached\n"); break;
            default: printf("Unknown State\n"); break;
        }
        
        printf("===================================\n");
        usleep(500000); // 0.5s refresh
    }

    close(file);
    return 0;
}

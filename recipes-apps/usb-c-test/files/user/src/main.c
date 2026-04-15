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
    if ((file = open(I2C_BUS, O_RDWR)) < 0) {
        perror("Failed to open the bus");
        return 1;
    }

    if (ioctl(file, I2C_SLAVE, DEVICE_ADDR) < 0) {
        perror("Failed to acquire bus access");
        return 1;
    }

    printf("--- USB-C Status Monitor (HD3SS3220) ---\n");
    
    while (1) {
        unsigned char regs[0x0B];
        for (int i = 0x08; i <= 0x0A; i++) {
            unsigned char reg = i;
            if (write(file, &reg, 1) != 1) {
                perror("I2C Write Error");
                break;
            }
            if (read(file, &regs[i], 1) != 1) {
                perror("I2C Read Error");
                break;
            }
        }

        int conn_state = (regs[0x08] >> 2) & 0x07;
        int orientation = regs[0x09] & 0x01;
        int shutdown = (regs[0x09] >> 5) & 0x01;
        int mode_select = regs[0x0A] & 0x03;

        printf("\033[H\033[J"); // Clear screen
        printf("=== USB Type-C Real-time Status ===\n");
        printf("Raw: 0x08=%02X, 0x09=%02X, 0x0A=%02X\n", regs[0x08], regs[0x09], regs[0x0A]);
        printf("-----------------------------------\n");
        printf("Chip Status: %s\n", shutdown ? "SHUTDOWN (Disabled)" : "ACTIVE");
        printf("Configured Mode: ");
        switch (mode_select) {
            case 0: printf("DRP (Dual Role Port)\n"); break;
            case 1: printf("UFP (Sink/Device)\n"); break;
            case 2: printf("DFP (Source/Host)\n"); break;
            default: printf("DRP (Dual Role Port)\n"); break;
        }

        printf("Cable Direction: %s\n", orientation ? "REVERSED (Side B)" : "NORMAL (Side A)");
        
        printf("Current Mode: ");
        switch (conn_state) {
            case 0: printf("Nothing Attached\n"); break;
            case 1: printf("Attached as SINK (Device mode)\n"); break;
            case 2: printf("Attached as SOURCE (Host mode)\n"); break;
            case 3: printf("Audio Accessory attached\n"); break;
            case 4: printf("Debug Accessory attached\n"); break;
            case 5: printf("Active Cable attached\n"); break;
            default: printf("Unknown State (%d)\n", conn_state); break;
        }
        
        printf("===================================\n");
        usleep(500000);
    }

    close(file);
    return 0;
}

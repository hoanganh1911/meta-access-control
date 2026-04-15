#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define UART_PORT "/dev/ttymxc0"

int setup_uart(int fd) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error from tcgetattr");
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        return -1;
    }
    return 0;
}

void print_hex(unsigned char *buf, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

int main() {
    printf("PN532 UART Test Application\n");
    printf("Opening %s...\n", UART_PORT);

    int fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", UART_PORT, strerror(errno));
        return EXIT_FAILURE;
    }

    if (setup_uart(fd) != 0) {
        close(fd);
        return EXIT_FAILURE;
    }

    // Wake up PN532: Send a long preamble
    unsigned char wakeup[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    printf("Sending wakeup sequence...\n");
    write(fd, wakeup, sizeof(wakeup));
    usleep(100000); // Wait 100ms
    
    // Clear any junk in the buffer
    tcflush(fd, TCIOFLUSH);

    // GetFirmwareVersion command for PN532
    // Frame: 00 00 FF [LEN] [LCS] D4 [CMD] [DATA] [DCS] 00
    // LEN = 2 (D4 02)
    // LCS = 0xFE (0x100 - 0x02)
    // DCS = 0x2A (0x100 - (0xD4+0x02) = 0x100 - 0xD6 = 0x2A)
    unsigned char get_version[] = {0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00};
    
    printf("Sending GetFirmwareVersion command...\n");
    if (write(fd, get_version, sizeof(get_version)) < 0) {
        perror("Failed to write to UART");
        close(fd);
        return EXIT_FAILURE;
    }

    unsigned char buffer[32];
    printf("Waiting for response...\n");
    
    // Read ACK first
    int n = read(fd, buffer, 6);
    if (n > 0) {
        printf("ACK sequence: ");
        print_hex(buffer, n);
    } else {
        printf("No ACK received.\n");
    }

    // Read Data Response
    n = read(fd, buffer, sizeof(buffer));
    if (n > 0) {
        printf("Response received (%d bytes): ", n);
        print_hex(buffer, n);
        
        // Response should start with 00 00 FF ... D5 03
        // If we see D5 03, it means success
        for(int i=0; i < n-1; i++) {
            if (buffer[i] == 0xD5 && buffer[i+1] == 0x03) {
                printf("Successfully identified PN532!\n");
                printf("Firmware Version: IC=0x%02X, Ver=%d, Rev=%d, Support=0x%02X\n", 
                        buffer[i+2], buffer[i+3], buffer[i+4], buffer[i+5]);
                break;
            }
        }
    } else {
        printf("No data response received. Check wiring (TX/RX Swap?) and PN532 power.\n");
    }

    close(fd);
    printf("Test finished.\n");
    return EXIT_SUCCESS;
}

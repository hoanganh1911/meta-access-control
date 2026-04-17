#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ttymxc1" // UART2 on Verdin i.MX8MP
#define BAUD_RATE B9600

/**
 * Configure the serial port
 */
int setup_serial(int fd) {
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    cfsetospeed(&tty, BAUD_RATE);
    cfsetispeed(&tty, BAUD_RATE);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                     // disable break processing
    tty.c_lflag = 0;                            // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                            // no remapping, no delays
    tty.c_cc[VMIN]  = 1;                        // read blocks until 1 character is available
    tty.c_cc[VTIME] = 5;                        // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);     // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);            // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);          // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int main() {
    int fd;
    unsigned char buf[4];
    int bytes_read;

    printf("RCWL-1670 Ultrasonic Sensor Test\n");
    printf("Opening %s at 9600 baud...\n", SERIAL_PORT);

    fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", SERIAL_PORT, strerror(errno));
        return 1;
    }

    if (setup_serial(fd) < 0) {
        close(fd);
        return 1;
    }

    printf("Waiting for data (sending 0x55 trigger)...\n");

    while (1) {
        // Send trigger byte 0x55
        unsigned char trigger = 0x55;
        if (write(fd, &trigger, 1) < 0) {
            perror("write error");
        }
        
        // Wait a bit for the sensor to compute and reply
        usleep(100000); // 100ms

        // Look for the start byte 0xFF
        unsigned char start_byte;
        if (read(fd, &start_byte, 1) > 0) {
            if (start_byte == 0xFF) {
                buf[0] = start_byte;
                
                // Read the next 3 bytes (Data_H, Data_L, Checksum)
                int extra = 0;
                while (extra < 3) {
                    int n = read(fd, &buf[1 + extra], 3 - extra);
                    if (n > 0) {
                        extra += n;
                    } else if (n < 0) {
                        perror("read error");
                        break;
                    }
                }

                if (extra == 3) {
                    unsigned char checksum = (buf[0] + buf[1] + buf[2]) & 0xFF;
                    if (checksum == buf[3]) {
                        int distance = (buf[1] << 8) | buf[2];
                        printf("Distance: %d mm\n", distance);
                    } else {
                        printf("Checksum error: expected 0x%02X, got 0x%02X\n", checksum, buf[3]);
                    }
                }
            }
        }
        
        // Sleep before the next trigger to avoid spamming the sensor
        usleep(400000); // 400ms
    }

    close(fd);
    return 0;
}

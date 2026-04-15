/*
 * PN532 NFC Reader - Linux UART Driver
 * Target: Verdin iMX8MP / /dev/ttymxc0
 *
 * Ported from the Adafruit_PN532 Arduino library (BSD License).
 * Implements: Wakeup, SAMConfig, GetFirmwareVersion, InListPassiveTarget.
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

/* ── UART device ─────────────────────────────────────────────────────────── */
#define UART_PORT   "/dev/ttymxc0"

/* ── PN532 frame constants ───────────────────────────────────────────────── */
#define PN532_PREAMBLE                  (0x00)
#define PN532_STARTCODE1                (0x00)
#define PN532_STARTCODE2                (0xFF)
#define PN532_POSTAMBLE                 (0x00)
#define PN532_HOSTTOPN532               (0xD4)
#define PN532_PN532TOHOST               (0xD5)

/* ── Commands ────────────────────────────────────────────────────────────── */
#define PN532_COMMAND_GETFIRMWAREVERSION    (0x02)
#define PN532_COMMAND_SAMCONFIGURATION      (0x14)
#define PN532_COMMAND_INLISTPASSIVETARGET   (0x4A)

/* ── Baud rate codes ─────────────────────────────────────────────────────── */
#define PN532_MIFARE_ISO14443A              (0x00)

/* ── ACK frame ───────────────────────────────────────────────────────────── */
static const uint8_t PN532_ACK[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

/* ── Timeouts ────────────────────────────────────────────────────────────── */
#define ACK_TIMEOUT_MS      500     /* ms to wait for an ACK frame         */
#define RESP_TIMEOUT_MS     1000    /* ms to wait for a response frame      */
#define CARD_POLL_TIMEOUT   0       /* 0 = block forever in InListPassive   */

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility helpers
 * ═════════════════════════════════════════════════════════════════════════*/

static void print_hex(const char *label, const uint8_t *buf, int len)
{
    printf("%s (%d): ", label, len);
    for (int i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

/** Return monotonic time in milliseconds. */
static uint64_t millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UART setup
 * ═════════════════════════════════════════════════════════════════════════*/

static int setup_uart(int fd)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    /* 8N1, no flow control */
    tty.c_cflag |=  (CLOCAL | CREAD);
    tty.c_cflag &=  ~CSIZE;
    tty.c_cflag |=  CS8;
    tty.c_cflag &=  ~PARENB;
    tty.c_cflag &=  ~CSTOPB;
    tty.c_cflag &=  ~CRTSCTS;

    /* Raw input */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                     PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    /* Non-blocking reads with 100 ms inter-byte timeout */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1; /* 100 ms */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Timed read helper
 *   Reads exactly `count` bytes within `timeout_ms` milliseconds.
 *   Returns number of bytes actually read (< count on timeout).
 * ═════════════════════════════════════════════════════════════════════════*/

static int read_bytes(int fd, uint8_t *buf, int count, uint32_t timeout_ms)
{
    int total = 0;
    uint64_t deadline = millis() + timeout_ms;

    while (total < count) {
        int n = read(fd, buf + total, count - total);
        if (n > 0) {
            total += n;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            break;
        }
        if (millis() > deadline)
            break;
        usleep(1000); /* 1 ms poll interval */
    }
    return total;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Wakeup (HSU / UART mode)
 *   Send 0x55 0x00 0x00 then wait 2 ms — matches Adafruit wakeup() for serial.
 * ═════════════════════════════════════════════════════════════════════════*/

static void pn532_wakeup(int fd)
{
    printf("[PN532] Sending wakeup sequence...\n");

    /* Flush any stale RX data first */
    tcflush(fd, TCIFLUSH);

    uint8_t wake[] = {0x55, 0x00, 0x00};
    if (write(fd, wake, sizeof(wake)) != sizeof(wake)) {
        perror("write wakeup");
    }
    usleep(2000); /* 2 ms — matches Arduino delay(2) */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Write a PN532 command frame (UART / I2C NFC normal frame format)
 *
 *  [PREAMBLE][START1][START2][LEN][LCS][TFI][cmd bytes...][DCS][POSTAMBLE]
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_write_command(int fd, const uint8_t *cmd, uint8_t cmdlen)
{
    uint8_t packet[64];
    int p = 0;

    uint8_t LEN = cmdlen + 1; /* +1 for TFI byte */
    uint8_t LCS = (uint8_t)(~LEN + 1);

    packet[p++] = PN532_PREAMBLE;
    packet[p++] = PN532_STARTCODE1;
    packet[p++] = PN532_STARTCODE2;
    packet[p++] = LEN;
    packet[p++] = LCS;
    packet[p++] = PN532_HOSTTOPN532;

    uint8_t sum = PN532_HOSTTOPN532;
    for (int i = 0; i < cmdlen; i++) {
        packet[p++] = cmd[i];
        sum += cmd[i];
    }

    packet[p++] = (uint8_t)(~sum + 1); /* DCS */
    packet[p++] = PN532_POSTAMBLE;

    print_hex("TX", packet, p);

    if (write(fd, packet, p) != p) {
        perror("write");
        return -1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Read and verify the ACK frame (6 bytes)
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_read_ack(int fd)
{
    uint8_t ack[6];
    int n = read_bytes(fd, ack, 6, ACK_TIMEOUT_MS);
    if (n < 6) {
        printf("[PN532] ACK timeout (%d/6 bytes received)\n", n);
        if (n > 0)
            print_hex("Partial ACK", ack, n);
        return -1;
    }

    print_hex("ACK", ack, 6);

    if (memcmp(ack, PN532_ACK, 6) != 0) {
        printf("[PN532] Invalid ACK frame!\n");
        return -2;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Send command and wait for ACK  (mirrors sendCommandCheckAck)
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_send_command_check_ack(int fd, const uint8_t *cmd, uint8_t cmdlen)
{
    if (pn532_write_command(fd, cmd, cmdlen) != 0)
        return -1;

    usleep(1000); /* 1 ms I2C/UART tuning delay (from Adafruit SLOWDOWN) */

    if (pn532_read_ack(fd) != 0)
        return -1;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Read a response frame from the PN532 into buf[0..n-1]
 *   Returns number of bytes placed in buf, or -1 on error.
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_read_response(int fd, uint8_t *buf, uint8_t maxlen)
{
    /* First read the fixed 6-byte header to get the length */
    uint8_t header[7]; /* preamble + start1 + start2 + LEN + LCS + TFI + CMD+1 */
    int n = read_bytes(fd, header, 6, RESP_TIMEOUT_MS);
    if (n < 6) {
        printf("[PN532] Response header timeout (%d/6 bytes)\n", n);
        return -1;
    }

    /* Validate preamble */
    if (header[0] != 0x00 || header[1] != 0x00 || header[2] != 0xFF) {
        print_hex("Bad preamble", header, 6);
        return -1;
    }

    uint8_t length = header[3];
    /* Read the rest: TFI + data (length-1 bytes) + DCS + postamble */
    uint8_t body_len = length + 1; /* length bytes of data + DCS */
    if (body_len > maxlen) body_len = maxlen;

    n = read_bytes(fd, buf, body_len, RESP_TIMEOUT_MS);
    print_hex("RX body", buf, n);
    return n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SAM Configuration — Normal Mode
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_SAMConfig(int fd)
{
    printf("[PN532] SAMConfig...\n");

    uint8_t cmd[] = {
        PN532_COMMAND_SAMCONFIGURATION,
        0x01, /* Normal mode */
        0x14, /* Timeout: 50ms * 20 = 1 s */
        0x01  /* Use IRQ pin (doesn't matter for UART) */
    };

    if (pn532_send_command_check_ack(fd, cmd, sizeof(cmd)) != 0) {
        printf("[PN532] SAMConfig: No ACK\n");
        return -1;
    }

    uint8_t resp[16];
    int n = pn532_read_response(fd, resp, sizeof(resp));
    if (n < 2) {
        printf("[PN532] SAMConfig: short response\n");
        return -1;
    }

    /* Expected: TFI(D5) + CMD+1(0x15) at resp[0..1] */
    if (resp[0] == PN532_PN532TOHOST && resp[1] == 0x15) {
        printf("[PN532] SAMConfig OK\n");
        return 0;
    }

    printf("[PN532] SAMConfig unexpected response\n");
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Get Firmware Version
 * ═════════════════════════════════════════════════════════════════════════*/

static uint32_t pn532_getFirmwareVersion(int fd)
{
    printf("[PN532] GetFirmwareVersion...\n");

    uint8_t cmd[] = {PN532_COMMAND_GETFIRMWAREVERSION};

    if (pn532_send_command_check_ack(fd, cmd, sizeof(cmd)) != 0) {
        printf("[PN532] GetFirmwareVersion: No ACK\n");
        return 0;
    }

    uint8_t resp[16];
    int n = pn532_read_response(fd, resp, sizeof(resp));
    if (n < 6) {
        printf("[PN532] GetFirmwareVersion: short response (%d bytes)\n", n);
        return 0;
    }

    /* resp[0]=D5, resp[1]=0x03, resp[2]=IC, resp[3]=Ver, resp[4]=Rev, resp[5]=Support */
    if (resp[0] != PN532_PN532TOHOST || resp[1] != 0x03) {
        printf("[PN532] GetFirmwareVersion: unexpected TFI/CMD\n");
        return 0;
    }

    uint32_t version =  ((uint32_t)resp[2] << 24) |
                        ((uint32_t)resp[3] << 16) |
                        ((uint32_t)resp[4] <<  8) |
                         (uint32_t)resp[5];

    printf("[PN532] IC: 0x%02X  Ver: %d.%d  Support: 0x%02X\n",
           resp[2], resp[3], resp[4], resp[5]);

    return version;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Read Passive Target ID  (ISO14443A — Mifare / NFC cards)
 *   uid[]      : output buffer (up to 7 bytes)
 *   uidLength  : length of UID found
 *   Returns 1 on card found, 0 otherwise.
 * ═════════════════════════════════════════════════════════════════════════*/

static int pn532_readPassiveTargetID(int fd, uint8_t *uid, uint8_t *uidLength)
{
    uint8_t cmd[] = {
        PN532_COMMAND_INLISTPASSIVETARGET,
        0x01,                       /* Max 1 card at a time */
        PN532_MIFARE_ISO14443A      /* Baud rate: ISO14443A */
    };

    if (pn532_send_command_check_ack(fd, cmd, sizeof(cmd)) != 0) {
        printf("[PN532] InListPassiveTarget: No ACK\n");
        return 0;
    }

    /* Wait for card — use a long timeout */
    uint8_t resp[32];
    int n = read_bytes(fd, resp, sizeof(resp), 5000 /* 5s card timeout */);
    if (n < 1) {
        printf("[PN532] InListPassiveTarget: timeout waiting for card\n");
        return 0;
    }
    print_hex("Card RX", resp, n);

    /* Find the PN532-to-host frame: 00 00 FF LEN LCS D5 4B ... */
    int offset = -1;
    for (int i = 0; i <= n - 3; i++) {
        if (resp[i] == 0x00 && resp[i+1] == 0x00 && resp[i+2] == 0xFF) {
            offset = i;
            break;
        }
    }
    if (offset < 0) {
        printf("[PN532] No valid frame found in card response\n");
        return 0;
    }

    /* Byte layout (from offset):
       [0]=00 [1]=00 [2]=FF [3]=LEN [4]=LCS [5]=D5 [6]=4B
       [7]=NumTags [8]=TagNum [9..10]=SENS_RES [11]=SEL_RES
       [12]=NfcIDLen [13..]=NfcID                              */

    int b = offset + 7; /* Tags Found */
    if (b >= n || resp[b] != 1) {
        printf("[PN532] No tags found (resp[%d]=0x%02X)\n", b, (b < n ? resp[b] : 0xFF));
        return 0;
    }

    int lenIdx = offset + 12;
    if (lenIdx >= n) return 0;

    uint8_t nfcidLen = resp[lenIdx];
    if (nfcidLen > 7) nfcidLen = 7;

    *uidLength = nfcidLen;
    for (int i = 0; i < nfcidLen && (lenIdx + 1 + i) < n; i++)
        uid[i] = resp[lenIdx + 1 + i];

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═════════════════════════════════════════════════════════════════════════*/

int main(void)
{
    printf("=== PN532 NFC Reader (Linux UART) ===\n");

    /* 1. Open UART */
    int fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open UART");
        return 1;
    }

    if (setup_uart(fd) != 0) {
        fprintf(stderr, "UART setup failed\n");
        close(fd);
        return 1;
    }

    /* 2. Wakeup */
    pn532_wakeup(fd);

    /* 3. SAMConfig (also completes wakeup in UART mode) */
    if (pn532_SAMConfig(fd) != 0) {
        fprintf(stderr, "SAMConfig failed — check wiring and UART port.\n");
        close(fd);
        return 1;
    }

    /* 4. Firmware version */
    uint32_t ver = pn532_getFirmwareVersion(fd);
    if (ver == 0) {
        fprintf(stderr, "GetFirmwareVersion failed — PN532 not responding.\n");
        close(fd);
        return 1;
    }

    printf("\n[PN532] Module ready. Waiting for NFC cards...\n");
    printf("        Press Ctrl+C to exit.\n\n");

    /* 5. Card polling loop */
    while (1) {
        uint8_t uid[7];
        uint8_t uidLen = 0;

        if (pn532_readPassiveTargetID(fd, uid, &uidLen)) {
            printf("[CARD DETECTED] UID (%d bytes): ", uidLen);
            for (int i = 0; i < uidLen; i++)
                printf("%02X%s", uid[i], (i < uidLen - 1) ? ":" : "");
            printf("\n\n");

            /* Small debounce — wait until card is removed */
            usleep(500000); /* 500 ms */
        } else {
            /* Brief pause before next poll */
            usleep(100000); /* 100 ms */
        }
    }

    close(fd);
    return 0;
}

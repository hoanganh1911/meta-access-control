#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cryptoauthlib/cryptoauthlib.h>

int main() {
    ATCA_STATUS status;
    uint8_t serial_number[9];
    bool is_locked = false;

    printf("ATECC608A Test Application (using CryptoAuthLib)\n");

    /* 
     * Configuration for I2C4 on Verdin i.MX8MP
     * Bus 3 corresponds to &i2c4 if i2c1, i2c2, i2c3 are also enabled.
     * Slave address 0xC0 is 0x60 << 1 (8-bit format required by HAL)
     */
    ATCAIfaceCfg cfg = {
        .iface_type = ATCA_I2C_IFACE,
        .devtype = ATECC608A,
        .atcai2c.address = 0xC0,      // Try 8-bit first
        .atcai2c.bus = 3,              // Back to bus 3
        .atcai2c.baud = 100000,        // Lower baud for better wake-up success
        .wake_delay = 1500,
        .rx_retries = 20
    };

    printf("Attempting to communicate with ATECC608A on I2C bus %d (8-bit addr 0x%02X)...\n", 
            cfg.atcai2c.bus, cfg.atcai2c.address);
    
    status = atcab_init(&cfg);
    if (status != ATCA_SUCCESS) {
        printf("atcab_init failed (attempt 1): 0x%02X. Trying 7-bit address 0x60...\n", status);
        cfg.atcai2c.address = 0x60;
        status = atcab_init(&cfg);
    }
    if (status != ATCA_SUCCESS) {
        fprintf(stderr, "atcab_init failed: 0x%02X. Please check wiring and bus number.\n", status);
        return EXIT_FAILURE;
    }

    printf("Waking up device...\n");
    status = atcab_wakeup();
    if (status != ATCA_SUCCESS) {
        fprintf(stderr, "atcab_wakeup failed: 0x%02X\n", status);
        atcab_release();
        return EXIT_FAILURE;
    }

    printf("Reading Serial Number...\n");
    status = atcab_read_serial_number(serial_number);
    if (status == ATCA_SUCCESS) {
        printf("Serial Number: ");
        for (int i = 0; i < 9; i++) {
            printf("%02X", serial_number[i]);
        }
        printf("\n");
    } else {
        fprintf(stderr, "Failed to read serial number: 0x%02X\n", status);
    }

    /* Check config lock status */
    status = atcab_is_locked(LOCK_ZONE_CONFIG, &is_locked);
    if (status == ATCA_SUCCESS) {
        printf("Config Zone Locked: %s\n", is_locked ? "Yes" : "No");
    }

    /* Check data lock status */
    status = atcab_is_locked(LOCK_ZONE_DATA, &is_locked);
    if (status == ATCA_SUCCESS) {
        printf("Data Zone Locked: %s\n", is_locked ? "Yes" : "No");
    }

    atcab_release();
    printf("Test finished.\n");
    return EXIT_SUCCESS;
}

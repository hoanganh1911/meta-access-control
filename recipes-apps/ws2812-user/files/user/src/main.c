#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define CHIP_PATH "/dev/gpiochip0" // GPIO1
#define LINE_OFFSET 8              // GPIO1_IO08
#define NUM_LEDS 7

void send_byte(struct gpiod_line_request *request, unsigned int offset, unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE);
            for(int j=0; j<40; j++) __asm__("nop"); 
            gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE);
            for(int j=0; j<20; j++) __asm__("nop");
        } else {
            gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE);
            for(int j=0; j<15; j++) __asm__("nop");
            gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE);
            for(int j=0; j<50; j++) __asm__("nop");
        }
    }
}

int main(int argc, char **argv) {
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_line_request *request;
    unsigned int offset = LINE_OFFSET;
    
    printf("WS2812 LED Strip Test v2 (SODIMM 218 / GPIO1_IO08)\n");

    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        fprintf(stderr, "Failed to open chip %s: %s\n", CHIP_PATH, strerror(errno));
        return 1;
    }

    settings = gpiod_line_settings_new();
    if (!settings) {
        perror("Failed to create settings");
        goto close_chip;
    }
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        perror("Failed to create line config");
        goto free_settings;
    }

    if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0) {
        perror("Failed to add line settings");
        goto free_line_cfg;
    }

    request = gpiod_chip_request_lines(chip, NULL, line_cfg);
    if (!request) {
        perror("Failed to request lines");
        goto free_line_cfg;
    }

    unsigned char r = 0, g = 0, b = 0;
    if (argc >= 4) {
        r = (unsigned char)atoi(argv[1]);
        g = (unsigned char)atoi(argv[2]);
        b = (unsigned char)atoi(argv[3]);
    } else {
        printf("Usage: %s <R> <G> <B>\n", argv[0]);
        printf("Setting default color: Red\n");
        r = 50;
    }

    printf("Setting %d LEDs to RGB(%d, %d, %d)...\n", NUM_LEDS, r, g, b);

    // WS2812 uses GRB order
    for (int i = 0; i < NUM_LEDS; i++) {
        send_byte(request, offset, g);
        send_byte(request, offset, r);
        send_byte(request, offset, b);
    }

    // Reset pulse (> 50us)
    gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE);
    usleep(100);

    gpiod_line_request_release(request);
free_line_cfg:
    gpiod_line_config_free(line_cfg);
free_settings:
    gpiod_line_settings_free(settings);
close_chip:
    gpiod_chip_close(chip);
    
    printf("Done.\n");
    return 0;
}

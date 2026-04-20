#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

#define GPIO_CHIP       "/dev/gpiochip4"
#define GPIO_TRIG       25
#define GPIO_ECHO       24

#define TANK_HEIGHT_CM  20.0f
#define MIN_DISTANCE_CM  1.0f
#define SAMPLE_COUNT    30
#define TIMEOUT_US      30000
#define SAMPLE_DELAY_MS 10
#define LOOP_DELAY_MS   500

static long timespec_diff_us(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000L +
           (end->tv_nsec - start->tv_nsec) / 1000L;
}

static long measure_once(struct gpiod_line_request *trig_req,
                         struct gpiod_line_request *echo_req)
{
    struct timespec ts_start, ts_now;

    gpiod_line_request_set_value(trig_req, GPIO_TRIG, GPIOD_LINE_VALUE_INACTIVE);
    usleep(2);
    gpiod_line_request_set_value(trig_req, GPIO_TRIG, GPIOD_LINE_VALUE_ACTIVE);
    usleep(10);
    gpiod_line_request_set_value(trig_req, GPIO_TRIG, GPIOD_LINE_VALUE_INACTIVE);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (gpiod_line_request_get_value(echo_req, GPIO_ECHO) == GPIOD_LINE_VALUE_INACTIVE) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if (timespec_diff_us(&ts_start, &ts_now) > TIMEOUT_US)
            return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (gpiod_line_request_get_value(echo_req, GPIO_ECHO) == GPIOD_LINE_VALUE_ACTIVE) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if (timespec_diff_us(&ts_start, &ts_now) > TIMEOUT_US)
            return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_now);

    return timespec_diff_us(&ts_start, &ts_now);
}

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *trig_req, *echo_req;
    unsigned int offset;
    int i, valid;
    long duration_us, sum_us;
    float distance_cm, water_level;

    printf("RCWL-1670 Water Level Monitor\n");

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        fprintf(stderr, "gpiod_request_config_new failed\n");
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "rcwl1670");

    /* --- setup TRIG as output --- */
    settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    line_cfg = gpiod_line_config_new();
    offset = GPIO_TRIG;
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

    trig_req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!trig_req) {
        perror("request trig");
        gpiod_request_config_free(req_cfg);
        gpiod_chip_close(chip);
        return 1;
    }

    /* --- setup ECHO as input --- */
    settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    line_cfg = gpiod_line_config_new();
    offset = GPIO_ECHO;
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

    echo_req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_request_config_free(req_cfg);

    if (!echo_req) {
        perror("request echo");
        gpiod_line_request_release(trig_req);
        gpiod_chip_close(chip);
        return 1;
    }

    while (1) {
        sum_us = 0;
        valid = 0;

        for (i = 0; i < SAMPLE_COUNT; i++) {
            duration_us = measure_once(trig_req, echo_req);
            if (duration_us > 0) {
                sum_us += duration_us;
                valid++;
            }
            usleep(SAMPLE_DELAY_MS * 1000);
        }

        printf("-------------------------\n");

        if (valid == 0) {
            printf("No valid samples received\n");
            usleep(LOOP_DELAY_MS * 1000);
            continue;
        }

        distance_cm = (sum_us / (float)valid) * 0.0343f / 2.0f;

        if (distance_cm < MIN_DISTANCE_CM)
            distance_cm = MIN_DISTANCE_CM;

        water_level = ((TANK_HEIGHT_CM - distance_cm) / TANK_HEIGHT_CM) * 100.0f;
        if (water_level < 0.0f)   water_level = 0.0f;
        if (water_level > 100.0f) water_level = 100.0f;

        printf("Distance:    %.2f cm  (%d/%d samples)\n", distance_cm, valid, SAMPLE_COUNT);
        printf("Water Level: %.1f %%\n", water_level);

        usleep(LOOP_DELAY_MS * 1000);
    }

    gpiod_line_request_release(trig_req);
    gpiod_line_request_release(echo_req);
    gpiod_chip_close(chip);
    return 0;
}

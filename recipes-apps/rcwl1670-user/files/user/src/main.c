#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

#define GPIO_CHIP       "gpiochip4"
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

static long measure_once(struct gpiod_line *trig, struct gpiod_line *echo)
{
    struct timespec ts_start, ts_now;

    gpiod_line_set_value(trig, 0);
    usleep(2);
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (gpiod_line_get_value(echo) == 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if (timespec_diff_us(&ts_start, &ts_now) > TIMEOUT_US)
            return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (gpiod_line_get_value(echo) == 1) {
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
    struct gpiod_line *trig, *echo;
    int i, valid;
    long duration_us, sum_us;
    float distance_cm, water_level;

    printf("RCWL-1670 Water Level Monitor\n");

    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    trig = gpiod_chip_get_line(chip, GPIO_TRIG);
    echo = gpiod_chip_get_line(chip, GPIO_ECHO);
    if (!trig || !echo) {
        fprintf(stderr, "Failed to get GPIO lines\n");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(trig, "rcwl1670-trig", 0) < 0) {
        perror("request trig output");
        gpiod_chip_close(chip);
        return 1;
    }
    if (gpiod_line_request_input(echo, "rcwl1670-echo") < 0) {
        perror("request echo input");
        gpiod_chip_close(chip);
        return 1;
    }

    while (1) {
        sum_us = 0;
        valid = 0;

        for (i = 0; i < SAMPLE_COUNT; i++) {
            duration_us = measure_once(trig, echo);
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

    gpiod_line_release(trig);
    gpiod_line_release(echo);
    gpiod_chip_close(chip);
    return 0;
}

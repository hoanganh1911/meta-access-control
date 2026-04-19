#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSFS_PATH "/sys/bus/platform/devices/ws2812/colors"

int main(int argc, char **argv) {
    int r = 0, g = 0, b = 0;

    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    } else {
        printf("Usage: %s <R> <G> <B>\n", argv[0]);
        printf("Setting default color: Red\n");
        r = 50;
    }

    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    FILE *f = fopen(SYSFS_PATH, "w");
    if (!f) {
        perror("Failed to open " SYSFS_PATH);
        return 1;
    }

    fprintf(f, "%d %d %d\n", r, g, b);
    fclose(f);

    printf("Set RGB(%d, %d, %d)\n", r, g, b);
    return 0;
}

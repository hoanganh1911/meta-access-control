#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vl53l5cx_api.h"

#define VALID_STATUS 5
#define GRID_SIZE    4

static void print_distance_grid(VL53L5CX_ResultsData *r)
{
	int row, col, zone;

	printf("\n  Distance map (mm) — '-' = invalid\n");
	printf("  +--------+--------+--------+--------+\n");

	for (row = 0; row < GRID_SIZE; row++) {
		printf("  |");
		for (col = 0; col < GRID_SIZE; col++) {
			zone = row * GRID_SIZE + col;
			if (r->target_status[VL53L5CX_NB_TARGET_PER_ZONE * zone] == VALID_STATUS)
				printf(" %6d |", r->distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * zone]);
			else
				printf("      - |");
		}
		printf("\n  +--------+--------+--------+--------+\n");
	}
	printf("\n");
}

int example1(VL53L5CX_Configuration *p_dev)
{
	uint8_t status, loop, isAlive, isReady;
	VL53L5CX_ResultsData Results;

	status = vl53l5cx_is_alive(p_dev, &isAlive);
	if (!isAlive || status) {
		printf("VL53L5CX not detected\n");
		return status;
	}

	status = vl53l5cx_init(p_dev);
	if (status) {
		printf("VL53L5CX init failed\n");
		return status;
	}

	printf("VL53L5CX ready (version: %s)\n", VL53L5CX_API_REVISION);
	printf("Capturing 10 frames — 4x4 zone grid, unit: mm\n");

	status = vl53l5cx_start_ranging(p_dev);

	loop = 0;
	while (loop < 10) {
		status = vl53l5cx_check_data_ready(p_dev, &isReady);
		if (isReady) {
			vl53l5cx_get_ranging_data(p_dev, &Results);
			printf("Frame #%u", p_dev->streamcount);
			print_distance_grid(&Results);
			loop++;
		}
		WaitMs(5);
	}

	vl53l5cx_stop_ranging(p_dev);
	printf("Done.\n");
	return status;
}

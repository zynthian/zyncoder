/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: libgpiod callbacks
 * 
 * Implements a callback mechanism for libgpiod
 * 
 * Copyright (C) 2015-2024 Fernando Moyano <jofemodo@zynthian.org>
 *
 * ******************************************************************
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE.txt file.
 * 
 * ******************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gpiod.h>
#include <pthread.h>

#include "gpiod_callback.h"

//-------------------------------------------------------------------
// Variables
//-------------------------------------------------------------------

// GPIO Chip Data Structure
struct gpiod_chip *gpio_chip = NULL;

// Array of callback structures
struct gpiod_callback rpi_gpiod_callbacks[NUM_GPIO_PINS];

// Bulk structure for callback lines
struct gpiod_line_bulk cb_line_bulk;

int end_callback_thread_flag = 0;
pthread_t callback_thread_tid;

//-------------------------------------------------------------------
// Pin number conversion arrays
//-------------------------------------------------------------------

// WiringPi => GPIO number (BCM)
int8_t wpi2gpio[32] = {
	17, // 0
	18, // 1
	27, // 2
	22, // 3
	23, // 4
	24, // 5
	25, // 6
	4,  // 7
	2,  // 8
	3,  // 9
	8,  // 10
	7,  // 11
	10, // 12
	9,  // 13
	11, // 14
	14, // 15
	15, // 16
	-1, // 17
	-1, // 18
	-1, // 19
	-1, // 20
	5,  // 21
	6,  // 22
	13, // 23
	19, // 24
	26, // 25
	12, // 26
	16, // 27
	20, // 28
	21, // 29
	0,  // 30
	1   // 31
};

// GPIO number (BCM) => WiringPi
int8_t gpio2wpi[28] = {
	30, // 0
	31, // 1
	8,  // 2
	9,  // 3
	7,  // 4
	21, // 5
	22, // 6
	11, // 7
	10, // 8
	13, // 9
	12, // 10
	14, // 11
	26, // 12
	23, // 13
	15, // 14
	16, // 15
	27, // 16
	0,  // 17
	1,  // 18
	24, // 19
	28, // 20
	29, // 21
	3,  // 22
	4,  // 23
	5,  // 24
	6,  // 25
	25, // 26
	2   // 27
};

//-------------------------------------------------------------------
// Initialization & functions
//-------------------------------------------------------------------

int gpiod_init_callbacks() {
	int i;

	// Initialize GPIOD callback data structures
	for (i=0; i<NUM_GPIO_PINS; i++) {
		rpi_gpiod_callbacks[i].pin = -1;
		rpi_gpiod_callbacks[i].line = NULL;
		rpi_gpiod_callbacks[i].callback = NULL;
	}

	// Determine the GPIO chip to use and initialize it
	char * gpio_chip_device = getenv ("GPIO_CHIP_DEVICE");
	if (!gpio_chip_device) gpio_chip_device = DEFAULT_GPIO_CHIP_DEVICE;
	gpio_chip = gpiod_chip_open(gpio_chip_device);
	if (!gpio_chip) {
		fprintf(stderr, "ZynCore->gpiod_init_callbacks(): Can't open RPI's GPIO chip: %s\n", gpio_chip_device);
		gpio_chip = NULL;
		return 0;
	}
	return 1;
}

int gpiod_line_register_callback(struct gpiod_line *line, void (*callback)(void)) {
	if (line) {
		int pin = gpiod_line_offset(line);
		rpi_gpiod_callbacks[pin].pin = pin;
		rpi_gpiod_callbacks[pin].line = line;
		rpi_gpiod_callbacks[pin].callback = callback;
		//fprintf(stderr, "ZynCore->gpiod_line_register_callback(): Registered callback on pin %d\n", pin);
		return 1;
	}
	return 0;
}

int gpiod_line_unregister_callback(struct gpiod_line *line) {
	if (line) {
		int pin = gpiod_line_offset(line);
		rpi_gpiod_callbacks[pin].pin = -1;
		rpi_gpiod_callbacks[pin].line = NULL;
		rpi_gpiod_callbacks[pin].callback = NULL;
		return 1;
	}
	return 0;
}

void * gpiod_callbacks_thread(void *arg) {
	end_callback_thread_flag = 0;
	struct timespec ts = { 1, 0 };
	struct gpiod_line_bulk event_bulk;
	struct gpiod_line *line;
	struct gpiod_line_event event;
	int ret = 0;
	int pin;
	int i;
	while (!end_callback_thread_flag) {
		ret = gpiod_line_event_wait_bulk(&cb_line_bulk, &ts, &event_bulk);
		if (ret > 0) {
			for (i=0; i<event_bulk.num_lines; i++) {
				line = event_bulk.lines[i];
				gpiod_line_event_read(line, &event);
				pin = gpiod_line_offset(line);
				//if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
				//	fprintf(stderr,"ZynCore->gpiod_callback_thread(): Got event on pin '%d'!\n", pin);
				rpi_gpiod_callbacks[pin].callback();
			}
		} else if (ret < 0) {
			fprintf(stderr, "ZynCore->gpiod_callback_thread(): Error while processing GPIO events!\n");
			break;
		} else {
			//fprintf(stderr, "ZynCore->gpiod_callback_thread(): Event loop timeout...\n");
		}
	}
	fprintf(stderr, "ZynCore->gpiod_callback_thread(): Finished succesfully\n");
}

int gpiod_start_callbacks() {
	int i, count;
	gpiod_line_bulk_init(&cb_line_bulk);
	for (i=0, count=0; i<NUM_GPIO_PINS; i++) {
		struct gpiod_line *line = rpi_gpiod_callbacks[i].line;
		if (line) {
			gpiod_line_bulk_add(&cb_line_bulk, line);
			count++;
		}
	}
	if (count > 0) {
		// Start callback thread
		int err = pthread_create(&callback_thread_tid, NULL, &gpiod_callbacks_thread, NULL);
		if (err != 0) {
			fprintf(stderr, "ZynCore->gpiod_start_callbacks: Can't create callback thread :[%s]", strerror(err));
			return 0;
		} else {
			fprintf(stderr, "ZynCore->gpiod_start_callbacks: Callback thread created successfully\n");
			return 1;
		}
	}
	return 0;
}

int gpiod_stop_callbacks() {
	end_callback_thread_flag = 1;
	return 1;
}

int gpiod_restart_callbacks() {
	gpiod_stop_callbacks();
	return gpiod_start_callbacks();
}

//-------------------------------------------------------------------
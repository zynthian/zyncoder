/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Headphones Volume Control for LM4811 amplifier
 * 
 * Library for interfacing the LM4811 headphones amplifier.
 * It implements the volume control using 2 GPIO pins:
 *   - AMP_VOL => 1=Up / 0=Down
 *   - AMP_CLK => Volume step control
 *
 * Copyright (C) 2015-2021 Fernando Moyano <jofemodo@zynthian.org>
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

#include "gpiod.h"
#include "gpiod_callback.h"
#include "lm4811.h"

//-------------------------------------------------------------------

//#define DEBUG

#if Z2_VERSION==1
	#define PIN_AMP_CLK 11  // wiringPi 14
	#define PIN_AMP_VOL 12  // wiringPi 26
#else
	#define PIN_AMP_CLK 4   // wiringPi 7
	#define PIN_AMP_VOL 16  // wiringPi 27
#endif

#define AMP_MAX_VOL 15
#define STEP_USECS 100

struct gpiod_line *line_clk;
struct gpiod_line *line_vol;
uint8_t current_volume = 0;

//-------------------------------------------------------------------

void lm4811_volume_steps(int n) {
	if (n==0) return;
	#ifdef DEBUG
	fprintf(stdout, "Sending %d volume steps to LM4811...\n", n);
	#endif
	if (n>0) {
		gpiod_line_set_value(line_vol, 1);
	} else {
		gpiod_line_set_value(line_vol, 0);
		n = -n;
	}
	int i;
	for (i=0;i<n;i++) {
		gpiod_line_set_value(line_clk, 1);
		usleep(STEP_USECS);
		gpiod_line_set_value(line_clk, 0);
		usleep(STEP_USECS);
	}
}

void lm4811_reset_volume() {
	lm4811_volume_steps(-AMP_MAX_VOL);
	current_volume = 0;
}

uint8_t lm4811_set_volume(uint8_t vol) {
	if (vol>AMP_MAX_VOL) vol = AMP_MAX_VOL;
	int n_steps = vol - current_volume;
	if (n_steps!=0) {
		lm4811_volume_steps(n_steps);
		current_volume = vol;
	}
	return current_volume;
}

uint8_t lm4811_get_volume() {
	return current_volume;
}

uint8_t lm4811_get_volume_max() {
	return AMP_MAX_VOL;
}

int lm4811_init() {
	line_clk = gpiod_chip_get_line(gpio_chip, PIN_AMP_CLK);
	line_vol = gpiod_chip_get_line(gpio_chip, PIN_AMP_VOL);
	if (!line_clk || !line_vol) {
		fprintf(stderr, "ZynCore->lm4811_init(): Can't get lines for lm4811\n");
		return 0;
	}
	if (gpiod_line_request_output(line_clk, ZYNCORE_CONSUMER, 0) < 0) {
		fprintf(stderr, "ZynCore->lm4811_init(): Can't request CLK output for lm4811\n");
		return 0;
	}
	if (gpiod_line_request_output(line_vol, ZYNCORE_CONSUMER, 0) < 0) {
		fprintf(stderr, "ZynCore->lm4811_init(): Can't request VOL output for lm4811\n");
		return 0;
	}
	usleep(STEP_USECS);
	lm4811_reset_volume();
	lm4811_set_volume(10);
}

int lm4811_end() {
	lm4811_reset_volume();
	return 1;
}

//-------------------------------------------------------------------

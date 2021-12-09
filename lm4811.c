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

#include <wiringPi.h>
#include "lm4811.h"

//-------------------------------------------------------------------

//#define DEBUG

#if Z2_VERSION==1
	#define PIN_AMP_CLK 14
	#define PIN_AMP_VOL 26
#else
	#define PIN_AMP_CLK 7
	#define PIN_AMP_VOL 27
#endif

#define AMP_MAX_VOL 15
#define STEP_USECS 100

uint8_t current_volume = 0;

//-------------------------------------------------------------------

void lm4811_volume_steps(int n) {
	if (n==0) return;
	#ifdef DEBUG
	fprintf(stdout, "Sending %d volume steps to LM4811...\n", n);
	#endif
	if (n>0) {
		digitalWrite(PIN_AMP_VOL, 1);
	} else {
		digitalWrite(PIN_AMP_VOL, 0);
		n = -n;
	}
	int i;
	for (i=0;i<n;i++) {
		digitalWrite(PIN_AMP_CLK,1);
		usleep(STEP_USECS);
		digitalWrite(PIN_AMP_CLK,0);
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

void lm4811_init() {
	pinMode(PIN_AMP_CLK, OUTPUT);
	pinMode(PIN_AMP_VOL, OUTPUT);
	digitalWrite(PIN_AMP_VOL,0);
	digitalWrite(PIN_AMP_CLK,0);
	usleep(STEP_USECS);
	lm4811_reset_volume();
}

void lm4811_end() {
	lm4811_reset_volume();
}

//-------------------------------------------------------------------

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncontrol Library for Zynthian Kits V1-V4
 * 
 * Initialize & configure control hardware for Zynthian Kits V1-V4
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wiringPi.h>

#include "zynmcp23008.h"
#include "zynpot.h"
#include "zyncoder.h"

#ifdef ZYNAPTIK_CONFIG
#include "zynaptik.h"
#endif

#ifdef ZYNTOF_CONFIG
#include "zyntof.h"
#endif

//-----------------------------------------------------------------------------
// WiringPi to BCM GPIO tables
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// GPIO Expander
//-----------------------------------------------------------------------------

#if defined(MCP23017_ENCODERS)
	// pins 100-115 are located on the MCP23017
	#define MCP23017_BASE_PIN 100
	// default I2C Address for MCP23017
	#if !defined(MCP23017_I2C_ADDRESS)
		#define MCP23017_I2C_ADDRESS 0x20
	#endif
	// default interrupt pins for the MCP23017
	#if !defined(MCP23017_INTA_PIN)
		#define MCP23017_INTA_PIN 27
	#endif
	#if !defined(MCP23017_INTB_PIN)
		#define MCP23017_INTB_PIN 25
	#endif
#elif defined(MCP23008_ENCODERS)
	// pins 100-107 are located on the MCP23008
	#define MCP23008_BASE_PIN 100
	#define MCP23008_I2C_ADDRESS 0x20
#endif

#if defined(MCP23017_ENCODERS)

// two ISR routines for the two banks
void zynmcp23017_ISR_bankA() {
	zynmcp23017_ISR(0, 0);
}
void zynmcp23017_ISR_bankB() {
	zynmcp23017_ISR(0, 1);
}
void (*zynmcp23017_ISRs[2]) = {
	zynmcp23017_ISR_bankA,
	zynmcp23017_ISR_bankB
};

void init_zynmcp23017s() {
	reset_zynmcp23017s();
	setup_zynmcp23017(0, MCP23017_BASE_PIN, MCP23017_I2C_ADDRESS, wpi2gpio[MCP23017_INTA_PIN], wpi2gpio[MCP23017_INTB_PIN], zynmcp23017_ISRs);
}

#endif


#if defined(MCP23008_ENCODERS)

void init_zynmcp23008s() {
	reset_zynmcp23008s();
	setup_zynmcp23008(0, MCP23008_BASE_PIN, MCP23008_I2C_ADDRESS);
}

#endif


//-----------------------------------------------------------------------------
// Get wiring config from environment
//-----------------------------------------------------------------------------

#define NUM_ZYNSWITCHES 36
#define NUM_ZYNPOTS 4

int16_t zynswitch_pins[NUM_ZYNSWITCHES];
int16_t zyncoder_pins_a[NUM_ZYNPOTS];
int16_t zyncoder_pins_b[NUM_ZYNPOTS];

extern uint16_t num_zynswitches;

void reset_wiring_config() {
	int16_t i;
	for (i=0;i<NUM_ZYNSWITCHES;i++) {
		zynswitch_pins[i] = -1;
	}
	for (i=0;i<NUM_ZYNPOTS;i++) {
		zyncoder_pins_a[i] = -1;
		zyncoder_pins_b[i] = -1;
	}
}

uint16_t parse_envar2intarr(const char *envar_name, int16_t *result, int16_t limit) {
	uint16_t i = 0;
	const char *envar_ptr = getenv(envar_name);
	if (envar_ptr) {
		char envar_cpy[128];
		char *save_ptr;
		int16_t res;
		strcpy(envar_cpy, envar_ptr);
		char *token = strtok_r(envar_cpy, ",", &save_ptr);
		while (token!=NULL && i<limit) {
			res = atoi(token);
			// Convert low pins (RPi pins) from wiringPi to GPIO numbers
			if (res >= 0 && res < 100) res = wpi2gpio[res];
			result[i++]  = res;
			token = strtok_r(NULL, ",", &save_ptr);
		}
	}
	return i;
}

void get_wiring_config() {
	reset_wiring_config();
	num_zynswitches = parse_envar2intarr("ZYNTHIAN_WIRING_SWITCHES", zynswitch_pins, NUM_ZYNSWITCHES);
	fprintf(stderr, "ZynCore: Configured %d x Logical Zynswitches...\n", num_zynswitches);
	parse_envar2intarr("ZYNTHIAN_WIRING_ENCODER_A", zyncoder_pins_a, NUM_ZYNPOTS);
	parse_envar2intarr("ZYNTHIAN_WIRING_ENCODER_B", zyncoder_pins_b, NUM_ZYNPOTS);
}

//-----------------------------------------------------------------------------
// 8 x ZynSwitches
//-----------------------------------------------------------------------------

void init_zynswitches() {
	reset_zynswitches();
	int16_t i, count;
	for (i=0, count=0; i<NUM_ZYNSWITCHES; i++) {
		if (zynswitch_pins[i] >= 0) {
			//fprintf(stderr, "ZynCore: Setting-up zynswitch in pin %d...\n", zynswitch_pins[i]);
			setup_zynswitch(i, zynswitch_pins[i], 1);
			count++;
		}
	}
	fprintf(stderr, "ZynCore: Setting-up %d x Physical Zynswitches...\n", count);
}

//-----------------------------------------------------------------------------
// 4 x Zynpots (zyncoder => Incremental encoder)
//-----------------------------------------------------------------------------

void init_zynpots() {
	reset_zynpots();
	reset_zyncoders();
	int16_t i, count;
	for (i=0, count=0; i<NUM_ZYNPOTS; i++) {
		if (zyncoder_pins_a[i] >= 0 && zyncoder_pins_b[i] >= 0) {
			//fprintf(stderr, "ZynCore: Setting-up zyncoder in pins (%d, %d)...\n", zyncoder_pins_a[i], zyncoder_pins_b[i]);
			setup_zyncoder(i, zyncoder_pins_a[i], zyncoder_pins_b[i]);
			setup_zynpot(i, ZYNPOT_ZYNCODER, i);
			count++;
		}
	}
	fprintf(stderr, "ZynCore: Setting-up %d x Zynpots (zyncoders)...\n", count);
}

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

int init_zyncontrol() {
	wiringPiSetupPinType(WPI_PIN_BCM);
	get_wiring_config();
	#if defined(MCP23017_ENCODERS)
		init_zynmcp23017s();
	#endif
	#if defined(MCP23008_ENCODERS)
		init_zynmcp23008s();
	#endif
	init_zynswitches();
	init_zynpots();
	#ifdef ZYNAPTIK_CONFIG
		init_zynaptik();
	#endif
	#ifdef ZYNTOF_CONFIG
		init_zyntof();
	#endif
	#if defined(MCP23008_ENCODERS)
		init_poll_zynswitches();
	#endif
	return 1;
}

int end_zyncontrol() {
	#if defined(MCP23008_ENCODERS)
		end_poll_zynswitches();
	#endif
	#ifdef ZYNTOF_CONFIG
		end_zyntof();
	#endif
	#ifdef ZYNAPTIK_CONFIG
		end_zynaptik();
	#endif
	reset_zynpots();
	reset_zyncoders();
	reset_zynswitches();
	#if defined(MCP23017_ENCODERS)
		reset_zynmcp23017s();
	#endif
	return 1;
}

//-----------------------------------------------------------------------------

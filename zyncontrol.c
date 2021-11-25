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

#include "zynpot.h"
#include "zyncoder.h"

//-----------------------------------------------------------------------------
// GPIO Expander
//-----------------------------------------------------------------------------

#if defined(HAVE_WIRINGPI_LIB)
	#if defined(MCP23017_ENCODERS)
		// pins 100-115 are located on the MCP23017
		#define MCP23017_BASE_PIN 100
		// define default I2C Address for MCP23017
		#if !defined(MCP23017_I2C_ADDRESS)
			#define MCP23017_I2C_ADDRESS 0x20
		#endif
		// define default interrupt pins for the MCP23017
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

#else
	#define MCP23008_BASE_PIN 100
	#define MCP23008_I2C_ADDRESS 0x20
#endif


#if defined(MCP23017_ENCODERS)

// WiringPi node struct for direct access to the mcp23017
struct wiringPiNodeStruct *zyncoder_mcp23017_node;

// two ISR routines for the two banks
void zyncoder_mcp23017_bankA_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node, MCP23017_BASE_PIN, 0);
}
void zyncoder_mcp23017_bankB_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node, MCP23017_BASE_PIN, 1);
}
void (*zyncoder_mcp23017_bank_ISRs[2])={
	zyncoder_mcp23017_bankA_ISR,
	zyncoder_mcp23017_bankB_ISR
};

#endif

//-----------------------------------------------------------------------------
// Get wiring config from environment
//-----------------------------------------------------------------------------

int zynswitch_pins[8];
int zyncoder_pins_a[4];
int zyncoder_pins_b[4];

void reset_wiring_config() {
	int i;
	for (i=0;i<16;i++) zynswitch_pins[i] = 0;
	for (i=0;i<4;i++) {
		zyncoder_pins_a[i] = 0;
		zyncoder_pins_b[i] = 0;
	}
}

void parse_envar2intarr(const char *envar_name, int *result, int limit) {
	const char *envar_ptr = getenv(envar_name);
	if (envar_ptr) {
		char envar_cpy[64];
		char *save_ptr;
		int i=0;
		strcpy(envar_cpy, envar_ptr);
		char *token = strtok_r(envar_cpy, ",", &save_ptr);
		result[i++] = atoi(token);
		while (token!=NULL && i<limit) {
			token = strtok_r(NULL, ",", &save_ptr);
			result[i++] = atoi(token);
		}
	}
}

void get_wiring_config() {
	reset_wiring_config();
	parse_envar2intarr("ZYNTHIAN_WIRING_SWITCHES", zynswitch_pins, 8);
	parse_envar2intarr("ZYNTHIAN_WIRING_ENCODER_A", zyncoder_pins_a, 4);
	parse_envar2intarr("ZYNTHIAN_WIRING_ENCODER_B", zyncoder_pins_b, 4);
}

//-----------------------------------------------------------------------------
// 8 x ZynSwitches
//-----------------------------------------------------------------------------

void init_zynswitches() {
	#if defined(MCP23017_ENCODERS)
	zyncoder_mcp23017_node = init_mcp23017(MCP23017_BASE_PIN, MCP23017_I2C_ADDRESS, MCP23017_INTA_PIN, MCP23017_INTB_PIN, zyncoder_mcp23017_bank_ISRs);
	#elif defined(MCP23008_ENCODERS)   
	mcp23008Setup(MCP23008_BASE_PIN, MCP23008_I2C_ADDRESS);
	init_poll_zynswitches();
	#endif

	printf("Setting-up 8 x Zynswitches...\n");
	int i;
	for (i=0;i<8;i++) {
		if (zynswitch_pins[i]>0) {
			setup_zynswitch(i, zynswitch_pins[i]);
		}
	}
}

//-----------------------------------------------------------------------------
// 4 x Zynpòts (Analog Encoder RV112)
//-----------------------------------------------------------------------------

void init_zynpots() {
	printf("Setting-up 4 x Zynpots (zyncoders)...\n");
	int i;
	for (i=0;i<4;i++) {
		if (zyncoder_pins_a[i]>0 && zyncoder_pins_b[i]>0) {
			setup_zyncoder(i, zyncoder_pins_a[i], zyncoder_pins_b[i]);
			setup_zynpot(i, ZYNPOT_ZYNCODER, i);
			setup_rangescale_zynpot(i,0,127,64,0);
		}
	}
}

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

int init_zyncontrol() {
	wiringPiSetup();
	reset_zynpots();
	reset_zyncoders();
	get_wiring_config();
	init_zynswitches();
	init_zynpots();
	return 1;
}

int end_zyncontrol() {
	reset_zynpots();
	reset_zyncoders();
	return 1;
}

//-----------------------------------------------------------------------------

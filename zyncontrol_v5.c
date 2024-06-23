/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncontrol Library for Zynthian Kit V5
 * 
 * Initialize & configure control hardware for Zynthian Kit V5
 * 
 * Copyright (C) 2015-2023 Fernando Moyano <jofemodo@zynthian.org>
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

#include "gpiod_callback.h"
#include "zynpot.h"
#include "zyncoder.h"
#include "tpa6130.h"

//-----------------------------------------------------------------------------
// Zynface V5
//-----------------------------------------------------------------------------

#ifdef ZYNAPTIK_CONFIG
#include "zynaptik.h"
#endif

//-----------------------------------------------------------------------------
// GPIO Expander 1
//-----------------------------------------------------------------------------

#define MCP23017_1_BASE_PIN 100
#define MCP23017_1_I2C_ADDRESS 0x20
#define MCP23017_1_INTA_PIN  5 // wiringPi 21
#define MCP23017_1_INTB_PIN  6 // wiringPi 22

void zynmcp23017_ISR_bankA_1() {
	zynmcp23017_ISR(0, 0);
}
void zynmcp23017_ISR_bankB_1() {
	zynmcp23017_ISR(0, 1);
}
void (*zynmcp23017_ISRs_1[2]) = {
	zynmcp23017_ISR_bankA_1,
	zynmcp23017_ISR_bankB_1
};

//-----------------------------------------------------------------------------
// GPIO Expander 2
//-----------------------------------------------------------------------------

#define MCP23017_2_BASE_PIN 200
#define MCP23017_2_I2C_ADDRESS 0x21
#if Z2_VERSION==1
	#define MCP23017_2_INTA_PIN  7 // wiringPi 11
	#define MCP23017_2_INTB_PIN  8 // wiringPi 10
#else
	#define MCP23017_2_INTA_PIN 17 // wiringPi 0
	#define MCP23017_2_INTB_PIN 27 // wiringPi 2
#endif

void zynmcp23017_ISR_bankA_2() {
	zynmcp23017_ISR(1, 0);
}
void zynmcp23017_ISR_bankB_2() {
	zynmcp23017_ISR(1, 1);
}
void (*zynmcp23017_ISRs_2[2]) = {
	zynmcp23017_ISR_bankA_2,
	zynmcp23017_ISR_bankB_2
};

//-----------------------------------------------------------------------------
// 2 x zynmcp23017
//-----------------------------------------------------------------------------

void init_zynmcp23017s() {
	reset_zynmcp23017s();
	setup_zynmcp23017(0, MCP23017_1_BASE_PIN, MCP23017_1_I2C_ADDRESS, MCP23017_1_INTA_PIN, MCP23017_1_INTB_PIN, zynmcp23017_ISRs_1);
	setup_zynmcp23017(1, MCP23017_2_BASE_PIN, MCP23017_2_I2C_ADDRESS, MCP23017_2_INTA_PIN, MCP23017_2_INTB_PIN, zynmcp23017_ISRs_2);
}

//-----------------------------------------------------------------------------
// 30 x ZynSwitches (16 on MCP23017_1, 8 on MCP23017_2)
//-----------------------------------------------------------------------------

extern uint16_t num_zynswitches;

void init_zynswitches() {
	reset_zynswitches();
	int i;
	fprintf(stderr, "ZynCore: Setting-up 20+4 x Zynswitches...\n");
	for (i=0;i<16;i++) setup_zynswitch(4+i, MCP23017_1_BASE_PIN + i, 1);
	for (i=0;i<8;i++) setup_zynswitch(20+i, MCP23017_2_BASE_PIN + i, 1);
	num_zynswitches = 28;
}

//-----------------------------------------------------------------------------
// 4 x Zynpòts (Analog Encoder RV112)
//-----------------------------------------------------------------------------

void init_zynpots() {
	reset_zyncoders();
	reset_zynpots();

	fprintf(stderr, "ZynCore: Setting-up Zynpots => 4 x PEC11 ...\n");
	setup_zyncoder(0, MCP23017_2_BASE_PIN + 8, MCP23017_2_BASE_PIN + 9);
	setup_zyncoder(1, MCP23017_2_BASE_PIN + 10, MCP23017_2_BASE_PIN + 11);
	setup_zyncoder(2, MCP23017_2_BASE_PIN + 12, MCP23017_2_BASE_PIN + 13);
	setup_zyncoder(3, MCP23017_2_BASE_PIN + 14, MCP23017_2_BASE_PIN + 15);
	int i;
	for (i=0;i<4;i++) {
		setup_zynpot(i,ZYNPOT_ZYNCODER,i);
	}
}

void end_zynpots() {
	reset_zynpots();
}

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

#ifdef TPA6130_DRIVER
uint8_t set_hpvol(uint8_t vol) { return tpa6130_set_volume(vol); }
uint8_t get_hpvol() { return tpa6130_get_volume(); }
uint8_t get_hpvol_max() { return tpa6130_get_volume_max(); }
#endif

int init_zyncontrol() {
	gpiod_init_callbacks();
	#ifdef TPA6130_DRIVER
	tpa6130_init();
	#endif
	init_zynmcp23017s();
	init_zynswitches();
	init_zynpots();
	#ifdef ZYNAPTIK_CONFIG
	init_zynaptik();
	#endif
	gpiod_start_callbacks();
	return 1;
}

int end_zyncontrol() {
	gpiod_stop_callbacks();
	#ifdef ZYNAPTIK_CONFIG
	end_zynaptik();
	#endif
	end_zynpots();
	reset_zyncoders();
	reset_zynswitches();
	reset_zynmcp23017s();
	#ifdef TPA6130_DRIVER
	tpa6130_end();
	#endif
	return 1;
}

//-----------------------------------------------------------------------------

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

#include <stdio.h>
#include <stdlib.h>

#include "zynpot.h"
#include "zyncoder.h"
#include "zynads1115.h"
#include "zynrv112.h"

//-----------------------------------------------------------------------------
// GPIO Expander 1
//-----------------------------------------------------------------------------

#define MCP23017_1_BASE_PIN 100
#define MCP23017_1_I2C_ADDRESS 0x20
#define MCP23017_1_INTA_PIN 21
#define MCP23017_1_INTB_PIN 22

struct wiringPiNodeStruct *zyncoder_mcp23017_node_1;

// ISR routines for each chip/bank
void zyncoder_mcp23017_1_bankA_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node_1, MCP23017_1_BASE_PIN, 0);
}
void zyncoder_mcp23017_1_bankB_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node_1, MCP23017_1_BASE_PIN, 1);
}
void (*zyncoder_mcp23017_1_bank_ISRs[2])={
	zyncoder_mcp23017_1_bankA_ISR,
	zyncoder_mcp23017_1_bankB_ISR
};

//-----------------------------------------------------------------------------
// GPIO Expander 2
//-----------------------------------------------------------------------------

#define MCP23017_2_BASE_PIN 200
#define MCP23017_2_I2C_ADDRESS 0x21
#define MCP23017_2_INTA_PIN 11
#define MCP23017_2_INTB_PIN 10

struct wiringPiNodeStruct *zyncoder_mcp23017_node_2;

// ISR routines for each chip/bank
void zyncoder_mcp23017_2_bankA_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node_2, MCP23017_2_BASE_PIN, 0);
}
void zyncoder_mcp23017_2_bankB_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node_2, MCP23017_2_BASE_PIN, 1);
}
void (*zyncoder_mcp23017_2_bank_ISRs[2])={
	zyncoder_mcp23017_2_bankA_ISR,
	zyncoder_mcp23017_2_bankB_ISR
};

//-----------------------------------------------------------------------------
// 30 x ZynSwitches (16 on MCP23017_1, 14 on MCP23017_2)
//-----------------------------------------------------------------------------

void init_zynswitches() {
	reset_zynswitches();

	zyncoder_mcp23017_node_1 = init_mcp23017(MCP23017_1_BASE_PIN, MCP23017_1_I2C_ADDRESS, MCP23017_1_INTA_PIN, MCP23017_1_INTB_PIN, zyncoder_mcp23017_1_bank_ISRs);
	zyncoder_mcp23017_node_2 = init_mcp23017(MCP23017_2_BASE_PIN, MCP23017_2_I2C_ADDRESS, MCP23017_2_INTA_PIN, MCP23017_2_INTB_PIN, zyncoder_mcp23017_2_bank_ISRs);

	int i;
	printf("Setting-up 30 x Zynswitches...\n");
	for (i=0;i<16;i++) setup_zynswitch(i, MCP23017_1_BASE_PIN + i);
	for (i=0;i<14;i++) setup_zynswitch(16+i, MCP23017_2_BASE_PIN + i);
}

//-----------------------------------------------------------------------------
// 4 x Zynpòts (Analog Encoder RV112)
//-----------------------------------------------------------------------------

#define RV112_ADS1115_I2C_ADDRESS_1 0x48
#define RV112_ADS1115_I2C_ADDRESS_2 0x49
#define RV112_ADS1115_BASE_PIN_1 500
#define RV112_ADS1115_BASE_PIN_2 532

#define RV112_ADS1115_GAIN ADS1115_GAIN_VREF_4_096
#define RV112_ADS1115_RATE ADS1115_RATE_475SPS

void init_zynpots() {
	reset_zynpots();
	reset_rv112s();

	ads1115_nodes[0] = init_ads1115(RV112_ADS1115_BASE_PIN_1, RV112_ADS1115_I2C_ADDRESS_1, RV112_ADS1115_GAIN, RV112_ADS1115_RATE);
	ads1115_nodes[1] = init_ads1115(RV112_ADS1115_BASE_PIN_2, RV112_ADS1115_I2C_ADDRESS_2, RV112_ADS1115_GAIN, RV112_ADS1115_RATE);

	printf("Setting-up 4 x Zynpots (RV112)...\n");

	setup_rv112(0, RV112_ADS1115_BASE_PIN_1, 0);
	setup_rv112(1, RV112_ADS1115_BASE_PIN_1, 0);
	setup_rv112(2, RV112_ADS1115_BASE_PIN_2, 0);
	setup_rv112(3, RV112_ADS1115_BASE_PIN_2, 1);
	init_poll_rv112();

	int i;
	for (i=0;i<4;i++) {
		setup_zynpot(i,ZYNPOT_RV112,i);
	}
}

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

int init_zyncontrol() {
	wiringPiSetup();
	init_zynswitches();
	init_zynpots();
	return 1;
}

int end_zyncontrol() {
	reset_zynpots();
	reset_zyncoders();
	reset_zynswitches();
	return 1;
}

//-----------------------------------------------------------------------------

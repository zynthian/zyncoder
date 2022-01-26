/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing Rotary Encoders & Switches connected 
 * to RBPi native GPIOs or expanded with MCP23008/MCP23017.
 * Includes an emulator mode for developing on desktop computers.
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

#include <lo/lo.h>
#include <wiringPi.h>

#include "zynmidirouter.h"

//-----------------------------------------------------------------------------
// MCP23017 stuff
//-----------------------------------------------------------------------------
#ifndef MCP23008_ENCODERS

struct wiringPiNodeStruct * init_mcp23017(int base_pin, uint8_t i2c_address, uint8_t inta_pin, uint8_t intb_pin, void (*isrs[2]));

// ISR routine for zynswitches & zyncoders
void zyncoder_mcp23017_ISR(struct wiringPiNodeStruct *wpns, uint16_t base_pin, uint8_t bank);

#endif

//-----------------------------------------------------------------------------
// MCP23008 stuff
//-----------------------------------------------------------------------------
#ifdef MCP23008_ENCODERS

//Switches Polling Thread (should be avoided!)
pthread_t init_poll_zynswitches();

#endif

//-----------------------------------------------------------------------------
// Zynswitch data & functions
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNSWITCHES 36

typedef struct zynswitch_st {
	uint8_t enabled;
	uint8_t pin;
	uint8_t push;
	unsigned long tsus;
	unsigned int dtus;
	// note that this status is like the pin_[ab]_last_state for the zyncoders
	uint8_t status;

	midi_event_t midi_event;
	int last_cvgate_note;
} zynswitch_t;
zynswitch_t zynswitches[MAX_NUM_ZYNSWITCHES];

void reset_zynswitches();
int get_num_zynswitches();
int get_last_zynswitch_index();

int setup_zynswitch(uint8_t i, uint8_t pin); 
int setup_zynswitch_midi(uint8_t i, midi_event_type midi_evt, uint8_t midi_chan, uint8_t midi_num, uint8_t midi_val);

unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus);
int get_next_pending_zynswitch(uint8_t i);

//-----------------------------------------------------------------------------
// Zyncoder data (Incremental Rotary Encoders)
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCODERS 4

// Number of ticks per retent in rotary encoders
#define ZYNCODER_TICKS_PER_RETENT 4

typedef struct zyncoder_st {
	uint8_t enabled;
	int32_t min_value;
	int32_t max_value;
	int32_t step;
	uint8_t inv;
	int32_t value;
	uint8_t value_flag;
	int8_t zpot_i;

	// Next fields are zyncoder-specific
	uint8_t pin_a;
	uint8_t pin_b;
	
	uint8_t pin_a_last_state;
	uint8_t pin_b_last_state;

	unsigned int subvalue;
	unsigned int last_encoded;
	unsigned long tsus;
	unsigned int dtus[ZYNCODER_TICKS_PER_RETENT];
} zyncoder_t;
zyncoder_t zyncoders[MAX_NUM_ZYNCODERS];

//-----------------------------------------------------------------------------
// Zyncoder's zynpot API
//-----------------------------------------------------------------------------

void reset_zyncoders();
int get_num_zyncoders();

int setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b);
int setup_rangescale_zyncoder(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, int32_t step);

int32_t get_value_zyncoder(uint8_t i);
uint8_t get_value_flag_zyncoder(uint8_t i);
int set_value_zyncoder(uint8_t i, int32_t v);

//-----------------------------------------------------------------------------

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

#if defined(HAVE_WIRINGPI_LIB)
	#include <wiringPi.h>
	#include "zynmcp23017.h"
	#include "zynmcp23008.h"
#else
	#include "wiringPiEmu.h"
#endif

#include "zynmidirouter.h"

//-----------------------------------------------------------------------------
// Zynswitch data & functions
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNSWITCHES 36

typedef struct zynswitch_st {
	uint8_t enabled;
	uint16_t pin;
	uint8_t push;
	unsigned long tsus;
	unsigned int dtus;
	uint8_t status;

	midi_event_t midi_event;
	int last_cvgate_note;
} zynswitch_t;
zynswitch_t zynswitches[MAX_NUM_ZYNSWITCHES];

void reset_zynswitches();
int get_num_zynswitches();
int get_last_zynswitch_index();

int setup_zynswitch(uint8_t i, uint16_t pin); 
int setup_zynswitch_midi(uint8_t i, midi_event_type midi_evt, uint8_t midi_chan, uint8_t midi_num, uint8_t midi_val);

unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus);
int get_next_pending_zynswitch(uint8_t i);

void send_zynswitch_midi(zynswitch_t *zsw, uint8_t status);
void update_zynswitch(uint8_t i, uint8_t status);

//-----------------------------------------------------------------------------
// Zyncoder data (Incremental Rotary Encoders)
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCODERS 4

// Number of ticks per retent in rotary encoders
#define ZYNCODER_TICKS_PER_RETENT 4

typedef struct zyncoder_st {
	uint8_t enabled;
	int32_t step;
	int32_t value;
	uint8_t value_flag;
	int8_t zpot_i;

	// Next fields are zyncoder-specific
	uint16_t pin_a;
	uint16_t pin_b;
	
	int32_t subvalue;
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

int setup_zyncoder(uint8_t i, uint16_t pin_a, uint16_t pin_b);

int setup_behaviour_zyncoder(uint8_t i, int32_t step);
int32_t get_value_zyncoder(uint8_t i);

//-----------------------------------------------------------------------------
// Zyncoder's specific functions
//-----------------------------------------------------------------------------

void update_zyncoder(uint8_t i, uint8_t msb, uint8_t lsb);

//-----------------------------------------------------------------------------

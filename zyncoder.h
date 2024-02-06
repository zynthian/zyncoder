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

#ifndef ZYNCODER_H
#define ZYNCODER_H

#include <gpiod.h>

#include "zynmcp23017.h"
//#include "zynmcp23008.h"
#include "zynmidirouter.h"

//-----------------------------------------------------------------------------
// Zynswitch data & functions
//-----------------------------------------------------------------------------

#if defined(ZYNAPTIK_CONFIG)
	#define MAX_NUM_ZYNSWITCHES 52
#else
	#define MAX_NUM_ZYNSWITCHES 36
#endif

typedef struct zynswitch_st {
	uint8_t enabled;

	struct gpiod_line *line;	// libgpiod line struct
	uint16_t pin;
	uint8_t off_state;
	uint8_t push;
	uint64_t tsus;
	unsigned int dtus;
	uint8_t status;

	midi_event_t midi_event;
	int last_cvgate_note;
} zynswitch_t;

void reset_zynswitches();
int get_num_zynswitches();
int get_last_zynswitch_index();

int setup_zynswitch(uint8_t i, uint16_t pin, uint8_t off_state);
int setup_zynswitch_midi(uint8_t i, midi_event_type midi_evt, uint8_t midi_chan, uint8_t midi_num, uint8_t midi_val);

unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus);
int get_next_pending_zynswitch(uint8_t i);

void send_zynswitch_midi(zynswitch_t *zsw);
void update_zynswitch(uint8_t i, uint8_t status);

//-----------------------------------------------------------------------------
// Zyncoder data (Incremental Rotary Encoders)
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCODERS 4

typedef struct zyncoder_st {
	uint8_t enabled;			// 1 to enable encoder
	int32_t step;				// Size of change in value for each detent of encoder
	int32_t value;				// Current encdoder value
	int8_t zpot_i;				// Zynpot index assigned to this encoder

	// Next fields are zyncoder-specific
	struct gpiod_line *line_a;	// libgpiod line struct
	struct gpiod_line *line_b;	// libgpiod line struct
	uint16_t pin_a;				// Data GPI
	uint16_t pin_b;				// Clock GPI
	uint8_t short_history;      // Quadrant encoder algorithm last two valid states (4 bits)
	uint8_t long_history;       // Quadrant encoder algorithm last four valid states (8 bits)

	uint64_t tsms;				// Absolute time of last encoder change in milliseconds
} zyncoder_t;

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

#endif
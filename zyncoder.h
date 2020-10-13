/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing Rotary Encoders & Switches connected 
 * to RBPi native GPIOs or expanded with MCP23008. Includes an 
 * emulator mode to ease developping.
 * 
 * Copyright (C) 2015-2018 Fernando Moyano <jofemodo@zynthian.org>
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

#include "zynmidirouter.h"
#include "zynaptik.h"
#include "zyntof.h"

//-----------------------------------------------------------------------------
// Zyncoder Library Initialization
//-----------------------------------------------------------------------------

int init_zynlib();
int end_zynlib();

int init_zyncoder();
int end_zyncoder();

struct wiringPiNodeStruct * init_mcp23017(int base_pin, uint8_t i2c_address, uint8_t inta_pin, uint8_t intb_pin, void (*isrs[2]));

// generic auxiliar ISR routine for zyncoders
void zyncoder_mcp23017_ISR(struct wiringPiNodeStruct *wpns, uint16_t base_pin, uint8_t bank);

//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNSWITCHES 16

struct zynswitch_st {
	uint8_t enabled;
	uint8_t pin;
	volatile unsigned long tsus;
	volatile unsigned int dtus;
	// note that this status is like the pin_[ab]_last_state for the 
	// zyncoders
	volatile uint8_t status;

	struct midi_event_st midi_event;

};
struct zynswitch_st zynswitches[MAX_NUM_ZYNSWITCHES];

struct zynswitch_st *setup_zynswitch(uint8_t i, uint8_t pin); 
int setup_zynswitch_midi(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num);
unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus);
unsigned int get_zynswitch_dtus(uint8_t i, unsigned int long_dtus);

//-----------------------------------------------------------------------------
// Rotary Encoders
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCODERS 4

// Number of ticks per retent in rotary encoders
#define ZYNCODER_TICKS_PER_RETENT 4

struct zyncoder_st {
	uint8_t enabled;
	uint8_t pin_a;
	uint8_t pin_b;
#ifdef MCP23017_ENCODERS
	volatile uint8_t pin_a_last_state;
	volatile uint8_t pin_b_last_state;
#endif
	uint8_t midi_chan;
	uint8_t midi_ctrl;
	unsigned int osc_port;
	lo_address osc_lo_addr;
	char osc_path[512];
	unsigned int max_value;
	unsigned int step;
	volatile unsigned int subvalue;
	volatile unsigned int value;
	volatile unsigned int last_encoded;
	volatile unsigned long tsus;
	unsigned int dtus[ZYNCODER_TICKS_PER_RETENT];
};
struct zyncoder_st zyncoders[MAX_NUM_ZYNCODERS];

void midi_event_zyncoders(uint8_t midi_chan, uint8_t midi_ctrl, uint8_t val);

struct zyncoder_st *setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b, uint8_t midi_chan, uint8_t midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step); 
unsigned int get_value_zyncoder(uint8_t i);
void set_value_zyncoder(uint8_t i, unsigned int v, int send);


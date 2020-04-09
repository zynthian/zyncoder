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

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_zynlib();
int end_zynlib();

int init_zyncoder();
int end_zyncoder();

//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

// Maximum 50 I2C switches
#define MAX_NUM_ZYNSWITCHES 50
#define HWC_ADDR 0x08 // I2C address of riban hardware controller

struct zynswitch_st {
	uint8_t enabled; // 1 if switch enabled
	uint8_t index; // physical switch index mapped to this logical switch
	volatile unsigned long tsus; // timestamp of switch close
	volatile unsigned int dtus; // duration of switch press after switch open
	volatile uint8_t status; // 0 if switch closed, 1 if switch open

	uint8_t midi_chan; // MIDI channel assigned to custom switch event
	uint8_t midi_cc; // MIDI control change assigned to custom switch event
};
struct zynswitch_st zynswitches[MAX_NUM_ZYNSWITCHES];

struct zynswitch_st *setup_zynswitch(uint8_t i, uint8_t pin);
unsigned int get_zynswitch(uint8_t i);
unsigned int get_zynswitch_dtus(uint8_t i, unsigned int long_dtus);
int hwci2c_fd; // File descriptor for I2C interface to hardware controller

//-----------------------------------------------------------------------------
// Rotary Encoders
//-----------------------------------------------------------------------------

// Maximum 30 I2C encoders
#define ZYNCODER_TICKS_PER_RETENT 4
#define MAX_NUM_ZYNCODERS 30

struct zyncoder_st {
	uint8_t enabled;
	uint8_t index;
	uint8_t midi_chan;
	uint8_t midi_ctrl;
	unsigned int osc_port;
	lo_address osc_lo_addr;
	char osc_path[512];
	unsigned int max_value;
	unsigned int step;
	volatile unsigned int value;
	volatile unsigned long tsus;
};
struct zyncoder_st zyncoders[MAX_NUM_ZYNCODERS];

void midi_event_zyncoders(uint8_t midi_chan, uint8_t midi_ctrl, uint8_t val);

struct zyncoder_st *setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b, uint8_t midi_chan, uint8_t midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step);
unsigned int get_value_zyncoder(uint8_t i);
void set_value_zyncoder(uint8_t i, unsigned int v, int send);

void handleRibanHwc();

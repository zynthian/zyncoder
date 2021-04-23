/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zynaptik Library
 * 
 * Library for interfacing external sensors and actuators.
 * It implements interfaces with extra MCP23017, ADS1115, etc.
 * 
 * Copyright (C) 2015-2019 Fernando Moyano <jofemodo@zynthian.org>
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

//-----------------------------------------------------------------------------
// MCP23017 Stuff
//-----------------------------------------------------------------------------

//Default config for Zynaptik's MCP23017
#if !defined(ZYNAPTIK_MCP23017_I2C_ADDRESS)
	#define ZYNAPTIK_MCP23017_I2C_ADDRESS 0x21
#endif
#if !defined(ZYNAPTIK_MCP23017_BASE_PIN)
	#define ZYNAPTIK_MCP23017_BASE_PIN 200
#endif
#if !defined(ZYNAPTIK_MCP23017_INTA_PIN)
	#define ZYNAPTIK_MCP23017_INTA_PIN 27
#endif
#if !defined(ZYNAPTIK_MCP23017_INTB_PIN)
	#define ZYNAPTIK_MCP23017_INTB_PIN 25
#endif

//-----------------------------------------------------------------------------
// ADS1115 Stuff
//-----------------------------------------------------------------------------

//Default config for Zynaptik's ADS1115
#if !defined(ZYNAPTIK_ADS1115_I2C_ADDRESS)
	#define ZYNAPTIK_ADS1115_I2C_ADDRESS 0x48
#endif
#if !defined(ZYNAPTIK_ADS1115_BASE_PIN)
	#define ZYNAPTIK_ADS1115_BASE_PIN 300
#endif

#define ADS115_GAIN_VREF_6_144 0
#define ADS115_GAIN_VREF_4_096 1
#define ADS115_GAIN_VREF_2_048 2
#define ADS115_GAIN_VREF_1_024 3
#define ADS115_GAIN_VREF_0_512 4
#define ADS115_GAIN_VREF_0_256 5

void init_ads1115(uint16_t base_pin, uint16_t i2c_address);
void set_ads1115_gain(uint16_t base_pin, uint8_t gain);

//-----------------------------------------------------------------------------
// MCP4728 Stuff
//-----------------------------------------------------------------------------

//Default config for Zynaptik's MCP4728
#if !defined(ZYNAPTIK_MCP4728_I2C_ADDRESS)
	#define ZYNAPTIK_MCP4728_I2C_ADDRESS 0x60
#endif

void init_mcp4728(uint16_t i2c_address);

//-----------------------------------------------------------------------------
// CV-IN: Generate MIDI from Analog Inputs: CC, Pitchbend, Channel Pressure
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCVINS 4

struct zyncvin_st {
	uint8_t enabled;
	uint16_t pin;
	uint16_t val;

	uint8_t midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;
	uint8_t midi_val;
};
struct zyncvin_st zyncvins[MAX_NUM_ZYNCVINS];

void setup_zynaptik_cvin(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zynaptik_cvin(uint8_t i);
void zynaptik_cvin_to_midi(uint8_t i);

//CV-IN Polling interval
#define POLL_ZYNAPTIK_CVINS_US 40000

pthread_t init_poll_zynaptik_cvins();

//-----------------------------------------------------------------------------
// CV-OUT: Set Analog Outputs from MIDI: CC, Notes (velocity+pitchbend)
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCVOUTS 4

struct zyncvout_st {
	uint8_t enabled;

	uint8_t midi_val;

	uint8_t midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;

	uint16_t midi_event_temp;
	uint16_t midi_event_mask;
};
struct zyncvout_st zyncvouts[MAX_NUM_ZYNCVOUTS];

void setup_zynaptik_cvout(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zynaptik_cvout(uint8_t i);
void zynaptik_midi_to_cvout(jack_midi_event_t *ev);

//CV-OUT Refresh interval
#define REFRESH_ZYNAPTIK_CVOUTS_US 40000

pthread_t init_refresh_zynaptik_cvouts();

//-----------------------------------------------------------------------------
// MIDI processing
//-----------------------------------------------------------------------------

void midi_event_zynaptik(uint32_t ev);

//-----------------------------------------------------------------------------
// Zynaptik Library Initialization
//-----------------------------------------------------------------------------

int init_zynaptik();
int end_zynaptik();

//-----------------------------------------------------------------------------

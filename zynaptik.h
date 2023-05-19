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

#include <jack/jack.h>
#include <jack/midiport.h>

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

//-----------------------------------------------------------------------------
// MCP4728 Stuff
//-----------------------------------------------------------------------------

//Default config for Zynaptik's MCP4728
#if !defined(ZYNAPTIK_MCP4728_I2C_ADDRESS)
	#if ZYNAPTIK_VERSION==1
		#define ZYNAPTIK_MCP4728_I2C_ADDRESS 0x60
	#else
		#define ZYNAPTIK_MCP4728_I2C_ADDRESS 0x61
	#endif
#endif

void init_mcp4728(uint16_t i2c_address);

//-----------------------------------------------------------------------------
// CV-IN: Generate MIDI from Analog Inputs: CC, Pitchbend, Channel Pressure
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCVINS 4
#define K_CVIN_VOLT_OCTAVE (12.0 * 6.144 / 32767.0)

#if !defined(ZYNAPTIK_CVIN_VOLTS_OCTAVE)
	#define ZYNAPTIK_CVIN_VOLTS_OCTAVE 1.0
#endif

struct zyncvin_st {
	uint8_t enabled;
	uint16_t pin;

	int midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;
	uint16_t midi_val;
};
struct zyncvin_st zyncvins[MAX_NUM_ZYNCVINS];

float k_cvin;
int note0_cvin;
void set_volts_octave_cvin(float vo);
float get_volts_octave_cvin();
void set_note0_cvin(int note0);
int get_note0_cvin();

void setup_zynaptik_cvin(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zynaptik_cvin(uint8_t i);
void zynaptik_cvin_to_midi(uint8_t i, uint16_t val);

//CV-IN Polling interval
#define POLL_ZYNAPTIK_CVINS_US 40000
pthread_mutex_t zynaptik_cvin_lock;
pthread_t init_poll_zynaptik_cvins();

//-----------------------------------------------------------------------------
// CV-OUT: Set Analog Outputs from MIDI: CC, Notes (velocity+pitchbend)
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNCVOUTS 4
#define K_CVOUT_VOLT_OCTAVE (60.0 / 127.0)

#if !defined(ZYNAPTIK_CVOUT_VOLTS_OCTAVE)
	#define ZYNAPTIK_CVOUT_VOLTS_OCTAVE 1.0
#endif

struct zyncvout_st {
	uint8_t enabled;

	int midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;

	uint16_t midi_event_temp;
	uint16_t midi_event_mask;

	uint16_t val;
};
struct zyncvout_st zyncvouts[MAX_NUM_ZYNCVOUTS];

float k_cvout;
int note0_cvout;
void set_volts_octave_cvout(float vo);
float get_volts_octave_cvout();
void set_note0_cvout(int note0);
int get_note0_cvout();

void setup_zynaptik_cvout(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zynaptik_cvout(uint8_t i);
void zynaptik_midi_to_cvout(jack_midi_event_t *ev);
void set_zynaptik_cvout(int i, uint16_t val);
void refresh_zynaptik_cvouts();

//CV-OUT Refresh interval
#define REFRESH_ZYNAPTIK_CVOUTS_US 40000

//-----------------------------------------------------------------------------
// GATE-OUT: Set Digital Outputs from MIDI Notes
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNGATEOUTS 24

struct zyngateout_st {
	uint8_t enabled;

	int midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;

	uint16_t midi_event_temp;
	uint16_t midi_event_mask;
};
struct zyngateout_st zyngateouts[MAX_NUM_ZYNGATEOUTS];

void setup_zynaptik_gateout(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zynaptik_gateout(uint8_t i);
void zynaptik_midi_to_gateout(jack_midi_event_t *ev);

//-----------------------------------------------------------------------------
// Zynaptik Library Initialization
//-----------------------------------------------------------------------------

int init_zynaptik();
int end_zynaptik();

//-----------------------------------------------------------------------------

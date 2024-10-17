/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: TOF Library
 *
 * Library for interfacing VL53L0X "Time Of Flight" sensor
 *
 * Copyright (C) 2015-2020 Fernando Moyano <jofemodo@zynthian.org>
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
// TCA954X (43/44/48) Stuff => I2C Multiplexer
//-----------------------------------------------------------------------------

// Default config for TCA9543 (I2C multiplexer)
#define TCA954X_I2C_ADDRESS 0x70

void select_zyntof_chan(uint8_t i);

//-----------------------------------------------------------------------------
// VL53L0X Stuff
//-----------------------------------------------------------------------------

#define VL53L0X_I2C_ADDRESS 0x29
#define VL53L0X_DISTANCE_MODE 1

//-----------------------------------------------------------------------------
// Generate MIDI events from Distance
//-----------------------------------------------------------------------------

#define MAX_NUM_ZYNTOFS 4
#define POLL_ZYNTOFS_US 1000

#define MIN_TOF_DISTANCE 60
#define MAX_TOF_DISTANCE 600

struct zyntof_st
{
	uint8_t enabled;
	uint8_t i;
	uint16_t val;

	uint8_t midi_evt;
	uint8_t midi_chan;
	uint8_t midi_num;
	uint8_t midi_val;
};

void setup_zyntof(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num);
void disable_zyntof(uint8_t i);
void send_zyntof_midi(uint8_t i);

pthread_t init_poll_zyntofs();

//-----------------------------------------------------------------------------
// TOFs Library Initialization
//-----------------------------------------------------------------------------

int init_zyntof();
int end_zyntof();

//-----------------------------------------------------------------------------

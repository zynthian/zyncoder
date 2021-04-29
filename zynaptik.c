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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h> 

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <mcp23017.h>
#include <mcp23x0817.h>
#include <ads1115.h>
#include <MCP4728.h>

#include "zyncoder.h"

//-----------------------------------------------------------------------------
// MCP23017 Stuff
//-----------------------------------------------------------------------------

// wiringpi node structure for direct access to the mcp23017
struct wiringPiNodeStruct *zynaptik_mcp23017_node;

// two ISR routines for the two banks
void zynaptik_mcp23017_bankA_ISR() {
	zyncoder_mcp23017_ISR(zynaptik_mcp23017_node, ZYNAPTIK_MCP23017_BASE_PIN, 0);
}
void zynaptik_mcp23017_bankB_ISR() {
	zyncoder_mcp23017_ISR(zynaptik_mcp23017_node, ZYNAPTIK_MCP23017_BASE_PIN, 1);
}
void (*zynaptik_mcp23017_bank_ISRs[2])={
	zynaptik_mcp23017_bankA_ISR,
	zynaptik_mcp23017_bankB_ISR
};

//-----------------------------------------------------------------------------
// ADS1115 Stuff
//-----------------------------------------------------------------------------

void init_ads1115(uint16_t base_pin, uint16_t i2c_address) {
	ads1115Setup(base_pin, i2c_address);
	set_ads1115_gain(base_pin, ADS115_GAIN_VREF_4_096);
}

void set_ads1115_gain(uint16_t base_pin, uint8_t gain) {
	digitalWrite(base_pin, gain);
}


//-----------------------------------------------------------------------------
// MCP4728 Stuff
//-----------------------------------------------------------------------------

void * mcp4728_chip;

void init_mcp4728(uint16_t i2c_address) {
	mcp4728_chip = mcp4728_initialize(2, 3, -1, i2c_address);
}

//-----------------------------------------------------------------------------
// CV-IN: Generate MIDI from Analog Inputs: CC, Pitchbend, Channel Pressure
//-----------------------------------------------------------------------------

void setup_zynaptik_cvin(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	zyncvins[i].pin = ZYNAPTIK_ADS1115_BASE_PIN + i;
	zyncvins[i].val = 0;
	zyncvins[i].midi_evt = midi_evt & 0xF;
	zyncvins[i].midi_chan = midi_chan & 0xF;
	zyncvins[i].midi_num = midi_num & 0xF7;
	zyncvins[i].midi_val = 0;
	zyncvins[i].enabled = 1;
}

void disable_zynaptik_cvin(uint8_t i) {
	zyncvins[i].enabled = 0;
}

void zynaptik_cvin_to_midi(uint8_t i) {
	uint8_t mv = zyncvins[i].val>>8;
	if (mv==zyncvins[i].midi_val) return;
	//printf("ZYNAPTIK CV-IN [%d] => MIDI %d\n", i, mv);
	zyncvins[i].midi_val = mv;
	if (zyncvins[i].midi_evt==PITCH_BENDING) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_pitchbend_change(zyncvins[i].midi_chan, zyncvins[i].val);
	} 
	else if (zyncvins[i].midi_evt==CTRL_CHANGE) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_ccontrol_change(zyncvins[i].midi_chan, zyncvins[i].midi_num, mv);
		//Update zyncoders
		midi_event_zyncoders(zyncvins[i].midi_chan, zyncvins[i].midi_num, mv);
		//Send MIDI event to UI
		write_zynmidi_ccontrol_change(zyncvins[i].midi_chan, zyncvins[i].midi_num, mv);
	} else if (zyncvins[i].midi_evt==CHAN_PRESS) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_chan_press(zyncvins[i].midi_chan, mv);
	} 
}

void * poll_zynaptik_cvins(void *arg) {
	int i;
	while (1) {
		for (i=0;i<MAX_NUM_ZYNCVINS;i++) {
			if (zyncvins[i].enabled) {
				zyncvins[i].val = analogRead(zyncvins[i].pin);
				if (zyncvins[i].midi_evt>0) zynaptik_cvin_to_midi(i);
				//printf("ZYNAPTIK CV-IN [%d] => %d\n", i, zyncvins[i].val);
			}
		}
		usleep(POLL_ZYNAPTIK_CVINS_US);
	}
	return NULL;
}

pthread_t init_poll_zynaptik_cvins() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zynaptik_cvins, NULL);
	if (err != 0) {
		printf("Zyncoder: Can't create zynaptik CV-IN poll thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("Zyncoder: Zynaptik CV-IN poll thread created successfully\n");
		return tid;
	}
}


//-----------------------------------------------------------------------------
// CV-OUT: Set Analog Outputs from MIDI: CC, Pitchbend, Channel Pressure, Notes (velocity+pitchbend)
//-----------------------------------------------------------------------------

void setup_zynaptik_cvout(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	zyncvouts[i].midi_val = 0;

	zyncvouts[i].midi_evt = midi_evt & 0xF;
	zyncvouts[i].midi_chan = midi_chan & 0xF;
	zyncvouts[i].midi_num = midi_num & 0xF7;

	if (midi_evt==PITCH_BENDING || midi_evt==CHAN_PRESS) {
		zyncvouts[i].midi_event_mask=0xFF00;
		zyncvouts[i].midi_event_temp=((midi_evt&0xF)<<12) | ((midi_chan&0xF)<<8);
	}
	else if (midi_evt==CTRL_CHANGE) {
		zyncvouts[i].midi_event_mask=0xFFF0;
		zyncvouts[i].midi_event_temp=((midi_evt&0xF)<<12) | ((midi_chan&0xF)<<8) | (midi_num&0xF7);
	}

	zyncvouts[i].enabled = 1;
}

void disable_zynaptik_cvout(uint8_t i) {
	zyncvouts[i].enabled = 0;
}

void zynaptik_midi_to_cvout(jack_midi_event_t *ev) {
	uint8_t event_type= ev->buffer[0] >> 4;

	for (int i=0;i<MAX_NUM_ZYNCVOUTS;i++) {
		if  (!zyncvouts[i].enabled) continue;

		//if (event_type==
	}
}

void * refresh_zynaptik_cvouts(void *arg) {
	int i;
	float buffer[MAX_NUM_ZYNCVOUTS];
	while (1) {
		for (i=0;i<MAX_NUM_ZYNCVOUTS;i++) {
			if (zyncvouts[i].enabled) {
				buffer[i] = 5.0*zyncvouts[i].midi_val/127.0;
			} else {
				buffer[i] = 0;
			}
			mcp4728_multipleinternal(mcp4728_chip, buffer, 0);
			//printf("ZYNAPTIK CV-OUT => [%f, %f, %f, %f]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
		}
		usleep(REFRESH_ZYNAPTIK_CVOUTS_US);
	}
	return NULL;
}

pthread_t init_refresh_zynaptik_cvouts() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &refresh_zynaptik_cvouts, NULL);
	if (err != 0) {
		printf("Zyncoder: Can't create zynaptik CV-OUT refresh thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("Zyncoder: Zynaptik CV-OUT refresh thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------
// Zynaptik Library Initialization
//-----------------------------------------------------------------------------

int init_zynaptik() {
	int i;
	for (i=0;i<MAX_NUM_ZYNCVINS;i++) {
		zyncvins[i].enabled=0;
	}
	for (i=0;i<MAX_NUM_ZYNCVOUTS;i++) {
		zyncvouts[i].enabled=0;
	}

	mcp4728_chip = NULL;

	if (strstr(ZYNAPTIK_CONFIG, "16xDIO")) {
		zynaptik_mcp23017_node = init_mcp23017(ZYNAPTIK_MCP23017_BASE_PIN, ZYNAPTIK_MCP23017_I2C_ADDRESS, ZYNAPTIK_MCP23017_INTA_PIN, ZYNAPTIK_MCP23017_INTB_PIN, zynaptik_mcp23017_bank_ISRs);
	}
	if (strstr(ZYNAPTIK_CONFIG, "4xAD")) {
		init_ads1115(ZYNAPTIK_ADS1115_BASE_PIN, ZYNAPTIK_ADS1115_I2C_ADDRESS);
		init_poll_zynaptik_cvins();
	}
	if (strstr(ZYNAPTIK_CONFIG, "4xDA") || 1) {
		init_mcp4728(ZYNAPTIK_MCP4728_I2C_ADDRESS);
		init_refresh_zynaptik_cvouts();
	}

	
	return 1;
}

int end_zynaptik() {
	return 1;
}

//-----------------------------------------------------------------------------

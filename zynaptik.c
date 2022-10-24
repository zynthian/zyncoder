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
#include <ads1115.h>
#include <MCP4728.h>

#include "zynpot.h"
#include "zyncoder.h"
#include "zynaptik.h"
#include "zynads1115.h"

//-----------------------------------------------------------------------------
// MCP23017 Stuff
//-----------------------------------------------------------------------------

// two ISR routines for the two banks
void zynaptik_mcp23017_bankA_ISR() {
	zynmcp23017_ISR(1, 0);
}
void zynaptik_mcp23017_bankB_ISR() {
	zynmcp23017_ISR(1, 1);
}
void (*zynaptik_mcp23017_bank_ISRs[2])={
	zynaptik_mcp23017_bankA_ISR,
	zynaptik_mcp23017_bankB_ISR
};

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

void setup_zynaptik_cvin(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	zyncvins[i].pin = ZYNAPTIK_ADS1115_BASE_PIN + i;
	zyncvins[i].midi_evt = midi_evt;
	zyncvins[i].midi_chan = midi_chan & 0xF;
	zyncvins[i].midi_num = midi_num & 0x7F;
	zyncvins[i].midi_val = 0;
	zyncvins[i].enabled = 1;
}

void disable_zynaptik_cvin(uint8_t i) {
	zyncvins[i].enabled = 0;
}

void set_k_cvin(float k) { k_cvin=k; }
float get_k_cvin() { return k_cvin; }

void zynaptik_cvin_to_midi(uint8_t i, uint16_t val) {
	if (zyncvins[i].midi_evt==PITCH_BENDING) {
		val>>=1;
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_pitchbend_change(zyncvins[i].midi_chan, val);
		zyncvins[i].midi_val=val;
		return;
	}
	val>>=8;
	if (val==zyncvins[i].midi_val) return;
	//fprintf(stderr, "ZYNAPTIK CV-IN [%d] => MIDI event %d, %d, %d\n", i, zyncvins[i].midi_evt, zyncvins[i].midi_num, val);
	if (zyncvins[i].midi_evt==CTRL_CHANGE) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_ccontrol_change(zyncvins[i].midi_chan, zyncvins[i].midi_num, val);
		//Update zyncoders
		midi_event_zynpot(zyncvins[i].midi_chan, zyncvins[i].midi_num, val);
		//Send MIDI event to UI
		write_zynmidi_ccontrol_change(zyncvins[i].midi_chan, zyncvins[i].midi_num, val);
	}
	else if (zyncvins[i].midi_evt==CHAN_PRESS) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_chan_press(zyncvins[i].midi_chan, val);
	} 
	zyncvins[i].midi_val = val;
}

void * poll_zynaptik_cvins(void *arg) {
	int i, val;
	while (1) {
		for (i=0;i<MAX_NUM_ZYNCVINS;i++) {
			if (zyncvins[i].enabled) {
				pthread_mutex_lock(&zynaptik_cvin_lock);
				val=analogRead(zyncvins[i].pin);
				pthread_mutex_unlock(&zynaptik_cvin_lock);
				val=(int)(k_cvin*(6.144/5.0)*val);
				if (val>32767) val=32767;
				else if (val<0) val=0;
				//fprintf(stderr, "ZYNAPTIK CV-IN [%d] => %d\n", i, val);
				zynaptik_cvin_to_midi(i,(uint16_t)val);
			}
		}
		usleep(POLL_ZYNAPTIK_CVINS_US);
	}
	return NULL;
}

pthread_t init_poll_zynaptik_cvins() {
	if (pthread_mutex_init(&zynaptik_cvin_lock, NULL) != 0) {
		fprintf(stderr,"ZynCore: Zynaptik CV-IN mutex init failed\n");
		return 0;
    }
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zynaptik_cvins, NULL);
	if (err != 0) {
		fprintf(stderr,"ZynCore: Can't create zynaptik CV-IN poll thread :[%s]", strerror(err));
		return 0;
	} else {
		fprintf(stderr, "ZynCore: Zynaptik CV-IN poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------
// CV-OUT: Set Analog Outputs from MIDI: CC, Pitchbend, Channel Pressure, Notes (velocity+pitchbend)
//-----------------------------------------------------------------------------

void setup_zynaptik_cvout(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	if (midi_evt==CVGATE_OUT_EVENT) {
		zyncvouts[i].midi_event_mask=0xEF00;
		zyncvouts[i].midi_event_temp=((NOTE_OFF&0xF)<<12) | ((midi_chan&0xF)<<8);
	}
	else if (midi_evt==PITCH_BENDING || midi_evt==CHAN_PRESS) {
		zyncvouts[i].midi_event_mask=0xFF00;
		zyncvouts[i].midi_event_temp=((midi_evt&0xF)<<12) | ((midi_chan&0xF)<<8);
	}
	else if (midi_evt==CTRL_CHANGE) {
		zyncvouts[i].midi_event_mask=0xFF7F;
		zyncvouts[i].midi_event_temp=((midi_evt&0xF)<<12) | ((midi_chan&0xF)<<8) | (midi_num&0x7F);
	}
	else {
		return;
	}
	zyncvouts[i].midi_evt = midi_evt;
	zyncvouts[i].midi_chan = midi_chan & 0xF;
	zyncvouts[i].midi_num = midi_num & 0x7F;
	zyncvouts[i].val = 0;
	zyncvouts[i].enabled = 1;
}

void disable_zynaptik_cvout(uint8_t i) {
	zyncvouts[i].val = 0;
	zyncvouts[i].enabled = 0;
}

float k_cvout=DEFAULT_K_CVOUT;
void set_k_cvout(float k) { k_cvout = k; }
float get_k_cvout() { return k_cvout; }

void zynaptik_midi_to_cvout(jack_midi_event_t *ev) {
	uint8_t event_type = ev->buffer[0] >> 4;
	if (event_type<NOTE_OFF || event_type>PITCH_BENDING) return;
	//fprintf(stderr, "ZYNAPTIK MIDI TO CV-OUT => [0x%x, %d, %d]\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);

	uint16_t ev_data = ev->buffer[0]<<8 | ev->buffer[1];
	for (int i=0;i<MAX_NUM_ZYNCVOUTS;i++) {
		if  (!zyncvouts[i].enabled) continue;
		//fprintf(stderr, "\t %d.) => 0x%x <=> 0x%x\n", i, ev_data & zyncvouts[i].midi_event_mask, zyncvouts[i].midi_event_temp);
		if (zyncvouts[i].midi_event_temp!=(ev_data & zyncvouts[i].midi_event_mask)) continue;

		if (event_type==NOTE_ON && ev->buffer[2]>0) {
			//fprintf(stderr, "ZYNAPTIK MIDI TO CVGATE-OUT %d NOTE-ON => %d, %d\n", zyncvouts[i].midi_num, ev->buffer[1], ev->buffer[2]);
			if (zyncvouts[i].val>0) {
				digitalWrite(zynswitches[zyncvouts[i].midi_num].pin,1);
			}
			zyncvouts[i].val = ev->buffer[1]<<7;
			//set_zynaptik_cvout(i, zyncvouts[i].val);
			refresh_zynaptik_cvouts();
			usleep(50);
			digitalWrite(zynswitches[zyncvouts[i].midi_num].pin,0);
		}
		else if (event_type==NOTE_OFF || event_type==NOTE_ON) {
			//fprintf(stderr, "ZYNAPTIK MIDI TO CVGATE-OUT %d NOTE-OFF => %d\n", zyncvouts[i].midi_num, ev->buffer[1]);
			if (zyncvouts[i].val==ev->buffer[1]<<7) {
				zyncvouts[i].val = 0;
				//set_zynaptik_cvout(i, zyncvouts[i].val);
				refresh_zynaptik_cvouts();
				digitalWrite(zynswitches[zyncvouts[i].midi_num].pin,1);
			}
		}
		else if (event_type==PITCH_BENDING) {
			zyncvouts[i].val = (ev->buffer[2]<<7) | ev->buffer[1];
			//set_zynaptik_cvout(i, zyncvouts[i].val);
			refresh_zynaptik_cvouts();
		}
		else if (event_type==CTRL_CHANGE) {
			zyncvouts[i].val = ev->buffer[2]<<7;
			//set_zynaptik_cvout(i, zyncvouts[i].val);
			refresh_zynaptik_cvouts();
		}
		else if (event_type==CHAN_PRESS) {
			zyncvouts[i].val = ev->buffer[2]<<7;
			//set_zynaptik_cvout(i, zyncvouts[i].val);
			refresh_zynaptik_cvouts();
		} 
	}
}

void set_zynaptik_cvout(int i, uint16_t val) {
	float vout=k_cvout*val/16384.0;
	//fprintf(stderr, "ZYNAPTIK CV-OUT %d => %f\n", i, vout);
	int err=mcp4728_singleexternal(mcp4728_chip, i, vout, 0);
	if (err!=0) {
		fprintf(stderr,"ZYNAPTIK CV-OUT => Can't write MCP4728 (DAC) register %d. ERROR %d\n", i, err);
	}
}

void refresh_zynaptik_cvouts() {
	int i, err;
	float buffer[MAX_NUM_ZYNCVOUTS];
	for (i=0;i<MAX_NUM_ZYNCVOUTS;i++) {
		if (zyncvouts[i].enabled) {
			buffer[i] = k_cvout*zyncvouts[i].val/16384.0;
		} else {
			buffer[i] = 0;
		}
	}
	//fprintf(stderr, "ZYNAPTIK CV-OUT => [%f, %f, %f, %f]\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	//err=mcp4728_multipleinternal(mcp4728_chip, buffer, 0);
	err=mcp4728_multipleexternal(mcp4728_chip, buffer, 0);
	if (err!=0) {
		fprintf(stderr,"ZYNAPTIK CV-OUT => Can't write MCP4728 (DAC) registers. ERROR %d\n", err);
	}
}

/*
void * _refresh_zynaptik_cvouts(void *arg) {
	while (1) {
		refresh_zynaptik_cvouts();
		usleep(REFRESH_ZYNAPTIK_CVOUTS_US);
	}
	return NULL;
}

pthread_t init_refresh_zynaptik_cvouts() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &_refresh_zynaptik_cvouts, NULL);
	if (err != 0) {
		fprintf(stderr,"Zyncoder: Can't create zynaptik CV-OUT refresh thread :[%s]", strerror(err));
		return 0;
	} else {
		fprintf(stderr, "Zyncoder: Zynaptik CV-OUT refresh thread created successfully.\n");
		return tid;
	}
}
*/

//-----------------------------------------------------------------------------
// GATE-OUT: Set Digital Outputs from MIDI Notes
//-----------------------------------------------------------------------------

void setup_zynaptik_gateout(uint8_t i, int midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	if (midi_evt==GATE_OUT_EVENT) {
		zyngateouts[i].midi_event_mask=0xEF7F;
		zyngateouts[i].midi_event_temp=((NOTE_OFF&0xF)<<12) | ((midi_chan&0xF)<<8) | (midi_num&0x7F);
	}
	else {
		return;
	}
	zyngateouts[i].midi_evt = midi_evt;
	zyngateouts[i].midi_chan = midi_chan & 0xF;
	zyngateouts[i].midi_num = midi_num & 0x7F;
	zyngateouts[i].enabled = 1;
}

void disable_zynaptik_gateout(uint8_t i) {
	zyngateouts[i].enabled = 0;
}

void zynaptik_midi_to_gateout(jack_midi_event_t *ev) {
	uint8_t event_type = ev->buffer[0] >> 4;
	if (event_type<NOTE_OFF || event_type>NOTE_ON) return;
	//fprintf(stderr, "ZYNAPTIK MIDI TO GATE-OUT => [0x%x, %d, %d]\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);

	uint16_t ev_data = ev->buffer[0]<<8 | ev->buffer[1];
	for (int i=0;i<MAX_NUM_ZYNGATEOUTS;i++) {
		if  (!zyngateouts[i].enabled) continue;
		//fprintf(stderr, "\t %d.) => 0x%x <=> 0x%x\n", i, ev_data & zyngateouts[i].midi_event_mask, zyngateouts[i].midi_event_temp);
		if (zyngateouts[i].midi_event_temp!=(ev_data & zyngateouts[i].midi_event_mask)) continue;

		if (event_type==NOTE_ON && ev->buffer[2]>0) {
			//fprintf(stderr, "ZYNAPTIK MIDI TO GATE-OUT %d NOTE-ON => %d, %d\n", i, ev->buffer[1], ev->buffer[2]);
			digitalWrite(zynswitches[i].pin,0);
		}
		else if (event_type==NOTE_OFF || event_type==NOTE_ON) {
			//fprintf(stderr, "ZYNAPTIK MIDI TO GATE-OUT %d NOTE-OFF => %d, 0\n", i, ev->buffer[1]);
			digitalWrite(zynswitches[i].pin,1);
		}
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
	for (i=0;i<MAX_NUM_ZYNGATEOUTS;i++) {
		zyngateouts[i].enabled=0;
	}

	mcp4728_chip = NULL;

	k_cvin=DEFAULT_K_CVIN;
	k_cvout=DEFAULT_K_CVOUT;

	if (strstr(ZYNAPTIK_CONFIG, "16xDIO")) {
		setup_zynmcp23017(1, ZYNAPTIK_MCP23017_BASE_PIN, ZYNAPTIK_MCP23017_I2C_ADDRESS, ZYNAPTIK_MCP23017_INTA_PIN, ZYNAPTIK_MCP23017_INTB_PIN, zynaptik_mcp23017_bank_ISRs);
		fprintf(stderr, "Setting-up %d x Zynaptik Switches...\n", 16);
		for (i=0;i<16;i++) {
			setup_zynswitch(8+i, ZYNAPTIK_MCP23017_BASE_PIN+i);
		}
	}
	if (strstr(ZYNAPTIK_CONFIG, "4xAD")) {
		//init_ads1115(ZYNAPTIK_ADS1115_BASE_PIN, ZYNAPTIK_ADS1115_I2C_ADDRESS, ADS1115_GAIN_VREF_4_096, ADS1115_RATE_860SPS);
		init_ads1115(ZYNAPTIK_ADS1115_BASE_PIN, ZYNAPTIK_ADS1115_I2C_ADDRESS, ADS1115_GAIN_VREF_6_144, ADS1115_RATE_128SPS);
		init_poll_zynaptik_cvins();
	}
	if (strstr(ZYNAPTIK_CONFIG, "4xDA") || 1) {
		init_mcp4728(ZYNAPTIK_MCP4728_I2C_ADDRESS);
		//init_refresh_zynaptik_cvouts();
	}

	return 1;
}

int end_zynaptik() {
	return 1;
}

//-----------------------------------------------------------------------------

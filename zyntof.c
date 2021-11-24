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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <tof.h>

#include "zynpot.h"
#include "zyncoder.h"
#include "zyntof.h"

//-----------------------------------------------------------------------------
// TCA954X (43/44/48) Stuff => I2C Multiplexer
//-----------------------------------------------------------------------------

int i2cmult_fd = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int init_i2c_multiplexer() {
	i2cmult_fd=wiringPiI2CSetup(TCA954X_I2C_ADDRESS);
	if (i2cmult_fd>0) {
		wiringPiI2CReadReg8(i2cmult_fd, 0x0);
		return 1;
	}
	return 0;
}

void select_zyntof_chan(uint8_t i) {
	if (i2cmult_fd>0) {
		wiringPiI2CWriteReg8(i2cmult_fd, 0x0, 0xF&(0x1<<i));
		//wiringPiI2CWriteReg8(i2cmult_fd, 0x0, 0x1);
		usleep(10);
	}
}

//-----------------------------------------------------------------------------
// VL53L0X Stuff
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Generate MIDI CC from Distance
//-----------------------------------------------------------------------------

void setup_zyntof(uint8_t i, uint8_t midi_evt, uint8_t midi_chan, uint8_t midi_num) {
	zyntofs[i].i = i;
	zyntofs[i].midi_evt = midi_evt;
	zyntofs[i].midi_chan = midi_chan;
	zyntofs[i].midi_num = midi_num;

	if (zyntofs[i].enabled==0) {
		zyntofs[i].val = 0;
		zyntofs[i].midi_val = 0;
		pthread_mutex_lock(&mutex);
		select_zyntof_chan(i);
		if (tofInit(1, VL53L0X_I2C_ADDRESS, VL53L0X_DISTANCE_MODE)!=1) {
			printf("ZynTOF: Can't setup zyntof device VL53L0X-%d.\n", i);
		} else {
			zyntofs[i].enabled = 1;
			int model, rev;
			tofGetModel(&model, &rev);
			printf("ZynTOF: Device VL53L0X-%d successfully opened (model %d, rev %d)\n", i, model, rev);
		}
		pthread_mutex_unlock(&mutex);
	}
}

void disable_zyntof(uint8_t i) {
	zyntofs[i].enabled = 0;
}

void send_zyntof_midi(uint8_t i) {
	uint32_t v;
	if (zyntofs[i].val<MIN_TOF_DISTANCE) {
		v = 0;
	} else if (zyntofs[i].val>MAX_TOF_DISTANCE) {
		v = 16384;
		return;
	} else {
		v = 16384 * (zyntofs[i].val - MIN_TOF_DISTANCE) / (MAX_TOF_DISTANCE - MIN_TOF_DISTANCE);
	}
	if (zyntofs[i].midi_evt==PITCH_BENDING) {
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_pitchbend_change(zyntofs[i].midi_chan, v);
		//printf("ZYNTOF [%d] => MIDI %d\n", i, v);
	} else {
		uint8_t mv = v>>7;
		if (mv!=zyntofs[i].midi_val) {
			//printf("ZYNTOF [%d] => MIDI %d\n", i, mv);
			zyntofs[i].midi_val = mv;
			if (zyntofs[i].midi_evt==CTRL_CHANGE) {
				//Send MIDI event to engines and ouput (ZMOPS)
				internal_send_ccontrol_change(zyntofs[i].midi_chan, zyntofs[i].midi_num, mv);
				//Update zyncoders
				midi_event_zynpot(zyntofs[i].midi_chan, zyntofs[i].midi_num, mv);
				//Send MIDI event to UI
				write_zynmidi_ccontrol_change(zyntofs[i].midi_chan, zyntofs[i].midi_num, mv);
			} else if (zyntofs[i].midi_evt==CHAN_PRESS) {
				//Send MIDI event to engines and ouput (ZMOPS)
				internal_send_chan_press(zyntofs[i].midi_chan, mv);
			} 
		}
	}
}

void * poll_zyntofs(void *arg) {
	int i;
	while (1) {
		for (i=0;i<MAX_NUM_ZYNTOFS;i++) {
			if (zyntofs[i].enabled) {
				pthread_mutex_lock(&mutex);
				select_zyntof_chan(i);
				zyntofs[i].val = tofReadDistance();
				pthread_mutex_unlock(&mutex);
				send_zyntof_midi(i);
				//printf("ZYNTOF [%d] => %d\n", i, zyntofs[i].val);
			}
		}
		usleep(POLL_ZYNTOFS_US);
	}
	return NULL;
}

pthread_t init_poll_zyntofs() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zyntofs, NULL);
	if (err != 0) {
		printf("ZynTOF: Can't create poll thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("ZynTOF: Poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------
// Zyntof Library Initialization
//-----------------------------------------------------------------------------

int init_zyntof() {
	int i;
	for (i=0;i<MAX_NUM_ZYNTOFS;i++) {
		zyntofs[i].enabled=0;
	}
	if (init_i2c_multiplexer()) {
		init_poll_zyntofs();
	}
	return 1;
}

int end_zyntof() {
	return 1;
}

//-----------------------------------------------------------------------------

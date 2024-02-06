/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing MCP23008 using polling (only zynswitches!).
 * 
 * Copyright (C) 2015-2022 Fernando Moyano <jofemodo@zynthian.org>
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

//#define DEBUG

#include "wiringPiI2C.h"
#include "mcp23008.h"
#include "zyncoder.h"

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern zynswitch_t zynswitches[MAX_NUM_ZYNSWITCHES];

zynmcp23008_t zynmcp23008s[MAX_NUM_MCP23008];
int end_poll_zynswitches_flag = 0;

//-----------------------------------------------------------------------------
// MCP23008 functions
//-----------------------------------------------------------------------------

void reset_zynmcp23008s() {
	int i;
	for (i=0;i<MAX_NUM_MCP23008;i++) {
		zynmcp23008s[i].fd = 0;
		zynmcp23008s[i].enabled = 0;
	}
}

int setup_zynmcp23008(uint8_t i, uint16_t base_pin, uint8_t i2c_address) {
	if (i >= MAX_NUM_MCP23008) {
		fprintf(stderr, "ZynCore->setup_zynmcp23008(%d, ...): Invalid index!\n", i);
		return 0;
	}

	// Setup IC using I2C bus
	int fd = wiringPiI2CSetup(i2c_address);
	if (fd < 0) {
		fprintf(stderr, "ZynCore->setup_zynmcp23008(%d, ...): Can't open I2C device at %d!\n", i, i2c_address);
		return 0;
	}
	// Initialize IC
	wiringPiI2CWriteReg8(fd, MCP23x08_IOCON, IOCON_INIT);
	uint8_t olat = wiringPiI2CReadReg8 (fd, MCP23x08_OLAT);

	// setup all the pins as inputs and disable pullups on the zyncoder input
	uint8_t reg = 0xff;
	wiringPiI2CWriteReg8(fd, MCP23x08_IODIR, reg);

	// enable pullups on the unused pins (high two bits)
	reg = 0xff;
	//reg = 0xc0;
	//reg = 0x60;
	wiringPiI2CWriteReg8(fd, MCP23x08_GPPU, reg);

	// disable polarity inversion
	reg = 0;
	wiringPiI2CWriteReg8(fd, MCP23x08_IPOL, reg);

	// disable the comparison to DEFVAL register
	reg = 0;
	wiringPiI2CWriteReg8(fd, MCP23x08_INTCON, reg);

	// Setup data struct
	zynmcp23008s[i].fd = fd;
	zynmcp23008s[i].base_pin = base_pin;
	zynmcp23008s[i].i2c_address = i2c_address;
	reg = wiringPiI2CReadReg8(fd, MCP23x08_GPIO);
	zynmcp23008s[i].enabled = 1;

	#ifdef DEBUG
	fprintf(stderr, "ZynCore->setup_zynmcp23008(%d, ...): I2C %x, base-pin %d\n", i, i2c_address, base_pin);
	#endif

	return 1;
}

int get_last_zynmcp23008_index() {
	int i;
	int li = 0;
	for (i=0;i<MAX_NUM_MCP23008;i++) {
		if (zynmcp23008s[i].enabled!=0) li = i;
	}
	return li;
}

int pin2index_zynmcp23008(uint16_t pin) {
	int i;
	for (i=0;i<MAX_NUM_MCP23008;i++) {
		if (zynmcp23008s[i].enabled) {
			if (pin >= zynmcp23008s[i].base_pin && pin < (zynmcp23008s[i].base_pin+8)) return i;
		}
	}
	return -1;
}

uint8_t read_pins_zynmcp23008(uint8_t i) {
	return wiringPiI2CReadReg8(zynmcp23008s[i].fd, MCP23x08_GPIO);
}

int read_pin_zynmcp23008(uint16_t pin) {
	int i = pin2index_zynmcp23008(pin);
	if (i>=0) {
		uint8_t bit = pin - zynmcp23008s[i].base_pin;
		uint16_t reg;
		if (bit<8) {
			reg = wiringPiI2CReadReg8(zynmcp23008s[i].fd, MCP23x08_GPIO);
			return bitRead(reg, bit);
		} else {
			fprintf(stderr, "ZynCore: read_pin_zynmcp23008(%d) => pin %d out of range!\n", pin);
			return -1;
		}
	}
	fprintf(stderr, "ZynCore: read_pin_zynmcp23008(%d) => invalid pin!\n", pin);
	return -1;
}

//-----------------------------------------------------------------------------
// MCP23008 Polling (only switches)
//-----------------------------------------------------------------------------

//Update Polled (Non-ISR) switches (expanded GPIO with MCP23008 without INT => legacy V1's 2in1 module only!)
void update_polled_zynswitches(int8_t i) {
	struct timespec ts;
	unsigned long int tsus;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	uint8_t rdata = wiringPiI2CReadReg8(zynmcp23008s[i].fd, MCP23x08_GPIO);

	int j;
	uint8_t bit;
	uint8_t status;
	for (j=0; j<MAX_NUM_ZYNSWITCHES; j++) {
		zynswitch_t *zsw = zynswitches + j;
		// This assumes all zynswitches configured with high pin (>=100) are polled!
		if (!zsw->enabled || zsw->pin<100) continue;
		bit = zsw->pin - zynmcp23008s[i].base_pin;
		if (bit<8) status=bitRead(rdata, bit);
		else {
			fprintf(stderr, "ZynCoder->update_polled_zynswitches(%d): Wrong pin number%d!\n", i, zsw->pin);
			status = 0;
		}
		//fprintf(stderr, "POLLING SWITCH %d (%d) => %d\n",i,zsw->pin,status);
		if (status==zsw->status) continue;
		zsw->status=status;
		send_zynswitch_midi(zsw);
		//fprintf(stderr, "POLLING SWITCH %d => STATUS=%d (%lu)\n",i,zsw->status,tsus);
		if (zsw->status==1) {
			if (zsw->tsus>0) {
				unsigned int dtus=tsus-zsw->tsus;
				zsw->tsus=0;
				//Ignore spurious ticks
				if (dtus<1000) return;
				//fprintf(stderr, "Debounced Switch %d\n",i);
				zsw->dtus=dtus;
			}
		} else zsw->tsus=tsus;
	}
}

void * poll_zynswitches(void *arg) {
	while (!end_poll_zynswitches_flag) {
		// Note this only polls first MCP23008 chip!!!
		update_polled_zynswitches(0);
		usleep(POLL_ZYNSWITCHES_US);
	}
	fprintf(stderr, "ZynCore->poll_zynswitches(): Zynswitches poll thread ended flawlessly\n");
	return NULL;
}

pthread_t init_poll_zynswitches() {
	pthread_t tid;
	end_poll_zynswitches_flag = 0;
	int err=pthread_create(&tid, NULL, &poll_zynswitches, NULL);
	if (err != 0) {
		fprintf(stderr, "ZynCore->init_poll_zynswitches(): Can't create zynswitches poll thread :[%s]", strerror(err));
		return 0;
	} else {
		fprintf(stderr, "ZynCore->init_poll_zynswitches(): Zynswitches poll thread created successfully\n");
		return tid;
	}
}

void end_poll_zynswitches() {
	end_poll_zynswitches_flag = 1;
}

//-----------------------------------------------------------------------------

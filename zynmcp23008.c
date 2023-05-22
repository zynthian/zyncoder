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

#include <wiringPi.h>
#include <mcp23x0817.h>
#include <mcp23008.h>

#include "zyncoder.h"

//-----------------------------------------------------------------------------
// MCP23008 Polling (only switches)
//-----------------------------------------------------------------------------

//Switch Polling interval
#define POLL_ZYNSWITCHES_US 10000

//Update Polled (Non-ISR) switches (expanded GPIO with MCP23008 without INT => legacy V1's 2in1 module only!)
void update_polled_zynswitches() {
	struct timespec ts;
	unsigned long int tsus;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	int i;
	uint8_t status;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		zynswitch_t *zsw = zynswitches + i;
		if (!zsw->enabled || zsw->pin<100) continue;
		status=digitalRead(zsw->pin);
		#ifdef DEBUG
		fprintf(stderr, "POLLING SWITCH %d (%d) => %d\n",i,zsw->pin,status);
		#endif
		if (status==zsw->status) continue;
		zsw->status=status;
		send_zynswitch_midi(zsw);
		#ifdef DEBUG
		fprintf(stderr, "POLLING SWITCH %d => STATUS=%d (%lu)\n",i,zsw->status,tsus);
		#endif
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
	while (1) {
		update_polled_zynswitches();
		usleep(POLL_ZYNSWITCHES_US);
	}
	return NULL;
}

pthread_t init_poll_zynswitches() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zynswitches, NULL);
	if (err != 0) {
		fprintf(stderr, "ZynCore: Can't create zynswitches poll thread :[%s]", strerror(err));
		return 0;
	} else {
		fprintf(stderr, "ZynCore: Zynswitches poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------

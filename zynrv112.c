
/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: RV112 + ADS1115
 * 
 * Library for reading RV112 infinite potentiometer using ADS1115.
 * 
 * Copyright (C) 2015-2021 Fernando Moyano <jofemodo@zynthian.org>
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
#include <boost/circular_buffer.hpp>

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <ads1115.h>

#include "zynpot.h"
#include "zynrv112.h"

//-----------------------------------------------------------------------------
// RV112's zynpot API
//-----------------------------------------------------------------------------

void init_rv112s() {
	int i,j;
	boost::circular_buffer<int32_t> *dvbuf;
	for (i=0;i<MAX_NUM_RV112;i++) {
		rv112s[i].enabled = 0;
		rv112s[i].value = 0;
		rv112s[i].value_flag = 0;
		rv112s[i].zpot_i = -1;
		rv112s[i].lastdv = 0;
		rv112s[i].valraw = 0;
		rv112s[i].dvavg = 0;
		rv112s[i].dvbuf = dvbuf = new boost::circular_buffer<int32_t>(DVBUF_SIZE);
		for (j=0;j<DVBUF_SIZE;j++) dvbuf->push_back(0);
	}
}

void end_rv112s() {
	int i;
	for (i=0;i<MAX_NUM_RV112;i++) {
		rv112s[i].enabled = 0;
		rv112s[i].value = 0;
		rv112s[i].value_flag = 0;
		rv112s[i].zpot_i = -1;
		rv112s[i].lastdv = 0;
		rv112s[i].valraw = 0;
		rv112s[i].dvavg = 0;
		delete (boost::circular_buffer<int32_t> *)rv112s[i].dvbuf;
		rv112s[i].dvbuf = NULL;
	}
}

int get_num_rv112s() {
	int i;
	int n = 0;
	for (i=0;i<MAX_NUM_RV112;i++) {
		if (rv112s[i].enabled!=0) n++;
	}
	return n;
}

int setup_rv112(uint8_t i, uint16_t base_pin, uint8_t inv) {
	if (i > MAX_NUM_RV112) {
		printf("ZynCore: RV112 index %d out of range!\n", i);
		return 0;
	}

	uint8_t pos = (i % 2) * 2;
	rv112s[i].base_pin = base_pin;
	if (inv==0) {
		rv112s[i].pinA = base_pin + pos + 1;
		rv112s[i].pinB = base_pin + pos;
	} else {
		rv112s[i].pinA = base_pin + pos;
		rv112s[i].pinB = base_pin + pos + 1;
	}
	rv112s[i].valA = analogRead(rv112s[i].pinA);
	rv112s[i].valB = analogRead(rv112s[i].pinB);
	rv112s[i].curseg = 0;
	rv112s[i].lastdv = 0;
	rv112s[i].value = 0;
	rv112s[i].value_flag = 0;
	rv112s[i].step = 1;
	rv112s[i].min_value = 0;
	rv112s[i].max_value = 127;
	rv112s[i].max_valraw = RV112_ADS1115_MAX_VALRAW;
	rv112s[i].valraw = 0;
	rv112s[i].enabled = 1;
	return 1;
}

int setup_rangescale_rv112(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, int32_t step) {
	if (i>=MAX_NUM_RV112 || rv112s[i].enabled==0) {
		printf("ZynCore->setup_rangescale_rv112(%d, ...): Invalid index!\n", i);
		return 0;
	}
	if (min_value>=max_value) {
		printf("ZynCore->setup_rangescale_rv112(%d, %d, %d, ...): Invalid range!\n", i, min_value, max_value);
		return 0;
	}

	if (value>max_value) value = max_value;
	else if (value<min_value) value = min_value;

	rv112s[i].step = step;
	rv112s[i].value = value;
	rv112s[i].min_value = min_value;
	rv112s[i].max_value = max_value;
	
	if (step==0) rv112s[i].max_valraw = RV112_ADS1115_MAX_VALRAW;
	else rv112s[i].max_valraw = RV112_ADS1115_MAX_VALRAW * step;
	rv112s[i].valraw = (rv112s[i].max_valraw - 1) * (value - min_value) / (1 + max_value - min_value);

	return 1;
}

int32_t get_value_rv112(uint8_t i) {
	if (i>=MAX_NUM_RV112 || rv112s[i].enabled==0) {
		printf("ZynCore->get_value_rv112(%d): Invalid index!\n", i);
		return 0;
	}
	rv112s[i].value_flag = 0;
	return rv112s[i].value;
}

uint8_t get_value_flag_rv112(uint8_t i) {
	if (i>=MAX_NUM_RV112 || rv112s[i].enabled==0)  {
		printf("ZynCore->get_value_rv112(%d): Invalid index!\n", i);
		return 0;
	}
	return rv112s[i].value_flag;
}


int set_value_rv112(uint8_t i, int32_t v) {
	if (i>=MAX_NUM_RV112 || rv112s[i].enabled==0) {
		printf("ZynCore->get_value_rv112(%d): Invalid index!\n", i);
		return 0;
	}
	if (v>rv112s[i].max_value) v = rv112s[i].max_value;
	else if (v<rv112s[i].min_value) v = rv112s[i].min_value;
	if (rv112s[i].value!=v) {
		rv112s[i].value = v;
		rv112s[i].valraw = (rv112s[i].max_valraw-1) * (rv112s[i].value - rv112s[i].min_value) / (1 + rv112s[i].max_value - rv112s[i].min_value);
		//rv112s[i].value_flag = 1;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// RV112 specific functions
//-----------------------------------------------------------------------------

int16_t read_rv112(uint8_t i) {
	int32_t vA = analogRead(rv112s[i].pinA);
	int32_t vB = analogRead(rv112s[i].pinB);
	int16_t d = 0;

	switch (rv112s[i].curseg) {
		case 0:
			if (vB < RV112_ADS1115_RANGE_25) {
				d = rv112s[i].valA - vA;
				break;
			}
			else if (vA > RV112_ADS1115_RANGE_75) {
				d = rv112s[i].valB - vB;
				rv112s[i].curseg = 1;
				break;
			}
			else if (vA < RV112_ADS1115_RANGE_25) {
				d = vB - rv112s[i].valB;
				rv112s[i].curseg = 3;
				break;
			}
			else if (vB > RV112_ADS1115_RANGE_75) {
				d = vA - rv112s[i].valA;
				rv112s[i].curseg = 2;
				break;
			}
			break;

		case 1:
			if (vA > RV112_ADS1115_RANGE_75) {
				d = rv112s[i].valB - vB;
				break;
			}
			else if (vB > RV112_ADS1115_RANGE_75) {
				d = vA - rv112s[i].valA;
				rv112s[i].curseg = 2;
				break;
			}
			else if (vB < RV112_ADS1115_RANGE_25) {
				d = rv112s[i].valA - vA;
				rv112s[i].curseg = 0;
				break;
			}
			else if (vA < RV112_ADS1115_RANGE_25) {
				d = vB - rv112s[i].valB;
				rv112s[i].curseg = 3;
				break;
			}
			break;

		case 2:
			if (vB > RV112_ADS1115_RANGE_75) {
				d = vA - rv112s[i].valA;
				break;
			}
			else if (vA < RV112_ADS1115_RANGE_25) {
				d = vB - rv112s[i].valB;
				rv112s[i].curseg = 3;
				break;
			}
			else if (vA > RV112_ADS1115_RANGE_75) {
				d = rv112s[i].valB - vB;
				rv112s[i].curseg = 1;
				break;
			}
			else if (vB < RV112_ADS1115_RANGE_25) {
				d = rv112s[i].valA - vA;
				rv112s[i].curseg = 0;
				break;
			}
			break;

		case 3:
			if (vA < RV112_ADS1115_RANGE_25) {
				d = vB - rv112s[i].valB;
				break;
			}
			else if (vB < RV112_ADS1115_RANGE_25) {
				d = rv112s[i].valA - vA;
				rv112s[i].curseg = 0;
				break;
			}
			else if (vB > RV112_ADS1115_RANGE_75) {
				d = vA - rv112s[i].valA;
				rv112s[i].curseg = 2;
				break;
			}
			else if (vA > RV112_ADS1115_RANGE_75) {
				d = rv112s[i].valB - vB;
				rv112s[i].curseg = 1;
				break;
			}
			break;
	}

	rv112s[i].valA = vA;
	rv112s[i].valB = vB;

	//printf("vA = %d, vB = %d\n", vA, vB);

	return d / RV112_ADS1115_NOISE_DIV;
}

void * poll_rv112(void *arg) {
	int i;
	int32_t vr, v;
	boost::circular_buffer<int32_t> *dvbuf;
	while (1) {
		for (i=0;i<MAX_NUM_RV112;i++) {
			if (rv112s[i].enabled) {
				rv112s[i].lastdv = read_rv112(i);
				// Calculate moving average for adaptative speed variation
				if (rv112s[i].step==0) {
					dvbuf = (boost::circular_buffer<int32_t> *)rv112s[i].dvbuf;
					dvbuf->push_back(abs(rv112s[i].lastdv));
					rv112s[i].dvavg += (*dvbuf)[DVBUF_SIZE-1] - (*dvbuf)[0];
					//fprintf(stdout, "DVAVG %d = %d\n", i, rv112s[i].dvavg);
				}
				if (rv112s[i].lastdv!=0) {
					// Adaptative speed variation using a moving average 
					if (rv112s[i].step==0) {
						if (rv112s[i].dvavg < 1000) rv112s[i].lastdv /= 8;
						else if (rv112s[i].dvavg < 2000) rv112s[i].lastdv /= 4;
						else if (rv112s[i].dvavg < 4000) rv112s[i].lastdv /= 2;
					}
					vr = rv112s[i].valraw + rv112s[i].lastdv;
					if (vr>=rv112s[i].max_valraw) vr = rv112s[i].max_valraw-1;
					else if (vr<0) vr = 0;
					if (vr!=rv112s[i].valraw) {
						rv112s[i].valraw = vr;
						v = rv112s[i].min_value + vr * (1 + rv112s[i].max_value - rv112s[i].min_value) / rv112s[i].max_valraw;
						if (v!=rv112s[i].value) {
							rv112s[i].value = v;
							rv112s[i].value_flag = 1;
							send_zynpot(rv112s[i].zpot_i);
							//fprintf(stdout, "V%d = %d\n", i, rv112s[i].value);
						}
					}
				}
			}
		}
	}
	return NULL;
}

pthread_t init_poll_rv112() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_rv112, NULL);
	if (err != 0) {
		fprintf(stderr, "ZynCore: Can't create RV112 poll thread :[%s]", strerror(err));
		return 0;
	} else {
		fprintf(stdout, "ZynCore: RV112 poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------

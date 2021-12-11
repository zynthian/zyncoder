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

#include <stdlib.h>
#include <wiringPi.h>

#include "zynads1115.h"

//-----------------------------------------------------------------------------
// ADS1115 data
//-----------------------------------------------------------------------------

#define ADS1115_VDD 3.3

#define MAX_NUM_ADS1115 2
struct wiringPiNodeStruct * ads1115_nodes[MAX_NUM_ADS1115];

//-----------------------------------------------------------------------------
// RV112 data
//-----------------------------------------------------------------------------

#define RV112_ADS1115_RANGE_100 (int)(0xFFFF*ADS1115_VDD/4.096)/2
#define RV112_ADS1115_RANGE_25 RV112_ADS1115_RANGE_100/4
#define RV112_ADS1115_RANGE_50 RV112_ADS1115_RANGE_100/2
#define RV112_ADS1115_RANGE_75 3*(RV112_ADS1115_RANGE_100/4)

//#define RV112_ADS1115_NOISE_DIV 32
#define RV112_ADS1115_NOISE_DIV 8
#define RV112_ADS1115_MAX_VALRAW 1270

#define MAX_NUM_RV112 4

typedef struct rv112_st {
	uint8_t enabled;
	int32_t min_value;
	int32_t max_value;
	int32_t step;
	int32_t value;
	uint8_t value_flag;
	int8_t zpot_i;

	// Next fields are RV112-specific
	uint16_t base_pin;
	uint16_t pinA;
	uint16_t pinB;

	int32_t valA;
	int32_t valB;
	uint8_t curseg;
	int16_t lastdv;
	int32_t valraw;
	int32_t max_valraw;
} rv112_t;
rv112_t rv112s[MAX_NUM_RV112];


#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// RV112's zynpot API
//-----------------------------------------------------------------------------

void reset_rv112s();
int get_num_rv112s();

int setup_rv112(uint8_t i, uint16_t base_pin, uint8_t inv);
int setup_rangescale_rv112(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, int32_t step);

int32_t get_value_rv112(uint8_t i);
uint8_t get_value_flag_rv112(uint8_t i);
int set_value_rv112(uint8_t i, int32_t v);

//-----------------------------------------------------------------------------
// RV112 specific functions
//-----------------------------------------------------------------------------

int16_t read_rv112(uint8_t i);
pthread_t init_poll_rv112();

//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

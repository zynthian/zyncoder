/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zynpot, wrapper library for rotaries
 * 
 * Library for interfacing rotaries of several types
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

#include <lo/lo.h>

//-----------------------------------------------------------------------------
// Zynpot data
//-----------------------------------------------------------------------------

#define ZYNPOT_NONE 0
#define ZYNPOT_ZYNCODER 1
#define ZYNPOT_RV112 2

#define MAX_NUM_ZYNPOTS 4

typedef struct zynpot_data_st {
	uint8_t enabled;
	int32_t step;
	int32_t value;
	int8_t zpot_i;
} zynpot_data_t;


typedef struct zynpot_st {
	uint8_t type;
	uint8_t i;
	zynpot_data_t *data;

	// Function pointers
	int (*setup_behaviour)(uint8_t, int32_t);
	int32_t (*get_value)(uint8_t);
} zynpot_t;
zynpot_t zynpots[MAX_NUM_ZYNPOTS];

void (*zynpot_cb)(int8_t, int32_t);

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Zynpot common API
//-----------------------------------------------------------------------------

/**
* Reset all zynpots
* @see setup_zynpot()
*/
void reset_zynpots();

/**
* Get the number of configured zynpots
* @return number of zynpots bond to a zyncoder or rv112 object
* @see setup_zynpot()
*/
int get_num_zynpots();

/**
* Set the callback function for zynpots
* @param i index of zynpot to setup
* @param cbfunc function being called when zynpot's value changes
*/
void setup_zynpot_cb(void (*cbfunc)(int8_t, int32_t));

/**
* Initialize a zynpot object, binding it to a zyncoder or rv112 object
* @param i index of zynpot to initialize
* @param type type of zynpot (ZYNPOT_NONE, ZYNPOT_ZYNCODER | ZYNPOT_RV112)
* @param ii index of zyncoder/rv112 to bind to
* @return 1 => OK, 0 => Error
* @see zyncoder.h
* @see zynrv112.h
*/
int setup_zynpot(uint8_t i, uint8_t type, uint8_t ii);

/**
* Setup the behaviour of a zynpot object
* @param i zynpot index
* @param step 0 => dynamic step, >=1 => step size
* @return 1 => OK, 0 => Error
*/
int setup_behaviour_zynpot(uint8_t i, int32_t step);

/**
* Get the value of a zynpot object
* It shouldn't be used if a CB function has been setup
* @param i zynpot index
* @param step 0 => dynamic step, >=1 => step size
* @return the accumulated value from the last call
*/
int32_t get_value_zynpot(uint8_t i);


//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif


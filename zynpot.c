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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zynpot.h"
#include "zyncoder.h"
#include "zynrv112.h"

//-----------------------------------------------------------------------------
// Zynpot common API
//-----------------------------------------------------------------------------

void reset_zynpots() {
	int i;
	for (i=0;i<MAX_NUM_ZYNPOTS;i++) {
		zynpots[i].type = ZYNPOT_NONE;
		zynpots[i].data = NULL;
	}
	zynpot_cb = NULL;
}

int get_num_zynpots() {
	int i;
	int n = 0;
	for (i=0;i<MAX_NUM_ZYNPOTS;i++) {
		if (zynpots[i].type!=ZYNPOT_NONE) n++;
	}
	return n;
}

void setup_zynpot_cb(void (*cbfunc)(int8_t, int32_t)) {
	zynpot_cb = cbfunc;
}

int setup_zynpot(uint8_t i, uint8_t type, uint8_t ii) {
	if (i>MAX_NUM_ZYNPOTS) {
		fprintf(stderr, "ZynCore->setup_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	zynpots[i].type = type;
	zynpots[i].i = ii;
	switch (type) {
		case ZYNPOT_ZYNCODER:
			zyncoders[ii].zpot_i = (int8_t)i;
			zynpots[i].data = (zynpot_data_t *) &zyncoders[ii];
			zynpots[i].setup_behaviour = setup_behaviour_zyncoder;
			zynpots[i].get_value = get_value_zyncoder;
			break;
		case ZYNPOT_RV112:
			rv112s[ii].zpot_i = (int8_t)i;
			zynpots[i].data = (zynpot_data_t *) &rv112s[ii];
			zynpots[i].setup_behaviour = setup_behaviour_rv112;
			zynpots[i].get_value = get_value_rv112;
			break;
	}
	return 1;
}

int setup_behaviour_zynpot(uint8_t i, int32_t step) {
	if (i>MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		fprintf(stderr, "ZynCore->setup_step_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	return zynpots[i].setup_behaviour(zynpots[i].i, step);
}

int32_t get_value_zynpot(uint8_t i) {
	if (i>=MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		fprintf(stderr, "ZynCore->get_value_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	return zynpots[i].get_value(zynpots[i].i);
}

//-----------------------------------------------------------------------------

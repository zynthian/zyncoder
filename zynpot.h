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
	int32_t min_value;
	int32_t max_value;
	int32_t step;
	uint8_t inv;
	int32_t value;
	uint8_t value_flag;
	int8_t zpot_i;
} zynpot_data_t;


typedef struct zynpot_st {
	uint8_t type;
	uint8_t i;
	zynpot_data_t *data;

	uint8_t midi_chan;
	uint8_t midi_cc;

	uint16_t osc_port;
	lo_address osc_lo_addr;
	char osc_path[512];

	// Function pointers
	int (*setup_rangescale)(uint8_t, int32_t, int32_t, int32_t, int32_t);
	int32_t (*get_value)(uint8_t);
	uint8_t (*get_value_flag)(uint8_t);
	int (*set_value)(uint8_t, int32_t);
} zynpot_t;
zynpot_t zynpots[MAX_NUM_ZYNPOTS];


#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Zynpot common API
//-----------------------------------------------------------------------------

void reset_zynpots();
int get_num_zynpots();

int setup_zynpot(uint8_t i, uint8_t type, uint8_t ii);
int setup_rangescale_zynpot(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, int32_t step);

int32_t get_value_zynpot(uint8_t i);
uint8_t get_value_flag_zynpot(uint8_t i);
int set_value_zynpot(uint8_t i, int32_t v, int send);
int set_value_noflag_zynpot(uint8_t i, int32_t v);

//-----------------------------------------------------------------------------
// Zynpot MIDI & OSC API
//-----------------------------------------------------------------------------

int setup_midi_zynpot(uint8_t i, uint8_t midi_chan, uint8_t midi_cc);
int setup_osc_zynpot(uint8_t i, char *osc_path);
int send_zynpot(uint8_t i);
int midi_event_zynpot(uint8_t midi_chan, uint8_t midi_cc, uint8_t val);

//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif


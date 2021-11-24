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
// Generic Rotaries
//-----------------------------------------------------------------------------

int setup_zynpot(uint8_t i, uint8_t type, uint8_t ii) {
	if (i>MAX_NUM_ZYNPOTS) {
		printf("ZynCore: Zynpot index %d out of range!\n", i);
		return 0;
	}
	zynpots[i].type = type;
	zynpots[i].i = ii;
	switch (type) {
		case ZYNPOT_ZYNCODER:
			zynpots[i].data = zyncoders + ii;
			zynpots[i].setup_rangescale = setup_rangescale_zyncoder;
			zynpots[i].get_value = get_value_zyncoder;
			zynpots[i].get_value_flag = get_value_flag_zyncoder;
			zynpots[i].set_value = set_value_zyncoder;
			break;
		case ZYNPOT_RV112:
			zynpots[i].data = rv112s + ii;
			zynpots[i].setup_rangescale = setup_rangescale_rv112;
			zynpots[i].get_value = get_value_rv112;
			zynpots[i].get_value_flag = get_value_flag_rv112;
			zynpots[i].set_value = set_value_rv112;
			break;
	}
	return 1;
}

int setup_rangescale_zynpot(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, unsigned int step) {
	if (i>MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore: Zynpot index %d out of range!\n", i);
		return 0;
	}
	return zynpots[i].setup_rangescale(zynpots[i].i, min_value, max_value, value, step);
}

int setup_midi_zynpot(uint8_t i, uint8_t midi_chan, uint8_t midi_cc) {
	if (i>MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore: Zynpot index %d out of range!\n", i);
		return 0;
	}
	zynpot_t *zpt = zynpots + i;

	//Setup MIDI/OSC bindings
	if (midi_chan>15) midi_chan=0;
	if (midi_cc>127) midi_cc=1;
	zpt->midi_chan = midi_chan;
	zpt->midi_cc = midi_cc;

	return 1;
}


int setup_osc_zynpot(uint8_t i, char *osc_path) {
	if (i>MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore: Zynpot index %d out of range!\n", i);
		return 0;
	}
	zynpot_t *zpt = zynpots + i;

	//printf("OSC PATH: %s\n",osc_path);
	if (osc_path) {
		char *osc_port_str=strtok(osc_path,":");
		zpt->osc_port=atoi(osc_port_str);
		if (zpt->osc_port>0) {
			zpt->osc_lo_addr=lo_address_new(NULL, osc_port_str);
			strcpy(zpt->osc_path,strtok(NULL,":"));
		} else {
			zpt->osc_path[0] = 0;
		}
	} else {
		zpt->osc_path[0] = 0;
	}
	return 1;
}

int32_t get_value_zynpot(uint8_t i) {
	if (i>=MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore->get_value_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	return zynpots[i].get_value(zynpots[i].i);
}

uint8_t get_value_flag_zynpot(uint8_t i) {
	if (i>=MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore->get_value_flag_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	return zynpots[i].get_value_flag(zynpots[i].i);
}


int set_value_zynpot(uint8_t i, int32_t v, int send) {
	if (i>=MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore->set_value_zynpot(%d, %d, %d): Invalid index!\n", i, v, send);
		return 0;
	}
	zynpots[i].set_value(zynpots[i].i, v);
	if (send) send_zynpot(i);
	return 1;
}

//-----------------------------------------------------------------------------
// MIDI & OSC event generation/sync
//-----------------------------------------------------------------------------

int send_zynpot(uint8_t i) {
	if (i>=MAX_NUM_ZYNPOTS || zynpots[i].type==ZYNPOT_NONE) {
		printf("ZynCore->send_zynpot(%d): Invalid index!\n", i);
		return 0;
	}
	zynpot_t *zpt = zynpots + i;

	if (zpt->midi_cc>0) {
		int32_t value = zpt->get_value(zpt->i);
		//Send to MIDI output
		internal_send_ccontrol_change(zpt->midi_chan, zpt->midi_cc, value);
		//Send to MIDI controller feedback => TODO: Reverse Mapping!!
		//ctrlfb_send_ccontrol_change(zpt->midi_chan, zpt->midi_cc, value);
		//printf("ZynCore: SEND MIDI CH#%d, CTRL %d = %d\n",zpt->midi_chan, zpt->midi_cc, value);
	} else if (zpt->osc_lo_addr!=NULL && zpt->osc_path[0]) {
		int32_t value = zpt->get_value(zpt->i);
		//TODO *************************
		//TODO FIX THIS SHIT!!!
		//TODO *************************
		//if (zyncoder->step >= 8) {
		if (0) {
			if (value>=64) {
				lo_send(zpt->osc_lo_addr, zpt->osc_path, "T");
				//printf("SEND OSC %s => T\n",zyncoder->osc_path);
			} else {
				lo_send(zpt->osc_lo_addr, zpt->osc_path, "F");
				//printf("SEND OSC %s => F\n",zpt->osc_path);
			}
		} else {
			lo_send(zpt->osc_lo_addr, zpt->osc_path, "i", value);
			//printf("SEND OSC %s => %d\n",zpt->osc_path,value);
		}
	}
	return 1;
}

//Update zyncoder value => TODO Optimize this function!
int midi_event_zynpot(uint8_t midi_chan, uint8_t midi_cc, uint8_t val) {
	int i;
	for (i=0;i<MAX_NUM_ZYNPOTS;i++) {
		if (zynpots[i].type && zynpots[i].midi_chan==midi_chan && zynpots[i].midi_cc==midi_cc) {
			zynpots[i].set_value(zynpots[i].i, val);
			//zyncoders[j].value=val;
			//zyncoders[j].subvalue=val*ZYNCODER_TICKS_PER_RETENT;
			//fprintf(stdout, "ZynMidiRouter: MIDI CC (%x, %x) => UI",midi_chan,midi_cc);
		}
	}
	return 1;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

void reset_zynpots() {
	int i;
	for (i=0;i<MAX_NUM_ZYNPOTS;i++) {
		zynpots[i].type = ZYNPOT_NONE;
		zynpots[i].data = NULL;
		zynpots[i].midi_chan = 0;
		zynpots[i].midi_cc = 0;
		zynpots[i].osc_path[0] = 0;
	}
}

//-----------------------------------------------------------------------------

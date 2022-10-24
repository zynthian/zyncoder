/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ZynMidiSwap Library
 * 
 * MIDI CC swap library: Implements MIDI CC swap
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

#include "zynmidiswap.h"

//-----------------------------------------------------------------------------
// Swap CC mapping => GRAPH THEORY
//-----------------------------------------------------------------------------
//	Definitions:
//-----------------------------------------------------------------------------
//	+ Node(c,n): 16 * 128 nodes
//	+ struct midi_event_st cc_swap[16][128] 
//		+ It's a weighted graph => Arrows have type: THRU_EVENT(T), SWAP_EVENT(S), CTRL_CHANGE(M)
//		+ Arrows of type T begins and ends in the same node.
//		+ Applied only to CC events => cc_swap[c][n], arrows Aij FROM Ni(c,n) TO Nj(.chan,.num), of type .type
//	+ Graph States: ST(t) => State "t" of Graph
//		+ Initial State ST(0) => One Arrow Aii of type T on every node Ni
//-----------------------------------------------------------------------------
//	Rules & Algorithms
//-----------------------------------------------------------------------------
//  + Rule A: Every node recives one arrow and emits one arrow
//		+ ALGORITHM: State Change 
//			=> ST(t) => ST(t+1) => Add/Remove an arrow Aij of type CTRL_CHANGE
//			=> Remove/Add needed extra arrows for enforcing Rule A.
//			=> Only extra arrows of type T & S can be removed/added to enforce Rule A
//			=> Added CTRL_CHANGE arrow can't begins/ends in a node that currently is the beggining/end of another CTRL_CHANGE arrow
//			=> In such a case, the previously existing CTRL_CHANGE arrow must be explicitly removed before
//	+ Rule B: All paths are closed 
//		+ ALGORITHM: Find the node Nh pointing to Ni
//			=> from Ni, follow the path to find Nh that points to Ni
//-----------------------------------------------------------------------------


void _set_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from, midi_event_type type_to, uint8_t chan_to, uint8_t num_to) {
	midi_event_t *cc_swap=&midi_filter.cc_swap[chan_from][num_from];
	cc_swap->type = type_to;
	cc_swap->chan = chan_to;
	cc_swap->num = num_to;
}

midi_event_t *_get_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from) {
	return &midi_filter.cc_swap[chan_from][num_from];
}

void _del_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from) {
	midi_filter.cc_swap[chan_from][num_from].type = THRU_EVENT;
	midi_filter.cc_swap[chan_from][num_from].chan = chan_from;
	midi_filter.cc_swap[chan_from][num_from].num = num_from;
}


int get_mf_arrow_from(uint8_t chan, uint8_t num, mf_arrow_t *arrow) {
	midi_event_t *to = _get_midi_filter_cc_swap(chan,num);
	if (!to) return 0;
	arrow->chan_from = chan;
	arrow->num_from = num;
	arrow->chan_to = to->chan;
	arrow->num_to = to->num;
	arrow->type = to->type;
#ifdef DEBUG
	//fprintf(stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_from %d, %d => %d, %d (%d)\n", arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
#endif
	return 1;
}

int get_mf_arrow_to(uint8_t chan, uint8_t num, mf_arrow_t *arrow) {
	int limit = 0;
	arrow->chan_to = chan;
	arrow->num_to = num;
	//Follow the rabbit ... ;-)
	do {
		if (++limit > 128) {
			fprintf(stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to => Not Closed Path or it's too long!\n");
			return 0;
		}
		if (!get_mf_arrow_from(arrow->chan_to, arrow->num_to, arrow)) {
			fprintf(stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to => Bad Path!\n");
			return 0;
		}
#ifdef DEBUG
		fprintf(stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to %d, %d, %d => %d, %d (%d)\n", limit, arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
#endif
	} while (arrow->chan_to != chan || arrow->num_to != num);
	//Return 1 => last arrow pointing to origin!
	return 1;
}


int set_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from, uint8_t chan_to, uint8_t num_to) {
	//---------------------------------------------------------------------------
	//Get current arrows "from origin" and "to destiny"
	//---------------------------------------------------------------------------
	mf_arrow_t arrow_from;
	mf_arrow_t arrow_to;
	if (!get_mf_arrow_from(chan_from, num_from, &arrow_from))
		return 0;
	if (!get_mf_arrow_to(chan_to, num_to, &arrow_to))
		return 0;

	//---------------------------------------------------------------------------
	//Check validity of new CC Arrow
	//---------------------------------------------------------------------------
	//No CTRL_CHANGE arrow from same origin
	if (arrow_from.type == CTRL_CHANGE) {
		fprintf(stderr, "ZynMidiRouter: MIDI filter CC set swap-map => Origin already has a CTRL_CHANGE map!\n");
		return 0;
	}
	//No CTRL_CHANGE arrow to same destiny
	if (arrow_to.type == CTRL_CHANGE) {
		fprintf(stderr, "ZynMidiRouter: MIDI filter CC set swap-map => Destiny already has a CTRL_CHANGE map!\n");
		return 0;
	}

	//Create CC Map from => to
	_set_midi_filter_cc_swap(chan_from, num_from, CTRL_CHANGE, chan_to, num_to);
#ifdef DEBUG
	fprintf(stderr, "ZynMidiRouter: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", chan_from, num_from, chan_to, num_to, CTRL_CHANGE);
#endif
	
	//Create extra mapping overwriting current extra mappings, to enforce Rule A
	midi_event_type type=SWAP_EVENT;
	if (arrow_from.chan_to==arrow_to.chan_from && arrow_from.num_to==arrow_to.num_from)
		type=THRU_EVENT;
	_set_midi_filter_cc_swap(arrow_to.chan_from, arrow_to.num_from, type,arrow_from.chan_to, arrow_from.num_to);
	//set_midi_filter_cc_swap(arrow_from.chan_to, arrow_from.num_to, type,arrow_to.chan_from, arrow_to.num_from);
#ifdef DEBUG
	fprintf(stderr, "ZynMidiRouter: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", arrow_to.chan_from, arrow_to.num_from, arrow_from.chan_to, arrow_from.num_to, type);
#endif

	return 1;
}


int del_midi_filter_cc_swap(uint8_t chan, uint8_t num) {
	//---------------------------------------------------------------------------
	//Get current arrow Axy (from origin to destiny)
	//---------------------------------------------------------------------------
	mf_arrow_t arrow;
	if (!get_mf_arrow_from(chan, num, &arrow))
		return 0;

	//---------------------------------------------------------------------------
	//Get current arrow pointing to origin (Ajx)
	//---------------------------------------------------------------------------
	mf_arrow_t arrow_to;
	if (!get_mf_arrow_to(chan, num, &arrow_to))
		return 0;

	//---------------------------------------------------------------------------
	//Get current arrow from destiny (Ayk)
	//---------------------------------------------------------------------------
	mf_arrow_t arrow_from;
	if (!get_mf_arrow_from(arrow.chan_to, arrow.num_to, &arrow_from))
		return 0;

	//---------------------------------------------------------------------------
	//Create/Delete extra arrows for enforcing Rule A
	//---------------------------------------------------------------------------

	if (arrow_to.type != SWAP_EVENT && arrow_from.type != SWAP_EVENT) {
		//Create Axy of type SWAP_EVENT => Replace CTRL_CHANGE by SWAP_EVENT
		_set_midi_filter_cc_swap(arrow.chan_from, arrow.num_from, SWAP_EVENT, arrow.chan_to, arrow.num_to);
	} else {
		if (arrow_to.type == SWAP_EVENT) {
			//Create Axx of type THRU_EVENT
			_del_midi_filter_cc_swap(arrow.chan_from, arrow.num_from);
		} else {
			//Create Axk of type SWAP_EVENT
			_set_midi_filter_cc_swap(arrow.chan_from, arrow.num_from, SWAP_EVENT, arrow_from.chan_to, arrow_from.num_to);
		}
		if (arrow_from.type == SWAP_EVENT) {
			//Create Ayy of type THRU_EVENT
			_del_midi_filter_cc_swap(arrow.chan_to, arrow.num_to);
		} else {
			//Create Ajy of type SWAP_EVENT
			_set_midi_filter_cc_swap(arrow_to.chan_from, arrow_to.num_from, SWAP_EVENT, arrow.chan_to, arrow.num_to);
		}
	}

	return 1;
}

uint16_t get_midi_filter_cc_swap(uint8_t chan, uint8_t num) {
	mf_arrow_t arrow;
	if (!get_mf_arrow_to(chan, num, &arrow))
		return 0;
	else {
		uint16_t res = (uint16_t)arrow.chan_from << 8 | (uint16_t)arrow.num_from;
		//fprintf(stderr,"GET CC SWAP %d, %d => %x\n",chan,num,res);
		return res;
	}
}

void reset_midi_filter_cc_swap() {
	int i, j;
	for (i = 0; i <16 ; i++) {
		for (j = 0; j < 128; j++) {
			midi_filter.cc_swap[i][j].type = THRU_EVENT;
			midi_filter.cc_swap[i][j].chan = i;
			midi_filter.cc_swap[i][j].num = j;
		}
	}
}



//-----------------------------------------------------------------------------
// Initialization code => init_midi_router()
//-----------------------------------------------------------------------------

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 128; j++) {
			midi_filter.cc_swap[i][j].type = THRU_EVENT;
			midi_filter.cc_swap[i][j].chan = i;
			midi_filter.cc_swap[i][j].num = j;
		}
	}


//-----------------------------------------------------------------------------
// This fragment should be inserted in jack_process()
//-----------------------------------------------------------------------------

		// Swap Mapping
		//fprintf(stderr, "PRESWAP MIDI EVENT: %d, %d, %d\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);
		if ((zmip->flags & FLAG_ZMIP_CCSWAP) && event_type == CTRL_CHANGE) {
			midi_event_t * cc_swap = &(midi_filter.cc_swap[event_chan][event_num]);
			//fprintf(stderr, "ZynMidiRouter: CC Swap %x, %x => ",ev->buffer[0],ev->buffer[1]);
			event_chan = cc_swap->chan;
			event_num = cc_swap->num;
			ev->buffer[0] = (event_type << 4) | event_chan;
			ev->buffer[1] = event_num;
			ev->buffer[2] = event_val;
			ev->size = 3; //!@todo Is it safe to assume we can change the size of the event buffer?
			//fprintf(stderr, "MIDI MSG => %x, %x\n",ev->buffer[0],ev->buffer[1]);
		}
		//fprintf(stderr, "POSTSWAP MIDI EVENT: %d, %d, %d\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);


//-----------------------------------------------------------------------------

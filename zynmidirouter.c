/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ZynMidiRouter Library
 * 
 * MIDI router library: Implements the MIDI router & filter 
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#include "zynpot.h"
#include "zynmidirouter.h"

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------


int init_zynmidirouter() {
	if (!init_zynmidi_buffer())
		return 0;
	if (!init_midi_router()) {
		end_zynmidi_buffer();
		return 0;
	}
	if (!init_jack_midi("ZynMidiRouter")) {
		end_midi_router();
		end_zynmidi_buffer();
		return 0;
	}
	return 1;
}

int end_zynmidirouter() {
	if (!end_jack_midi())
		return 0;
	if (!end_midi_router())
		return 0;
	if (!end_zynmidi_buffer())
		return 0;
	return 1;
}

//-----------------------------------------------------------------------------
// MIDI filter management
//-----------------------------------------------------------------------------

int init_midi_router() {
	int i, j, k;

	midi_filter.master_chan =- 1;
	midi_filter.active_chan = -1;
	midi_filter.last_active_chan = -1;
	midi_filter.tuning_pitchbend = -1;
	midi_filter.system_events = 1;
	midi_filter.cc_automode = 1;
	midi_learning_mode = 0;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			midi_filter.clone[i][j].enabled = 0;
			memset(midi_filter.clone[i][j].cc, 0, 128);
			for (k = 0; k < sizeof(default_cc_to_clone); k++) {
				midi_filter.clone[i][j].cc[default_cc_to_clone[k] & 0x7F] = 1;
			}
		}
	}
	for (i = 0; i < 16; i++) {
		midi_filter.noterange[i].note_low = 0;
		midi_filter.noterange[i].note_high = 127;
		midi_filter.noterange[i].octave_trans = 0;
		midi_filter.noterange[i].halftone_trans = 0;
		midi_filter.last_pb_val[i] = 8192;
	}
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 0; k < 128; k++) {
				midi_filter.event_map[i][j][k].type = THRU_EVENT;
				midi_filter.event_map[i][j][k].chan = j;
				midi_filter.event_map[i][j][k].num = k;
			}
		}
	}
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 128; j++) {
			midi_filter.cc_swap[i][j].type = THRU_EVENT;
			midi_filter.cc_swap[i][j].chan = i;
			midi_filter.cc_swap[i][j].num = j;
		}
	}
	memset(midi_filter.ctrl_mode, 0, 16 * 128);
	memset(midi_filter.ctrl_relmode_count, 0, 16 * 128);
	memset(midi_filter.last_ctrl_val, 0, 16 * 128);
	memset(midi_filter.note_state, 0, 16 * 128);

	return 1;
}

int end_midi_router() {
	return 1;
}

//MIDI special featured channels

void set_midi_master_chan(int chan) {
	if (chan > 15 || chan < -1) {
		fprintf(stderr, "ZynMidiRouter: MIDI Master channel (%d) is out of range!\n",chan);
		return;
	}
	midi_filter.master_chan = chan;
}

int get_midi_master_chan() {
	return midi_filter.master_chan;
}

void set_midi_active_chan(int chan) {
	if (chan > 15 || chan < -1) {
		fprintf(stderr, "ZynMidiRouter: MIDI Active channel (%d) is out of range!\n",chan);
		return;
	}
	if (chan != midi_filter.active_chan) {
		midi_filter.last_active_chan = midi_filter.active_chan;
		midi_filter.active_chan = chan;
	}
}

int get_midi_active_chan() {
	return midi_filter.active_chan;
}

//MIDI filter pitch-bending fine-tuning

void set_midi_filter_tuning_freq(double freq) {
	if (freq == 440.0) {
		midi_filter.tuning_pitchbend = -1;
		// Clear pitchbend already applied
		for (int i = 0; i < 16; ++i)
			ui_send_pitchbend_change(i, 0x2000); //!@todo Ideally reset only playing channels to current pitchbend offset
	} else {
		double pb = 6 * log((double)freq / 440.0) / log(2.0);
		if (pb < 1.0 && pb > -1.0) {
			midi_filter.tuning_pitchbend = ((int)(8192.0 * (1.0 + pb))) & 0x3FFF;
			fprintf(stdout, "ZynMidiRouter: MIDI tuning frequency set to %f Hz (%d)\n",freq,midi_filter.tuning_pitchbend);
		} else {
			fprintf(stderr, "ZynMidiRouter: MIDI tuning frequency (%f) out of range!\n",freq);
		}
	}
}

int get_midi_filter_tuning_pitchbend() {
	return midi_filter.tuning_pitchbend;
}

int get_tuned_pitchbend(int pb) {
	int tpb = midi_filter.tuning_pitchbend + pb - 8192;
	if (tpb < 0)
		tpb = 0;
	else if (tpb > 16383)
		tpb = 16383;
	return tpb;
}

//MIDI filter clone

void set_midi_filter_clone(uint8_t chan_from, uint8_t chan_to, int v) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return;
	}
	if (chan_to > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n", chan_to);
		return;
	}
	midi_filter.clone[chan_from][chan_to].enabled = v;
}

int get_midi_filter_clone(uint8_t chan_from, uint8_t chan_to) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return 0;
	}
	if (chan_to > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n", chan_to);
		return 0;
	}
	return midi_filter.clone[chan_from][chan_to].enabled;
}

void reset_midi_filter_clone(uint8_t chan_from) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return;
	}
	int j, k;
	for (j = 0; j < 16; j++) {
		midi_filter.clone[chan_from][j].enabled = 0;
		memset(midi_filter.clone[chan_from][j].cc, 0, 128);
		for (k = 0; k < sizeof(default_cc_to_clone); k++) {
			midi_filter.clone[chan_from][j].cc[default_cc_to_clone[k] & 0x7F] = 1;
		}
	}
}

void set_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to, uint8_t cc[128]) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return;
	}
	if (chan_to > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n", chan_to);
		return;
	}
	int i;
	for (i = 0; i < 128; i++) {
		midi_filter.clone[chan_from][chan_to].cc[i] = cc[i];
	}
}

uint8_t *get_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return NULL;
	}
	if (chan_to > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n", chan_to);
		return NULL;
	}
	return midi_filter.clone[chan_from][chan_to].cc;
}


void reset_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to) {
	if (chan_from > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n", chan_from);
		return;
	}
	if (chan_to > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n", chan_to);
		return;
	}

	int i;
	memset(midi_filter.clone[chan_from][chan_to].cc, 0, 128);
	for (i = 0; i < sizeof(default_cc_to_clone); i++) {
		midi_filter.clone[chan_from][chan_to].cc[default_cc_to_clone[i] & 0x7F] = 1;
	}
}

//MIDI Note-range & Transposing

void set_midi_filter_note_range(uint8_t chan, uint8_t nlow, uint8_t nhigh, int8_t oct_trans, int8_t ht_trans) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].note_low = nlow;
	midi_filter.noterange[chan].note_high = nhigh;
	midi_filter.noterange[chan].octave_trans = oct_trans;
	midi_filter.noterange[chan].halftone_trans = ht_trans;
}

void set_midi_filter_note_low(uint8_t chan, uint8_t nlow) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].note_low = nlow;
}

void set_midi_filter_note_high(uint8_t chan, uint8_t nhigh) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].note_high = nhigh;
}

void set_midi_filter_octave_trans(uint8_t chan, int8_t oct_trans) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].octave_trans = oct_trans;
}

void set_midi_filter_halftone_trans(uint8_t chan, int8_t ht_trans) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].halftone_trans = ht_trans;
}

uint8_t get_midi_filter_note_low(uint8_t chan) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return 0;
	}
	return midi_filter.noterange[chan].note_low;
}

uint8_t get_midi_filter_note_high(uint8_t chan) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return 0;
	}
	return midi_filter.noterange[chan].note_high;
}

int8_t get_midi_filter_octave_trans(uint8_t chan) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return 0;
	}
	return midi_filter.noterange[chan].octave_trans;
}

int8_t get_midi_filter_halftone_trans(uint8_t chan) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return 0;
	}
	return midi_filter.noterange[chan].halftone_trans;
}

void reset_midi_filter_note_range(uint8_t chan) {
	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI note-range chan (%d) is out of range!\n", chan);
		return;
	}
	midi_filter.noterange[chan].note_low = 0;
	midi_filter.noterange[chan].note_high = 127;
	midi_filter.noterange[chan].octave_trans = 0;
	midi_filter.noterange[chan].halftone_trans = 0;
}

//Core MIDI filter functions

int validate_midi_event(midi_event_t *ev) {
	if (ev->type > 0xE) {
		fprintf(stderr, "ZynMidiRouter: MIDI Event type (%d) is out of range!\n", ev->type);
		return 0;
	}
	if (ev->chan > 15) {
		fprintf(stderr, "ZynMidiRouter: MIDI Event channel (%d) is out of range!\n", ev->chan);
		return 0;
	}
	if (ev->num > 127) {
		fprintf(stderr, "ZynMidiRouter: MIDI Event num (%d) is out of range!\n", ev->num);
		return 0;
	}
	return 1;
}

void set_midi_filter_event_map_st(midi_event_t *ev_from, midi_event_t *ev_to) {
	if (validate_midi_event(ev_from) && validate_midi_event(ev_to)) {
		//memcpy(&midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num],ev_to,sizeof(ev_to));
		midi_event_t *event_map=&midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num];
		event_map->type = ev_to->type;
		event_map->chan = ev_to->chan;
		event_map->num = ev_to->num;
	}
}

void set_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from, midi_event_type type_to, uint8_t chan_to, uint8_t num_to) {
	midi_event_t ev_from = { .type = type_from, .chan = chan_from, .num = num_from };
	midi_event_t ev_to = { .type = type_to, .chan = chan_to, .num = num_to };
	set_midi_filter_event_map_st(&ev_from, &ev_to);
}

void set_midi_filter_event_ignore_st(midi_event_t *ev_from) {
	if (validate_midi_event(ev_from)) {
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].type = IGNORE_EVENT;
	}
}

void set_midi_filter_event_ignore(midi_event_type type_from, uint8_t chan_from, uint8_t num_from) {
	midi_event_t ev_from = { .type = type_from, .chan = chan_from, .num = num_from };
	set_midi_filter_event_ignore_st(&ev_from);
}

midi_event_t *get_midi_filter_event_map_st(midi_event_t *ev_from) {
	if (validate_midi_event(ev_from)) {
		return &midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num];
	}
	return NULL;
}

midi_event_t *get_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from) {
	midi_event_t ev_from = { .type = type_from, .chan = chan_from, .num = num_from };
	return get_midi_filter_event_map_st(&ev_from);
}

void del_midi_filter_event_map_st(midi_event_t *ev_from) {
	if (validate_midi_event(ev_from)) {
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].type = THRU_EVENT;
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].chan = ev_from->chan;
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].num = ev_from->num;
	}
}

void del_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from) {
	midi_event_t ev_from = { .type = type_from, .chan = chan_from, .num = num_from };
	del_midi_filter_event_map_st(&ev_from);
}

void reset_midi_filter_event_map() {
	int i, j, k;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 0; k < 128; k++) {
				midi_filter.event_map[i][j][k].type = THRU_EVENT;
				midi_filter.event_map[i][j][k].chan = j;
				midi_filter.event_map[i][j][k].num = k;
			}
		}
	}
}

//Simple CC mapping

void set_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from, uint8_t chan_to, uint8_t cc_to) {
	set_midi_filter_event_map(CTRL_CHANGE, chan_from, cc_from, CTRL_CHANGE, chan_to, cc_to);
}

void set_midi_filter_cc_ignore(uint8_t chan_from, uint8_t cc_from) {
	set_midi_filter_event_ignore(CTRL_CHANGE, chan_from, cc_from);
}

//TODO: It doesn't take into account if chan_from!=chan_to
uint8_t get_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from) {
	midi_event_t *ev = get_midi_filter_event_map(CTRL_CHANGE, chan_from, cc_from);
	return ev->num;
}

void del_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from) {
	del_midi_filter_event_map(CTRL_CHANGE, chan_from, cc_from);
}

void reset_midi_filter_cc_map() {
	int i, j;
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 128; j++) {
			del_midi_filter_event_map(CTRL_CHANGE, i, j);
		}
	}
}

//MIDI Controller Automode
void set_midi_filter_cc_automode(int mfccam) {
	midi_filter.cc_automode = mfccam;
}

//MIDI System Messages enable/disable
void set_midi_filter_system_events(int mfse) {
	midi_filter.system_events = mfse;
}

//MIDI Learning Mode
void set_midi_learning_mode(int mlm) {
	midi_learning_mode = mlm;
}

int get_midi_learning_mode() {
	return midi_learning_mode;
}

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
	if (arrow_from.type ==CTRL_CHANGE) {
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
// ZynMidi Input/Ouput Port management
//-----------------------------------------------------------------------------

int zmop_init(int iz, char *name, int midi_chan, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad index (%d) initializing ouput port '%s'.\n", iz, name);
		return 0;
	}

	if (name != NULL) {
		//Create Jack Output Port
		zmops[iz].jport = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
		if (zmops[iz].jport == NULL) {
			fprintf(stderr, "ZynMidiRouter: Error creating jack midi output port '%s'.\n", name);
			return 0;
		}
	} else {
		zmops[iz].jport = NULL;
	}

	//Set init values
	zmops[iz].n_connections = 0;
	zmops[iz].flags = flags;
	zmops[iz].buffer = NULL;

	int i;
	//Listen midi_chan, don't translate.
	//Channel -1 means "all channels"
	for (i = 0; i < 16; i++) {
		if (midi_chan<0 || i == midi_chan)
			zmops[iz].midi_chans[i] = i;
		else
			zmops[iz].midi_chans[i] = -1;
	}
	//Reset routes
	for (i = 0; i < MAX_NUM_ZMIPS; i++)
		zmops[iz].route_from_zmips[i] = 0;

	return 1;
}

int zmop_set_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].flags = flags;
	return 1;
}

int zmop_has_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & flags) == flags;
}

int zmop_chain_set_flag_droppc(int ch, uint8_t flag) {
	if (ch < 0 || ch >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chain number (%d).\n", ch);
		return 0;
	}
	if (flag)
		zmops[ZMOP_CH0 + ch].flags |= (uint32_t)FLAG_ZMOP_DROPPC;
	else
		zmops[ZMOP_CH0 + ch].flags &= ~(uint32_t)FLAG_ZMOP_DROPPC;
	//fprintf(stderr, "ZynMidiRouter: ZMOPS flags for chain (%d) => %x\n", ch, zmops[ZMOP_CH0 + ch].flags);
	return 1;
}

int zmop_chain_get_flag_droppc(int ch) {
	if (ch < 0 || ch >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chain number (%d).\n", ch);
		return 0;
	}
	return zmops[ZMOP_CH0 + ch].flags & (uint32_t)FLAG_ZMOP_DROPPC;
}

int zmop_reset_midi_chans(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i++)
		zmops[iz].midi_chans[i] = -1;
	return 1;
}

int zmop_set_midi_chan(int iz, int midi_chan_from, int midi_chan_to) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (midi_chan_from < 0 || midi_chan_from >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan_from number (%d).\n", midi_chan_from);
		return 0;
	}
	if (midi_chan_to < -1 || midi_chan_to >= 16) {
		midi_chan_to = -1;
	}
	zmops[iz].midi_chans[midi_chan_from] = midi_chan_to;
	return 1;
}

int zmop_get_midi_chan(int iz, int midi_chan) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return -1;
	}
	if (midi_chan < 0 || midi_chan >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan number (%d).\n", midi_chan);
		return 0;
	}
	return zmops[iz].midi_chans[midi_chan];
}

int zmop_reset_route_from(int iz) {
	if (iz<0 || iz>=MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	int i;
	for (i = 0; i < MAX_NUM_ZMIPS; i++)
		zmops[iz].route_from_zmips[i] = 0;
	return 1;
}

int zmop_set_route_from(int izmop, int izmip, int route) {
	if (izmop < 0 || izmop >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmop);
		return 0;
	}
	if (izmip < 0 || izmip >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmip);
		return 0;
	}
	zmops[izmop].route_from_zmips[izmip] = route;
	return 1;
}

int zmop_get_route_from(int izmop, int izmip) {
	if (izmop < 0 || izmop >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmop);
		return -1;
	}
	if (izmip < 0 || izmip >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmip);
		return -1;
	}
	return zmops[izmop].route_from_zmips[izmip];
}

int zmip_init(int iz, char *name, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad index (%d) initializing input port '%s'.\n", iz, name);
		return 0;
	}

	if (name != NULL) {
		//Create Jack Output Port
		zmips[iz].jport = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
		if (zmips[iz].jport == NULL) {
			fprintf(stderr, "ZynMidiRouter: Error creating jack midi input port '%s'.\n", name);
			return 0;
		}
	} else {
		zmips[iz].jport = NULL;
	}
	
	//Set init values
	zmips[iz].flags = flags;

	switch (iz) {
		case ZMIP_FAKE_INT:
			zmips[iz].buffer = jack_ring_internal_buffer;
			zmips[iz].event.buffer = int_buffer;
			zmips[iz].event.size = 3;
			break;
		case ZMIP_FAKE_UI:
			zmips[iz].buffer = jack_ring_ui_buffer;
			zmips[iz].event.buffer = ui_buffer;
			zmips[iz].event.size = 3;
			break;
		case ZMIP_FAKE_CTRL_FB:
			zmips[iz].buffer = jack_ring_ctrlfb_buffer;
			zmips[iz].event.buffer = ctrlfb_buffer;
			zmips[iz].event.size = 3;
			break;
		default:
			zmips[iz].buffer = NULL;
	}

	zmips[iz].event.time = 0xFFFFFFFF;

	return 1;
}

int zmip_set_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	zmips[iz].flags = flags;
	return 1;
}

int zmip_has_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return (zmips[iz].flags & flags) == flags;
}

//-----------------------------------------------------------------------------
// Jack MIDI processing
//-----------------------------------------------------------------------------

int init_jack_midi(char *name) {
	if ((jack_client = jack_client_open(name, JackNullOption, 0, 0))==NULL) {
		fprintf(stderr, "ZynMidiRouter: Error connecting with jack server.\n");
		return 0;
	}

	int i, j;
	char port_name[12];

	//Init Output Ports
	for (i = 0; i < NUM_ZMOP_CHAINS; i++) {
		sprintf(port_name, "ch%d_out", i);
		if (!zmop_init(ZMOP_CH0 + i, port_name, i, ZMOP_MAIN_FLAGS)) return 0;
	}
	if (!zmop_init(ZMOP_MAIN, "main_out", -1, ZMOP_MAIN_FLAGS)) return 0;
	if (!zmop_init(ZMOP_MIDI, "midi_out", -1, 0)) return 0;
	if (!zmop_init(ZMOP_NET, "net_out", -1, 0)) return 0;
	if (!zmop_init(ZMOP_CTRL, "ctrl_out", -1, 0)) return 0;
	if (!zmop_init(ZMOP_STEP, "step_out", -1, 0)) return 0;

	//Init Ring-Buffers
	jack_ring_internal_buffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
	if(!jack_ring_internal_buffer) {
		fprintf(stderr, "ZynMidiRouter: Error creating internal ring-buffer.\n");
		return 0;
	}
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(jack_ring_internal_buffer)) {
		fprintf(stderr, "ZynMidiRouter: Error locking memory for internal ring-buffer.\n");
		return 0;
	}
	jack_ring_ui_buffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
	if(!jack_ring_ui_buffer) {
		fprintf(stderr, "ZynMidiRouter: Error creating UI ring-buffer.\n");
		return 0;
	}
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(jack_ring_ui_buffer)) {
		fprintf(stderr, "ZynMidiRouter: Error locking memory for UI ring-buffer.\n");
		return 0;
	}
	jack_ring_ctrlfb_buffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
	if(!jack_ring_ctrlfb_buffer) {
		fprintf(stderr, "ZynMidiRouter: Error creating controller feedback ring-buffer.\n");
		return 0;
	}
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(jack_ring_ctrlfb_buffer)) {
		fprintf(stderr, "ZynMidiRouter: Error locking memory for controller feedback ring-buffer.\n");
		return 0;
	}

	//Init Input Ports
	for (i = 0; i < NUM_ZMIP_DEVS; i++) {
		sprintf(port_name, "dev%d_in", i);
		if (!zmip_init(ZMIP_DEV0 + i, port_name, ZMIP_MAIN_FLAGS)) return 0;
	}
	if (!zmip_init(ZMIP_NET, "net_in", ZMIP_MAIN_FLAGS)) return 0;
	if (!zmip_init(ZMIP_SEQ, "seq_in", ZMIP_SEQ_FLAGS)) return 0;
	if (!zmip_init(ZMIP_STEP, "step_in", ZMIP_STEP_FLAGS)) return 0;
	if (!zmip_init(ZMIP_CTRL, "ctrl_in", ZMIP_CTRL_FLAGS)) return 0;
	if (!zmip_init(ZMIP_FAKE_INT, NULL, 0)) return 0;
	if (!zmip_init(ZMIP_FAKE_UI, NULL, 0)) return 0;
	if (!zmip_init(ZMIP_FAKE_CTRL_FB, NULL, 0)) return 0;

	//Route Input to Output Ports
	for (i = 0; i < ZMOP_CTRL; i++) {
		//External Controllers to all ZMOPS
		for (j = 0; j < NUM_ZMIP_DEVS; j++) {
			if (!zmop_set_route_from(i, ZMIP_DEV0 + j, 1)) return 0;
		}
		//MIDI player to all ZMOPS
		if (!zmop_set_route_from(i, ZMIP_SEQ, 1)) return 0;
		//Network input to all ZMOPS except Network output
		if (i != ZMOP_NET) {
			if (!zmop_set_route_from(i, ZMIP_NET, 1)) return 0;
		}
		//Sequencer input to all ZMOPS except Sequencer output
		if (i != ZMOP_STEP) {
			if (!zmop_set_route_from(i, ZMIP_STEP, 1)) return 0;
		}
		//Internal MIDI to all ZMOPS
		if (!zmop_set_route_from(i, ZMIP_FAKE_INT, 1)) return 0;
		//MIDI from UI to Layer's ZMOPS 
		if (i == ZMOP_MAIN || (i >= ZMOP_CH0 && i <= ZMOP_CH15)) {
			if (!zmop_set_route_from(i, ZMIP_FAKE_UI, 1)) return 0;
		}
	}
	//ZMOP_CTRL only receive feedback from Zynthian UI
	if (!zmop_set_route_from(ZMOP_CTRL, ZMIP_FAKE_CTRL_FB, 1)) return 0;

	// ZMIP_CTRL is not routed to any output port, only captured by Zynthian UI

	//Init Jack Process
	jack_set_port_connect_callback(jack_client, jack_connect_cb, 0);
	jack_set_process_callback(jack_client, jack_process, 0);
	if (jack_activate(jack_client)) {
		fprintf(stderr, "ZynMidiRouter: Error activating jack client.\n");
		return 0;
	}

	return 1;
}

int end_jack_midi() {
	if (jack_client_close(jack_client)) {
		fprintf(stderr, "ZynMidiRouter: Error closing jack client.\n");
	}
	jack_ringbuffer_free(jack_ring_internal_buffer);
	jack_ringbuffer_free(jack_ring_ui_buffer);
	jack_ringbuffer_free(jack_ring_ctrlfb_buffer);
	return 1;
}

// Populate zmip event with next event from its input queue / buffer
// izmip: Index of zmip
// returns: Pointer to zmip structure
void populate_zmip_event(struct zmip_st * zmip) {
	if (zmip->jport) {
		// Jack input buffer used for jack input ports
		if (zmip->next_event >= zmip->event_count || jack_midi_event_get(&(zmip->event), zmip->buffer, zmip->next_event++) != 0)
			zmip->event.time = 0xFFFFFFFF; // events with time 0xFFFFFFFF are ignored
	} else {
		// Jack ringbuffer used for virtual MIDI queues
		if (jack_ringbuffer_read_space(zmip->buffer) >= 3) {
			jack_ringbuffer_read(zmip->buffer, zmip->event.buffer, 3);
			zmip->event.time = 0; // Put internal events at start of buffer (it is arbitary so begining is as good as anywhere)
		} else {
			zmip->event.time = 0xFFFFFFFF;
		}
	}
}

//-----------------------------------------------------
// Jack Process
//-----------------------------------------------------

int jack_process(jack_nframes_t nframes, void *arg) {

	// Initialise zomps (MIDI output structures)
	for (int i = 0; i < MAX_NUM_ZMOPS; ++i) {
		zmops[i].buffer = jack_port_get_buffer(zmops[i].jport, nframes);
		if (zmops[i].buffer)
			jack_midi_clear_buffer(zmops[i].buffer);
	}

	// Initialise input structure for each MIDI input
	for (int i = 0; i < MAX_NUM_ZMIPS; ++i) {
		struct zmip_st * zmip = zmips + i;
		if (zmip->jport) {
			zmip->buffer = jack_port_get_buffer(zmip->jport, nframes);
			zmip->event_count = jack_midi_get_event_count(zmip->buffer);
			zmip->next_event = 0;
		}
		populate_zmip_event(zmip);
	}

	uint8_t event_type;
	uint8_t event_chan;
	uint8_t event_num;
	uint8_t event_val;
	uint32_t ui_event;
	int j;

	// Process MIDI input messages in the order they were received
	while (1) {

		// Find the earliest unprocessed event from all input buffers
		jack_nframes_t 	event_time = 0xFFFFFFFE; // Time of earliest unprocessed event (processed events have time set to 0xFFFFFFFF)
		struct zmip_st * current_zmip = NULL;
		int izmip = -1;
		for (int i = 0; i < MAX_NUM_ZMIPS; ++i) {
			struct zmip_st * zmip = zmips + i;
			if (zmip->event.time < event_time) {
				event_time = zmip->event.time;
				current_zmip = zmip;
				izmip = i;
			}
		}
		if(!current_zmip)
			break;
		jack_midi_event_t * ev = &(current_zmip->event);
		//printf("Found earliest event %0X at time %u:%u from input %d\n", ev->buffer[0], jack_last_frame_time(jack_client), ev->time, izmip);

		// Ignore Active Sense & SysEx messages
		if (ev->buffer[0] == ACTIVE_SENSE || ev->buffer[0] == SYSTEM_EXCLUSIVE)
			goto event_processed; //!@todo Handle SysEx and Active Sense

		// Get event type & chan
		if (ev->buffer[0] >= SYSTEM_EXCLUSIVE) {
			//Ignore System Events depending on flag
			if (!midi_filter.system_events)
				goto event_processed;
			event_type = ev->buffer[0];
			event_chan = 0;
		} else {
			event_type = ev->buffer[0] >> 4;
			event_chan = ev->buffer[0] & 0xF;
		}

		// Get event details depending of event type & size
		if (event_type == PITCH_BENDING) {
			event_num = 0;
			event_val = ev->buffer[2] & 0x7F; //!@todo handle 14-bit pitchbend
		} else if (event_type == CHAN_PRESS) {
			event_num = 0;
			event_val = ev->buffer[1] & 0x7F;
		} else if (ev->size == 3) {
			event_num = ev->buffer[1] & 0x7F;
			event_val = ev->buffer[2] & 0x7F;
		} else if (ev->size == 2) {
			event_num = ev->buffer[1] & 0x7F;
			event_val = 0;
		} else {
			event_num=event_val = 0;
		}

		// Active channel (stage mode)
		if ((current_zmip->flags & FLAG_ZMIP_ACTIVE_CHAN) && midi_filter.active_chan >= 0 
				&& ev->buffer[0] < SYSTEM_EXCLUSIVE && event_chan != midi_filter.master_chan) {
			//Active Channel => When set, move all channel events to active_chan
			int destiny_chan = midi_filter.active_chan;

			if (midi_filter.last_active_chan >= 0) { 
				// Release pressed notes across active channel changes, excluding cloned channels
				//!@todo Releasing notes across active channel changes is currently disabled but still using CPU cycles!!!
				if (event_type == NOTE_OFF || (event_type == NOTE_ON && event_val == 0)) {
					for (j = 0; j < 16; j++) {
						if (j != destiny_chan && midi_filter.note_state[j][event_num] > 0 && !midi_filter.clone[destiny_chan][j].enabled) {
							destiny_chan = j;
							//internal_send_note_off(j, event_num, event_val);
						}
					}
				} else if (event_type == CTRL_CHANGE && event_num == 64) {
					// Manage sustain pedal across active_channel changes, excluding cloned channels
					for (j = 0; j < 16; j++) {
						if (j != destiny_chan && midi_filter.last_ctrl_val[j][64] > 0 && !midi_filter.clone[destiny_chan][j].enabled) {
							internal_send_ccontrol_change(j, 64, event_val);
						}
					}
				} else if (event_type == NOTE_ON && event_val > 0) {
					// Re-send sustain pedal on new active_channel if it was pressed before change
					for (j = 0; j < 16; j++) {
						if (j != destiny_chan && midi_filter.last_ctrl_val[j][64] > midi_filter.last_ctrl_val[destiny_chan][64]) {
							internal_send_ccontrol_change(destiny_chan, 64, midi_filter.last_ctrl_val[j][64]);
						}
					}
				}
			}
			ev->buffer[0] = (ev->buffer[0] & 0xF0) | (destiny_chan & 0x0F);
			event_chan = destiny_chan;
		}

		//if (ev->buffer[0] != 0xfe)
		//	fprintf(stderr, "MIDI EVENT: %x, %x, %x\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);

		// Capture events for UI: before filtering => [Control-Change for MIDI learning]
		ui_event = 0;
		if ((current_zmip->flags & FLAG_ZMIP_UI) && midi_learning_mode && (event_type == CTRL_CHANGE || event_type==NOTE_ON || event_type==NOTE_OFF)) {
			ui_event = (ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]);
		}

		// Event Mapping
		if ((current_zmip->flags & FLAG_ZMIP_FILTER) && event_type >= NOTE_OFF && event_type <= PITCH_BENDING) {
			midi_event_t * event_map = &(midi_filter.event_map[event_type & 0x07][event_chan][event_num]);
			//Ignore event...
			if (event_map->type == IGNORE_EVENT) {
				fprintf(stdout, "IGNORE => %x, %x, %x\n",event_type, event_chan, event_num);
				goto event_processed;
			}
			//Map event ...
			if (event_map->type >= 0) {
				fprintf(stdout, "ZynMidiRouter: Event Map %x, %x => ",ev->buffer[0],ev->buffer[1]);
				event_type = event_map->type;
				event_chan = event_map->chan;
				ev->buffer[0] = (event_type << 4) | event_chan;
				if (event_map->type == PROG_CHANGE || event_map->type == CHAN_PRESS) {
					ev->buffer[1] = event_num;
					event_val = 0;
					ev->size=2;
				} else if (event_map->type == PITCH_BENDING) {
					event_num = 0;
					ev->buffer[1] = 0;
					ev->buffer[2] = event_val;
					ev->size=3;
				} else {
					event_num = event_map->num;
					ev->buffer[1] = event_num;
					ev->buffer[2] = event_val;
					ev->size = 3;
				}
				//fprintf(stdout, "MIDI MSG => %x, %x\n",ev->buffer[0],ev->buffer[1]);
			}
		}

		// Capture events for UI: MASTER CHANNEL + Program Change
		if (current_zmip->flags & FLAG_ZMIP_UI) {
			if (event_chan == midi_filter.master_chan) {
				write_zynmidi((ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]));
				goto event_processed;
			}
			if (event_type == PROG_CHANGE) {
				write_zynmidi((ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]));
			}
		}

		// MIDI CC messages => TODO: Clone behaviour?!!
		if (event_type == CTRL_CHANGE) {

			//Auto Relative-Mode
			if (midi_filter.ctrl_mode[event_chan][event_num] == 1) {
				// Change to absolute mode
				if (midi_filter.ctrl_relmode_count[event_chan][event_num] > 1) {
					midi_filter.ctrl_mode[event_chan][event_num] = 0;
					//printf("Changing Back to Absolute Mode ...\n");
				} else if (event_val == 64) {
					// Every 2 messages, rel-mode mark. Between 2 marks, can't have a val of 64.
					if (midi_filter.ctrl_relmode_count[event_chan][event_num] == 1) {
						midi_filter.ctrl_relmode_count[event_chan][event_num] = 0;
						goto event_processed;
					} else {
						midi_filter.ctrl_mode[event_chan][event_num] = 0;
						//printf("Changing Back to Absolute Mode ...\n");
					}
				} else {
					int16_t last_val = midi_filter.last_ctrl_val[event_chan][event_num];
					int16_t new_val = last_val + (int16_t)event_val - 64;
					if (new_val > 127) new_val = 127;
					if (new_val < 0) new_val = 0;
					ev->buffer[2] = event_val = (uint8_t)new_val;
					midi_filter.ctrl_relmode_count[event_chan][event_num]++;
					//printf("Relative Mode! => val=%d\n",new_val);
				}
			}

			//Absolute Mode
			if (midi_filter.ctrl_mode[event_chan][event_num] == 0 && midi_filter.cc_automode ==1) {
				if (event_val == 64) {
					//printf("Tenting Relative Mode ...\n");
					midi_filter.ctrl_mode[event_chan][event_num] = 1;
					midi_filter.ctrl_relmode_count[event_chan][event_num] = 0;
					// Here we lost a tick when an absolut knob moves fast and touch val=64,
					// but if we want auto-detect rel-mode and change softly to it, it's the only way.
					int16_t last_val = midi_filter.last_ctrl_val[event_chan][event_num];
					if (abs(last_val - event_val) > 4)
						goto event_processed;
				}
			}

			//Save last controller value ...
			midi_filter.last_ctrl_val[event_chan][event_num] = event_val;

			//Ignore Bank Change events when FLAG_ZMIP_UI
			//if ((current_zmip->flags & FLAG_ZMIP_UI) && (event_num==0 || event_num==32)) {
			//	goto event_processed;
			//}
		} else if ((current_zmip->flags & FLAG_ZMIP_NOTERANGE) && (event_type == NOTE_OFF || event_type == NOTE_ON)) {
		
		// Note-range & Transpose Note-on/off messages => TODO: Bizarre clone behaviour?
		
			int discard_note = 0;
			int note = ev->buffer[1];
			// Note-range
			if (note < midi_filter.noterange[event_chan].note_low || note > midi_filter.noterange[event_chan].note_high)
				discard_note = 1;
			// Transpose
			if (!discard_note) {
				note += 12 * midi_filter.noterange[event_chan].octave_trans;
				note += midi_filter.noterange[event_chan].halftone_trans;
				//If result note is out of range, ignore it ...
				if (note > 0x7F || note < 0)
					discard_note = 1;
				else
					event_num = ev->buffer[1] = (uint8_t)(note & 0x7F);
			}
			if (discard_note) {
				//If already captured, forward event to UI
				if (ui_event)
					write_zynmidi(ui_event);
				goto event_processed;
			}
		}

		// Save note state ...
		if (event_type == NOTE_ON)
			midi_filter.note_state[event_chan][event_num] = event_val;
		else if (event_type == NOTE_OFF)
			midi_filter.note_state[event_chan][event_num] = 0;

		// Capture events for UI: after filtering => [Note-Off, Note-On, Control-Change, SysEx]
		if (!ui_event && (current_zmip->flags & FLAG_ZMIP_UI) && (event_type == NOTE_OFF || event_type == NOTE_ON || event_type == CTRL_CHANGE || event_type == PITCH_BENDING || event_type >= SYSTEM_EXCLUSIVE)) {
			ui_event = (ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]);
		}

		// Forward event to UI
		if (ui_event)
			write_zynmidi(ui_event);

		// Swap Mapping
		//fprintf(stderr, "PRESWAP MIDI EVENT: %d, %d, %d\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);
		if ((current_zmip->flags & FLAG_ZMIP_FILTER) && event_type == CTRL_CHANGE) {
			midi_event_t *cc_swap = &(midi_filter.cc_swap[event_chan][event_num]);
			//fprintf(stdout, "ZynMidiRouter: CC Swap %x, %x => ",ev->buffer[0],ev->buffer[1]);
			event_chan = cc_swap->chan;
			event_num = cc_swap->num;
			ev->buffer[0] = (event_type << 4) | event_chan;
			ev->buffer[1] = event_num;
			ev->buffer[2] = event_val;
			ev->size=3;
			//fprintf(stdout, "MIDI MSG => %x, %x\n",ev->buffer[0],ev->buffer[1]);
		}
		//fprintf(stderr, "POSTSWAP MIDI EVENT: %d, %d, %d\n", ev->buffer[0], ev->buffer[1], ev->buffer[2]);

		// Send the processed message to configured output queues
		for(int izmop = 0; izmop < MAX_NUM_ZMOPS; ++ izmop) {
			struct zmop_st * zmop = zmops + izmop;

			// Do not send to disconnected outputs
			if (!zmop->n_connections)
				continue;

			// Drop "Program Change" from engine zmops
			if (event_type == PROG_CHANGE && (zmop->flags & FLAG_ZMOP_DROPPC) && izmip != ZMIP_FAKE_UI)
				continue;

			// Only send on configured routes and if not filtered by MIDI channel
			if (!zmop->route_from_zmips[izmip] || zmop->midi_chans[event_chan] == -1)
				continue;

			// Add processed event to MIDI outputs
			zomp_push_event(zmop, ev, izmop);

			// Handle cloned events
			if ((current_zmip->flags & FLAG_ZMIP_CLONE) && (event_type == NOTE_OFF || event_type == NOTE_ON || event_type == PITCH_BENDING || event_type == KEY_PRESS || event_type == CHAN_PRESS || event_type == CTRL_CHANGE)) {
				int clone_from_chan = event_chan;

				// Check for next "clone_to" channel ...
				for (int clone_to_chan = 0; clone_to_chan < 16; ++clone_to_chan) {
					if (!midi_filter.clone[clone_from_chan][clone_to_chan].enabled || (event_type == CTRL_CHANGE && !midi_filter.clone[clone_from_chan][clone_to_chan].cc[event_num]))
						continue;

					// Clone from last event...
					ev->buffer[0] = (ev->buffer[0] & 0xF0) | clone_to_chan;

					event_type = ev->buffer[0] >> 4;

					//Get event details depending of event type & size
					if (event_type == PITCH_BENDING) {
						event_num = 0;
						event_val = ev->buffer[2] & 0x7F;
					} else if (event_type == CHAN_PRESS) {
						event_num = 0;
						event_val = ev->buffer[1] & 0x7F;
					} else if (ev->size == 3) {
						event_num = ev->buffer[1] & 0x7F;
						event_val = ev->buffer[2] & 0x7F;
					} else if (ev->size == 2) {
						event_num = ev->buffer[1] & 0x7F;
						event_val = 0;
					} else {
						event_num=event_val = 0;
					}

					//loggin.debug("CLONING EVENT %d => %d [0x%x, %d]\n", clone_from_chan, clone_to_chan, event_type, event_num);

					// Add cloned event to MIDI outputs
					zomp_push_event(zmops + clone_to_chan, ev, izmop);
				}
			}
		}

		// Mark event as processed by setting its time beyond searchable range then get next event
		event_processed:
		//current_zmip->event.time = 0xFFFFFFFF; //!@todo Is this needed? We replace the event with next event
		populate_zmip_event(current_zmip);
	}
	return 0;
}

//  Post-process midi message and add to output buffer
//	zomp: Pointer to the zomp describing the MIDI output
//	ev: Pointer to a valid jack midi event
//	izmop: Index of zmop (only for debug - can be removed)
void zomp_push_event(struct zmop_st * zmop, jack_midi_event_t * ev, int izmop) {
	if (!zmop)
		return;

	uint8_t event_type = ev->buffer[0] >> 4;
	uint8_t event_chan = ev->buffer[0] & 0x0F;

	// Fine-tunning event
	jack_midi_event_t xev;
	jack_midi_data_t xev_buffer[3];
	xev.buffer = (jack_midi_data_t *) &xev_buffer;

	// Channel filter & translation
	if (event_type >= NOTE_OFF && event_type <= PITCH_BENDING) {
		if (zmop->midi_chans[event_chan] >= 0) {
			event_chan = zmop->midi_chans[event_chan] & 0x0F;
			ev->buffer[0] = (ev->buffer[0] & 0xF0) | event_chan;
		}
	}
	
	// Fine-Tuning, using pitch-bending messages ...
	xev.size=0;
	if ((zmop->flags & FLAG_ZMOP_TUNING) && midi_filter.tuning_pitchbend >= 0) {
		if (event_type == NOTE_ON) {
			int pb = midi_filter.last_pb_val[event_chan];
			//printf("NOTE-ON PITCHBEND=%d (%d)\n",pb,midi_filter.tuning_pitchbend);
			pb = get_tuned_pitchbend(pb);
			//printf("NOTE-ON TUNED PITCHBEND=%d\n",pb);
			xev.buffer[0] = (PITCH_BENDING << 4) | event_chan;
			xev.buffer[1] = pb & 0x7F;
			xev.buffer[2] = (pb >> 7) & 0x7F;
			xev.size=3;
			xev.time = ev->time;
		} else if (event_type == PITCH_BENDING) {
			//Get received PB
			int pb = (ev->buffer[2] << 7) | ev->buffer[1];
			//Save last received PB value ...
			midi_filter.last_pb_val[event_chan] = pb;
			//Calculate tuned PB
			//printf("PITCHBEND=%d\n",pb);
			pb = get_tuned_pitchbend(pb);
			//printf("TUNED PITCHBEND=%d\n",pb);
			ev->buffer[1] = pb & 0x7F;
			ev->buffer[2] = (pb >> 7) & 0x7F;
		}
	}

	// Add core event to output
	if (jack_midi_event_write(zmop->buffer, ev->time, ev->buffer, ev->size))
		fprintf(stderr, "ZynMidiRouter: Error writing jack midi output event!\n");
	//else {printf("Sent"); for(int i=0; i<ev->size;++i) printf(" %02X", ev->buffer[i]); printf(" to output %d\n", izmop);}

	// Add tuning event to output
	if (xev.size > 0)
		if (jack_midi_event_write(zmop->buffer, xev.time, xev.buffer, ev->size))
			fprintf(stderr, "ZynMidiRouter: Error writing jack midi output event!\n");
		//else {printf("Sent microtune"); for(int i=0; i<xev.size; ++i) printf(" %02X", xev.buffer[i]); printf(" to output %d\n", izmop);}
}


void jack_connect_cb(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
	//---------------------------------
	// Get number of connection of Output Ports
	//---------------------------------
	for (int i = 0; i < MAX_NUM_ZMOPS; i++) {
		zmops[i].n_connections = jack_port_connected(zmops[i].jport);
	}
	//fprintf(stderr, "ZynMidiRouter: Num. of connections refreshed\n");

}


//-----------------------------------------------------
// MIDI Internal Input <= Internal (zyncoder, etc.)
//-----------------------------------------------------

//------------------------------
// Event Ring-Buffer Management
//------------------------------

uint8_t internal_midi_data[JACK_MIDI_BUFFER_SIZE];

int write_internal_midi_event(uint8_t *event_buffer) {
	if (jack_ringbuffer_write_space(jack_ring_internal_buffer) >= 3) {
		if (jack_ringbuffer_write(jack_ring_internal_buffer, event_buffer, 3) != 3) {
			fprintf(stderr, "ZynMidiRouter: Error writing internal ring-buffer: INCOMPLETE\n");
			return 0;
		}
	} else {
		fprintf(stderr, "ZynMidiRouter: Error writing internal ring-buffer: FULL\n");
		return 0;
	}

	//Set last CC value
	if (event_buffer[0] & (CTRL_CHANGE << 4)) {
		uint8_t chan = event_buffer[0] & 0x0F;
		uint8_t num = event_buffer[1];
		uint8_t val = event_buffer[2];
		midi_filter.last_ctrl_val[chan][num] = val;
	} else if (event_buffer[0] & (NOTE_ON << 4)) {
		//Set note state
		uint8_t chan=event_buffer[0] & 0x0F;
		uint8_t num=event_buffer[1];
		uint8_t val=event_buffer[2];
		midi_filter.last_ctrl_val[chan][num] = val;
	} else if (event_buffer[0] & (NOTE_OFF << 4)) {
		uint8_t chan=event_buffer[0] & 0x0F;
		uint8_t num=event_buffer[1];
		midi_filter.last_ctrl_val[chan][num] = 0;
	}

	return 1;
}

//------------------------------
// Send Functions
//------------------------------

int internal_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_internal_midi_event(buffer);
}

int internal_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_internal_midi_event(buffer);
}

int internal_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return write_internal_midi_event(buffer);
}

int internal_send_program_change(uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return write_internal_midi_event(buffer);
}

int internal_send_chan_press(uint8_t chan, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xD0 + (chan & 0x0F);
	buffer[1] = val;
	buffer[2] = 0;
	return write_internal_midi_event(buffer);
}

int internal_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return write_internal_midi_event(buffer);
}

int internal_send_all_notes_off() {
	int chan, note;
	for (chan = 0; chan < 16; chan++) {
		for (note = 0; note < 128; note++) {
			if (midi_filter.note_state[chan][note] > 0) 
				if (!internal_send_note_off(chan, note, 0))
					return 0;
		}
	}
	return 1;
}

int internal_send_all_notes_off_chan(uint8_t chan) {
	int note;

	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter:internal_send_all_notes_off_chan(chan) => chan (%d) is out of range!\n",chan);
		return 0;
	}

	for (note = 0; note < 128; note++) {
		if (midi_filter.note_state[chan][note] > 0) 
			if (!internal_send_note_off(chan, note, 0))
				return 0;
	}
	return 1;
}


//-----------------------------------------------------
// MIDI UI input <= UI
//-----------------------------------------------------

//------------------------------
// Event Ring-Buffer Management
//------------------------------

uint8_t ui_midi_data[JACK_MIDI_BUFFER_SIZE];

int write_ui_midi_event(uint8_t *event_buffer) {
	if (jack_ringbuffer_write_space(jack_ring_ui_buffer) >= 3) {
		if (jack_ringbuffer_write(jack_ring_ui_buffer, event_buffer, 3) != 3) {
			fprintf(stderr, "ZynMidiRouter: Error writing UI ring-buffer: INCOMPLETE\n");
			return 0;
		}
	} else {
		fprintf(stderr, "ZynMidiRouter: Error writing UI ring-buffer: FULL\n");
		return 0;
	}

	//Set last CC value
	if (event_buffer[0] & (CTRL_CHANGE << 4)) {
		uint8_t chan=event_buffer[0] & 0x0F;
		uint8_t num=event_buffer[1];
		uint8_t val=event_buffer[2];
		midi_filter.last_ctrl_val[chan][num] = val;
	} else if (event_buffer[0] & (NOTE_ON << 4)) {
		//Set note state
		uint8_t chan=event_buffer[0] & 0x0F;
		uint8_t num=event_buffer[1];
		uint8_t val=event_buffer[2];
		midi_filter.last_ctrl_val[chan][num] = val;
	} else if (event_buffer[0] & (NOTE_OFF << 4)) {
		uint8_t chan=event_buffer[0] & 0x0F;
		uint8_t num=event_buffer[1];
		midi_filter.last_ctrl_val[chan][num] = 0;
	}

	return 1;
}

//------------------------------
// Send Functions
//------------------------------

int ui_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_ui_midi_event(buffer);
}

int ui_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_ui_midi_event(buffer);
}

int ui_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return write_ui_midi_event(buffer);
}

int ui_send_program_change(uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return write_ui_midi_event(buffer);
}

int ui_send_chan_press(uint8_t chan, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xD0 + (chan & 0x0F);
	buffer[1] = val;
	buffer[2] = 0;
	return write_ui_midi_event(buffer);
}

int ui_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return write_ui_midi_event(buffer);
}

int ui_send_master_ccontrol_change(uint8_t ctrl, uint8_t val) {
	if (midi_filter.master_chan>=0) {
		return ui_send_ccontrol_change(midi_filter.master_chan, ctrl, val);
	}
}

int ui_send_all_notes_off() {
	int chan, note;
	for (chan = 0; chan < 16; chan++) {
		for (note = 0; note < 128; note++) {
			if (midi_filter.note_state[chan][note] > 0) 
				if (!ui_send_note_off(chan, note, 0))
					return 0;
		}
	}
	return 1;
}

int ui_send_all_notes_off_chan(uint8_t chan) {
	int note;

	if (chan > 15) {
		fprintf(stderr, "ZynMidiRouter:ui_send_all_notes_off_chan(chan) => chan (%d) is out of range!\n",chan);
		return 0;
	}

	for (note = 0; note < 128; note++) {
		if (midi_filter.note_state[chan][note] > 0) 
			if (!ui_send_note_off(chan, note, 0))
				return 0;
	}
	return 1;
}

//-----------------------------------------------------
// MIDI Controller Feedback <= UI and internal
//-----------------------------------------------------

//------------------------------
// Event Ring-Buffer Management
//------------------------------

uint8_t ctrlfb_midi_data[JACK_MIDI_BUFFER_SIZE];

int write_ctrlfb_midi_event(uint8_t *event_buffer) {
	if (jack_ringbuffer_write_space(jack_ring_ctrlfb_buffer) >= 3) {
		if (jack_ringbuffer_write(jack_ring_ctrlfb_buffer, event_buffer, 3) != 3) {
			fprintf(stderr, "ZynMidiRouter: Error writing controller feedback ring-buffer: INCOMPLETE\n");
			return 0;
		}
	} else {
		fprintf(stderr, "ZynMidiRouter: Error writing controller feedback ring-buffer: FULL\n");
		return 0;
	}
	return 1;
}

//------------------------------
// Send Functions
//------------------------------

int ctrlfb_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_ctrlfb_midi_event(buffer);
}

int ctrlfb_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_ctrlfb_midi_event(buffer);
}

int ctrlfb_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return write_ctrlfb_midi_event(buffer);
}

int ctrlfb_send_program_change(uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return write_ctrlfb_midi_event(buffer);
}

int ctrlfb_send_chan_press(uint8_t chan, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xD0 + (chan & 0x0F);
	buffer[1] = val;
	buffer[2] = 0;
	return write_ctrlfb_midi_event(buffer);
}

int ctrlfb_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return write_ctrlfb_midi_event(buffer);
}


//-----------------------------------------------------------------------------
// MIDI Internal Ouput Events Buffer => UI
//-----------------------------------------------------------------------------

int init_zynmidi_buffer() {
	zynmidi_buffer = jack_ringbuffer_create(ZYNMIDI_BUFFER_SIZE);
	if(!zynmidi_buffer) {
		fprintf(stderr, "ZynMidiRouter: Error creating zynmidi ring-buffer.\n");
		return 0;
	}
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(zynmidi_buffer)) {
		fprintf(stderr, "ZynMidiRouter: Error locking memory for zynmidi ring-buffer.\n");
		return 0;
	}
	return 1;
}

int end_zynmidi_buffer() {
	jack_ringbuffer_free(zynmidi_buffer);
	return 1;
}

int write_zynmidi(uint32_t ev) {
	if (jack_ringbuffer_write_space(zynmidi_buffer) < 4)
		return 0;
	if (jack_ringbuffer_write(zynmidi_buffer, (uint8_t*)&ev, 4) != 4)
		return 0;
	return 1;
}

uint32_t read_zynmidi() {
	if (jack_ringbuffer_read_space(zynmidi_buffer) < 4)
		return 0;
	uint32_t ev;
	jack_ringbuffer_read(zynmidi_buffer, (uint8_t*)&ev, 4);
	return ev;
}

//-----------------------------------------------------------------------------
// MIDI Internal Output: Send Functions => UI
//-----------------------------------------------------------------------------

int write_zynmidi_ccontrol_change(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0xB0 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_note_on(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0x90 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_note_off(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0x80 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_program_change(uint8_t chan, uint8_t num) {
	uint32_t ev = ((0xC0 | (chan & 0x0F)) << 16) | (num << 8);
	return write_zynmidi(ev);
}

//-----------------------------------------------------------------------------

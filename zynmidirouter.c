/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ZynMidiRouter Library
 * 
 * MIDI router library: Implements the MIDI router & filter 
 * 
 * Copyright (C) 2015-2023 Fernando Moyano <jofemodo@zynthian.org>
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
// Global variables
//-----------------------------------------------------------------------------

int tuning_pitchbend;					// Global tunning, implemented using MIDI pitchbend messages
int active_chain;						// Index of the active chain zmop
int midi_master_chan;					// MIDI Master channel. -1 to disable master channel.
int midi_system_events;					// Flag to enable/disable system events globally
int midi_learning_mode;					// To flag "MIDI learning" from UI => Is it needed?

midi_filter_t midi_filter;
struct zmip_st zmips[MAX_NUM_ZMIPS];
struct zmop_st zmops[MAX_NUM_ZMOPS];

uint8_t event_buffer[JACK_MIDI_BUFFER_SIZE];		// Buffer for processing internal/direct MIDI events

jack_client_t * jack_client;
jack_ringbuffer_t * zynmidi_buffer;

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_zynmidirouter() {
	// Init global settings
	active_chain = -1;
	tuning_pitchbend = -1;
	midi_master_chan =- 1;
	midi_system_events = 1;
	midi_learning_mode = 0;

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

int init_midi_router() {
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
	return 1;
}

int end_midi_router() {
	return 1;
}

//-----------------------------------------------------------------------------
// Global settings management
//-----------------------------------------------------------------------------

void set_active_chain(int iz) {
	if (iz > NUM_ZMOP_CHAINS || iz < -1) {
		fprintf(stderr, "ZynMidiRouter: Active chain (%d) is out of range!\n", iz);
		return;
	}
	if (iz != active_chain) {
		active_chain = iz;
	}
}

int get_active_chain() {
	return active_chain;
}

// Global tuning based in MIDI pitch-bending
void set_tuning_freq(double freq) {
	if (freq == 440.0) {
		tuning_pitchbend = -1;
		// Clear pitchbend already applied
		for (int i = 0; i < 16; ++i)
			zmip_send_pitchbend_change(ZMIP_FAKE_UI, i, 0x2000); //!@todo Ideally reset only playing channels to current pitchbend offset
	} else {
		double pb = 6 * log((double)freq / 440.0) / log(2.0);
		if (pb < 1.0 && pb > -1.0) {
			tuning_pitchbend = ((int)(8192.0 * (1.0 + pb))) & 0x3FFF;
			fprintf(stderr, "ZynMidiRouter: MIDI tuning frequency set to %f Hz (%d)\n", freq, tuning_pitchbend);
		} else {
			fprintf(stderr, "ZynMidiRouter: MIDI tuning frequency (%f) out of range!\n", freq);
		}
	}
}

int get_tuning_pitchbend() {
	return tuning_pitchbend;
}

int get_tuned_pitchbend(int pb) {
	int tpb = tuning_pitchbend + pb - 8192;
	if (tpb < 0)
		tpb = 0;
	else if (tpb > 16383)
		tpb = 16383;
	return tpb;
}

void set_midi_master_chan(int chan) {
	if (chan > 15 || chan < -1) {
		fprintf(stderr, "ZynMidiRouter: MIDI Master channel (%d) is out of range!\n",chan);
		return;
	}
	midi_master_chan = chan;
}

int get_midi_master_chan() {
	return midi_master_chan;
}

// Enable/Disable System messages globally
void set_midi_system_events(int flag) {
	midi_system_events = flag;
}

int get_midi_system_events() {
	return midi_system_events;
}

// MIDI Learning Mode
void set_midi_learning_mode(int mlm) {
	midi_learning_mode = mlm;
}

int get_midi_learning_mode() {
	return midi_learning_mode;
}

// -----------------------------------------------------------------------------
// Core MIDI filter functions
// -----------------------------------------------------------------------------

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

// Simple CC mapping

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

// -----------------------------------------------------------------------------
// MIDI Input Ports management
// -----------------------------------------------------------------------------

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

	//Set initial values
	zmips[iz].buffer = NULL;
	zmips[iz].rbuffer = NULL;
	zmips[iz].event.buffer = NULL;
	zmips[iz].event.time = 0xFFFFFFFF;
	zmips[iz].event_count = 0;
	zmips[iz].flags = flags;
	memset(zmips[iz].ctrl_mode, CTRL_MODE_ABS, 16 * 128);
	memset(zmips[iz].ctrl_relmode_count, 0, 16 * 128);
	memset(zmips[iz].last_ctrl_val, 0, 16 * 128);

	// Create direct input ring-buffer
	if (flags & FLAG_ZMIP_DIRECTIN) {
		zmips[iz].rbuffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
		if (!zmips[iz].rbuffer) {
			fprintf(stderr, "ZynMidiRouter: Error creating ZMIP ring-buffer.\n");
			return 0;
		}
		// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
		if (jack_ringbuffer_mlock(zmips[iz].rbuffer)) {
			fprintf(stderr, "ZynMidiRouter: Error locking memory for ZMIP ring-buffer.\n");
			return 0;
		}
	}

	return 1;
}

int zmip_end(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	zmips[iz].buffer = NULL;
	if (zmips[iz].rbuffer) {
		jack_ringbuffer_free(zmips[iz].rbuffer);
		zmips[iz].rbuffer = NULL;
	}
	return 1;
}

int zmip_get_num_devs() {
	return NUM_ZMIP_DEVS;
}

int zmip_set_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	zmips[iz].flags = flags;
	return 1;
}

uint32_t zmip_get_flags(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return zmips[iz].flags;
}

int zmip_has_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return (zmips[iz].flags & flags) == flags;
}

int zmip_set_flag_cc_auto_mode(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmips[iz].flags |= (uint32_t)FLAG_ZMIP_CC_AUTO_MODE;
	else
		zmips[iz].flags &= ~(uint32_t)FLAG_ZMIP_CC_AUTO_MODE;
	return 1;
}

int zmip_get_flag_cc_auto_mode(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmips[iz].flags & (uint32_t)FLAG_ZMIP_CC_AUTO_MODE) > 0;
}

int zmip_set_flag_active_chain(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port number (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmips[ZMIP_DEV0 + iz].flags |= (uint32_t)FLAG_ZMIP_ACTIVE_CHAIN;
	else
		zmips[ZMIP_DEV0 + iz].flags &= ~(uint32_t)FLAG_ZMIP_ACTIVE_CHAIN;
	//fprintf(stderr, "ZynMidiRouter: Flags for zmip (%d) => %x\n", iz, zmips[ZMIP_DEV0 + iz].flags);
	return 1;
}

int zmip_get_flag_active_chain(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port number (%d).\n", iz);
		return 0;
	}
	return zmips[ZMIP_DEV0 + iz].flags & (uint32_t)FLAG_ZMIP_ACTIVE_CHAIN;
}

//Route/unroute a MIDI input device (zmip) to *ALL* chain zmops
int zmip_set_route_chains(int iz, int route) {
	if (iz < 0 || iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	for (int i = 0; i < ZMOP_CTRL; i++) {
		if (!zmop_set_route_from(i, iz, route)) return 0;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// MIDI Output Ports management
//-----------------------------------------------------------------------------

int zmop_init(int iz, char *name, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad index (%d) initializing ouput port '%s'.\n", iz, name);
		return 0;
	}

	if (name != NULL) {
		// Create Jack Output Port
		zmops[iz].jport = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
		if (zmops[iz].jport == NULL) {
			fprintf(stderr, "ZynMidiRouter: Error creating jack midi output port '%s'.\n", name);
			return 0;
		}
	} else {
		zmops[iz].jport = NULL;
	}

	// Set initial values
	zmops[iz].buffer = NULL;
	zmops[iz].rbuffer = NULL;
	zmops[iz].n_connections = 0;
	zmops[iz].flags = flags;
	zmops[iz].note_low = 0;
	zmops[iz].note_high = 127;
	zmops[iz].transpose_octave = 0;
	zmops[iz].transpose_semitone = 0;
	memset(zmops[iz].note_state, 0, 128);
	int i;
	for (i = 0; i < 16; i++) {
		zmops[iz].last_pb_val[i] = 8192;
	}

	// Create direct output ring-buffer
	if (flags & FLAG_ZMOP_DIRECTOUT) {
		zmops[iz].rbuffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
		if (!zmops[iz].rbuffer) {
			fprintf(stderr, "ZynMidiRouter: Error creating ZMOP ring-buffer.\n");
			return 0;
		}
		// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
		if (jack_ringbuffer_mlock(zmops[iz].rbuffer)) {
			fprintf(stderr, "ZynMidiRouter: Error locking memory for ZMOP ring-buffer.\n");
			return 0;
		}
	}

	// Reset MIDI channels
	zmop_reset_midi_chans(iz);

	// Reset routes
	for (i = 0; i < MAX_NUM_ZMIPS; i++) {
		zmops[iz].route_from_zmips[i] = 0;
	}

	return 1;
}

int zmop_end(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].buffer = NULL;
	if (zmops[iz].rbuffer) {
		jack_ringbuffer_free(zmops[iz].rbuffer);
		zmops[iz].rbuffer = NULL;
	}
	return 1;
}

int zmop_get_num_chains() {
	return NUM_ZMOP_CHAINS;
}

int zmop_get_num_devs() {
	return NUM_ZMOP_DEVS;
}

// Flags management

int zmop_set_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].flags = flags;
	return 1;
}

uint32_t zmop_get_flags(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return zmops[iz].flags;
}

int zmop_has_flags(int iz, uint32_t flags) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & flags) == flags;
}

int zmop_set_flag_droppc(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_DROPPC;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_DROPPC;
	//fprintf(stderr, "ZynMidiRouter: Flags for zmop (%d) => %x\n", iz, zmops[iz].flags);
	return 1;
}

int zmop_get_flag_droppc(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_DROPPC) > 0;
}

int zmop_set_flag_dropcc(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_DROPCC;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_DROPCC;
	return 1;
}

int zmop_get_flag_dropcc(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_DROPCC) > 0;
}

int zmop_set_flag_dropsys(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_DROPSYS;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_DROPSYS;
	return 1;
}

int zmop_get_flag_dropsys(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_DROPSYS) > 0;
}

int zmop_set_flag_dropsysex(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_DROPSYSEX;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_DROPSYSEX;
	return 1;
}

int zmop_get_flag_dropsysex(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_DROPSYSEX) > 0;
}

int zmop_set_flag_dropnote(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_DROPNOTE;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_DROPNOTE;
	return 1;
}

int zmop_get_flag_dropnote(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_DROPNOTE) > 0;
}

int zmop_set_flag_tuning(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_TUNING;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_TUNING;
	return 1;
}

int zmop_get_flag_tuning(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_TUNING) > 0;
}

int zmop_set_flag_chan_transfilter(int iz, uint8_t flag) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (flag)
		zmops[iz].flags |= (uint32_t)FLAG_ZMOP_CHAN_TRANSFILTER;
	else
		zmops[iz].flags &= ~(uint32_t)FLAG_ZMOP_CHAN_TRANSFILTER;
	//fprintf(stderr, "ZynMidiRouter: Flags for zmop (%d) => %x\n", iz, zmops[iz].flags);
	return 1;
}

int zmop_get_flag_chan_transfilter(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return (zmops[iz].flags & (uint32_t)FLAG_ZMOP_CHAN_TRANSFILTER) > 0;
}

// MIDI channel management

int zmop_reset_midi_chans(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i++) {
		zmops[iz].midi_chans[i] = -1;
	}
	zmops[iz].midi_chan = -1;
	zmop_set_flag_chan_transfilter(iz, 1);
	return 1;
}

int zmop_set_midi_chan(int iz, int midi_chan) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (midi_chan < 0 || midi_chan >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan number (%d).\n", midi_chan);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i++) {
		zmops[iz].midi_chans[i] = -1;
	}
	zmops[iz].midi_chan = midi_chan;
	zmops[iz].midi_chans[midi_chan] = midi_chan;
	zmop_set_flag_chan_transfilter(iz, 1);
	return 1;
}

int zmop_set_midi_chan_trans(int iz, int midi_chan, int midi_chan_trans) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	if (midi_chan < 0 || midi_chan >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan number (%d).\n", midi_chan);
		return 0;
	}
	if (midi_chan_trans < 0 || midi_chan_trans >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan trans number (%d).\n", midi_chan_trans);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i++) {
		zmops[iz].midi_chans[i] = -1;
	}
	zmops[iz].midi_chan = midi_chan;
	zmops[iz].midi_chans[midi_chan] = midi_chan_trans;
	zmop_set_flag_chan_transfilter(iz, 1);
	return 1;
}

int zmop_set_midi_chan_all(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i ++) {
		zmops[iz].midi_chans[i] = i;
	}
	zmops[iz].midi_chan = -1;
	zmop_set_flag_chan_transfilter(iz, 0);
	return 1;
}

int zmop_set_midi_chan_all_trans(int iz, int midi_chan) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	int i;
	for (i = 0; i < 16; i ++) {
		zmops[iz].midi_chans[i] = midi_chan;
	}
	zmops[iz].midi_chan = -1;
	zmop_set_flag_chan_transfilter(iz, 0);
	return 1;
}

int zmop_set_midi_chan_to(int iz, int midi_chan_from, int midi_chan_to) {
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

int zmop_get_midi_chan_to(int iz, int midi_chan_from) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return -1;
	}
	if (midi_chan_from < 0 || midi_chan_from >= 16) {
		fprintf(stderr, "ZynMidiRouter: Bad chan number (%d).\n", midi_chan_from);
		return 0;
	}
	return zmops[iz].midi_chans[midi_chan_from];
}

int zmop_get_midi_chan_info(int iz, int *buffer) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return -1;
	}
	memcpy((void *)buffer, (void *)zmops[iz].midi_chans, 16 * sizeof(int));
	return 1;
}

// Routing zmip => zmop

int zmop_reset_routes_from(int iz) {
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
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", izmip);
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

int zmop_get_routes_info(int izmop, int *buffer) {
	if (izmop < 0 || izmop >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmop);
		return -1;
	}
	memcpy((void *)buffer, (void *)zmops[izmop].route_from_zmips, MAX_NUM_ZMIPS * sizeof(int));
	return 1;
}

int zmop_get_routes_info_all(int *buffer) {
	int iz;
	for (iz=0; iz<MAX_NUM_ZMIPS; iz++) {
		memcpy((void *)buffer, (void *)zmops[iz].route_from_zmips, MAX_NUM_ZMIPS * sizeof(int));
		buffer += MAX_NUM_ZMIPS;
	}
	return 1;
}

// Note range & Transpose

int zmop_set_note_low(int iz, uint8_t nlow) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].note_low = nlow;
	return 1;
}

int zmop_set_note_high(int iz, uint8_t nhigh) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].note_high = nhigh;
	return 1;
}

int zmop_set_transpose_octave(int iz, int8_t trans_oct) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].transpose_octave = trans_oct;
	return 1;
}

int zmop_set_transpose_semitone(int iz, int8_t trans_semi) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].transpose_semitone = trans_semi;
	return 1;
}

uint8_t zmop_get_note_low(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return zmops[iz].note_low;
}

uint8_t zmop_get_note_high(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 127;
	}
	return zmops[iz].note_high;
}

int8_t zmop_get_transpose_octave(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return zmops[iz].transpose_octave;
}

int8_t zmop_get_transpose_semitone(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	return zmops[iz].transpose_semitone;
}

int zmop_set_note_range_transpose(int iz, uint8_t nlow, uint8_t nhigh, int8_t trans_oct, int8_t trans_semi) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].note_low = nlow;
	zmops[iz].note_high = nhigh;
	zmops[iz].transpose_octave = trans_oct;
	zmops[iz].transpose_semitone = trans_semi;
	return 1;
}

int zmop_reset_note_range_transpose(int iz) {
	if (iz < 0 || iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].note_low = 0;
	zmops[iz].note_high = 127;
	zmops[iz].transpose_octave = 0;
	zmops[iz].transpose_semitone = 0;
	return 1;
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

	// Init MIDI Input Ports (ZMIPs)
	for (i = 0; i < NUM_ZMIP_DEVS; i++) {
		sprintf(port_name, "dev%d_in", i);
		if (!zmip_init(ZMIP_DEV0 + i, port_name, ZMIP_DEV_FLAGS)) return 0;
	}
	if (!zmip_init(ZMIP_SEQ, "seq_in", ZMIP_SEQ_FLAGS)) return 0;
	if (!zmip_init(ZMIP_STEP, "step_in", ZMIP_STEP_FLAGS)) return 0;
	if (!zmip_init(ZMIP_CTRL, "ctrl_in", ZMIP_CTRL_FLAGS)) return 0;
	if (!zmip_init(ZMIP_FAKE_INT, NULL, ZMIP_INT_FLAGS)) return 0;
	if (!zmip_init(ZMIP_FAKE_UI, NULL, ZMIP_UI_FLAGS)) return 0;

	// Init MIDI Output Ports (ZMOPs)
	for (i = 0; i < NUM_ZMOP_CHAINS; i++) {
		sprintf(port_name, "ch%d_out", i);
		if (!zmop_init(ZMOP_CH0 + i, port_name, ZMOP_CHAIN_FLAGS)) return 0;
		//zmop_set_midi_chan(ZMOP_CH0 + i, i);
	}
	if (!zmop_init(ZMOP_MOD, "mod_out", ZMOP_CHAIN_FLAGS)) return 0;
	zmop_set_midi_chan_all(ZMOP_MOD);
	if (!zmop_init(ZMOP_STEP, "step_out", FLAG_ZMOP_DROPSYSEX)) return 0;
	zmop_set_midi_chan_all(ZMOP_STEP);
	if (!zmop_init(ZMOP_CTRL, "ctrl_out", FLAG_ZMOP_DIRECTOUT)) return 0;
	zmop_set_midi_chan_all(ZMOP_CTRL);
	for (i = 0; i < NUM_ZMOP_DEVS; i++) {
		sprintf(port_name, "dev%d_out", i);
		if (!zmop_init(ZMOP_DEV0 + i, port_name, FLAG_ZMOP_DIRECTOUT)) return 0;
		zmop_set_midi_chan_all(ZMOP_DEV0 + i);
	}

	// Route MIDI Input to MIDI Output Ports: ZMIPs => ZMOPs
	for (i = 0; i < ZMOP_CTRL; i++) {
		// External Input Devices to all ZMOPS => By default, all chains receive from all devices
		for (j = 0; j < NUM_ZMIP_DEVS; j++) {
			if (!zmop_set_route_from(i, ZMIP_DEV0 + j, 1)) return 0;
		}
		// MIDI player to all ZMOPS
		if (!zmop_set_route_from(i, ZMIP_SEQ, 1)) return 0;
		//Sequencer input to all ZMOPS except Sequencer output
		if (i != ZMOP_STEP) {
			if (!zmop_set_route_from(i, ZMIP_STEP, 1)) return 0;
		}
		// Internal MIDI to all ZMOPS
		if (!zmop_set_route_from(i, ZMIP_FAKE_INT, 1)) return 0;
		//MIDI from UI to Chain's ZMOPS 
		if (i >= ZMOP_CH0 && i <= ZMOP_CH0 + NUM_ZMOP_CHAINS) {
			if (!zmop_set_route_from(i, ZMIP_FAKE_UI, 1)) return 0;
		}
	}
	// ZMIP_CTRL is not routed to any output port, only captured by Zynthian UI

	// Init Jack Process
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
	int i;
	for (i = 0; i < MAX_NUM_ZMOPS; i++) zmop_end(i);
	for (i = 0; i < MAX_NUM_ZMIPS; i++) zmip_end(i);
	return 1;
}

// Populate event from ring-buffer
// rb: ring buffer pointer
// event: event struct pointer
// returns: Pointer to zmip structure
void populate_midi_event_from_rb(jack_ringbuffer_t *rb, jack_midi_event_t *event) {
	event->size = 3;
	event->time = 0xFFFFFFFF;	// Ignore message if it's not complete
	event->buffer = event_buffer;
	if (jack_ringbuffer_read_space(rb) >= 3) {
		jack_ringbuffer_read(rb, event->buffer, 3);
		// Manage SysEx events
		if (event->buffer[0] == 0xF0) {
			while (jack_ringbuffer_read_space(rb) >= 1) {
				jack_ringbuffer_read(rb, event->buffer + event->size++, 1);
				// SysEx completed => Messages can't be fragmented across buffers!
				if (event->buffer[event->size - 1] == 0xF7) {
					event->time = 0;
					break;
				}
			}
		} else {
			// Put internal events at start of buffer (it is arbitrary so beginning is as good as anywhere)
			event->time = 0;
		}
	}
}

// Populate zmip event with next event from its input queue / buffer
// izmip: Index of zmip
// returns: Pointer to zmip structure
void populate_zmip_event(struct zmip_st * zmip) {
	if (zmip->jport) {
		// Jack input buffer used for jack input ports
		if (zmip->next_event >= zmip->event_count || jack_midi_event_get(&(zmip->event), zmip->buffer, zmip->next_event++) != 0)
			zmip->event.time = 0xFFFFFFFF; // events with time 0xFFFFFFFF are ignored
	} else if ((zmip->flags & FLAG_ZMIP_DIRECTIN) && zmip->rbuffer!=NULL) {
		populate_midi_event_from_rb(zmip->rbuffer, &zmip->event);
	}
}

//-----------------------------------------------------
// Jack Process
//-----------------------------------------------------

int jack_process(jack_nframes_t nframes, void *arg) {

	// Initialise zmops (MIDI output structures)
	struct zmop_st * zmop;
	for (int i = 0; i < MAX_NUM_ZMOPS; ++i) {
		zmops[i].buffer = jack_port_get_buffer(zmops[i].jport, nframes);
		if (zmops[i].buffer)
			jack_midi_clear_buffer(zmops[i].buffer);
	}

	// Initialise input structure for each MIDI input
	struct zmip_st * zmip;
	for (int i = 0; i < MAX_NUM_ZMIPS; ++i) {
		zmip = zmips + i;
		if (midi_learning_mode && i == ZMIP_CTRL)
			continue; // Don't feedback controls when learning
		if (zmip->jport) {
			zmip->buffer = jack_port_get_buffer(zmip->jport, nframes);
			zmip->event_count = jack_midi_get_event_count(zmip->buffer);
			zmip->next_event = 0;
		}
		populate_zmip_event(zmip);
	}

	uint8_t event_idev;
	uint8_t event_type;
	uint8_t event_chan;
	uint8_t event_chan_translated;
	uint8_t event_num;
	uint8_t event_val;
	uint32_t ui_event;
	int j, xch;

	// Process MIDI input messages in the order they were received
	while (1) {
		// Find the earliest unprocessed event from all input buffers
		jack_nframes_t 	event_time = 0xFFFFFFFE; // Time of earliest unprocessed event (processed events have time set to 0xFFFFFFFF)
		int izmip = -1;
		for (int i = 0; i < MAX_NUM_ZMIPS; ++i) {
			zmip = zmips + i;
			if (zmip->event.time < event_time) {
				event_time = zmip->event.time;
				izmip = i;
			}
		}
		if(izmip < 0)
			break;
		zmip = zmips + izmip;
		jack_midi_event_t * ev = &(zmip->event);
		//fprintf(stderr, "Found earliest event %0X at time %u:%u from input %d\n", ev->buffer[0], jack_last_frame_time(jack_client), ev->time, izmip);

		// MIDI device index
		event_idev = (uint8_t)izmip;

		// Ignore Active Sense
		//if (ev->buffer[0] == ACTIVE_SENSE || ev->buffer[0] == SYSTEM_EXCLUSIVE) // and SysEx messages
		if (ev->buffer[0] == ACTIVE_SENSE)
			goto event_processed; //!@TODO Handle Active Sense and SysEx

		// Get event type & chan
		if (ev->buffer[0] >= SYSTEM_EXCLUSIVE) {
			// Ignore System Events depending on global flag
			if (!midi_system_events)
				goto event_processed;
			event_type = ev->buffer[0];
			event_chan = 0;
		} else {
			event_type = ev->buffer[0] >> 4;
			event_chan = ev->buffer[0] & 0xF;
		}

		// Get event details depending of event type & size
		if (event_type == PITCH_BEND) {
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

		//fprintf(stderr, "MIDI EVENT: "); for(int x = 0; x < ev->size; ++x) fprintf(stderr, "%x ", ev->buffer[x]); fprintf(stderr, "\n");

		// Event Mapping
		if ((zmip->flags & FLAG_ZMIP_FILTER) && event_type >= NOTE_OFF && event_type <= PITCH_BEND) {
			midi_event_t * event_map = &(midi_filter.event_map[event_type & 0x07][event_chan][event_num]);
			//Ignore event...
			if (event_map->type == IGNORE_EVENT) {
				//fprintf(stderr, "IGNORE => %x, %x, %x\n",event_type, event_chan, event_num);
				goto event_processed;
			}
			//Map event ...
			if (event_map->type >= 0) {
				//fprintf(stderr, "ZynMidiRouter: Event Map %x, %x => ",ev->buffer[0],ev->buffer[1]);
				event_type = event_map->type;
				event_chan = event_map->chan;
				ev->buffer[0] = (event_type << 4) | event_chan;
				if (event_map->type == PROG_CHANGE || event_map->type == CHAN_PRESS) {
					ev->buffer[1] = event_num;
					event_val = 0;
					ev->size=2;
				} else if (event_map->type == PITCH_BEND) {
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
				//fprintf(stderr, "MIDI EVENT: "); for(int x = 0; x < ev->size; ++x) fprintf(stderr, "%x ", ev->buffer[x]); fprintf(stderr, "\n");
			}
		}

		// Just after mapping: Capture for UI or ignore MASTER CHANNEL events
		if (event_type < SYSTEM_EXCLUSIVE && event_chan == midi_master_chan) {
			if (zmip->flags & FLAG_ZMIP_UI) {
				write_zynmidi((event_idev << 24) | (ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]));
			}
			goto event_processed;
		}

		// MIDI CC messages
		if (event_type == CTRL_CHANGE) {
			//Auto Relative-Mode
			if (zmip->flags & FLAG_ZMIP_CC_AUTO_MODE) {
				if (zmip->ctrl_mode[event_chan][event_num] == CTRL_MODE_REL_2) {
					// Change to absolute mode
					if (zmip->ctrl_relmode_count[event_chan][event_num] > 1) {
						zmip->ctrl_mode[event_chan][event_num] = CTRL_MODE_ABS;
						//fprintf(stderr, "Changing Back to Absolute Mode ...\n");
					} else if (event_val == 64) {
						// Every 2 messages, rel-mode mark. Between 2 marks, can't have a val of 64.
						if (zmip->ctrl_relmode_count[event_chan][event_num] == 1) {
							zmip->ctrl_relmode_count[event_chan][event_num] = 0;
							goto event_processed;
						} else {
							zmip->ctrl_mode[event_chan][event_num] = CTRL_MODE_ABS;
							//fprintf(stderr, "Changing Back to Absolute Mode ...\n");
						}
					} else {
						int16_t last_val = zmip->last_ctrl_val[event_chan][event_num];
						int16_t new_val = last_val + (int16_t)event_val - 64;
						if (new_val > 127) new_val = 127;
						if (new_val < 0) new_val = 0;
						ev->buffer[2] = event_val = (uint8_t)new_val;
						zmip->ctrl_relmode_count[event_chan][event_num]++;
						//fprintf(stderr, "Relative Mode! => val=%d\n",new_val);
					}
				}

				//Absolute Mode
				if (zmip->ctrl_mode[event_chan][event_num] == CTRL_MODE_ABS) {
					if (event_val == 64) {
						//fprintf(stderr, "Tenting Relative Mode ...\n");
						zmip->ctrl_mode[event_chan][event_num] = CTRL_MODE_REL_2;
						zmip->ctrl_relmode_count[event_chan][event_num] = 0;
						// Here we lost a tick when an absolute knob moves fast and touch val=64,
						// but if we want auto-detect rel-mode and change softly to it, it's the only way.
						int16_t last_val = zmip->last_ctrl_val[event_chan][event_num];
						if (abs(last_val - event_val) > 4)
							goto event_processed;
					}
				}
			}

			//Save last controller value ...
			zmip->last_ctrl_val[event_chan][event_num] = event_val;

			//Ignore Bank Change events when FLAG_ZMIP_UI
			//if ((zmip->flags & FLAG_ZMIP_UI) && (event_num==0 || event_num==32)) {
			//	goto event_processed;
			//}
		}

		// Capture events for UI ...
		if (zmip->flags & FLAG_ZMIP_UI) {
			if (event_type == SYSTEM_EXCLUSIVE) {
				// Capture System Exclusive not currently working => message size is 4 bytes only!!
				// TODO: Send fragments??
			} else {
				write_zynmidi((event_idev << 24) | (ev->buffer[0] << 16) | (ev->buffer[1] << 8) | (ev->buffer[2]));
			}
		}

		//printf("ZynMidiRouter: Processing event from zmip %d, type %d, channel %d, translated to channel %d\n", izmip, event_type, event_chan, event_chan_translated);

		// Send the processed message to configured output queues
		uint8_t event_b0 = ev->buffer[0];
		uint8_t event_chan_trans;
		for (int izmop = 0; izmop < MAX_NUM_ZMOPS; ++izmop) {
			zmop = zmops + izmop;

			// Don't waste CPU cycles with unconnected output ports. Nobody is listening there!!
			if (zmop->n_connections==0)
				continue;

			// Do not send to unrouted output ports
			if (!zmop->route_from_zmips[izmip])
				continue;

			// Channel messages ...
			if (event_type < SYSTEM_EXCLUSIVE) {
				event_chan_trans = event_chan;
				// If zmop have enabled Channel Translation / Channel Filtering (ACTI/MULTI) and
				// has a single midi_chan (aka it's not using multi-channel mapping! => ALL CHANS)
				if (zmop->flags & FLAG_ZMOP_CHAN_TRANSFILTER && zmop->midi_chan >= 0) {
					// ACTI => route events to active chain, translating channel as required  ...
					if (zmip->flags & FLAG_ZMIP_ACTIVE_CHAIN) {
						// Translate to active zmop's MIDI channel
						if (izmop == active_chain && zmop->midi_chans[zmop->midi_chan] >= 0) {
							// NOTE-OFF => Release pressed notes across active chain changes
							if (event_type == NOTE_OFF || (event_type == NOTE_ON && event_val == 0)) {
								// If not matching note-on on this chain, try rest of chains ...
								if (zmop->note_state[event_num] == 0) {
									for (j = 1; j < NUM_ZMOP_CHAINS; j++) {
										int xiz = (izmop + j) % NUM_ZMOP_CHAINS;
										// If found a matching note-on for this note-off event on other chain
										if (zmops[xiz].note_state[event_num] > 0 && zmops[xiz].midi_chan >= 0 && zmops[xiz].n_connections > 0  && zmops[xiz].route_from_zmips[izmip]) {
											zmop = 	zmops + xiz;
											break;
										}
									}
								}
							}
							// Update event data with the translated MIDI channel
							event_chan_trans = zmop->midi_chan;
							ev->buffer[0] = (ev->buffer[0] & 0xF0) | (event_chan_trans & 0x0F);
						}
						// or discard message from not active zmops
						else {
							continue;
						}
					}
					// MULTI => no translate, but filter MIDI channels not configured in zmop
					else if (zmop->midi_chans[event_chan] == -1) {
						continue;
					}
				}
				// No Channel Translation & No Channel Filtering:
				// + Ignore ACTI/MULTI flag
				// + ALL channel messages pass untranslated
				else {
					// Leave MIDI channel untouched
				}

				// Drop "CC messages" if configured in zmop options, except from internal sources (UI, etc.)
				if (event_type == CTRL_CHANGE && (zmop->flags & FLAG_ZMOP_DROPCC) && izmip <= ZMIP_CTRL)
					goto zmop_event_processed;

				// Drop "Program Change" if configured in zmop options, except from internal sources (UI)
				if (event_type == PROG_CHANGE && (zmop->flags & FLAG_ZMOP_DROPPC) && izmip != ZMIP_FAKE_UI)
					goto zmop_event_processed;

				// Drop "Note On/Off" if configured in zmop options, except from internal sources (UI)
				if ((zmop->flags & FLAG_ZMOP_DROPNOTE) && (event_type == NOTE_ON || event_type == NOTE_OFF) && izmip != ZMIP_FAKE_UI)
					goto zmop_event_processed;

				// Save note state for each zmop
				if (event_type == NOTE_ON)
					zmop->note_state[event_num] = event_val;
				else if (event_type == NOTE_OFF)
					zmop->note_state[event_num] = 0;
			}
			// Drop "System messages" if configured in zmop options, except from internal sources (UI)
			else if ((event_type > SYSTEM_EXCLUSIVE) && (zmop->flags & FLAG_ZMOP_DROPSYS) && izmip != ZMIP_FAKE_UI) {
			 	continue;
			}
			// Drop "System Exclusive messages" if configured in zmop options
			else if ((event_type == SYSTEM_EXCLUSIVE) && (zmop->flags & FLAG_ZMOP_DROPSYSEX)) {
			 	continue;
			}

			// Add processed event to MIDI output port buffer
			zmop_push_event(zmop, ev);

			zmop_event_processed:
 			// Restore original channel in event object before processing next zmop
 			ev->buffer[0] = event_b0;
		}

		event_processed:
		// After processing (or ignoring) event, get the next event from this input queue and try it all again...
		populate_zmip_event(zmip);
	}

	// Flush ZMOP direct events from ring-buffers (FLAG_ZMOP_DIRECTOUT)
	jack_midi_event_t ev;
	for (int izmop = 0; izmop < MAX_NUM_ZMOPS; izmop++) {
		zmop = zmops + izmop;
		if ((zmop->flags & FLAG_ZMOP_DIRECTOUT) && zmop->rbuffer!=NULL) {
			// Take events from ring-buffer and write them to jack output buffer ...
			while (1) {
				populate_midi_event_from_rb(zmop->rbuffer, &ev);
				if (ev.time==0xFFFFFFFF) break;
				// Do not send to unconnected output ports
				if (zmop->n_connections==0)
					continue;
				if (jack_midi_event_write(zmop->buffer, ev.time, ev.buffer, ev.size))
					fprintf(stderr, "ZynMidiRouter: Error writing jack midi output event!\n");
			}
		}
	}
	return 0;
}

//  Post-process midi message and add to output buffer
//	zmop: Pointer to the zmop describing the MIDI output
//	ev: Pointer to a valid jack midi event
void zmop_push_event(struct zmop_st * zmop, jack_midi_event_t * ev) {
	if (!zmop)
		return;

	uint8_t event_type = ev->buffer[0] >> 4;
	uint8_t event_chan = ev->buffer[0] & 0x0F;
	int temp_note = -1;

	if ((zmop->flags & FLAG_ZMOP_NOTERANGE) && (event_type == NOTE_OFF || event_type == NOTE_ON)) {		
		// Note-range & Transpose Note-on/off messages
		int note = ev->buffer[1];

		// Note-range
		if (note < zmop->note_low || note > zmop->note_high)
			return;

		// Transpose
		note += zmop->transpose_octave * 12 + zmop->transpose_semitone;
		if (note > 0x7F || note < 0)
			return; // Transposed note out of range

		// Store original note from before transpose to restore after sending this event
		temp_note = ev->buffer[1];
		ev->buffer[1] = (uint8_t)(note & 0x7F);
	}

	// Channel translation => Should this honors CHAN_TRANSFILTER flag?
	if (event_type >= NOTE_OFF && event_type <= PITCH_BEND) {
		event_chan = zmop->midi_chans[event_chan] & 0x0F;
		ev->buffer[0] = (ev->buffer[0] & 0xF0) | event_chan;
	}
	
	// Fine-Tuning, using pitch-bending messages ...
	jack_midi_event_t xev;
	jack_midi_data_t xev_buffer[3];
	xev.size=0;
	if ((zmop->flags & FLAG_ZMOP_TUNING) && tuning_pitchbend >= 0) {
		if (event_type == NOTE_ON) {
			int pb = zmop->last_pb_val[event_chan];
			//fprintf(stderr, "NOTE-ON PITCHBEND=%d (%d)\n", pb, zmop->tuning_pitchbend);
			pb = get_tuned_pitchbend(pb);
			//fprintf(stderr, "NOTE-ON TUNED PITCHBEND=%d\n",pb);
			xev.buffer = (jack_midi_data_t *) &xev_buffer;
			xev.buffer[0] = (PITCH_BEND << 4) | event_chan;
			xev.buffer[1] = pb & 0x7F;
			xev.buffer[2] = (pb >> 7) & 0x7F;
			xev.size=3;
			xev.time = ev->time;
		} else if (event_type == PITCH_BEND) {
			//Get received PB
			int pb = (ev->buffer[2] << 7) | ev->buffer[1];
			//Save last received PB value ...
			zmop->last_pb_val[event_chan] = pb;
			//Calculate tuned PB
			//fprintf(stderr, "PITCHBEND=%d\n",pb);
			pb = get_tuned_pitchbend(pb);
			//fprintf(stderr, "TUNED PITCHBEND=%d\n",pb);
			ev->buffer[1] = pb & 0x7F;
			ev->buffer[2] = (pb >> 7) & 0x7F;
		}
	}

	// Add core event to output
	if (jack_midi_event_write(zmop->buffer, ev->time, ev->buffer, ev->size))
		fprintf(stderr, "ZynMidiRouter: Error writing jack midi output event!\n");

	// Add tuning event to output
	if (xev.size > 0)
		if (jack_midi_event_write(zmop->buffer, xev.time, xev.buffer, ev->size))
			fprintf(stderr, "ZynMidiRouter: Error writing jack midi output event!\n");
	
	// Restore the original note from before transpose
	if (temp_note >= 0)
		ev->buffer[1] = (uint8_t)(temp_note & 0x7F);
}


void jack_connect_cb(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
	// Get number of connection of Output Ports
	for (int i = 0; i < MAX_NUM_ZMOPS; i++) {
		zmops[i].n_connections = jack_port_connected(zmops[i].jport);
	}
	//fprintf(stderr, "ZynMidiRouter: Num. of connections refreshed\n");

}

//-----------------------------------------------------------------------------
// Direct Send Event Ring-Buffer write
//-----------------------------------------------------------------------------

int write_rb_midi_event(jack_ringbuffer_t *rb, uint8_t *event_buffer, int event_size) {
	if (jack_ringbuffer_write_space(rb) >= event_size) {
		if (jack_ringbuffer_write(rb, event_buffer, event_size) != event_size) {
			fprintf(stderr, "ZynMidiRouter: Error writing ring-buffer: INCOMPLETE\n");
			return 0;
		}
	} else {
		fprintf(stderr, "ZynMidiRouter: Error writing ring-buffer: FULL\n");
		return 0;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// ZMIP Direct Send Functions
//-----------------------------------------------------------------------------

int zmip_send_midi_event(uint8_t iz, uint8_t *event_buffer, int event_size) {
	if (iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return write_rb_midi_event(zmips[iz].rbuffer, event_buffer, event_size);
}

int zmip_send_note_off(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_note_on(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_ccontrol_change(uint8_t iz, uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_master_ccontrol_change(uint8_t iz, uint8_t ctrl, uint8_t val) {
	if (midi_master_chan >= 0) {
		return zmip_send_ccontrol_change(iz, midi_master_chan, ctrl, val);
	}
}

int zmip_send_program_change(uint8_t iz, uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_chan_press(uint8_t iz, uint8_t chan, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xD0 + (chan & 0x0F);
	buffer[1] = val;
	buffer[2] = 0;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_pitchbend_change(uint8_t iz, uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return zmip_send_midi_event(iz, buffer, 3);
}

int zmip_send_all_notes_off(uint8_t iz) {
	if (iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	int izmop, chan, note;
	uint8_t buffer[3];
	buffer[2] = 0;
	for (izmop = 0; izmop < ZMOP_CTRL; izmop++) {
		chan = zmops[izmop].midi_chan;
		if (chan < 0) chan = 0;
		buffer[0] = 0x80 + (chan & 0x0F);
		for (note = 0; note < 128; note++) {
			buffer[1] = note;
			if (zmops[izmop].note_state[note] > 0)
				if (!write_rb_midi_event(zmips[iz].rbuffer, buffer, 3))
					return 0;
		}
	}
	return 1;
}

int zmip_send_all_notes_off_chain(uint8_t iz, uint8_t izmop) {
	if (iz >= MAX_NUM_ZMIPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	if (izmop >= ZMOP_CTRL) {
		fprintf(stderr, "ZynMidiRouter:zmip_send_all_notes_off_chain(%d, zmop) => zmop (%d) is out of range!\n", iz, izmop);
		return 0;
	}
	uint8_t note;
	uint8_t chan = zmops[izmop].midi_chan;
	if (chan < 0) chan = 0;
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[2] = 0;
	for (note = 0; note < 128; note++) {
		buffer[1] = note;
		if (zmops[izmop].note_state[note] > 0)
			if (!write_rb_midi_event(zmips[iz].rbuffer, buffer, 3))
				return 0;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// ZMIP_FAKE_UI Direct Send Functions
//-----------------------------------------------------------------------------

int ui_send_midi_event(uint8_t *event_buffer, int event_size) {
	zmip_send_midi_event(ZMIP_FAKE_UI, event_buffer, event_size);
}

int ui_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	zmip_send_note_off(ZMIP_FAKE_UI, chan, note, vel);
}

int ui_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	zmip_send_note_on(ZMIP_FAKE_UI, chan, note, vel);
}

int ui_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	zmip_send_ccontrol_change(ZMIP_FAKE_UI, chan, ctrl, val);
}

int ui_send_master_ccontrol_change(uint8_t ctrl, uint8_t val) {
	zmip_send_master_ccontrol_change(ZMIP_FAKE_UI, ctrl, val);
}

int ui_send_program_change(uint8_t chan, uint8_t prgm) {
	zmip_send_program_change(ZMIP_FAKE_UI, chan, prgm);
}

int ui_send_chan_press(uint8_t chan, uint8_t val) {
	zmip_send_chan_press(ZMIP_FAKE_UI, chan, val);
}

int ui_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	zmip_send_pitchbend_change(ZMIP_FAKE_UI, chan, pb);
}

int ui_send_all_notes_off() {
	zmip_send_all_notes_off(ZMIP_FAKE_UI);
}

int ui_send_all_notes_off_chain(uint8_t izmop) {
	zmip_send_all_notes_off_chain(ZMIP_FAKE_UI, izmop);
}

//-----------------------------------------------------------------------------
// ZMOP Direct Send Functions
//-----------------------------------------------------------------------------

int zmop_send_midi_event(uint8_t iz, uint8_t *event_buffer, int event_size) {
	if (iz >= MAX_NUM_ZMOPS) {
		fprintf(stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return write_rb_midi_event(zmops[iz].rbuffer, event_buffer, event_size);
}

int zmop_send_note_off(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return zmop_send_midi_event(iz, buffer, 3);
}

int zmop_send_note_on(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return zmop_send_midi_event(iz, buffer, 3);
}

int zmop_send_ccontrol_change(uint8_t iz, uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return zmop_send_midi_event(iz, buffer, 3);
}

int zmop_send_program_change(uint8_t iz, uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return zmop_send_midi_event(iz, buffer, 3);
}

int zmop_send_chan_press(uint8_t iz, uint8_t chan, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xD0 + (chan & 0x0F);
	buffer[1] = val;
	buffer[2] = 0;
	return zmop_send_midi_event(iz, buffer, 3);
}

int zmop_send_pitchbend_change(uint8_t iz, uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return zmop_send_midi_event(iz, buffer, 3);
}

//-----------------------------------------------------------------------------
// ZMOP_CTRL Direct Send Functions
//-----------------------------------------------------------------------------

int ctrlfb_send_midi_event(uint8_t *event_buffer, int event_size) {
	zmop_send_midi_event(ZMOP_CTRL, event_buffer, event_size);
}

int ctrlfb_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	zmop_send_note_off(ZMOP_CTRL, chan, note, vel);
}

int ctrlfb_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	zmop_send_note_on(ZMOP_CTRL, chan, note, vel);
}

int ctrlfb_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	zmop_send_ccontrol_change(ZMOP_CTRL, chan, ctrl, val);
}

int ctrlfb_send_program_change(uint8_t chan, uint8_t prgm) {
	zmop_send_program_change(ZMOP_CTRL, chan, prgm);
}

//-----------------------------------------------------------------------------
// ZMOP_DEV Direct Send Functions
//-----------------------------------------------------------------------------

int dev_send_midi_event(uint8_t idev, uint8_t *event_buffer, int event_size) {
	zmop_send_midi_event(ZMOP_DEV0 + idev, event_buffer, event_size);
}

int dev_send_note_off(uint8_t idev, uint8_t chan, uint8_t note, uint8_t vel) {
	zmop_send_note_off(ZMOP_DEV0 + idev, chan, note, vel);
}

int dev_send_note_on(uint8_t idev, uint8_t chan, uint8_t note, uint8_t vel) {
	zmop_send_note_on(ZMOP_DEV0 + idev, chan, note, vel);
}

int dev_send_ccontrol_change(uint8_t idev, uint8_t chan, uint8_t ctrl, uint8_t val) {
	zmop_send_ccontrol_change(ZMOP_DEV0 + idev, chan, ctrl, val);
}

int dev_send_program_change(uint8_t idev, uint8_t chan, uint8_t prgm) {
	zmop_send_program_change(ZMOP_DEV0 + idev, chan, prgm);
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

int read_zynmidi_buffer(uint32_t *buffer, int n) {
	int nr = jack_ringbuffer_read_space(zynmidi_buffer) >> 2;
	if (n > nr) n = nr;
	if (n > 0) {
		jack_ringbuffer_read(zynmidi_buffer, (uint8_t *)buffer, n << 2);
	}
	return n;
}

// Returns the max number of 4-bytes MIDI messages (uint32_t) in the buffer
int get_zynmidi_num_max() {
	return ZYNMIDI_BUFFER_SIZE >> 2;
}

int get_zynmidi_num_pending() {
	return jack_ringbuffer_read_space(zynmidi_buffer) >> 2;
}

//-----------------------------------------------------------------------------
// MIDI Internal Output: Send Functions => UI
//-----------------------------------------------------------------------------

int write_zynmidi_note_on(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0x90 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_note_off(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0x80 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_ccontrol_change(uint8_t chan, uint8_t num, uint8_t val) {
	uint32_t ev = ((0xB0 | (chan & 0x0F)) << 16) | (num << 8) | val;
	return write_zynmidi(ev);
}

int write_zynmidi_program_change(uint8_t chan, uint8_t num) {
	uint32_t ev = ((0xC0 | (chan & 0x0F)) << 16) | (num << 8);
	return write_zynmidi(ev);
}

//-----------------------------------------------------------------------------

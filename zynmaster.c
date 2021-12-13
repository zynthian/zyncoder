/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Master Output Library
 * 
 * Jack client for managing Master Audio & MIDI output
 * 
 * Copyright (C) 2015-2019 Fernando Moyano <jofemodo@zynthian.org>
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
#include <jack/jack.h>
#include <jack/midiport.h>

#ifdef ZYNAPTIK_CONFIG
	#include "zynaptik.h"
#endif

//-----------------------------------------------------------------------------

jack_client_t *zynmaster_jack_client;
jack_port_t  *zynmaster_jack_port_midi_in;
jack_port_t  *zynmaster_jack_port_midi_out;

int zynmaster_jack_process(jack_nframes_t nframes, void *arg);

//-----------------------------------------------------------------------------

int init_zynmaster_jack() {
	if ((zynmaster_jack_client=jack_client_open("ZynMaster", JackNullOption, 0, 0))==NULL) {
		fprintf(stderr, "ZynMaster: Error connecting with jack server.\n");
		return 0;
	}

	zynmaster_jack_port_midi_in=jack_port_register(zynmaster_jack_client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (zynmaster_jack_port_midi_in==NULL) {
		fprintf(stderr, "ZynMaster: Error creating jack midi input port.\n");
		return 0;
	}

	zynmaster_jack_port_midi_out=jack_port_register(zynmaster_jack_client, "midi_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (zynmaster_jack_port_midi_in==NULL) {
		fprintf(stderr, "ZynMaster: Error creating jack midi output port.\n");
		return 0;
	}

	//Init Jack Process
	jack_set_process_callback(zynmaster_jack_client, zynmaster_jack_process, 0);
	if (jack_activate(zynmaster_jack_client)) {
		fprintf(stderr, "ZynMaster: Error activating jack client.\n");
		return 0;
	}

	return 1;
}

int end_zynmaster_jack() {
	if (jack_client_close(zynmaster_jack_client)) {
		fprintf(stderr, "ZynMaster: Error closing jack client.\n");
		return 0;
	}
	return 1;
}

int zynmaster_jack_process(jack_nframes_t nframes, void *arg) {
	//Read jackd data buffer
	void *input_port_buffer = jack_port_get_buffer(zynmaster_jack_port_midi_in, nframes);
	if (input_port_buffer==NULL) {
		fprintf(stderr, "ZynMaster: Error getting jack input port buffer: %d frames\n", nframes);
		return -1;
	}

	//Get jack output data buffer and clear it
	void *output_port_buffer = jack_port_get_buffer(zynmaster_jack_port_midi_out, nframes);
	if (output_port_buffer==NULL) {
		fprintf(stderr, "ZynMaster: Error getting jack output port buffer: %d frames\n", nframes);
		return -1;
	}
	jack_midi_clear_buffer(output_port_buffer);

	//Process MIDI messages
	int i=0;
	jack_midi_event_t ev;
	while (jack_midi_event_get(&ev, input_port_buffer, i++)==0) {
		#ifdef ZYNAPTIK_CONFIG
		zynaptik_midi_to_cvout(&ev);
		#endif
		
		if (jack_midi_event_write(output_port_buffer, ev.time, ev.buffer, ev.size)!=0) {
			fprintf(stderr, "ZynMaster: Error writing jack midi output event!\n");
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ZynMidiRouter Library
 * 
 * MIDI router library: Implements the MIDI router & filter 
 * 
 * Copyright (C) 2015-2018 Fernando Moyano <jofemodo@zynthian.org>
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
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <lo/lo.h>

#include "zynmidirouter.h"
#include "zyncoder.h"

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------


int init_zynmidirouter() {

	if (!init_zynmidi_buffer()) return 0;
	if (!init_midi_router()) return 0;
	if (!init_jack_midi("ZynMidiRouter")) return 0; //ZynMidiRouter
	return 1;
}

int end_zynmidirouter() {
	if (!end_midi_router()) return 0;
	if (!end_jack_midi()) return 0;
	return 1;
}

//-----------------------------------------------------------------------------
// MIDI filter management
//-----------------------------------------------------------------------------

int init_midi_router() {
	int i,j,k;
	midi_filter.master_chan=-1;
	midi_filter.tuning_pitchbend=-1;
	for (i=0;i<16;i++) {
		midi_filter.transpose[i]=0;
	}
	for (i=0;i<16;i++) {
		for (j=0;j<16;j++) {
			midi_filter.clone[i][j]=0;
		}
	}
	for (i=0;i<8;i++) {
		for (j=0;j<16;j++) {
			for (k=0;k<128;k++) {
				midi_filter.event_map[i][j][k].type=THRU_EVENT;
				midi_filter.event_map[i][j][k].chan=j;
				midi_filter.event_map[i][j][k].num=k;
			}
		}
	}
	for (i=0;i<16;i++) {
		for (j=0;j<128;j++) {
			midi_filter.last_ctrl_val[i][j]=0;
		}
	}
	for (i=0;i<16;i++) {
		midi_filter.last_pb_val[i]=8192;
	}
	return 1;
}

int end_midi_router() {
	return 1;
}

void set_midi_master_chan(int chan) {
	if (chan>15 || chan<0) {
		fprintf (stderr, "ZynMidiRouter: MIDI Master channel (%d) is out of range!\n",chan);
		return;
	}
	midi_filter.master_chan=chan;
}

//MIDI filter pitch-bending fine-tuning

void set_midi_filter_tuning_freq(int freq) {
	double pb=6*log((double)freq/440.0)/log(2.0);
	if (pb<1.0 && pb>-1.0) {
		midi_filter.tuning_pitchbend=((int)(8192.0*(1.0+pb)))&0x3FFF;
		fprintf (stdout, "ZynMidiRouter: MIDI tuning frequency set to %d Hz (%d)\n",freq,midi_filter.tuning_pitchbend);
	} else {
		fprintf (stderr, "ZynMidiRouter: MIDI tuning frequency out of range!\n");
	}
}

int get_midi_filter_tuning_pitchbend() {
	return midi_filter.tuning_pitchbend;
}

int get_tuned_pitchbend(int pb) {
	int tpb=midi_filter.tuning_pitchbend+pb-8192;
	if (tpb<0) tpb=0;
	else if (tpb>16383) tpb=16383;
	return tpb;
}

//MIDI filter transposing

void set_midi_filter_transpose(uint8_t chan, int offset) {
	if (chan>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI Transpose channel (%d) is out of range!\n",chan);
		return;
	}
	if (offset<-60 || offset>60) {
		fprintf (stderr, "ZynMidiRouter: MIDI Transpose offset (%d) is out of range!\n",offset);
		return;
	}
	midi_filter.transpose[chan]=offset;
}

int get_midi_filter_transpose(uint8_t chan) {
	if (chan>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI Transpose channel (%d) is out of range!\n",chan);
		return 0;
	}
	return midi_filter.transpose[chan];
}

//MIDI filter clone
void set_midi_filter_clone(uint8_t chan_from, uint8_t chan_to, int v) {
	if (chan_from>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n",chan_from);
	}
	if (chan_to>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n",chan_to);
	}
	midi_filter.clone[chan_from][chan_to]=v;
}

int get_midi_filter_clone(uint8_t chan_from, uint8_t chan_to) {
	if (chan_from>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n",chan_from);
		return 0;
	}
	if (chan_to>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI clone chan_to (%d) is out of range!\n",chan_to);
		return 0;
	}
	return midi_filter.clone[chan_from][chan_to];
}

void reset_midi_filter_clone(uint8_t chan_from) {
	if (chan_from>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI clone chan_from (%d) is out of range!\n",chan_from);
	}
	int j;
	for (j=0;j<16;j++) {
		midi_filter.clone[chan_from][j]=0;
	}
}

//Core MIDI filter functions

int validate_midi_event(struct midi_event_st *ev) {
	if (ev->type>0xE) {
		fprintf (stderr, "ZynMidiRouter: MIDI Event type (%d) is out of range!\n",ev->type);
		return 0;
	}
	if (ev->chan>15) {
		fprintf (stderr, "ZynMidiRouter: MIDI Event channel (%d) is out of range!\n",ev->chan);
		return 0;
	}
	if (ev->num>127) {
		fprintf (stderr, "ZynMidiRouter: MIDI Event num (%d) is out of range!\n",ev->num);
		return 0;
	}
	return 1;
}

void set_midi_filter_event_map_st(struct midi_event_st *ev_from, struct midi_event_st *ev_to) {
	if (validate_midi_event(ev_from) && validate_midi_event(ev_to)) {
		//memcpy(&midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num],ev_to,sizeof(ev_to));
		struct midi_event_st *event_map=&midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num];
		event_map->type=ev_to->type;
		event_map->chan=ev_to->chan;
		event_map->num=ev_to->num;
	}
}

void set_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from,
															enum midi_event_type_enum type_to, uint8_t chan_to, uint8_t num_to) {
	struct midi_event_st ev_from={ .type=type_from, .chan=chan_from, .num=num_from };
	struct midi_event_st ev_to={ .type=type_to, .chan=chan_to, .num=num_to };
	set_midi_filter_event_map_st(&ev_from, &ev_to);
}

void set_midi_filter_event_ignore_st(struct midi_event_st *ev_from) {
	if (validate_midi_event(ev_from)) {
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].type=IGNORE_EVENT;
	}
}

void set_midi_filter_event_ignore(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from) {
	struct midi_event_st ev_from={ .type=type_from, .chan=chan_from, .num=num_from };
	set_midi_filter_event_ignore_st(&ev_from);
}

struct midi_event_st *get_midi_filter_event_map_st(struct midi_event_st *ev_from) {
	if (validate_midi_event(ev_from)) {
		return &midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num];
	}
	return NULL;
}

struct midi_event_st *get_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from) {
	struct midi_event_st ev_from={ .type=type_from, .chan=chan_from, .num=num_from };
	return get_midi_filter_event_map_st(&ev_from);
}

void del_midi_filter_event_map_st(struct midi_event_st *ev_from) {
	if (validate_midi_event(ev_from)) {
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].type=THRU_EVENT;
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].chan=ev_from->chan;
		midi_filter.event_map[ev_from->type&0x7][ev_from->chan][ev_from->num].num=ev_from->num;
	}
}

void del_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from) {
	struct midi_event_st ev_from={ .type=type_from, .chan=chan_from, .num=num_from };
	del_midi_filter_event_map_st(&ev_from);
}

void reset_midi_filter_event_map() {
	int i,j,k;
	for (i=0;i<8;i++) {
		for (j=0;j<16;j++) {
			for (k=0;k<128;k++) {
				midi_filter.event_map[i][j][k].type=THRU_EVENT;
				midi_filter.event_map[i][j][k].chan=j;
				midi_filter.event_map[i][j][k].num=k;
			}
		}
	}
}

//Simple CC mapping

void set_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from, uint8_t chan_to, uint8_t cc_to) {
	set_midi_filter_event_map(CTRL_CHANGE,chan_from,cc_from,CTRL_CHANGE,chan_to,cc_to);
}

void set_midi_filter_cc_ignore(uint8_t chan_from, uint8_t cc_from) {
	set_midi_filter_event_ignore(CTRL_CHANGE,chan_from,cc_from);
}

//TODO: It doesn't take into account if chan_from!=chan_to
uint8_t get_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from) {
	struct midi_event_st *ev=get_midi_filter_event_map(CTRL_CHANGE,chan_from,cc_from);
	return ev->num;
}

void del_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from) {
	del_midi_filter_event_map(CTRL_CHANGE,chan_from,cc_from);
}

void reset_midi_filter_cc_map() {
	int i,j;
	for (i=0;i<16;i++) {
		for (j=0;j<128;j++) {
			del_midi_filter_event_map(CTRL_CHANGE,i,j);
		}
	}
}

//-----------------------------------------------------------------------------
// Swap CC mapping => GRAPH THEORY
//-----------------------------------------------------------------------------
//	Definitions:
//-----------------------------------------------------------------------------
//	+ Node(c,n): 16 * 128 nodes
//	+ struct midi_event_st event_map[8][16][128] 
//		+ It's a weighted graph => Arrows have type: THRU_EVENT(T), SWAP_EVENT(S), CTRL_CHANGE(M)
//		+ Arrows of type T begins and ends in the same node.
//		+ Applied only to CC events => event_map[CTRL_CHANGE][c][n], arrows Aij FROM Ni(c,n) TO Nj(.chan,.num), of type .type
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


int get_mf_arrow_from(enum midi_event_type_enum type, uint8_t chan, uint8_t num, struct mf_arrow_st *arrow) {
	struct midi_event_st *to=get_midi_filter_event_map(type,chan,num);
	if (!to) return 0;
	arrow->chan_from=chan;
	arrow->num_from=num;
	arrow->chan_to=to->chan;
	arrow->num_to=to->num;
	arrow->type=to->type;
#ifdef DEBUG
	//fprintf (stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_from %d, %d => %d, %d (%d)\n", arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
#endif
	return 1;
}

int get_mf_arrow_to(enum midi_event_type_enum type, uint8_t chan, uint8_t num, struct mf_arrow_st *arrow) {
	int limit=0;
	arrow->chan_to=chan;
	arrow->num_to=num;
	//Follow the rabbit ... ;-)
	do {
		if (++limit>128) {
			fprintf (stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to => Not Closed Path or it's too long!\n");
			return 0;
		}
		if (!get_mf_arrow_from(type,arrow->chan_to,arrow->num_to,arrow)) {
			fprintf (stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to => Bad Path!\n");
			return 0;
		}
#ifdef DEBUG
		fprintf (stderr, "ZynMidiRouter: MIDI filter get_mf_arrow_to %d, %d, %d => %d, %d (%d)\n", limit, arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
#endif
		if (arrow->type>0) type=arrow->type;
	} while (arrow->chan_to!=chan || arrow->num_to!=num);
	//Return 1 => last arrow pointing to origin!
	return 1;
}


int set_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from, uint8_t chan_to, uint8_t num_to) {
	//---------------------------------------------------------------------------
	//Get current arrows "from origin" and "to destiny"
	//---------------------------------------------------------------------------
	struct mf_arrow_st arrow_from;
	struct mf_arrow_st arrow_to;
	if (!get_mf_arrow_from(CTRL_CHANGE,chan_from,num_from,&arrow_from)) return 0;
	if (!get_mf_arrow_to(CTRL_CHANGE,chan_to,num_to,&arrow_to)) return 0;

	//---------------------------------------------------------------------------
	//Check validity of new CC Arrow
	//---------------------------------------------------------------------------
	//No CTRL_CHANGE arrow from same origin
	if (arrow_from.type==CTRL_CHANGE) {
		fprintf (stderr, "ZynMidiRouter: MIDI filter CC set swap-map => Origin already has a CTRL_CHANGE map!\n");
		return 0;
	}
	//No CTRL_CHANGE arrow to same destiny
	if (arrow_to.type==CTRL_CHANGE) {
		fprintf (stderr, "ZynMidiRouter: MIDI filter CC set swap-map => Destiny already has a CTRL_CHANGE map!\n");
		return 0;
	}

	//Create CC Map from => to
	set_midi_filter_event_map(CTRL_CHANGE,chan_from,num_from,CTRL_CHANGE,chan_to,num_to);
#ifdef DEBUG
	fprintf (stderr, "ZynMidiRouter: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", chan_from, num_from, chan_to, num_to, CTRL_CHANGE);
#endif
	
	//Create extra mapping overwriting current extra mappings, to enforce Rule A
	enum midi_event_type_enum type=SWAP_EVENT;
	if (arrow_from.chan_to==arrow_to.chan_from && arrow_from.num_to==arrow_to.num_from) type=THRU_EVENT;
	set_midi_filter_event_map(CTRL_CHANGE,arrow_to.chan_from,arrow_to.num_from,type,arrow_from.chan_to,arrow_from.num_to);
	//set_midi_filter_event_map(CTRL_CHANGE,arrow_from.chan_to,arrow_from.num_to,type,arrow_to.chan_from,arrow_to.num_from);
#ifdef DEBUG
	fprintf (stderr, "ZynMidiRouter: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", arrow_to.chan_from, arrow_to.num_from, arrow_from.chan_to, arrow_from.num_to, type);
#endif

	return 1;
}


int del_midi_filter_cc_swap(uint8_t chan, uint8_t num) {
	//---------------------------------------------------------------------------
	//Get current arrow Axy (from origin to destiny)
	//---------------------------------------------------------------------------
	struct mf_arrow_st arrow;
	if (!get_mf_arrow_from(CTRL_CHANGE,chan,num,&arrow)) return 0;

	//---------------------------------------------------------------------------
	//Get current arrow pointing to origin (Ajx)
	//---------------------------------------------------------------------------
	struct mf_arrow_st arrow_to;
	if (!get_mf_arrow_to(CTRL_CHANGE,chan,num,&arrow_to)) return 0;

	//---------------------------------------------------------------------------
	//Get current arrow from destiny (Ayk)
	//---------------------------------------------------------------------------
	struct mf_arrow_st arrow_from;
	if (!get_mf_arrow_from(CTRL_CHANGE,arrow.chan_to,arrow.num_to,&arrow_from)) return 0;

	//---------------------------------------------------------------------------
	//Create/Delete extra arrows for enforcing Rule A
	//---------------------------------------------------------------------------

	if (arrow_to.type!=SWAP_EVENT && arrow_from.type!=SWAP_EVENT) {
		//Create Axy of type SWAP_EVENT => Replace CTRL_CHANGE by SWAP_EVENT
		set_midi_filter_event_map(CTRL_CHANGE,arrow.chan_from,arrow.num_from,SWAP_EVENT,arrow.chan_to,arrow.num_to);
	} else {
		if (arrow_to.type==SWAP_EVENT) {
			//Create Axx of type THRU_EVENT
			del_midi_filter_cc_map(arrow.chan_from,arrow.num_from);
		} else {
			//Create Axk of type SWAP_EVENT
			set_midi_filter_event_map(CTRL_CHANGE,arrow.chan_from,arrow.num_from,SWAP_EVENT,arrow_from.chan_to,arrow_from.num_to);
		}
		if (arrow_from.type==SWAP_EVENT) {
			//Create Ayy of type THRU_EVENT
			del_midi_filter_cc_map(arrow.chan_to,arrow.num_to);
		} else {
			//Create Ajy of type SWAP_EVENT
			set_midi_filter_event_map(CTRL_CHANGE,arrow_to.chan_from,arrow_to.num_from,SWAP_EVENT,arrow.chan_to,arrow.num_to);
		}
	}

	return 1;
}

uint8_t get_midi_filter_cc_swap(uint8_t chan, uint8_t num) {
	struct mf_arrow_st arrow;
	if (!get_mf_arrow_to(CTRL_CHANGE,chan,num,&arrow)) return 0;
	else return arrow.num_from;
}

//-----------------------------------------------------------------------------
// ZynMidi Input/Ouput Port management
//-----------------------------------------------------------------------------

int zmop_init(int iz, char *name, int ch) {
	if (iz<0 || iz>=MAX_NUM_ZMOPS) {
		fprintf (stderr, "ZynMidiRouter: Bad index (%d) initializing ouput port '%s'.\n", iz, name);
		return 0;
	}
	//Create Jack Output Port
	zmops[iz].jport = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (zmops[iz].jport == NULL) {
		fprintf (stderr, "ZynMidiRouter: Error creating jack midi output port '%s'.\n", name);
		return 0;
	}
	//Set init values
	zmops[iz].n_data=0;
	zmops[iz].midi_channel=ch;
	zmops[iz].n_connections=0;
	return 1;
}

int zmop_push_event(int iz, jack_midi_event_t ev, int ch) {
	if (iz<0 || iz>=MAX_NUM_ZMOPS) {
		fprintf (stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return -1;
	}
	struct zmop_st *zmop=zmops+iz;
	if (zmop->midi_channel<0 || zmop->midi_channel==ch) {
		memcpy(zmop->data+zmop->n_data, ev.buffer, ev.size);
		zmop->n_data+=ev.size;
		return ev.size;
	}
	return 0;
}

int zmop_clear_data(int iz) {
	if (iz<0 || iz>=MAX_NUM_ZMOPS) {
		fprintf (stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
		return 0;
	}
	zmops[iz].n_data=0;
	return 1;
}

int zmops_clear_data() {
	int i;
	for (i=0;i<MAX_NUM_ZMOPS;i++) {
		zmops[i].n_data=0;
	}
	return 1;
}

int zmip_init(int iz, char *name, uint32_t flags) {
	if (iz<0 || iz>=MAX_NUM_ZMIPS) {
		fprintf (stderr, "ZynMidiRouter: Bad index (%d) initializing input port '%s'.\n", iz, name);
		return 0;
	}
	//Create Jack Output Port
	zmips[iz].jport = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (zmips[iz].jport == NULL) {
		fprintf (stderr, "ZynMidiRouter: Error creating jack midi input port '%s'.\n", name);
		return 0;
	}
	//Clear zmop forwarding flags
	int i;
	for (i=0;i<MAX_NUM_ZMOPS;i++)
		zmips[iz].fwd_zmops[i]=0;

	//Set flag init value
	zmips[iz].flags=flags;

	return 1;
}

int zmip_set_forward(int izmip, int izmop, int fwd) {
	if (izmip<0 || izmip>=MAX_NUM_ZMIPS) {
		fprintf (stderr, "ZynMidiRouter: Bad input port index (%d).\n", izmip);
		return 0;
	}
	if (izmop<0 || izmop>=MAX_NUM_ZMOPS) {
		fprintf (stderr, "ZynMidiRouter: Bad output port index (%d).\n", izmop);
		return 0;
	}
	zmips[izmip].fwd_zmops[izmop]=fwd;
	return 1;
}

int zmip_set_flags(int iz, uint32_t flags) {
	if (iz<0 || iz>=MAX_NUM_ZMIPS) {
		fprintf (stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	zmips[iz].flags=flags;
	return 1;
}

int zmip_has_flags(int iz, uint32_t flags) {
	if (iz<0 || iz>=MAX_NUM_ZMIPS) {
		fprintf (stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
		return 0;
	}
	return (zmips[iz].flags & flags)==flags;
}

//-----------------------------------------------------------------------------
// Jack MIDI processing
//-----------------------------------------------------------------------------

int init_jack_midi(char *name) {
	if ((jack_client = jack_client_open(name, JackNullOption , 0 , 0 )) == NULL) {
		fprintf (stderr, "ZynMidiRouter: Error connecting with jack server.\n");
		return 0;
	}

	int i;

	//Init Output Ports
	if (!zmop_init(ZMOP_MAIN,"main_out",-1)) return 0;
	if (!zmop_init(ZMOP_NET,"net_out",-1)) return 0;
	char port_name[12];
	for (i=0;i<16;i++) {
		sprintf(port_name,"ch%d_out",i);
		if (!zmop_init(ZMOP_CH0+i,port_name,i)) return 0;
	}

	//Init Input Ports
	if (!zmip_init(ZMIP_MAIN,"main_in",ZMIP_MAIN_FLAGS)) return 0;
	if (!zmip_init(ZMIP_NET,"net_in",ZMIP_MAIN_FLAGS)) return 0;
	if (!zmip_init(ZMIP_SEQ,"seq_in",ZMIP_SEQ_FLAGS)) return 0;
	if (!zmip_init(ZMIP_CTRL,"ctrl_in",ZMIP_CTRL_FLAGS)) return 0;
	
	//Route Input to Output Ports
	for (i=0;i<MAX_NUM_ZMOPS;i++) {
		if (!zmip_set_forward(ZMIP_MAIN, i, 1)) return 0;
		if (i!=ZMOP_NET) {
			if (!zmip_set_forward(ZMIP_NET, i, 1)) return 0;
		}
		if (!zmip_set_forward(ZMIP_SEQ, i, 1)) return 0;
		//TODO => Define ZMIP_CTRL behaviour
	}

	jack_ring_output_buffer = jack_ringbuffer_create(JACK_MIDI_BUFFER_SIZE);
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(jack_ring_output_buffer)) {
		fprintf (stderr, "ZynMidiRouter: Error locking memory for jack ring output buffer.\n");
		return 0;
	}
	jack_set_process_callback(jack_client, jack_process, 0);
	if (jack_activate(jack_client)) {
		fprintf (stderr, "ZynMidiRouter: Error activating jack client.\n");
		return 0;
	}
	return 1;
}

int end_jack_midi() {
	return jack_client_close(jack_client);
}


//-----------------------------------------------------
// Process ZynMidi Input Port (zmip)
// forwarding the output to several zmops
//-----------------------------------------------------

int jack_process_zmip(int iz, jack_nframes_t nframes) {
	if (iz<0 || iz>=MAX_NUM_ZMIPS) {
		fprintf (stderr, "ZynMidiRouter: Bad input port index (%d).\n", iz);
	}
	struct zmip_st *zmip=zmips+iz;

	int i=0;
	int j;
	uint8_t event_type;
	uint8_t event_chan;
	uint8_t event_num;
	uint8_t event_val;

	//Read jackd data buffer
	void *input_port_buffer = jack_port_get_buffer(zmip->jport, nframes);
	if (input_port_buffer==NULL) {
		fprintf (stderr, "ZynMidiRouter: Error allocating jack input port buffer: %d frames\n", nframes);
		return -1;
	}

	//Process MIDI messages
	jack_midi_event_t ev;
	int clone_from_chan=-1;
	int clone_to_chan=-1;
	while (1) {

		//Test if reached max num of events
		if (i>nframes) {
			fprintf (stderr, "ZynMidiRouter: Error processing jack midi input events: TOO MANY EVENTS\n");
			return -1;
		}

		//Clone from last event ...
		if (clone_from_chan>=0 && clone_to_chan>=0 && clone_to_chan<16) {
			event_chan=clone_to_chan;
			ev.buffer[0]=(event_type << 4) | event_chan;
			//fprintf (stdout, "CLONE %x => %d, %d\n",event_type, clone_from_chan, clone_to_chan);
			clone_to_chan++;
		}
		//Or get next event ...
		else {
			if (jack_midi_event_get(&ev, input_port_buffer, i++)!=0) break;

			//Ignore SysEx messages
			if (ev.buffer[0]==SYSTEM_EXCLUSIVE) continue;

			//Get event type & chan
			if (ev.buffer[0]>=SYSTEM_EXCLUSIVE) {
				event_type=ev.buffer[0];
				event_chan=0;
			}
			else {
				event_type=ev.buffer[0] >> 4;
				event_chan=ev.buffer[0] & 0xF;
			}

			//Get event details depending of event size & type
			if (ev.size==3) {
				if (event_type==PITCH_BENDING) {
					event_num=0;
					event_val=ev.buffer[2] & 0x7F;
				} else {
					event_num=ev.buffer[1] & 0x7F;
					event_val=ev.buffer[2] & 0x7F;
				}
			}
			else if (ev.size==2) {
				event_num=ev.buffer[1] & 0x7F;
				event_val=0;
			}
			else {
				event_num=event_val=0;
			}

			//Is it a clonable event?
			if ((zmip->flags & FLAG_ZMIP_CLONE) && (event_type==NOTE_OFF || event_type==NOTE_ON || event_type==PITCH_BENDING || event_type==KEY_PRESS || event_type==CHAN_PRESS)) {
				clone_from_chan=event_chan;
				clone_to_chan=0;
			}
			else {
				clone_from_chan=-1;
				clone_to_chan=-1;
			}
		}

		//Check for next clone_to channel ...
		if (clone_from_chan>=0 && clone_to_chan>=0) {
			while (clone_to_chan<16 && !midi_filter.clone[clone_from_chan][clone_to_chan]) clone_to_chan++;
			//fprintf (stdout, "NEXT CLONE %x => %d, %d\n",event_type, clone_from_chan, clone_to_chan);
		}

		//fprintf(stdout, "MIDI MSG => %x, %x\n", ev.buffer[0], ev.buffer[1]);

		//Capture events for UI: before filtering => [Control-Change]
		if ((zmip->flags & FLAG_ZMIP_UI) && event_type==CTRL_CHANGE) {
			write_zynmidi((ev.buffer[0]<<16)|(ev.buffer[1]<<8)|(ev.buffer[2]));
		}

		//Event Mapping
		if ((zmip->flags & FLAG_ZMIP_FILTER) && event_type>=NOTE_OFF && event_type<=PITCH_BENDING) {
			struct midi_event_st *event_map=&midi_filter.event_map[event_type & 0x7][event_chan][event_num];
			//Ignore event...
			if (event_map->type==IGNORE_EVENT) {
				//fprintf (stdout, "IGNORE => %x, %x, %x\n",event_type, event_chan, event_num);
				continue;
			}
			//Map event ...
			if (event_map->type>=0 || event_map->type==SWAP_EVENT) {
				//fprintf (stdout, "ZynMidiRouter: Event Map %d, %d => ",ev.buffer[0],ev.buffer[1]);
				if (event_map->type!=SWAP_EVENT) event_type=event_map->type;
				event_chan=event_map->chan;
				ev.buffer[0]=(event_type << 4) | event_chan;
				if (event_map->type==PROG_CHANGE || event_map->type==CHAN_PRESS) {
					ev.buffer[1]=event_num;
					event_val=0;
					ev.size=2;
				} else if (event_map->type==PITCH_BENDING) {
					event_num=0;
					ev.buffer[1]=0;
					ev.buffer[2]=event_val;
					ev.size=3;
				} else {
					event_num=event_map->num;
					ev.buffer[1]=event_num;
					ev.buffer[2]=event_val;
					ev.size=3;
				}
				//fprintf (stdout, "MIDI MSG => %x, %x\n",ev.buffer[0],ev.buffer[1]);
			}
		}

		//MIDI CC messages
		if ((zmip->flags & FLAG_ZMIP_ZYNCODER) && event_type==CTRL_CHANGE) {
			//Update zyncoder value => TODO Optimize this fragment!!!
			for (j=0;j<MAX_NUM_ZYNCODERS;j++) {
				if (zyncoders[j].enabled && zyncoders[j].midi_chan==event_chan && zyncoders[j].midi_ctrl==event_num) {
					zyncoders[j].value=event_val;
					zyncoders[j].subvalue=event_val*ZYNCODER_TICKS_PER_RETENT;
				}
			}
		}

		//Transpose Note-on/off messages
		else if ((zmip->flags & FLAG_ZMIP_TRANSPOSE) && midi_filter.transpose[event_chan]!=0) {
			if (event_type==NOTE_OFF || event_type==NOTE_ON) {
				int note=ev.buffer[1]+midi_filter.transpose[event_chan];
				//If transposed note is out of range, ignore message ...
				if (note>0x7F || note<0) continue;
				ev.buffer[1]=(uint8_t)(note & 0x7F);
			}
		}

		// Fine-Tuning, using pitch-bending messages ...
		if ((zmip->flags & FLAG_ZMIP_TUNING) && midi_filter.tuning_pitchbend>=0) {
			if (event_type==NOTE_ON) {
				int pb=midi_filter.last_pb_val[event_chan];
				//printf("NOTE-ON PITCHBEND=%d (%d)\n",pb,midi_filter.tuning_pitchbend);
				pb=get_tuned_pitchbend(pb);
				//printf("NOTE-ON TUNED PITCHBEND=%d\n",pb);
				zynmidi_send_pitchbend_change(event_chan,pb);
			} else if (event_type==PITCH_BENDING) {
				//Get received PB
				int pb=(ev.buffer[2] << 7) | ev.buffer[1];
				//Save last received PB value ...
				midi_filter.last_pb_val[event_chan]=pb;
				//Calculate tuned PB
				//printf("PITCHBEND=%d\n",pb);
				pb=get_tuned_pitchbend(pb);
				//printf("TUNED PITCHBEND=%d\n",pb);
				ev.buffer[1]=pb & 0x7F;
				ev.buffer[2]=(pb >> 7) & 0x7F;
			}
		}

		//Capture events for UI: after filtering => [Note-Off, Note-On, Program-Change]
		if ((zmip->flags & FLAG_ZMIP_UI) && (event_type==NOTE_OFF || event_type==NOTE_ON || event_type==PROG_CHANGE)) {
			write_zynmidi((ev.buffer[0]<<16)|(ev.buffer[1]<<8)|(ev.buffer[2]));
		}

		//Forward message to the configured output ports
		for (j=0;j<MAX_NUM_ZMOPS;j++) {
			if (zmip->fwd_zmops[j] && zmops[j].n_connections>0) {
				zmop_push_event(j, ev, event_chan);
			}
		}

	}
	return 0;
}

//-----------------------------------------------------
// Process ZynMidi Output Port (zmop)
//-----------------------------------------------------

int jack_process_zmop(int iz, jack_nframes_t nframes) {
	if (iz<0 || iz>=MAX_NUM_ZMOPS) {
		fprintf (stderr, "ZynMidiRouter: Bad output port index (%d).\n", iz);
	}
	struct zmop_st *zmop=zmops+iz;

	int i=0;
	int j;
	uint8_t event_type;
	uint8_t event_chan;
	uint8_t event_num;
	uint8_t event_val;
	uint8_t event_size;

	//Get MIDI jack data buffer and clear it
	void *output_port_buffer = jack_port_get_buffer(zmop->jport, nframes);
	if (output_port_buffer==NULL) {
		fprintf (stderr, "ZynMidiRouter: Error allocating jack output port buffer: %d frames\n", nframes);
		return -1;
	}
	jack_midi_clear_buffer(output_port_buffer);

	//Write MIDI data
	int pos=0;
	while (pos < zmop->n_data) {
		event_type= zmop->data[pos] >> 4;

		if (zmop->data[pos]>=0xF4) event_size=1;
		else if (event_type==PROG_CHANGE || event_type==CHAN_PRESS || event_type==TIME_CODE_QF || event_type==SONG_SELECT) event_size=2;
		else event_size=3;

		//Channel filter
		if (zmop->midi_channel>=0) {
			if (event_type<NOTE_OFF || event_type>PITCH_BENDING || zmop->midi_channel!=(zmop->data[pos]&0xF)) {
				pos+=event_size;
				i++;
				continue;
			}
		}

		/*
		//Master Channel Control
		if (event_type==CTRL_CHANGE) {
			event_chan=zmop->data[pos] & 0xF;
			event_num=zmop->data[pos+1] & 0x7F;
			event_val=zmop->data[pos+2] & 0x7F;

			//Save last controller values for Master Channel calculation ...
			midi_filter.last_ctrl_val[event_chan][event_num]=event_val;

			//Captured Controllers => volume
			if (event_num==0x7) {
				if (midi_filter.master_chan>=0) {
					//if channel is master, resend ctrl messages to all normal channels ...
					if (event_chan==midi_filter.master_chan) {
						for (j=0;j<16;j++) {
							if (j==midi_filter.master_chan) continue;
							zynmidi_send_ccontrol_change(j,event_num,midi_filter.last_ctrl_val[j][event_num]);
						}
					//if channel is not master, scale value proportionally to Master Channel value ...
					} else {
						zmop->data[pos+2]=((int32_t)event_val*(uint32_t)midi_filter.last_ctrl_val[midi_filter.master_chan][event_num])>>7;
					}
				}
			}
		}
		*/

		//Write to Jackd buffer
		uint8_t *buffer = jack_midi_event_reserve(output_port_buffer, i, event_size);
		memcpy(buffer, zmop->data+pos, event_size);
		pos+=event_size;

		if (i>nframes) {
			fprintf (stderr, "ZynMidiRouter: Error processing jack midi output events: TOO MANY EVENTS\n");
			return -1;
		}
		i++;
	}

	return 0;
}


//-----------------------------------------------------
// Jack Process
//-----------------------------------------------------

int jack_process(jack_nframes_t nframes, void *arg) {
	int i;

	//---------------------------------
	// Clear Output Port Data Buffers
	//---------------------------------
	zmops_clear_data();

	//---------------------------------
	// Get number of connection of Output Ports
	//---------------------------------
	for (i=0;i<MAX_NUM_ZMOPS;i++) {
		zmops[i].n_connections=jack_port_connected(zmops[i].jport);
	}

	//---------------------------------
	//MIDI Input
	//---------------------------------
	for (i=0;i<MAX_NUM_ZMIPS;i++) {
		if (jack_process_zmip(i, nframes)<0) return -1;
	}

	//---------------------------------
	//Internal MIDI Thru
	//---------------------------------
	//Forward internal MIDI data from ringbuffer to all ZMOPS
	if (forward_internal_midi_data()<0) return -1;

	//---------------------------------
	//MIDI Output
	//---------------------------------
	for (i=0;i<MAX_NUM_ZMOPS;i++) {
		if (zmops[i].n_connections>0) {
			if (jack_process_zmop(i, nframes)<0) return -1;
		}
	}

	return 0;
}

//-----------------------------------------------------
// MIDI Internal Input Event Buffer <= UI and internal
//-----------------------------------------------------

uint8_t internal_midi_data[JACK_MIDI_BUFFER_SIZE];

int write_internal_midi_event(uint8_t *event_buffer, int event_size) {
	if (jack_ringbuffer_write_space(jack_ring_output_buffer)>=event_size) {
		if (jack_ringbuffer_write(jack_ring_output_buffer, event_buffer, event_size)!=event_size) {
			fprintf (stderr, "ZynMidiRouter: Error writing jack ring output buffer: INCOMPLETE\n");
			return 0;
		}
	}
	else {
		fprintf (stderr, "ZynMidiRouter: Error writing jack ring output buffer: FULL\n");
		return 0;
	}
	return 1;
}

//Get MIDI data from ringbuffer
int forward_internal_midi_data() {
	int nb=jack_ringbuffer_read_space(jack_ring_output_buffer);
	if (jack_ringbuffer_read(jack_ring_output_buffer, internal_midi_data, nb)!=nb) {
		fprintf (stderr, "ZynMidiRouter: Error reading midi data from jack ring output buffer: %d bytes\n", nb);
		return -1;
	}
	int i;
	for (i=0;i<MAX_NUM_ZMOPS;i++) {
		memcpy(zmops[i].data+zmops[i].n_data, internal_midi_data, nb);
		zmops[i].n_data+=nb;
	}
	return nb;
}

//-----------------------------------------------------------------------------
// MIDI Internal Ouput Events Buffer => UI
//-----------------------------------------------------------------------------

uint32_t zynmidi_buffer[ZYNMIDI_BUFFER_SIZE];
int zynmidi_buffer_read;
int zynmidi_buffer_write;

int init_zynmidi_buffer() {
	int i;
	for (i=0;i<ZYNMIDI_BUFFER_SIZE;i++) zynmidi_buffer[i]=0;
	zynmidi_buffer_read=zynmidi_buffer_write=0;
	return 1;
}

int write_zynmidi(uint32_t ev) {
	int nptr=zynmidi_buffer_write+1;
	if (nptr>=ZYNMIDI_BUFFER_SIZE) nptr=0;
	if (nptr==zynmidi_buffer_read) return 0;
	zynmidi_buffer[zynmidi_buffer_write]=ev;
	zynmidi_buffer_write=nptr;
	return 1;
}

uint32_t read_zynmidi() {
	if (zynmidi_buffer_read==zynmidi_buffer_write) return 0;
	uint32_t ev=zynmidi_buffer[zynmidi_buffer_read++];
	if (zynmidi_buffer_read>=ZYNMIDI_BUFFER_SIZE) zynmidi_buffer_read=0;
	return ev;
}

//-----------------------------------------------------------------------------
// MIDI Send Functions
//-----------------------------------------------------------------------------

int zynmidi_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_internal_midi_event(buffer,3);
}

int zynmidi_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return write_internal_midi_event(buffer,3);
}

int zynmidi_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return write_internal_midi_event(buffer,3);
}

int zynmidi_send_program_change(uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return write_internal_midi_event(buffer,3);
}

int zynmidi_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return write_internal_midi_event(buffer,3);
}

int zynmidi_send_master_ccontrol_change(uint8_t ctrl, uint8_t val) {
	if (midi_filter.master_chan>=0) {
		return zynmidi_send_ccontrol_change(midi_filter.master_chan, ctrl, val);
	}
}

//-----------------------------------------------------------------------------

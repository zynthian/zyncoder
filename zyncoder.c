/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing Rotary Encoders & Switches connected 
 * to RBPi native GPIOs or expanded with MCP23008. Includes an 
 * emulator mode to ease developping.
 * 
 * Copyright (C) 2015-2016 Fernando Moyano <jofemodo@zynthian.org>
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
#include <pthread.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <lo/lo.h>

#include "zyncoder.h"

#if defined(MCP23017_ENCODERS) && defined(HAVE_WIRINGPI_LIB)
	// pins 100-115 are located on our mcp23017
	#define MCP23017_BASE_PIN 100
	// interrupt pins for the mcp
	#define MCP23017_INTA_PIN 27
	#define MCP23017_INTB_PIN 25
	#include <wiringPi.h>
	#include <mcp23017.h>
	#include <mcp23x0817.h>
	#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
	#define bitSet(value, bit) ((value) |= (1UL << (bit)))
	#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
	#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#elif HAVE_WIRINGPI_LIB
	#define MCP23008_BASE_PIN 100
	#include <wiringPi.h>
	#include <mcp23008.h>
#else
	#define MCP23008_BASE_PIN 100
	#include "wiringPiEmu.h"
#endif

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

//Switch Polling interval
int poll_zynswitches_us=10000;

pthread_t init_poll_zynswitches();
int init_zyncoder_osc(int osc_port);
int end_zyncoder_osc();
int init_zyncoder_midi(char *name);
int end_zyncoder_midi();

#ifdef MCP23017_ENCODERS
// wiringpi node structure for direct access to the mcp23017
struct wiringPiNodeStruct *mcp23017_node;

// two ISR routines for the two banks
void mcp23017_bank_ISR(uint8_t bank);
void mcp23017_bankA_ISR() { mcp23017_bank_ISR(0); }
void mcp23017_bankB_ISR() { mcp23017_bank_ISR(1); }
void (*mcp23017_bank_ISRs[2])={
	mcp23017_bankA_ISR,
	mcp23017_bankB_ISR
};

unsigned int int_to_int(unsigned int k) {
	return (k == 0 || k == 1 ? k : ((k % 2) + 10 * int_to_int(k / 2)));
}
#endif

int init_zyncoder(int osc_port) {
	int i,j;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) zynswitches[i].enabled=0;
	for (i=0;i<MAX_NUM_ZYNCODERS;i++) {
		zyncoders[i].enabled=0;
		for (j=0;j<ZYNCODER_TICKS_PER_RETENT;j++) zyncoders[i].dtus[j]=0;
	}
	for (i=0;i<ZYNMIDI_BUFFER_SIZE;i++) zynmidi_buffer[i]=0;
	zynmidi_buffer_read=zynmidi_buffer_write=0;
	init_midi_filter();
	wiringPiSetup();
#ifdef MCP23017_ENCODERS
	uint8_t reg;

	wiringPiSetup();
	mcp23017Setup(MCP23017_BASE_PIN, 0x20);

	// get the node cooresponding to our mcp23017 so we can do direct writes
	mcp23017_node = wiringPiFindNode(MCP23017_BASE_PIN);

	// setup all the pins on the banks as inputs and disable pullups on
	// the zyncoder input
	reg = 0xff;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IODIRA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IODIRB, reg);

	// enable pullups on the unused pins (high two bits on each bank)
	reg = 0x60;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_GPPUA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_GPPUB, reg);

	// disable polarity inversion
	reg = 0;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IPOLA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IPOLB, reg);

	// disable the comparison to DEFVAL register
	reg = 0;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_INTCONA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_INTCONB, reg);

	// configure the interrupt behavior for bank A
	uint8_t ioconf_value = wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_IOCON);
	bitWrite(ioconf_value, 6, 0);	// banks are not mirrored
	bitWrite(ioconf_value, 2, 0);	// interrupt pin is not floating
	bitWrite(ioconf_value, 1, 1);	// interrupt is signaled by high
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IOCON, ioconf_value);

	// configure the interrupt behavior for bank B
	ioconf_value = wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_IOCONB);
	bitWrite(ioconf_value, 6, 0);	// banks are not mirrored
	bitWrite(ioconf_value, 2, 0);	// interrupt pin is not floating
	bitWrite(ioconf_value, 1, 1);	// interrupt is signaled by high
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IOCONB, ioconf_value);

	// finally, enable the interrupt pins for banks a and b
	// enable interrupts on all pins
	reg = 0xff;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_GPINTENA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_GPINTENB, reg);

	// pi ISRs for the 23017
	// bank A
	wiringPiISR(MCP23017_INTA_PIN, INT_EDGE_RISING, mcp23017_bank_ISRs[0]);
	// bank B
	wiringPiISR(MCP23017_INTB_PIN, INT_EDGE_RISING, mcp23017_bank_ISRs[1]);

#ifdef DEBUG
	printf("mcp23017 initialized\n");
#endif
#else
	mcp23008Setup (100, 0x20);
	init_poll_zynswitches();
#endif
	init_zyncoder_osc(osc_port);
	return init_zyncoder_midi("Zyncoder");
}

int end_zyncoder() {
	end_zyncoder_osc();
	return end_zyncoder_midi();
}

//-----------------------------------------------------------------------------
// OSC Message processing
//-----------------------------------------------------------------------------

lo_address osc_lo_addr;
char osc_port_str[8];

int init_zyncoder_osc(int osc_port) {
	if (osc_port) {
		sprintf(osc_port_str,"%d",osc_port);
		//printf("OSC PORT: %s\n",osc_port_str);
		osc_lo_addr=lo_address_new(NULL,osc_port_str);
		return 0;
	}
	return -1;
}

int end_zyncoder_osc() {
	return 0;
}

//-----------------------------------------------------------------------------
// MIDI filter management
//-----------------------------------------------------------------------------

void init_midi_filter() {
	int i,j,k;
	midi_filter.master_chan=-1;
	midi_filter.tuning_pitchbend=-1;
	for (i=0;i<16;i++) {
		midi_filter.transpose[i]=0;
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
	
}

void set_midi_master_chan(int chan) {
	if (chan>15 || chan<0) {
		fprintf (stderr, "Zyncoder: MIDI Master channel (%d) is out of range!\n",chan);
		return;
	}
	midi_filter.master_chan=chan;
}

//MIDI pitch-bending fine-tuning

void set_midi_filter_tuning_freq(int freq) {
	double pb=6*log((double)freq/440.0)/log(2.0);
	if (pb<1.0 && pb>-1.0) {
		midi_filter.tuning_pitchbend=((int)(8192.0*(1.0+pb)))&0x3FFF;
		fprintf (stdout, "Zyncoder: MIDI tuning frequency set to %d Hz (%d)\n",freq,midi_filter.tuning_pitchbend);
	} else {
		fprintf (stderr, "Zyncoder: MIDI tuning frequency out of range!\n");
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

//MIDI transposing

void set_midi_filter_transpose(uint8_t chan, int offset) {
	if (chan>15) {
		fprintf (stderr, "Zyncoder: MIDI Transpose channel (%d) is out of range!\n",chan);
		return;
	}
	if (offset<-60 || offset>60) {
		fprintf (stderr, "Zyncoder: MIDI Transpose offset (%d) is out of range!\n",offset);
		return;
	}
	midi_filter.transpose[chan]=offset;
}

int get_midi_filter_transpose(uint8_t chan) {
	if (chan>15) {
		fprintf (stderr, "Zyncoder: MIDI Transpose channel (%d) is out of range!\n",chan);
		return 0;
	}
	return midi_filter.transpose[chan];
}

//Core MIDI filter functions

int validate_midi_event(struct midi_event_st *ev) {
	if (ev->type>0xE) {
		fprintf (stderr, "Zyncoder: MIDI Event type (%d) is out of range!\n",ev->type);
		return 0;
	}
	if (ev->chan>15) {
		fprintf (stderr, "Zyncoder: MIDI Event channel (%d) is out of range!\n",ev->chan);
		return 0;
	}
	if (ev->num>127) {
		fprintf (stderr, "Zyncoder: MIDI Event num (%d) is out of range!\n",ev->num);
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
	//fprintf (stderr, "Zyncoder: MIDI filter get_mf_arrow_from %d, %d => %d, %d (%d)\n", arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
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
			fprintf (stderr, "Zyncoder: MIDI filter get_mf_arrow_to => Not Closed Path or it's too long!\n");
			return 0;
		}
		if (!get_mf_arrow_from(type,arrow->chan_to,arrow->num_to,arrow)) {
			fprintf (stderr, "Zyncoder: MIDI filter get_mf_arrow_to => Bad Path!\n");
			return 0;
		}
#ifdef DEBUG
		fprintf (stderr, "Zyncoder: MIDI filter get_mf_arrow_to %d, %d, %d => %d, %d (%d)\n", limit, arrow->chan_from, arrow->num_from, arrow->chan_to, arrow->num_to, arrow->type);
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
		fprintf (stderr, "Zyncoder: MIDI filter CC set swap-map => Origin already has a CTRL_CHANGE map!\n");
		return 0;
	}
	//No CTRL_CHANGE arrow to same destiny
	if (arrow_to.type==CTRL_CHANGE) {
		fprintf (stderr, "Zyncoder: MIDI filter CC set swap-map => Destiny already has a CTRL_CHANGE map!\n");
		return 0;
	}

	//Create CC Map from => to
	set_midi_filter_event_map(CTRL_CHANGE,chan_from,num_from,CTRL_CHANGE,chan_to,num_to);
#ifdef DEBUG
	fprintf (stderr, "Zyncoder: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", chan_from, num_from, chan_to, num_to, CTRL_CHANGE);
#endif
	
	//Create extra mapping overwriting current extra mappings, to enforce Rule A
	enum midi_event_type_enum type=SWAP_EVENT;
	if (arrow_from.chan_to==arrow_to.chan_from && arrow_from.num_to==arrow_to.num_from) type=THRU_EVENT;
	set_midi_filter_event_map(CTRL_CHANGE,arrow_to.chan_from,arrow_to.num_from,type,arrow_from.chan_to,arrow_from.num_to);
	//set_midi_filter_event_map(CTRL_CHANGE,arrow_from.chan_to,arrow_from.num_to,type,arrow_to.chan_from,arrow_to.num_from);
#ifdef DEBUG
	fprintf (stderr, "Zyncoder: MIDI filter set_mf_arrow %d, %d => %d, %d (%d)\n", arrow_to.chan_from, arrow_to.num_from, arrow_from.chan_to, arrow_from.num_to, type);
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
// MIDI Input Events Buffer Management
//-----------------------------------------------------------------------------

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
// Jack MIDI processing
//-----------------------------------------------------------------------------

jack_client_t *jack_client;
jack_port_t *jack_midi_output_port;
jack_port_t *jack_midi_input_port;
jack_ringbuffer_t *jack_ring_output_buffer;
uint8_t jack_midi_data[3*1024];

int jack_process(jack_nframes_t nframes, void *arg);
int jack_write_midi_event(uint8_t *event, int event_size);

int init_zyncoder_midi(char *name) {
	if ((jack_client = jack_client_open(name, JackNullOption , 0 , 0 )) == NULL) {
		fprintf (stderr, "Zyncoder: Error connecting with jack server.\n");
		return -1;
	}
	jack_midi_output_port = jack_port_register(jack_client, "output", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (jack_midi_output_port == NULL) {
		fprintf (stderr, "Zyncoder: Error creating jack midi output port.\n");
		return -2;
	}
	jack_midi_input_port = jack_port_register(jack_client, "input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (jack_midi_input_port == NULL) {
		fprintf (stderr, "Zyncoder: Error creating jack midi input port.\n");
		return -2;
	}
	jack_ring_output_buffer = jack_ringbuffer_create(3*1024);
	// lock the buffer into memory, this is *NOT* realtime safe, do it before using the buffer!
	if (jack_ringbuffer_mlock(jack_ring_output_buffer)) {
		fprintf (stderr, "Zyncoder: Error locking memory for jack ring output buffer.\n");
		return -3;
	}
	jack_set_process_callback(jack_client, jack_process, 0);
	if (jack_activate(jack_client)) {
		fprintf (stderr, "Zyncoder: Error activating jack client.\n");
		return -4;
	}
	return 0;
}

int end_zyncoder_midi() {
	return jack_client_close(jack_client);
}

int jack_write_midi_event(uint8_t *event_buffer, int event_size) {
	if (jack_ringbuffer_write_space(jack_ring_output_buffer)>=event_size) {
		if (jack_ringbuffer_write(jack_ring_output_buffer, event_buffer, event_size)!=event_size) {
			fprintf (stderr, "Zyncoder: Error writing jack ring output buffer: INCOMPLETE\n");
			return -1;
		}
	}
	else {
		fprintf (stderr, "Zyncoder: Error writing jack ring output buffer: FULL\n");
		return -1;
	}
	return 0;
}

int jack_process(jack_nframes_t nframes, void *arg) {
	int i=0;
	int j;
	uint8_t event_type;
	uint8_t event_chan;
	uint8_t event_num;
	uint8_t event_val;
	uint8_t event_size;
	uint8_t *buffer;

	//---------------------------------
	//MIDI Input
	//---------------------------------

	//Read jackd data buffer
	void *input_port_buffer = jack_port_get_buffer(jack_midi_input_port, nframes);
	if (input_port_buffer==NULL) {
		fprintf (stderr, "Zyncoder: Error allocating jack input port buffer: %d frames\n", nframes);
		return -1;
	}
	//Process MIDI messages
	jack_midi_event_t ev;
	while (jack_midi_event_get(&ev, input_port_buffer, i)==0) {
		//Ignore SysEx messages
		if (ev.buffer[0]==SYSTEM_EXCLUSIVE) continue;

		event_type=ev.buffer[0] >> 4;
		event_chan=ev.buffer[0] & 0xF;
		ev.buffer[1]&=0x7F;
		ev.buffer[2]&=0x7F;

		if (ev.size==3) {
			if (event_type==PITCH_BENDING) {
				event_num=0;
				event_val=ev.buffer[2];
			} else {
				event_num=ev.buffer[1];
				event_val=ev.buffer[2];
			}
		} else {
			event_num=0;
			event_val=ev.buffer[1];
		}

		//fprintf(stdout, "MIDI MSG => %x, %x\n", ev.buffer[0], ev.buffer[1]);

		//Capture events for GUI: before filtering => [Control-Change]
		if (event_type==CTRL_CHANGE) {
			write_zynmidi((ev.buffer[0]<<16)|(ev.buffer[1]<<8)|(ev.buffer[2]));
		}

		//Event Mapping
		struct midi_event_st *event_map=&midi_filter.event_map[event_type & 0x7][event_chan][event_num];
		//Ignore event...
		if (event_map->type==IGNORE_EVENT)
			continue;
		//Map event ...
		if (event_map->type>=0 || event_map->type==SWAP_EVENT) {
			//fprintf (stdout, "Zyncoder: Event Map %d, %d => ",ev.buffer[0],ev.buffer[1]);
			if (event_map->type!=SWAP_EVENT) event_type=event_map->type;
			event_chan=event_map->chan;
			ev.buffer[0]=(event_type << 4) | event_chan;
			if (event_map->type==PROG_CHANGE || event_map->type==CHAN_PRESS) {
				event_num=0;
				ev.buffer[1]=event_val;
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

		//MIDI CC messages
		if (event_type==CTRL_CHANGE) {
			//Update Zyncoder value => TODO Optimize this fragment!!!
			for (j=0;j<MAX_NUM_ZYNCODERS;j++) {
				if (zyncoders[j].enabled && zyncoders[j].midi_chan==event_chan && zyncoders[j].midi_ctrl==event_num) {
					zyncoders[j].value=event_val;
					zyncoders[j].subvalue=event_val*ZYNCODER_TICKS_PER_RETENT;
				}
			}
		}
		//Note-on/off messages
		else if (event_type==NOTE_OFF || event_type==NOTE_ON) {
			//Transpose
			if (midi_filter.transpose[event_chan]!=0) {
				int note=ev.buffer[1]+midi_filter.transpose[event_chan];
				//If transposed note is out of range, ignore message ...
				if (note>0x7F || note<0) {
					i++;
					continue;
				}
				ev.buffer[1]=(uint8_t)(note & 0x7F);
			}
		}

		// Fine-Tuning, using pitch-bending messages ...
		if (midi_filter.tuning_pitchbend>=0) {
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

		//Capture events for GUI: after filtering => [Note-Off, Note-On, Program-Change]
		if (event_type==NOTE_OFF || event_type==NOTE_ON || event_type==PROG_CHANGE) {
			write_zynmidi((ev.buffer[0]<<16)|(ev.buffer[1]<<8)|(ev.buffer[2]));
		}

		//Forward message
		jack_write_midi_event(ev.buffer,ev.size);

		if (i>nframes) {
			fprintf (stderr, "Zyncoder: Error processing jack midi input events: TOO MANY EVENTS\n");
			return -1;
		}
		i++;
	}

	//---------------------------------
	//MIDI Output
	//---------------------------------

	//Get internal MIDI data buffer
	void *output_port_buffer = jack_port_get_buffer(jack_midi_output_port, nframes);
	if (output_port_buffer==NULL) {
		fprintf (stderr, "Zyncoder: Error allocating jack output port buffer: %d frames\n", nframes);
		return -1;
	}
	jack_midi_clear_buffer(output_port_buffer);
	int nb=jack_ringbuffer_read_space(jack_ring_output_buffer);
	if (jack_ringbuffer_read(jack_ring_output_buffer, jack_midi_data, nb)!=nb) {
		fprintf (stderr, "Zyncoder: Error reading midi data from jack ring output buffer: %d bytes\n", nb);
		return -1;
	}

	//Write MIDI data
	int pos=0;
	i=0;
	while (pos < nb) {
		event_type= jack_midi_data[pos] >> 4;

		if (jack_midi_data[pos]>=0xF4) event_size=1;
		else if (event_type==PROG_CHANGE || event_type==CHAN_PRESS || event_type==TIME_CODE_QF || event_type==SONG_SELECT) event_size=2;
		else event_size=3;

		/*
		//Master Channel Control
		if (event_type==CTRL_CHANGE) {
			event_chan=jack_midi_data[pos] & 0xF;
			event_num=jack_midi_data[pos+1] & 0x7F;
			event_val=jack_midi_data[pos+2] & 0x7F;

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
						jack_midi_data[pos+2]=((int32_t)event_val*(uint32_t)midi_filter.last_ctrl_val[midi_filter.master_chan][event_num])>>7;
					}
				}
			}
		}
		*/

		//Write to Jackd buffer
		buffer = jack_midi_event_reserve(output_port_buffer, i, event_size);
		memcpy(buffer, jack_midi_data+pos, event_size);
		pos+=event_size;

		if (i>nframes) {
			fprintf (stderr, "Zyncoder: Error processing jack midi output events: TOO MANY EVENTS\n");
			return -1;
		}
		i++;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// MIDI Send Functions
//-----------------------------------------------------------------------------

int zynmidi_send_note_off(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x80 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return jack_write_midi_event(buffer,3);
}

int zynmidi_send_note_on(uint8_t chan, uint8_t note, uint8_t vel) {
	uint8_t buffer[3];
	buffer[0] = 0x90 + (chan & 0x0F);
	buffer[1] = note;
	buffer[2] = vel;
	return jack_write_midi_event(buffer,3);
}

int zynmidi_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val) {
	uint8_t buffer[3];
	buffer[0] = 0xB0 + (chan & 0x0F);
	buffer[1] = ctrl;
	buffer[2] = val;
	return jack_write_midi_event(buffer,3);
}

int zynmidi_send_program_change(uint8_t chan, uint8_t prgm) {
	uint8_t buffer[3];
	buffer[0] = 0xC0 + (chan & 0x0F);
	buffer[1] = prgm;
	buffer[2] = 0;
	return jack_write_midi_event(buffer,3);
}

int zynmidi_send_pitchbend_change(uint8_t chan, uint16_t pb) {
	uint8_t buffer[3];
	buffer[0] = 0xE0 + (chan & 0x0F);
	buffer[1] = pb & 0x7F;
	buffer[2] = (pb >> 7) & 0x7F;
	return jack_write_midi_event(buffer,3);
}

int zynmidi_send_master_ccontrol_change(uint8_t ctrl, uint8_t val) {
	if (midi_filter.master_chan>=0) {
		return zynmidi_send_ccontrol_change(midi_filter.master_chan, ctrl, val);
	}
}


//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

#ifdef MCP23017_ENCODERS
// Update the mcp23017 based switches from ISR routine
void update_zynswitch(uint8_t i, uint8_t status) {
#else
//Update ISR switches (native GPIO)
void update_zynswitch(uint8_t i) {
#endif
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	struct zynswitch_st *zynswitch = zynswitches + i;
	if (zynswitch->enabled==0) return;

#ifndef MCP23017_ENCODERS
	uint8_t status=digitalRead(zynswitch->pin);
#endif
	if (status==zynswitch->status) return;
	zynswitch->status=status;

	struct timespec ts;
	unsigned long int tsus;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	//printf("SWITCH ISR %d => STATUS=%d (%lu)\n",i,zynswitch->status,tsus);
	if (zynswitch->status==1) {
		int dtus=tsus-zynswitch->tsus;
		//Ignore spurious ticks
		if (dtus<1000) return;
		//printf("Debounced Switch %d\n",i);
		if (zynswitch->tsus>0) zynswitch->dtus=dtus;
	} else zynswitch->tsus=tsus;
}

#ifndef MCP23017_ENCODERS
void update_zynswitch_0() { update_zynswitch(0); }
void update_zynswitch_1() { update_zynswitch(1); }
void update_zynswitch_2() { update_zynswitch(2); }
void update_zynswitch_3() { update_zynswitch(3); }
void update_zynswitch_4() { update_zynswitch(4); }
void update_zynswitch_5() { update_zynswitch(5); }
void update_zynswitch_6() { update_zynswitch(6); }
void update_zynswitch_7() { update_zynswitch(7); }
void (*update_zynswitch_funcs[8])={
	update_zynswitch_0,
	update_zynswitch_1,
	update_zynswitch_2,
	update_zynswitch_3,
	update_zynswitch_4,
	update_zynswitch_5,
	update_zynswitch_6,
	update_zynswitch_7
};
#endif

//Update NON-ISR switches (expanded GPIO)
void update_expanded_zynswitches() {
	struct timespec ts;
	unsigned long int tsus;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	int i;
	uint8_t status;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		struct zynswitch_st *zynswitch = zynswitches + i;
		if (!zynswitch->enabled || zynswitch->pin<100) continue;
		status=digitalRead(zynswitch->pin);
		//printf("POLLING SWITCH %d (%d) => %d\n",i,zynswitch->pin,status);
		if (status==zynswitch->status) continue;
		zynswitch->status=status;
		//printf("POLLING SWITCH %d => STATUS=%d (%lu)\n",i,zynswitch->status,tsus);
		if (zynswitch->status==1) {
			int dtus=tsus-zynswitch->tsus;
			//Ignore spurious ticks
			if (dtus<1000) return;
			//printf("Debounced Switch %d\n",i);
			if (zynswitch->tsus>0) zynswitch->dtus=dtus;
		} else zynswitch->tsus=tsus;
	}
}

void * poll_zynswitches(void *arg) {
	while (1) {
		update_expanded_zynswitches();
		usleep(poll_zynswitches_us);
	}
	return NULL;
}

pthread_t init_poll_zynswitches() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zynswitches, NULL);
	if (err != 0) {
		printf("Zyncoder: Can't create zynswitches poll thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("Zyncoder: Zynswitches poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------

struct zynswitch_st *setup_zynswitch(uint8_t i, uint8_t pin) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		printf("Zyncoder: Maximum number of zynswitches exceeded: %d\n", MAX_NUM_ZYNSWITCHES);
		return NULL;
	}
	
	struct zynswitch_st *zynswitch = zynswitches + i;
	zynswitch->enabled = 1;
	zynswitch->pin = pin;
	zynswitch->tsus = 0;
	zynswitch->dtus = 0;
	zynswitch->status = 0;

	if (pin>0) {
		pinMode(pin, INPUT);
		pullUpDnControl(pin, PUD_UP);
#ifndef MCP23017_ENCODERS
		if (pin<MCP23008_BASE_PIN) {
			wiringPiISR(pin,INT_EDGE_BOTH, update_zynswitch_funcs[i]);
			update_zynswitch(i);
		}
#else
		// this is a bit brute force, but update all the banks
		mcp23017_bank_ISR(0);
		mcp23017_bank_ISR(1);
#endif
	}

	return zynswitch;
}

unsigned int get_zynswitch_dtus(uint8_t i) {
	if (i >= MAX_NUM_ZYNSWITCHES) return 0;
	unsigned int dtus=zynswitches[i].dtus;
	zynswitches[i].dtus=0;
	return dtus;
}

unsigned int get_zynswitch(uint8_t i) {
	return get_zynswitch_dtus(i);
}

//-----------------------------------------------------------------------------
// Generic Rotary Encoders
//-----------------------------------------------------------------------------

void send_zyncoder(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS) return;
	struct zyncoder_st *zyncoder = zyncoders + i;
	if (zyncoder->enabled==0) return;
	if (zyncoder->midi_ctrl>0) {
		zynmidi_send_ccontrol_change(zyncoder->midi_chan,zyncoder->midi_ctrl,zyncoder->value);
		//printf("SEND MIDI CHAN %d, CTRL %d = %d\n",zyncoder->midi_chan,zyncoder->midi_ctrl,zyncoder->value);
	} else if (osc_lo_addr!=NULL && zyncoder->osc_path[0]) {
		if (zyncoder->step >= 8) {
			if (zyncoder->value>=64) {
				lo_send(osc_lo_addr,zyncoder->osc_path, "T");
				//printf("SEND OSC %s => T\n",zyncoder->osc_path);
			} else {
				lo_send(osc_lo_addr,zyncoder->osc_path, "F");
				//printf("SEND OSC %s => F\n",zyncoder->osc_path);
			}
		} else {
			lo_send(osc_lo_addr,zyncoder->osc_path, "i",zyncoder->value);
			//printf("SEND OSC %s => %d\n",zyncoder->osc_path,zyncoder->value);
		}
	}
}

#ifdef MCP23017_ENCODERS
void update_zyncoder(uint8_t i, uint8_t MSB, uint8_t LSB) {
#else
void update_zyncoder(uint8_t i) {
#endif
	if (i>=MAX_NUM_ZYNCODERS) return;
	struct zyncoder_st *zyncoder = zyncoders + i;
	if (zyncoder->enabled==0) return;

#ifndef MCP23017_ENCODERS
	uint8_t MSB = digitalRead(zyncoder->pin_a);
	uint8_t LSB = digitalRead(zyncoder->pin_b);
#endif
	uint8_t encoded = (MSB << 1) | LSB;
	uint8_t sum = (zyncoder->last_encoded << 2) | encoded;
	uint8_t up=(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011);
	uint8_t down=0;
	if (!up) down=(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000);
#ifdef DEBUG
	printf("zyncoder %2d - %08d\t%08d\t%d\t%d\n", i, int_to_int(encoded), int_to_int(sum), up, down);
#endif
	zyncoder->last_encoded=encoded;

	if (zyncoder->step==0) {
		//Get time interval from last tick
		struct timespec ts;
		unsigned long int tsus;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;
		unsigned int dtus=tsus-zyncoder->tsus;
		//printf("ZYNCODER ISR %d => SUBVALUE=%d (%u)\n",i,zyncoder->subvalue,dtus);
		//Ignore spurious ticks
		if (dtus<1000) return;
		//printf("ZYNCODER DEBOUNCED ISR %d => SUBVALUE=%d (%u)\n",i,zyncoder->subvalue,dtus);
		//Calculate average dtus for the last ZYNCODER_TICKS_PER_RETENT ticks
		int j;
		unsigned int dtus_avg=dtus;
		for (j=0;j<ZYNCODER_TICKS_PER_RETENT;j++) dtus_avg+=zyncoder->dtus[j];
		dtus_avg/=(ZYNCODER_TICKS_PER_RETENT+1);
		//Add last dtus to fifo array
		zyncoder->dtus[0]=zyncoder->dtus[1];
		zyncoder->dtus[1]=zyncoder->dtus[2];
		zyncoder->dtus[2]=zyncoder->dtus[3];
		zyncoder->dtus[3]=dtus;
		//Calculate step value
		unsigned int dsval=1;
		if (dtus_avg < 10000) dsval=ZYNCODER_TICKS_PER_RETENT;
		else if (dtus_avg < 30000) dsval=ZYNCODER_TICKS_PER_RETENT/2;

		int value=-1;
		if (up) {
			if (zyncoder->max_value-zyncoder->subvalue>=dsval) zyncoder->subvalue=(zyncoder->subvalue+dsval);
			else zyncoder->subvalue=zyncoder->max_value;
			value=zyncoder->subvalue/ZYNCODER_TICKS_PER_RETENT;
		}
		else if (down) {
			if (zyncoder->subvalue>=dsval) zyncoder->subvalue=(zyncoder->subvalue-dsval);
			else zyncoder->subvalue=0;
			value=(zyncoder->subvalue+ZYNCODER_TICKS_PER_RETENT-1)/ZYNCODER_TICKS_PER_RETENT;
		}

		zyncoder->tsus=tsus;
		if (value>=0 && zyncoder->value!=value) {
			//printf("DTUS=%d, %d (%d)\n",dtus_avg,value,dsval);
			zyncoder->value=value;
			send_zyncoder(i);
		}
	} 
	else {
		unsigned int last_value=zyncoder->value;
		if (zyncoder->value>zyncoder->max_value) zyncoder->value=zyncoder->max_value;
		if (zyncoder->max_value-zyncoder->value>=zyncoder->step && up) zyncoder->value+=zyncoder->step;
		else if (zyncoder->value>=zyncoder->step && down) zyncoder->value-=zyncoder->step;
		if (last_value!=zyncoder->value) send_zyncoder(i);
	}

}

#ifndef MCP23017_ENCODERS
void update_zyncoder_0() { update_zyncoder(0); }
void update_zyncoder_1() { update_zyncoder(1); }
void update_zyncoder_2() { update_zyncoder(2); }
void update_zyncoder_3() { update_zyncoder(3); }
void update_zyncoder_4() { update_zyncoder(4); }
void update_zyncoder_5() { update_zyncoder(5); }
void update_zyncoder_6() { update_zyncoder(6); }
void update_zyncoder_7() { update_zyncoder(7); }
void (*update_zyncoder_funcs[8])={
	update_zyncoder_0,
	update_zyncoder_1,
	update_zyncoder_2,
	update_zyncoder_3,
	update_zyncoder_4,
	update_zyncoder_5,
	update_zyncoder_6,
	update_zyncoder_7
};
#endif

//-----------------------------------------------------------------------------

struct zyncoder_st *setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b, uint8_t midi_chan, uint8_t midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step) {
	if (i > MAX_NUM_ZYNCODERS) {
		printf("Zyncoder: Maximum number of zyncoders exceded: %d\n", MAX_NUM_ZYNCODERS);
		return NULL;
	}

	struct zyncoder_st *zyncoder = zyncoders + i;
	if (midi_chan>15) midi_chan=0;
	if (midi_ctrl>127) midi_ctrl=1;
	if (value>max_value) value=max_value;
	zyncoder->midi_chan = midi_chan;
	zyncoder->midi_ctrl = midi_ctrl;
	//printf("OSC PATH: %s\n",osc_path);
	if (osc_path) strcpy(zyncoder->osc_path,osc_path);
	else zyncoder->osc_path[0]=0;
	zyncoder->step = step;
	if (step>0) {
		zyncoder->value = value;
		zyncoder->subvalue = 0;
		zyncoder->max_value = max_value;
	} else {
		zyncoder->value = value;
		zyncoder->subvalue = ZYNCODER_TICKS_PER_RETENT*value;
		zyncoder->max_value = ZYNCODER_TICKS_PER_RETENT*max_value;
	}

	if (zyncoder->enabled==0 || zyncoder->pin_a!=pin_a || zyncoder->pin_b!=pin_b) {
		zyncoder->enabled = 1;
		zyncoder->pin_a = pin_a;
		zyncoder->pin_b = pin_b;
		zyncoder->last_encoded = 0;
		zyncoder->tsus = 0;

		if (zyncoder->pin_a!=zyncoder->pin_b) {
			pinMode(pin_a, INPUT);
			pinMode(pin_b, INPUT);
			pullUpDnControl(pin_a, PUD_UP);
			pullUpDnControl(pin_b, PUD_UP);
#ifndef MCP23017_ENCODERS
			wiringPiISR(pin_a,INT_EDGE_BOTH, update_zyncoder_funcs[i]);
			wiringPiISR(pin_b,INT_EDGE_BOTH, update_zyncoder_funcs[i]);
#else
			// this is a bit brute force, but update all the banks
			mcp23017_bank_ISR(0);
			mcp23017_bank_ISR(1);
#endif
		}
	}

	return zyncoder;
}

unsigned int get_value_zyncoder(uint8_t i) {
	if (i >= MAX_NUM_ZYNCODERS) return 0;
	return zyncoders[i].value;
}

void set_value_zyncoder(uint8_t i, unsigned int v, int send) {
	if (i >= MAX_NUM_ZYNCODERS) return;
	struct zyncoder_st *zyncoder = zyncoders + i;
	if (zyncoder->enabled==0) return;

	//unsigned int last_value=zyncoder->value;
	if (zyncoder->step==0) {
		v*=ZYNCODER_TICKS_PER_RETENT;
		if (v>zyncoder->max_value) zyncoder->subvalue=zyncoder->max_value;
		else zyncoder->subvalue=v;
		zyncoder->value=zyncoder->subvalue/ZYNCODER_TICKS_PER_RETENT;
	} else {
		if (v>zyncoder->max_value) zyncoder->value=zyncoder->max_value;
		else zyncoder->value=v;
	}
	if (send) send_zyncoder(i);
}

#ifdef MCP23017_ENCODERS
//-----------------------------------------------------------------------------
// MCP23017 based encoders & switches
//-----------------------------------------------------------------------------

// ISR for handling the mcp23017 interrupts
void mcp23017_bank_ISR(uint8_t bank) {
	// the interrupt has gone off for a pin change on the mcp23017
	// read the appropriate bank and compare pin states to last
	// on a change, call the update function as appropriate
	int i;
	uint8_t reg;
	uint8_t pin_min, pin_max;

	if (bank == 0) {
		reg = wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_GPIOA);
		pin_min = MCP23017_BASE_PIN;
	} else {
		reg = wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_GPIOB);
		pin_min = MCP23017_BASE_PIN + 8;
	}
	pin_max = pin_min + 7;

	// search all encoders and switches for a pin in the bank's range
	// if the last state != current state then this pin has changed
	// call the update function
	for (i=0; i<MAX_NUM_ZYNCODERS; i++) {
		struct zyncoder_st *zyncoder = zyncoders + i;
		if (zyncoder->enabled==0) continue;

		// if either pin is in the range
		if ((zyncoder->pin_a >= pin_min && zyncoder->pin_a <= pin_max) ||
		    (zyncoder->pin_b >= pin_min && zyncoder->pin_b <= pin_max)) {
			uint8_t bit_a = zyncoder->pin_a - pin_min;
			uint8_t bit_b = zyncoder->pin_b - pin_min;
			uint8_t state_a = bitRead(reg, bit_a);
			uint8_t state_b = bitRead(reg, bit_b);
			// if either bit is different
			if ((state_a != zyncoder->pin_a_last_state) ||
			    (state_b != zyncoder->pin_b_last_state)) {
				// call the update function
				update_zyncoder(i, state_a, state_b);
				// update the last state
				zyncoder->pin_a_last_state = state_a;
				zyncoder->pin_b_last_state = state_b;
			}
		}
	}
	for (i = 0; i < MAX_NUM_ZYNSWITCHES; ++i) {
		struct zynswitch_st *zynswitch = zynswitches + i;
		if (zynswitch->enabled == 0) continue;

		// check the pin range
		if (zynswitch->pin >= pin_min && zynswitch->pin <= pin_max) {
			uint8_t bit = zynswitch->pin - pin_min;
			uint8_t state = bitRead(reg, bit);
			if (state != zynswitch->status) {
				update_zynswitch(i, state);
				// note that the update function updates status with state
			}
		}
	}
}

#endif

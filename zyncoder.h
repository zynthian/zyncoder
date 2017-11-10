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

//#undef MCP23017_ENCODERS
//#define MCP23017_ENCODERS

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_zyncoder(int osc_port);
int end_zyncoder();

//-----------------------------------------------------------------------------
// MIDI filter
//-----------------------------------------------------------------------------

enum midi_event_type_enum {
	IGNORE_EVENT=-2,
	THRU_EVENT=-1,
	NOTE_OFF=0x8,
	NOTE_ON=0x9,
	KEY_PRESS=0xA,
	CTRL_CHANGE=0xB,
	PROG_CHANGE=0xC,
	CHAN_PRESS=0xD,
	PITCH_BENDING=0xE
};

struct midi_event_st {
	enum midi_event_type_enum type;
	uint8_t chan;
	uint8_t num;
};

struct midi_filter_st {
	int tuning_pitchbend;
	int transpose[16];
	struct midi_event_st event_map[8][16][128];

	int master_chan;
	uint8_t last_ctrl_val[16][128];
	uint16_t last_pb_val[16];
};
struct midi_filter_st midi_filter;

void init_midi_filter();

void set_midi_master_chan(int chan);

void set_midi_filter_tuning_freq(int freq);
int get_midi_filter_tuning_pitchbend();

void set_midi_filter_transpose(uint8_t chan, int offset);
int get_midi_filter_transpose(uint8_t chan);

void set_midi_filter_event_map_st(struct midi_event_st *ev_from, struct midi_event_st *ev_to);
void set_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from,
															 enum midi_event_type_enum type_to, uint8_t chan_to, uint8_t num_to);
void set_midi_filter_event_ignore_st(struct midi_event_st *ev_from);
void set_midi_filter_event_ignore(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from);
struct midi_event_st *get_midi_filter_event_map_st(struct midi_event_st *ev_from);
struct midi_event_st *get_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from);
void del_midi_filter_event_map_st(struct midi_event_st *ev_filter);
void del_midi_filter_event_map(enum midi_event_type_enum type_from, uint8_t chan_from, uint8_t num_from);
void reset_midi_filter_event_map();

void set_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from, uint8_t chan_to, uint8_t cc_to);
void set_midi_filter_cc_ignore(uint8_t chan, uint8_t cc_from);
uint8_t get_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void del_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void reset_midi_filter_cc_map();

//-----------------------------------------------------------------------------
// MIDI Input Events Buffer Management
//-----------------------------------------------------------------------------

#define ZYNMIDI_BUFFER_SIZE 32
uint32_t zynmidi_buffer[ZYNMIDI_BUFFER_SIZE];
int zynmidi_buffer_read;
int zynmidi_buffer_write;

int write_zynmidi(uint32_t ev);
uint32_t read_zynmidi();

//-----------------------------------------------------------------------------
// MIDI Send Functions
//-----------------------------------------------------------------------------

int zynmidi_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int zynmidi_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int zynmidi_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int zynmidi_send_program_change(uint8_t chan, uint8_t prgm);
int zynmidi_send_pitchbend_change(uint8_t chan, uint16_t pb);

int zynmidi_send_master_ccontrol_change(uint8_t ctrl, uint8_t val);

//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

// The real limit in RPi2 is 17
#define MAX_NUM_ZYNSWITCHES 8

struct zynswitch_st {
	uint8_t enabled;
	uint8_t pin;
	volatile unsigned long tsus;
	volatile unsigned int dtus;
	// note that this status is like the pin_[ab]_last_state for the 
	// zyncoders
	volatile uint8_t status;
};
struct zynswitch_st zynswitches[MAX_NUM_ZYNSWITCHES];

struct zynswitch_st *setup_zynswitch(uint8_t i, uint8_t pin); 
unsigned int get_zynswitch(uint8_t i);
unsigned int get_zynswitch_dtus(uint8_t i);

//-----------------------------------------------------------------------------
// MIDI Rotary Encoders
//-----------------------------------------------------------------------------

// Number of ticks per retent in rotary encoders
#define ZYNCODER_TICKS_PER_RETENT 4

// 17 pins / 2 pins per encoder = 8 maximum encoders
#define MAX_NUM_ZYNCODERS 8

struct zyncoder_st {
	uint8_t enabled;
	uint8_t pin_a;
	uint8_t pin_b;
#ifdef MCP23017_ENCODERS
	volatile uint8_t pin_a_last_state;
	volatile uint8_t pin_b_last_state;
#endif
	uint8_t midi_chan;
	uint8_t midi_ctrl;
	char osc_path[512];
	unsigned int max_value;
	unsigned int step;
	volatile unsigned int subvalue;
	volatile unsigned int value;
	volatile unsigned int last_encoded;
	volatile unsigned long tsus;
	unsigned int dtus[ZYNCODER_TICKS_PER_RETENT];
};
struct zyncoder_st zyncoders[MAX_NUM_ZYNCODERS];

struct zyncoder_st *setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b, uint8_t midi_chan, uint8_t midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step); 
unsigned int get_value_zyncoder(uint8_t i);
void set_value_zyncoder(uint8_t i, unsigned int v);


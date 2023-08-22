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

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_zynmidirouter();
int end_zynmidirouter();

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

typedef enum midi_event_type_enum {
	//Router-internal pseudo-message codes
	CTRL_SWITCH_EVENT=-7,
	GATE_OUT_EVENT=-6,
	CVGATE_OUT_EVENT=-5,
	CVGATE_IN_EVENT=-4,
	SWAP_EVENT=-3,
	IGNORE_EVENT=-2,
	THRU_EVENT=-1,
	NONE_EVENT=0,
	//Channel 3-bytes-messages
	NOTE_OFF=0x8,
	NOTE_ON=0x9,
	KEY_PRESS=0xA,
	CTRL_CHANGE=0xB,
	PITCH_BEND=0xE,
	//Channel 2-bytes-messages
	PROG_CHANGE=0xC,
	CHAN_PRESS=0xD,
	//System 3-bytes-messages
	SONG_POSITION=0xF2,
	//System 2-bytes-messages
	TIME_CODE_QF=0xF1,
	SONG_SELECT=0xF3,
	//System 1-byte messages
	TUNE_REQUEST=0xF6,
	//System Real-Time
	TIME_CLOCK=0xF8,
	TRANSPORT_START=0xFA,
	TRANSPORT_CONTINUE=0xFB,
	TRANSPORT_STOP=0xFC,
	ACTIVE_SENSE=0xFE,
	MIDI_RESET=0xFF,
	//System Multi-byte (SysEx)
	SYSTEM_EXCLUSIVE=0xF0,
	END_SYSTEM_EXCLUSIVE=0xF7
} midi_event_type;

typedef enum ctrl_mode {
	CTRL_MODE_ABS		= 0, // Absolute immediate
	CTRL_MODE_ABS_JP	= 1, // Absolute jump prevention
	CTRL_MODE_REL_1		= 2, // Relative 2's complement
	CTRL_MODE_REL_2 	= 3, // Relative offset
	CTRL_MODE_REL_3		= 4, // Relative sign bit
} midi_ctrl_mode;

typedef struct midi_event_st {
	midi_event_type type;
	uint8_t chan;
	uint8_t num;
	uint8_t val;
} midi_event_t;

typedef struct mf_clone_st {
	int enabled;
	uint8_t cc[128];
} mf_clone_t;

static uint8_t default_cc_to_clone[]={ 1, 2, 64, 65, 66, 67, 68 };

typedef struct mf_noterange_st {
	uint8_t note_low;
	uint8_t note_high;
	int8_t octave_trans;
	int8_t halftone_trans;
} mf_noterange_t;

typedef struct midi_filter_st {
	int tuning_pitchbend;
	int master_chan;
	int active_chan;
	int last_active_chan;
	int system_events;
	int cc_automode;		// This should be enabled/disabled by device (zmip)

	mf_noterange_t noterange[16];
	mf_clone_t clone[16][16];

	midi_event_t event_map[8][16][128];
	//midi_event_t cc_swap[16][128];

	uint8_t ctrl_mode[16][128];
	uint8_t ctrl_relmode_count[16][128];

	uint8_t last_ctrl_val[16][128];
	uint16_t last_pb_val[16];

	uint8_t note_state[16][128];
} midi_filter_t;

//-----------------------------------------------------------------------------
// MIDI Filter Functions
//-----------------------------------------------------------------------------

//MIDI filter initialization
int init_midi_router();
int end_midi_router();

//MIDI special featured channels
void set_midi_master_chan(int chan);
int get_midi_master_chan();
void set_midi_active_chan(int chan);
int get_midi_active_chan();

//MIDI filter fine tuning => Pitch-Bending based
void set_midi_filter_tuning_freq(double freq);
int get_midi_filter_tuning_pitchbend();

//MIDI filter clone
void set_midi_filter_clone(uint8_t chan_from, uint8_t chan_to, int v);
int get_midi_filter_clone(uint8_t chan_from, uint8_t chan_to);
void reset_midi_filter_clone(uint8_t chan_from);
void set_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to, uint8_t cc[128]);
uint8_t *get_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to);
void reset_midi_filter_clone_cc(uint8_t chan_from, uint8_t chan_to);

//MIDI Note Range & Transpose
void set_midi_filter_note_range(uint8_t chan, uint8_t nlow, uint8_t nhigh, int8_t oct_trans, int8_t ht_trans);
void set_midi_filter_note_low(uint8_t chan, uint8_t nlow);
void set_midi_filter_note_high(uint8_t chan, uint8_t nhigh);
void set_midi_filter_octave_trans(uint8_t chan, int8_t oct_trans);
void set_midi_filter_halftone_trans(uint8_t chan, int8_t ht_trans);
uint8_t get_midi_filter_note_low(uint8_t chan);
uint8_t get_midi_filter_note_high(uint8_t chan);
int8_t get_midi_filter_octave_trans(uint8_t chan);
int8_t get_midi_filter_halftone_trans(uint8_t chan);
void reset_midi_filter_note_range(uint8_t chan);

//MIDI Filter Core functions
void set_midi_filter_event_map_st(midi_event_t *ev_from, midi_event_t *ev_to);
void set_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from, midi_event_type type_to, uint8_t chan_to, uint8_t num_to);
void set_midi_filter_event_ignore_st(midi_event_t *ev_from);
void set_midi_filter_event_ignore(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
midi_event_t *get_midi_filter_event_map_st(midi_event_t *ev_from);
midi_event_t *get_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
void del_midi_filter_event_map_st(midi_event_t *ev_filter);
void del_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
void reset_midi_filter_event_map();

//MIDI Filter Mapping
void set_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from, uint8_t chan_to, uint8_t cc_to);
void set_midi_filter_cc_ignore(uint8_t chan, uint8_t cc_from);
uint8_t get_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void del_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void reset_midi_filter_cc_map();

// MIDI Controller Auto-Mode (Absolut <=> Relative)
void set_midi_filter_cc_automode(int mfccam);

// MIDI System Events enable/disable
void set_midi_filter_system_events(int mfse);

// MIDI Thru enable/disable => Route MIDI input to output
int set_midi_thru(int flag);
int get_midi_thru();

//MIDI Learning Mode
void set_midi_learning_mode(int mlm);
int get_midi_learning_mode();

//-----------------------------------------------------------------------------
// Zynmidi Ports
//-----------------------------------------------------------------------------

#define JACK_MIDI_BUFFER_SIZE 4096

#define ZMOP_CH0 0
#define ZMOP_CH1 1
#define ZMOP_CH2 2
#define ZMOP_CH3 3
#define ZMOP_CH4 4
#define ZMOP_CH5 5
#define ZMOP_CH6 6
#define ZMOP_CH7 7
#define ZMOP_CH8 8
#define ZMOP_CH9 9
#define ZMOP_CH10 10
#define ZMOP_CH11 11
#define ZMOP_CH12 12
#define ZMOP_CH13 13
#define ZMOP_CH14 14
#define ZMOP_CH15 15
#define ZMOP_MAIN 16
#define ZMOP_MOD 17
#define ZMOP_MIDI 18
#define ZMOP_NET 19
#define ZMOP_STEP 20
#define ZMOP_CTRL 21
#define MAX_NUM_ZMOPS 22
#define NUM_ZMOP_CHAINS 16

#define ZMIP_DEV0 0
#define ZMIP_DEV1 1
#define ZMIP_DEV2 2
#define ZMIP_DEV3 3
#define ZMIP_DEV4 4
#define ZMIP_DEV5 5
#define ZMIP_DEV6 6
#define ZMIP_DEV7 7
#define ZMIP_DEV8 8
#define ZMIP_DEV9 9
#define ZMIP_DEV10 10
#define ZMIP_DEV11 11
#define ZMIP_DEV12 12
#define ZMIP_DEV13 13
#define ZMIP_DEV14 14
#define ZMIP_DEV15 15
#define ZMIP_NET 16
#define ZMIP_SEQ 17
#define ZMIP_STEP 18
#define ZMIP_CTRL 19
#define ZMIP_FAKE_INT 20
#define ZMIP_FAKE_UI 21
#define ZMIP_FAKE_CTRL_FB 22
#define MAX_NUM_ZMIPS 23
#define NUM_ZMIP_DEVS 16

#define FLAG_ZMOP_DROPPC 1
#define FLAG_ZMOP_DROPCC 2
#define FLAG_ZMOP_DROPSYS 4
#define FLAG_ZMOP_DROPNOTE 8
#define FLAG_ZMOP_TUNING 16
#define FLAG_ZMOP_NOTERANGE 32

#define ZMOP_MAIN_FLAGS (FLAG_ZMOP_TUNING|FLAG_ZMOP_NOTERANGE|FLAG_ZMOP_DROPCC|FLAG_ZMOP_DROPSYS)

#define FLAG_ZMIP_UI 1
#define FLAG_ZMIP_CLONE 2
#define FLAG_ZMIP_FILTER 4
#define FLAG_ZMIP_ACTIVE_CHAN 8
#define FLAG_ZMIP_CC_AUTO_MODE 16

#define ZMIP_MAIN_FLAGS (FLAG_ZMIP_UI|FLAG_ZMIP_CLONE|FLAG_ZMIP_FILTER|FLAG_ZMIP_ACTIVE_CHAN|FLAG_ZMIP_CC_AUTO_MODE)
#define ZMIP_SEQ_FLAGS (FLAG_ZMIP_UI)
#define ZMIP_STEP_FLAGS (FLAG_ZMIP_UI|FLAG_ZMIP_CLONE|FLAG_ZMIP_FILTER)
#define ZMIP_CTRL_FLAGS (FLAG_ZMIP_UI)


// Structure describing a MIDI output
struct zmop_st {
	jack_port_t *jport;    // jack midi port
	void * buffer;         // pointer to jack midi output buffer
	int midi_chans[16];    // MIDI channel translation map (-1 to disable a MIDI channel)
	int route_from_zmips[MAX_NUM_ZMIPS]; // Flags indicating which inputs to route to this output
	uint32_t flags;        // Bitwise flags influencing output behaviour
	int n_connections;     // Quantity of jack connections (used for optimisation)
};

int zmop_init(int iz, char *name, int midi_chan, uint32_t flags);
int zmop_set_flags(int iz, uint32_t flags);
int zmop_has_flags(int iz, uint32_t flag);
int zmop_chain_set_flag_droppc(int iz, uint8_t flag);
int zmop_chain_get_flag_droppc(int ch);
int zmop_chain_set_flag_dropcc(int iz, uint8_t flag);
int zmop_chain_get_flag_dropcc(int ch);
int zmop_reset_midi_chans(int iz);
int zmop_set_midi_chan(int iz, int midi_chan_from, int midi_chan_to);
int zmop_get_midi_chan(int iz, int midi_chan);
int zmop_reset_route_from(int iz);
int zmop_set_route_from(int izmop, int izmip, int route);
int zmop_get_route_from(int izmop, int izmip);
int zmop_reset_event_counters(int iz);
jack_midi_event_t *zmop_pop_event(int izmop, int *izmip);
void zmop_push_event(struct zmop_st * zmop, jack_midi_event_t * ev); // Add event to MIDI output

// Structure describing a MIDI input
struct zmip_st {
	jack_port_t *jport; // jack midi port
	void * buffer;      // Pointer to the jack midi buffer 
	uint32_t flags;     // Bitwise flags influencing input behaviour
	uint32_t event_count; // Quantity of events in input event queue (not fake queues)
	uint32_t next_event; // Index of the next event to be processed (not fake queues)
	jack_midi_event_t event; // Event currently being processed
};

int zmip_init(int iz, char *name, uint32_t flags);
int zmip_set_flags(int iz, uint32_t flags);
int zmip_has_flags(int iz, uint32_t flag);
int zmip_set_route_extdev(int iz, int route);

//-----------------------------------------------------------------------------
// Jack MIDI Process
//-----------------------------------------------------------------------------

int init_jack_midi(char *name);
int end_jack_midi();
int jack_process(jack_nframes_t nframes, void *arg);
void jack_connect_cb(jack_port_id_t a, jack_port_id_t b, int connect, void *arg);

//-----------------------------------------------------------------------------
// MIDI Input Events Buffer Management and Send functions
//-----------------------------------------------------------------------------

#define ZYNMIDI_BUFFER_SIZE 4096

//-----------------------------------------------------
// MIDI Internal Input <= internal (zyncoder)
//-----------------------------------------------------

int write_internal_midi_event(uint8_t *event);

int internal_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int internal_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int internal_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int internal_send_program_change(uint8_t chan, uint8_t prgm);
int internal_send_chan_press(uint8_t chan, uint8_t val);
int internal_send_pitchbend_change(uint8_t chan, uint16_t pb);

//-----------------------------------------------------
// MIDI UI Input <= UI
//-----------------------------------------------------

int write_ui_event(uint8_t *event, int event_size);

int ui_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int ui_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int ui_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int ui_send_program_change(uint8_t chan, uint8_t prgm);
int ui_send_chan_press(uint8_t chan, uint8_t val);
int ui_send_pitchbend_change(uint8_t chan, uint16_t pb);
int ui_send_master_ccontrol_change(uint8_t ctrl, uint8_t val);
int ui_send_all_notes_off();
int ui_send_all_notes_off_chan(uint8_t chan);

//-----------------------------------------------------
// MIDI Controller Feedback <= UI & internal (zyncoder)
//-----------------------------------------------------

int write_ctrlfb_event(uint8_t *event, int event_size);

int ctrlfb_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int ctrlfb_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int ctrlfb_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int ctrlfb_send_program_change(uint8_t chan, uint8_t prgm);
int ctrlfb_send_chan_press(uint8_t chan, uint8_t val);
int ctrlfb_send_pitchbend_change(uint8_t chan, uint16_t pb);

//-----------------------------------------------------------------------------
// MIDI Internal Ouput Events Buffer => UI
//-----------------------------------------------------------------------------

int init_zynmidi_buffer();
int end_zynmidi_buffer();
int write_zynmidi(uint32_t ev);
uint32_t read_zynmidi();

int write_zynmidi_ccontrol_change(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_note_on(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_note_off(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_program_change(uint8_t chan, uint8_t num);

//-----------------------------------------------------------------------------

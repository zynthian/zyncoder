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

// MIDI filter initialization
int init_midi_router();
int end_midi_router();

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

typedef enum midi_event_type_enum {
    // Router-internal pseudo-message codes
    CTRL_SWITCH_EVENT    = -7,
    GATE_OUT_EVENT       = -6,
    CVGATE_OUT_EVENT     = -5,
    CVGATE_IN_EVENT      = -4,
    SWAP_EVENT           = -3,
    IGNORE_EVENT         = -2,
    THRU_EVENT           = -1,
    NONE_EVENT           = 0,
    // Channel 3-bytes-messages
    NOTE_OFF             = 0x8,
    NOTE_ON              = 0x9,
    KEY_PRESS            = 0xA,
    CTRL_CHANGE          = 0xB,
    PITCH_BEND           = 0xE,
    // Channel 2-bytes-messages
    PROG_CHANGE          = 0xC,
    CHAN_PRESS           = 0xD,
    // System 3-bytes-messages
    SONG_POSITION        = 0xF2,
    // System 2-bytes-messages
    TIME_CODE_QF         = 0xF1,
    SONG_SELECT          = 0xF3,
    // System 1-byte messages
    TUNE_REQUEST         = 0xF6,
    // System Real-Time
    TIME_CLOCK           = 0xF8,
    TRANSPORT_START      = 0xFA,
    TRANSPORT_CONTINUE   = 0xFB,
    TRANSPORT_STOP       = 0xFC,
    ACTIVE_SENSE         = 0xFE,
    MIDI_RESET           = 0xFF,
    // System Multi-byte (SysEx)
    SYSTEM_EXCLUSIVE     = 0xF0,
    END_SYSTEM_EXCLUSIVE = 0xF7
} midi_event_type;

typedef enum ctrl_mode {
    CTRL_MODE_ABS    = 0, // Absolute immediate
    CTRL_MODE_ABS_JP = 1, // Absolute jump prevention
    CTRL_MODE_REL_1  = 2, // Relative 2's complement
    CTRL_MODE_REL_2  = 3, // Relative offset
    CTRL_MODE_REL_3  = 4, // Relative sign bit
} midi_ctrl_mode;

typedef struct midi_event_st {
    midi_event_type type;
    uint8_t chan;
    uint8_t num;
    uint8_t val;
} midi_event_t;

//-----------------------------------------------------------------------------
// Global Settings management
//-----------------------------------------------------------------------------

// Active chain (zmop) management
void set_active_chain(int iz);
int get_active_chain();

// Active MIDI chan flag
void set_active_midi_chan(int flag);
int get_active_midi_chan();

// Global tuning => Based on MIDI Pitch-Bending messages
void set_tuning_freq(double freq);
int get_tuning_pitchbend();

// Master chan management
void set_midi_master_chan(int chan);
int get_midi_master_chan();

// Global System Events flag
void set_midi_system_events(int flag);
int get_midi_system_events();

// MIDI Learning Mode
void set_midi_learning_mode(int mlm);
int get_midi_learning_mode();

//-----------------------------------------------------------------------------
// MIDI Filter
//-----------------------------------------------------------------------------

typedef struct midi_filter_st {
    midi_event_t event_map[8][16][128];
} midi_filter_t;

// MIDI Filter Core functions
void set_midi_filter_event_map_st(midi_event_t* ev_from, midi_event_t* ev_to);
void set_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from, midi_event_type type_to, uint8_t chan_to, uint8_t num_to);
void set_midi_filter_event_ignore_st(midi_event_t* ev_from);
void set_midi_filter_event_ignore(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
midi_event_t* get_midi_filter_event_map_st(midi_event_t* ev_from);
midi_event_t* get_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
void del_midi_filter_event_map_st(midi_event_t* ev_filter);
void del_midi_filter_event_map(midi_event_type type_from, uint8_t chan_from, uint8_t num_from);
void reset_midi_filter_event_map();

// MIDI Filter Mapping
void set_midi_filter_cc_map(uint8_t chan_from, uint8_t cc_from, uint8_t chan_to, uint8_t cc_to);
void set_midi_filter_cc_ignore(uint8_t chan, uint8_t cc_from);
uint8_t get_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void del_midi_filter_cc_map(uint8_t chan, uint8_t cc_from);
void reset_midi_filter_cc_map();

//-----------------------------------------------------------------------------
// MIDI Input Ports (ZMIPs)
//-----------------------------------------------------------------------------

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
#define ZMIP_DEV16 16
#define ZMIP_DEV17 17
#define ZMIP_DEV18 18
#define ZMIP_DEV19 19
#define ZMIP_DEV20 20
#define ZMIP_DEV21 21
#define ZMIP_DEV22 22
#define ZMIP_DEV23 23
#define ZMIP_SEQ 24      // MIDI from SMF player
#define ZMIP_STEP 25     // MIDI from StepSeq
#define ZMIP_FAKE_INT 26 // BUFFER: Internal MIDI (to ALL zmops => MUST BE CHANGED!!) => Used by zyncoder, zynaptik (CV/Gate), zyntof, etc.
#define ZMIP_CTRL 27     // Engine's controller feedback (setBfree, others?) => It's hardcoded in chain_manager. Update if this number changes!!
#define ZMIP_FAKE_UI 28  // BUFFER: MIDI from UI (to Chain zmops)
#define MAX_NUM_ZMIPS 29
#define NUM_ZMIP_DEVS 24

#define FLAG_ZMIP_UI 1
#define FLAG_ZMIP_FILTER 1
#define FLAG_ZMIP_CC_AUTO_MODE 4
#define FLAG_ZMIP_ACTIVE_CHAIN 8
#define FLAG_ZMIP_DIRECTIN 16

#define ZMIP_DEV_FLAGS (FLAG_ZMIP_UI | FLAG_ZMIP_FILTER | FLAG_ZMIP_ACTIVE_CHAIN)
#define ZMIP_SEQ_FLAGS (FLAG_ZMIP_UI)
#define ZMIP_STEP_FLAGS (FLAG_ZMIP_UI | FLAG_ZMIP_FILTER)
#define ZMIP_CTRL_FLAGS (FLAG_ZMIP_UI)
#define ZMIP_INT_FLAGS (FLAG_ZMIP_UI | FLAG_ZMIP_FILTER | FLAG_ZMIP_DIRECTIN)
#define ZMIP_UI_FLAGS (FLAG_ZMIP_DIRECTIN)

// Structure describing a MIDI input
struct zmip_st {
    jack_port_t* jport;         // jack midi port
    void* buffer;               // Pointer to the jack midi buffer
    jack_ringbuffer_t* rbuffer; // Direct input ring buffer => Used when DIRECTIN flag is set

    uint32_t flags; // Bitwise flags influencing input behaviour

    uint32_t event_count;    // Quantity of events in input event queue (not fake queues)
    uint32_t next_event;     // Index of the next event to be processed (not fake queues)
    jack_midi_event_t event; // Event currently being processed

    uint8_t ctrl_mode[16][128];          // Controller mode for all 128 CCs x 16 chans
    uint8_t ctrl_relmode_count[16][128]; // Counter array used for mode auto-detection
    uint8_t last_ctrl_val[16][128];      // Last CC value tracked for each CC x 16 chans
};

// MIDI Input port (ZMIPs) management
int zmip_init(int iz, char* name, uint32_t flags);
int zmip_end(int iz);
int zmip_get_num_devs();
int zmip_get_seq_index();
int zmip_get_step_index();
int zmip_get_int_index();
int zmip_get_ctrl_index();

// Flag management
int zmip_set_flags(int iz, uint32_t flags);
uint32_t zmip_get_flags(int iz);
int zmip_has_flags(int iz, uint32_t flag);
int zmop_set_flag_cc_auto_mode(int iz, uint8_t flag);
uint8_t zmop_get_flag_cc_auto_mode(int iz);
int zmip_set_flag_active_chain(int iz, uint8_t flag);
int zmip_get_flag_active_chain(int iz);
// Routing
int zmip_set_route_chains(int iz, int route); // Route/un-route a MIDI input port (zmip) to/from *ALL* zmop chains

//-----------------------------------------------------------------------------
// MIDI Output Ports (ZMOPs)
//-----------------------------------------------------------------------------

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
#define ZMOP_MOD 16  // MOD-UI
#define ZMOP_STEP 17 // StepSeq
#define ZMOP_CTRL 18 // Controller Feedback => To replace!
#define ZMOP_DEV0 19
#define ZMOP_DEV1 20
#define ZMOP_DEV2 21
#define ZMOP_DEV3 22
#define ZMOP_DEV4 23
#define ZMOP_DEV5 24
#define ZMOP_DEV6 25
#define ZMOP_DEV7 26
#define ZMOP_DEV8 27
#define ZMOP_DEV9 28
#define ZMOP_DEV10 29
#define ZMOP_DEV11 30
#define ZMOP_DEV12 31
#define ZMOP_DEV13 32
#define ZMOP_DEV14 33
#define ZMOP_DEV15 34
#define ZMOP_DEV16 35
#define ZMOP_DEV17 36
#define ZMOP_DEV18 37
#define ZMOP_DEV19 38
#define ZMOP_DEV20 39
#define ZMOP_DEV21 40
#define ZMOP_DEV22 41
#define ZMOP_DEV23 42
#define MAX_NUM_ZMOPS 43
#define NUM_ZMOP_CHAINS 17
#define NUM_ZMOP_DEVS 24

#define FLAG_ZMOP_DROPPC 1
#define FLAG_ZMOP_DROPCC 2
#define FLAG_ZMOP_DROPSYS 4
#define FLAG_ZMOP_DROPSYSEX 8
#define FLAG_ZMOP_DROPNOTE 16
#define FLAG_ZMOP_TUNING 32
#define FLAG_ZMOP_NOTERANGE 64
#define FLAG_ZMOP_CHAN_TRANSFILTER 128
#define FLAG_ZMOP_DIRECTOUT 256

// #define ZMOP_CHAIN_FLAGS (FLAG_ZMOP_TUNING|FLAG_ZMOP_NOTERANGE|FLAG_ZMOP_DROPSYS|FLAG_ZMOP_DROPSYSEX|FLAG_ZMOP_CHAN_TRANSFILTER|FLAG_ZMOP_DIRECTOUT)
#define ZMOP_CHAIN_FLAGS (FLAG_ZMOP_TUNING | FLAG_ZMOP_NOTERANGE | FLAG_ZMOP_DROPSYSEX | FLAG_ZMOP_CHAN_TRANSFILTER | FLAG_ZMOP_DIRECTOUT)

// Structure describing a MIDI output
struct zmop_st {
    jack_port_t* jport;         // jack midi port
    void* buffer;               // pointer to jack midi output buffer
    jack_ringbuffer_t* rbuffer; // direct output ring buffer (optional)

    int midi_chan;                       // Single MIDI channel. -1 for using channel translation map only.
    int midi_chans[16];                  // MIDI channel translation map (-1 to disable a MIDI channel)
    int route_from_zmips[MAX_NUM_ZMIPS]; // Flags indicating which inputs to route to this output
    uint8_t cc_route[128];               // CCs routed to output (0 = blocked, 1 = routed)
    uint32_t flags;                      // Bitwise flags influencing output behaviour
    uint8_t note_low;                    // Note range => Low note
    uint8_t note_high;                   // Note range => High note
    int8_t transpose_octave;             // Transpose coarse => octave
    int8_t transpose_semitone;           // Transpose fine => semitone

    uint8_t note_state[128];    // Note state array for managing pressed notes across active chain changes.
    int8_t note_transpose[128]; // Note transpose array for managing pressed notes across transpose changes.
    uint16_t last_pb_val[16];   // Last pitch-bending value. Do we need multi-channel tracking for MPE?

    int n_connections; // Quantity of jack connections (used for optimisation)
};

// MIDI output port (ZMOPs) management
int zmop_init(int iz, char* name, uint32_t flags);
int zmop_end(int iz);
int zmop_get_num_chains();
int zmop_get_num_devs();
// Flag management
int zmop_set_flags(int iz, uint32_t flags);
uint32_t zmop_get_flags(int iz);
int zmop_has_flags(int iz, uint32_t flag);
int zmop_set_flag_droppc(int iz, uint8_t flag);
int zmop_get_flag_droppc(int iz);
int zmop_set_flag_dropcc(int iz, uint8_t flag);
int zmop_get_flag_dropcc(int iz);
int zmop_set_flag_dropsys(int iz, uint8_t flag);
int zmop_get_flag_dropsys(int iz);
int zmop_set_flag_dropsysex(int iz, uint8_t flag);
int zmop_get_flag_dropsysex(int iz);
int zmop_set_flag_dropnote(int iz, uint8_t flag);
int zmop_get_flag_dropnote(int iz);
int zmop_set_flag_tuning(int iz, uint8_t flag);
int zmop_get_flag_tuning(int iz);
int zmop_set_flag_chan_transfilter(int iz, uint8_t flag);
int zmop_get_flag_chan_transfilter(int iz);
// MIDI channel management
int zmop_reset_midi_chans(int iz);
int zmop_set_midi_chan(int iz, int midi_chan);
int zmop_set_midi_chan_trans(int iz, int midi_chan, int midi_chan_trans);
int zmop_set_midi_chan_all(int iz);
int zmop_set_midi_chan_all_trans(int iz, int midi_chan);
int zmop_set_midi_chan_to(int iz, int midi_chan_from, int midi_chan_to);
int zmop_get_midi_chan_to(int iz, int midi_chan_from);
int zmop_get_midi_chan_info(int iz, int* buffer);
// Routing zmip => zmop
int zmop_reset_routes_from(int izmop);
int zmop_set_route_from(int izmop, int izmip, int route);
int zmop_get_route_from(int izmop, int izmip);
int zmop_get_routes_info(int izmop, int* buffer);
int zmop_get_routes_info_all(int* buffer);
// MIDI Note Range & Transpose
int zmop_set_note_low(int iz, uint8_t nlow);
int zmop_set_note_high(int iz, uint8_t nhigh);
int zmop_set_transpose_octave(int iz, int8_t trans_oct);
int zmop_set_transpose_semitone(int iz, int8_t trans_semi);
int set_global_transpose(int8_t transpose);
uint8_t zmop_get_note_low(int iz);
uint8_t zmop_get_note_high(int iz);
int8_t zmop_get_transpose_octave(int iz);
int8_t zmop_get_transpose_semitone(int iz);
int8_t get_global_transpose();
int zmop_set_note_range_transpose(int iz, uint8_t nlow, uint8_t nhigh, int8_t trans_oct, int8_t trans_semi);
int zmop_reset_note_range_transpose(int iz);
// CC routing
int zmop_reset_cc_route(int iz);
int zmop_set_cc_route(int iz, uint8_t* cc_route);
int zmop_get_cc_route(int iz, uint8_t* cc_route);
// ----------------------------------------------------------------------------
// This is called from jack process!!
void zmop_push_event(struct zmop_st* zmop, jack_midi_event_t* ev); // Add event to MIDI output port

//-----------------------------------------------------------------------------
// Jack MIDI Process
//-----------------------------------------------------------------------------

int init_jack_midi(char* name);
int end_jack_midi();
void populate_midi_event_from_rb(jack_ringbuffer_t* rb, jack_midi_event_t* event);
void populate_zmip_event(struct zmip_st* zmip);
int jack_process(jack_nframes_t nframes, void* arg);
int jack_buffer_size_change(jack_nframes_t nframes, void* arg);
void jack_connect_cb(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);

//-----------------------------------------------------------------------------
// MIDI Events Buffer Management and Direct Send functions
//-----------------------------------------------------------------------------

#define JACK_MIDI_BUFFER_SIZE 16384

// Direct Send Event Ring-Buffer write
int write_rb_midi_event(jack_ringbuffer_t* rb, uint8_t* event_buffer, int event_size);

// ZMIP Direct Send Functions
int zmip_send_midi_event(uint8_t iz, uint8_t* event_buffer, int event_size);
int zmip_send_note_off(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel);
int zmip_send_note_on(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel);
int zmip_send_ccontrol_change(uint8_t iz, uint8_t chan, uint8_t ctrl, uint8_t val);
int zmip_send_master_ccontrol_change(uint8_t iz, uint8_t ctrl, uint8_t val);
int zmip_send_program_change(uint8_t iz, uint8_t chan, uint8_t prgm);
int zmip_send_chan_press(uint8_t iz, uint8_t chan, uint8_t val);
int zmip_send_pitchbend_change(uint8_t iz, uint8_t chan, uint16_t pb);
int zmip_send_all_notes_off(uint8_t iz);
int zmip_send_all_notes_off_chan(uint8_t iz, uint8_t izmop);

// ZMIP_FAKE_UI Direct Send Functions
int ui_send_midi_event(uint8_t* event_buffer, int event_size);
int ui_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int ui_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int ui_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int ui_send_master_ccontrol_change(uint8_t ctrl, uint8_t val);
int ui_send_program_change(uint8_t chan, uint8_t prgm);
int ui_send_chan_press(uint8_t chan, uint8_t val);
int ui_send_pitchbend_change(uint8_t chan, uint16_t pb);
int ui_send_all_notes_off();
int ui_send_all_notes_off_chan(uint8_t izmop);

// ZMOP Direct Send Functions
int zmop_send_midi_event(uint8_t iz, uint8_t* event_buffer, int event_size);
int zmop_send_note_off(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel);
int zmop_send_note_on(uint8_t iz, uint8_t chan, uint8_t note, uint8_t vel);
int zmop_send_ccontrol_change(uint8_t iz, uint8_t chan, uint8_t ctrl, uint8_t val);
int zmop_send_program_change(uint8_t iz, uint8_t chan, uint8_t prgm);
int zmop_send_chan_press(uint8_t iz, uint8_t chan, uint8_t val);
int zmop_send_pitchbend_change(uint8_t iz, uint8_t chan, uint16_t pb);

// ZMOP_CTRL Direct Send Functions
int ctrlfb_send_midi_event(uint8_t* event_buffer, int event_size);
int ctrlfb_send_note_off(uint8_t chan, uint8_t note, uint8_t vel);
int ctrlfb_send_note_on(uint8_t chan, uint8_t note, uint8_t vel);
int ctrlfb_send_ccontrol_change(uint8_t chan, uint8_t ctrl, uint8_t val);
int ctrlfb_send_program_change(uint8_t chan, uint8_t prgm);

// ZMOP_DEV Direct Send Functions
int dev_send_midi_event(uint8_t idev, uint8_t* event_buffer, int event_size);
int dev_send_note_off(uint8_t idev, uint8_t chan, uint8_t note, uint8_t vel);
int dev_send_note_on(uint8_t idev, uint8_t chan, uint8_t note, uint8_t vel);
int dev_send_ccontrol_change(uint8_t idev, uint8_t chan, uint8_t ctrl, uint8_t val);
int dev_send_program_change(uint8_t idev, uint8_t chan, uint8_t prgm);

//-----------------------------------------------------------------------------
// MIDI Internal Ouput Events Buffer => UI
//-----------------------------------------------------------------------------

// Size in bytes. Each message is 4-bytes long (uint32_t)
#define ZYNMIDI_BUFFER_SIZE 16384

int init_zynmidi_buffer();
int end_zynmidi_buffer();
int write_zynmidi(uint32_t ev);
uint32_t read_zynmidi();
int read_zynmidi_buffer(uint32_t* buffer, int n);
int get_zynmidi_num_max();
int get_zynmidi_num_pending();

int write_zynmidi_note_off(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_note_on(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_ccontrol_change(uint8_t chan, uint8_t num, uint8_t val);
int write_zynmidi_program_change(uint8_t chan, uint8_t num);

//-----------------------------------------------------------------------------

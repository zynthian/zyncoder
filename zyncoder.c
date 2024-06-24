/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing Rotary Encoders & Switches connected 
 * to RBPi native GPIOs or expanded with MCP23008/MCP23017.
 * Includes an emulator mode for developing on desktop computers.
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
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>

//#define DEBUG

#include "gpiod_callback.h"

#ifdef ZYNAPTIK_CONFIG
	#include "zynaptik.h"
#endif

#include "zynpot.h"
#include "zyncoder.h"
#include "zynmcp23017.h"

//-----------------------------------------------------------------------------
// Function headers
//-----------------------------------------------------------------------------

void send_zynswitch_midi(zynswitch_t *zsw);

void zynswitch_rbpi_ISR(uint8_t i);
void (*zynswitch_rbpi_ISRs[]);
void zyncoder_rbpi_ISR(uint8_t i);
void (*zyncoder_rbpi_ISRs[]);

extern void (*zynpot_cb)(int8_t, int32_t);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern zynmcp23017_t zynmcp23017s[MAX_NUM_MCP23017];
extern struct zmip_st zmips[MAX_NUM_ZMIPS];

zynswitch_t zynswitches[MAX_NUM_ZYNSWITCHES];
zyncoder_t zyncoders[MAX_NUM_ZYNCODERS];

uint16_t num_zynswitches = 0;

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

#ifdef DEBUG
unsigned int int_to_int(unsigned int k) {
	return (k == 0 || k == 1 ? k : ((k % 2) + 10 * int_to_int(k / 2)));
}
#endif

// Table of valid encoder states
static const uint8_t valid_quadrant_states[] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

//-----------------------------------------------------------------------------
// Zynswitch functions
//-----------------------------------------------------------------------------

void reset_zynswitches() {
	int i;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		zynswitches[i].enabled = 0;
		zynswitches[i].midi_event.type = NONE_EVENT;
		zynswitches[i].last_cvgate_note = -1;
	}
}

int get_num_zynswitches() {
	return num_zynswitches;
/*
	int i;
	int n = 0;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		if (zynswitches[i].enabled!=0) n++;
	}
	return n;
*/
}

int get_last_zynswitch_index() {
	int i;
	int li = 0;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		if (zynswitches[i].enabled!=0) li = i;
	}
	return li;
}

void update_zynswitch(uint8_t i, uint8_t status) {
	zynswitch_t *zsw = zynswitches + i;

	if (status==zsw->status) return;
	zsw->status=status;

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long int tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	//fprintf(stderr, "SWITCH ISR %d => STATUS=%d (%lu)\n",i,status,tsus);

	//If pushed ...
	if (zsw->tsus>0) {
		unsigned int dtus;
		dtus=tsus-zsw->tsus;

		//SW debouncing => Ignore spurious clicks
		if (dtus<1000) return;

		//Release
		if (zsw->status==zsw->off_state) {
			zsw->tsus=0;
			//fprintf(stderr, "Debounced Switch %d\n",i);
			zsw->dtus=dtus;
		}
	}
	//Push
	else if (zsw->status!=zsw->off_state) {
		zsw->push=1;
		zsw->tsus=tsus;		// Save push timestamp
	}
	//Send MIDI
	send_zynswitch_midi(zsw);
}

int setup_zynswitch(uint8_t i, uint16_t pin, uint8_t off_state) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		fprintf(stderr, "ZynCore->setup_zynswitch(%d, ...): Invalid index!\n", i);
		return 0;
	}
	
	zynswitch_t *zsw = zynswitches + i;
	zsw->enabled = 0;
	zsw->push=0;
	zsw->tsus = 0;
	zsw->dtus = 0;
	zsw->status = 0;

	if (pin>0) {
		if (off_state) zsw->off_state = 1;
		else zsw->off_state = 0;

		// RBPi GPIO pin
		if (pin<100) {
			struct gpiod_line *line = gpiod_chip_get_line(gpio_chip, pin);
			if (line) {
				int flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
				if (!off_state) flags |= GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW;
				if (gpiod_line_request_both_edges_events_flags(line, ZYNCORE_CONSUMER, flags)>=0) {
					zsw->enabled = 1;
					zsw->pin = pin;
					zsw->line = line;
					gpiod_line_register_callback(line, zynswitch_rbpi_ISRs[i]);
					zynswitch_rbpi_ISR(i);
					return 1;
				} else {
					fprintf(stderr, "ZynCore->setup_zynswitch(%d, %d, ...): Can't request line events from RPI GPIO\n", i, pin);
					return 0;
				}
			} else {
				fprintf(stderr, "ZynCore->setup_zynswitch(%d, %d, ...): Can't get line from RPI GPIO\n", i, pin);
				return 0;
			}
		} 
		// MCP23017 pin
		else if (pin>=100) {
			#if defined(MCP23008_ENCODERS)
				zsw->pin = pin;
				zsw->enabled = 1;
			#elif defined(MCP23017_ENCODERS)
				uint8_t j = pin2index_zynmcp23017(pin);
				if (j>=0) {
					uint8_t bit = pin - zynmcp23017s[j].base_pin;
					if (bit<16) {
						zsw->pin = pin;
						zsw->enabled = 1;
						setup_pin_action_zynmcp23017(pin, ZYNSWITCH_PIN_ACTION, i);
						zynswitch_update_zynmcp23017(i);
						return 1;
					}
					else {
						fprintf(stderr, "ZynCore->setup_zynswitch(%d, %d, ...): Pin out of range!\n",i, pin);
						return 0;
					}
				}
				else {
					fprintf(stderr, "ZynCore->setup_zynswitch(%d, %d, ...): Pin is not a MPC23017 pin!\n",i, pin);
					return 0;
				}
			#endif
		}
	}
	return 0;
}

int setup_zynswitch_midi(uint8_t i, enum midi_event_type_enum midi_evt, uint8_t midi_chan, uint8_t midi_num, uint8_t midi_val) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		fprintf(stderr, "ZynCore->setup_zynswitch_midi(%d, ...): Invalid index!\n", i);
		return 0;
	}

	zynswitch_t *zsw = zynswitches + i;
	zsw->midi_event.type = midi_evt;
	zsw->midi_event.chan = midi_chan;
	zsw->midi_event.num = midi_num;
	zsw->midi_event.val = midi_val;
	//fprintf(stderr, "Zyncoder: Set Zynswitch %u MIDI %d: %u, %u, %u\n", i, midi_evt, midi_chan, midi_num, midi_val);

	//zsw->last_cvgate_note = -1;

	//**********************************************
	// TODO => Refactorize zynaptik functionality!!!
	#ifdef ZYNAPTIK_CONFIG
	if (midi_evt==CVGATE_OUT_EVENT) {
		pinMode(zsw->pin, OUTPUT);
		digitalWrite(zsw->pin, zsw->off_state);
		zynaptik_setup_cvout(midi_num, midi_evt, midi_chan, i);
	}
	else if (midi_evt==GATE_OUT_EVENT) {
		pinMode(zsw->pin, OUTPUT);
		digitalWrite(zsw->pin, zsw->off_state);
		zynaptik_setup_gateout(i, midi_evt, midi_chan, midi_num);
	}
	#endif
	//**********************************************

	return 1;
}

unsigned int get_zynswitch_dtus(uint8_t i, unsigned int long_dtus) {
	unsigned int dtus = zynswitches[i].dtus;
	if (dtus > 0) {
		zynswitches[i].dtus = 0;
		return dtus;
	}
	else if (zynswitches[i].tsus > 0) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		dtus = ts.tv_sec * 1000000 + ts.tv_nsec / 1000 - zynswitches[i].tsus;
		if (dtus>long_dtus) {
			zynswitches[i].tsus = 0;
			return dtus;
		}
	}
	return -1;
}

unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		fprintf(stderr, "ZynCore->get_zynswitch(%d, ...): Invalid index!\n", i);
		return 0;
	}
	if (zynswitches[i].push) {
		zynswitches[i].push = 0;
		return 0;
	} else  {
		return get_zynswitch_dtus(i, long_dtus);
	}
}

int get_next_pending_zynswitch(uint8_t i) {
	while (i<MAX_NUM_ZYNSWITCHES) {
		if (zynswitches[i].dtus>0 || zynswitches[i].tsus>0) return (int)i;
		i++;
	} 
	return -1;
}

void send_zynswitch_midi(zynswitch_t *zsw) {

	if (zsw->midi_event.type==CTRL_CHANGE) {
		uint8_t val;
		if (zsw->status!=zsw->off_state) val=zsw->midi_event.val;
		else val=0;
		//Send MIDI event to engines and ouput (ZMOPS)
		zmip_send_ccontrol_change(ZMIP_FAKE_INT, zsw->midi_event.chan, zsw->midi_event.num, val);
		//Send MIDI event to UI
		write_zynmidi_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
		//fprintf(stderr, "ZynCore: Zynswitch MIDI CC event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, val);
	}
	else if (zsw->midi_event.type==CTRL_SWITCH_EVENT) {
		if (zsw->status!=zsw->off_state) {
			uint8_t val;
			uint8_t last_val = zmips[ZMIP_FAKE_INT].last_ctrl_val[zsw->midi_event.chan][zsw->midi_event.num];
			if (last_val>=64) val = 0;
			else val = 127;
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_ccontrol_change(ZMIP_FAKE_INT, zsw->midi_event.chan, zsw->midi_event.num, val);
			//Send MIDI event to UI
			write_zynmidi_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
			//fprintf(stderr, "ZynCore: Zynswitch MIDI CC-Switch event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, val);
		}
	}
	else if (zsw->midi_event.type==NOTE_ON) {
		if (zsw->status!=zsw->off_state) {
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_note_on(ZMIP_FAKE_INT, zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
			//Send MIDI event to UI
			write_zynmidi_note_on(zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
			//fprintf(stderr, "ZynCore: Zynswitch MIDI Note-On event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
		}
		else {
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_note_off(ZMIP_FAKE_INT, zsw->midi_event.chan, zsw->midi_event.num, 0);
			//Send MIDI event to UI
			write_zynmidi_note_off(zsw->midi_event.chan, zsw->midi_event.num, 0);
			//fprintf(stderr, "ZynCore: Zynswitch MIDI Note-Off event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, 0);
		}
	}
	#ifdef ZYNAPTIK_CONFIG
	else if (zsw->midi_event.type==CVGATE_IN_EVENT && zsw->midi_event.num<4) {
		//fprintf(stderr, "ZynCore: Zynswitch CV/Gate-IN EVENT (PIN %d) => %d\n",zsw->pin, zsw->status);
		if (zsw->status!=zsw->off_state) {
			pthread_mutex_lock(&zynaptik_cvin_lock);
			int val=analogRead(ZYNAPTIK_ADS1115_BASE_PIN + zsw->midi_event.num);
			pthread_mutex_unlock(&zynaptik_cvin_lock);
			//zsw->last_cvgate_note=(int)((k_cvin*6.144/(5.0*256.0))*val);

			zsw->last_cvgate_note = note0_cvin + (int)(k_cvin * val);
			if (zsw->last_cvgate_note>127) zsw->last_cvgate_note=127;
			else if (zsw->last_cvgate_note<0) zsw->last_cvgate_note=0;
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_note_on(ZMIP_FAKE_INT, zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, zsw->midi_event.val);
			//Send MIDI event to UI
			write_zynmidi_note_on(zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, zsw->midi_event.val);
			//fprintf(stderr, "ZynCore: Zynswitch CV/Gate-IN NOTE-ON (chan=%d, raw=%d, num=%d) => %d\n",zsw->midi_event.chan, val, zsw->last_cvgate_note, zsw->midi_event.val);
		}
		else {
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_note_off(ZMIP_FAKE_INT, zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, 0);
			//Send MIDI event to UI
			write_zynmidi_note_off(zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, 0);
			//fprintf(stderr, "ZynCore: Zynswitch CV/Gate-IN NOTE-OFF (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->last_cvgate_note, 0);
		}
	}
	#endif
	else if (zsw->midi_event.type==PROG_CHANGE) {
		if (zsw->status!=zsw->off_state) {
			//Send MIDI event to engines and ouput (ZMOPS)
			zmip_send_program_change(ZMIP_FAKE_INT, zsw->midi_event.chan, zsw->midi_event.num);
			//Send MIDI event to UI
			write_zynmidi_program_change(zsw->midi_event.chan, zsw->midi_event.num);
			//fprintf(stderr, "ZynCore: Zynswitch MIDI Program Change event (chan=%d, num=%d)\n",zsw->midi_event.chan, zsw->midi_event.num);
		}
	}
	else if (zsw->midi_event.type==TIME_CLOCK || zsw->midi_event.type==TRANSPORT_START || zsw->midi_event.type==TRANSPORT_CONTINUE || zsw->midi_event.type==TRANSPORT_STOP) {
		//Send MIDI event to engines and ouput (ZMOPS)
		uint8_t buffer[3];
		buffer[0] = zsw->midi_event.type;
		buffer[1] = 0;
		buffer[2] = 0;
		zmip_send_midi_event(ZMIP_FAKE_INT, buffer, 3);
		//Send MIDI event to UI
		write_zynmidi((uint32_t)zsw->midi_event.type << 16);
		//fprintf(stderr, "ZynCore: Zynswitch MIDI SYSTEM RT event=> %d\n", zsw->midi_event.type);
	}
}

//-----------------------------------------------------------------------------
// Zyncoder's zynpot API
//-----------------------------------------------------------------------------

void reset_zyncoders() {
	int i,j;
	for (i=0;i<MAX_NUM_ZYNCODERS;i++) {
		zyncoders[i].enabled = 0;
		zyncoders[i].value = 0;
 		zyncoders[i].zpot_i = -1;
 		zyncoders[i].short_history = 0;
 		zyncoders[i].long_history = 0;
		zyncoders[i].tsms = 0;
	}
}

int get_num_zyncoders() {
	int i;
	int n = 0;
	for (i=0;i<MAX_NUM_ZYNCODERS;i++) {
		if (zyncoders[i].enabled!=0) n++;
	}
	return n;
}

void update_zyncoder(uint8_t i, uint8_t msb, uint8_t lsb) {
	zyncoder_t *zcdr = zyncoders + i;

	//fprintf(stderr, "ZynCore->update_zyncoder(%d, %d, %d)\n", i, msb, lsb);

	// step == 0 so use software filter algorithm and speed based scaling
	// Shift last read state to top of short history
	zcdr->short_history <<= 2;
	// Add current state to bottom of short history
	if (!msb)
		zcdr->short_history |= 0x02;
	if (!lsb)
		zcdr->short_history |= 0x01;
	zcdr->short_history &= 0x0f; // Mask short history to 4 bits
	// Look up in table for valid transition from previous to current state
	if (valid_quadrant_states[zcdr->short_history]) {
		// Shift previous valid transition and store this transition in long history
		zcdr->long_history <<= 4;
		zcdr->long_history |= zcdr->short_history;
		int8_t dval = 0;
		if (zcdr->long_history == 0xd4) {
			// Last transition in CW direction before rest detent
			dval = 1;
		}
		else if (zcdr->long_history == 0xe8) {
			// Last transition in CCW direction before rest detent
			dval = -1;
		} else {
			// Not at rest detent so ignore - if want finer resolution could count every quadrant of detent, not just rest detent
			return;
		}
		if (zcdr->step) {
			dval *= zcdr->step;
		} else {
			//Get time interval from last tick
			struct timespec ts;
			uint64_t tsms;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			tsms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
			int64_t dtms = tsms - zcdr->tsms; // milliseconds since last encoder change
			// Rotation acceleration
			if (dtms < 40)
				dval *= (((40 - dtms) / 10) + 1);
			zcdr->tsms = tsms;
		}
		zcdr->value += dval;

		//Call CB function
		if (zynpot_cb) {
			zynpot_cb(zcdr->zpot_i, zcdr->value);
			zcdr->value = 0;
		}
	}
}

int setup_zyncoder(uint8_t i, uint16_t pin_a, uint16_t pin_b) {
	if (i>=MAX_NUM_ZYNCODERS) {
		fprintf(stderr, "ZynCore->setup_zyncoder(%d, ...): Invalid index!\n", i);
		return 0;
	}
	zyncoder_t *zcdr = zyncoders + i;

	//setup_rangescale_zyncoder(i,0,127,64,0);
	zcdr->enabled = 0;
	zcdr->step = 1;
	zcdr->value = 0;
	zcdr->tsms = 0;
	zcdr->short_history = 0;
	zcdr->long_history = 0;

	if (pin_a!=pin_b) {
		// RBPi GPIO pins
		if (pin_a<100 && pin_b<100) {
			struct gpiod_line *line_a = gpiod_chip_get_line(gpio_chip, pin_a);
			struct gpiod_line *line_b = gpiod_chip_get_line(gpio_chip, pin_b);
			if (line_a && line_b) {
				int flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
				if (gpiod_line_request_both_edges_events_flags(line_a, ZYNCORE_CONSUMER, flags) >=0 &&
					gpiod_line_request_both_edges_events_flags(line_b, ZYNCORE_CONSUMER, flags) >=0) {
					zcdr->line_a = line_a;
					zcdr->line_b = line_b;
					zcdr->pin_a = pin_a;
					zcdr->pin_b = pin_b;
					zcdr->enabled = 1;
					gpiod_line_register_callback(line_a, zyncoder_rbpi_ISRs[i]);
					gpiod_line_register_callback(line_b, zyncoder_rbpi_ISRs[i]);
					zyncoder_rbpi_ISR(i);
					return 1;
				} else {
					fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't request line events from RPI GPIO\n", i, pin_a, pin_b);
					return 0;
				}
			} else {
				fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't get line from RPI GPIO\n", i, pin_a, pin_b);
				return 0;
			}
		}
		// MCP23017 pins
		else if (pin_a>=100 && pin_b>=100) {
			#if defined(MCP23017_ENCODERS)
				uint8_t j = pin2index_zynmcp23017(pin_a);
				uint8_t k = pin2index_zynmcp23017(pin_b);
				if (j>=0 && k>=0 && j==k) {
					uint8_t bit_a = pin_a - zynmcp23017s[j].base_pin;
					uint8_t bit_b = pin_b - zynmcp23017s[k].base_pin;
					if (bit_a<16 && bit_b<16) {
						uint8_t bank_a, bank_b;
						if (bit_a<8) bank_a=0;
						else bank_a=1;
						if (bit_b<8) bank_b=0;
						else bank_b=1;
						if (bank_a == bank_b) {
							zcdr->pin_a = pin_a;
							zcdr->pin_b = pin_b;
							zcdr->enabled = 1;
							setup_pin_action_zynmcp23017(pin_a, ZYNCODER_PIN_ACTION, i);
							setup_pin_action_zynmcp23017(pin_b, ZYNCODER_PIN_ACTION, i);
							zyncoder_update_zynmcp23017(i);
							return 1;
						}
						else {
							fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't configure zyncoder with pins on different banks!\n", i, pin_a, pin_b);
							return 0;
						}
					}
					else {
						fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Pin numbers out of range!\n", i, pin_a, pin_b);
						return 0;
					}
				}
				else {
					fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't configure zyncoder with pins on different MCP23017!\n", i, pin_a, pin_b);
					return 0;
				}
			#endif
		}
		else {
			fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't configure zyncoder with mixed pins (RBPi & MCP23017)!\n", i, pin_a, pin_b);
			return 0;
		}
	}
	else {
		fprintf(stderr, "ZynCore->setup_zyncoder(%d, %d, %d): Can't configure zyncoder on a single pin!\n", i, pin_a, pin_b);
		return 0;
	}
	return 0;
}

int setup_behaviour_zyncoder(uint8_t i, int32_t step) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		fprintf(stderr, "ZynCore->setup_behaviour_zyncoder(%d, ...): Invalid index!\n", i);
		return 0;
	}

	// Allowed step values for encoders are 0 & 1.
	if (step==0) zyncoders[i].step = 0;
	else zyncoders[i].step = 1;
	
	zyncoders[i].value = 0;
	zyncoders[i].tsms = 0;
	zyncoders[i].short_history = 0;
	zyncoders[i].long_history = 0;
	return 1;
}

int32_t get_value_zyncoder(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		fprintf(stderr, "ZynCore->get_value_zyncoder(%d): Invalid index!\n", i);
		return 0;
	}
	int32_t res = zyncoders[i].value;
	if (res!=0) {
		zyncoders[i].value = 0;
	}
	return res;
}


//-----------------------------------------------------------------------------
// RBPi GPIO ISR
//-----------------------------------------------------------------------------

void zynswitch_rbpi_ISR(uint8_t i) {
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zynswitch_t *zsw = zynswitches + i;
	if (zsw->enabled==0) return;
	update_zynswitch(i, (uint8_t)gpiod_line_get_value(zsw->line));
}

void zynswitch_rbpi_ISR_0() { zynswitch_rbpi_ISR(0); }
void zynswitch_rbpi_ISR_1() { zynswitch_rbpi_ISR(1); }
void zynswitch_rbpi_ISR_2() { zynswitch_rbpi_ISR(2); }
void zynswitch_rbpi_ISR_3() { zynswitch_rbpi_ISR(3); }
void zynswitch_rbpi_ISR_4() { zynswitch_rbpi_ISR(4); }
void zynswitch_rbpi_ISR_5() { zynswitch_rbpi_ISR(5); }
void zynswitch_rbpi_ISR_6() { zynswitch_rbpi_ISR(6); }
void zynswitch_rbpi_ISR_7() { zynswitch_rbpi_ISR(7); }
void (*zynswitch_rbpi_ISRs[8])={
	zynswitch_rbpi_ISR_0,
	zynswitch_rbpi_ISR_1,
	zynswitch_rbpi_ISR_2,
	zynswitch_rbpi_ISR_3,
	zynswitch_rbpi_ISR_4,
	zynswitch_rbpi_ISR_5,
	zynswitch_rbpi_ISR_6,
	zynswitch_rbpi_ISR_7
};


void zyncoder_rbpi_ISR(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS) return;
	zyncoder_t *zcdr = zyncoders + i;
	if (zcdr->enabled==0) return;
	update_zyncoder(i, (uint8_t)gpiod_line_get_value(zcdr->line_a), (uint8_t)gpiod_line_get_value(zcdr->line_b));
}

void zyncoder_rbpi_ISR_0() { zyncoder_rbpi_ISR(0); }
void zyncoder_rbpi_ISR_1() { zyncoder_rbpi_ISR(1); }
void zyncoder_rbpi_ISR_2() { zyncoder_rbpi_ISR(2); }
void zyncoder_rbpi_ISR_3() { zyncoder_rbpi_ISR(3); }
void (*zyncoder_rbpi_ISRs[4])={
	zyncoder_rbpi_ISR_0,
	zyncoder_rbpi_ISR_1,
	zyncoder_rbpi_ISR_2,
	zyncoder_rbpi_ISR_3,
};

//-----------------------------------------------------------------------------

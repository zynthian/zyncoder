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

//#define DEBUG

#if defined(HAVE_WIRINGPI_LIB)
	#include <wiringPi.h>
	#include <wiringPiI2C.h>
	#include <mcp23017.h>
	#include <mcp23x0817.h>
	#include <mcp23008.h>
#else
	#include "wiringPiEmu.h"
#endif

#ifdef ZYNAPTIK_CONFIG
	#include "zynaptik.h"
#endif

#include "zynpot.h"
#include "zyncoder.h"

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

//-----------------------------------------------------------------------------
// Function headers
//-----------------------------------------------------------------------------

void send_zynswitch_midi(zynswitch_t *zsw, uint8_t status);

void zynswitch_rbpi_ISR(uint8_t i);
void (*zynswitch_rbpi_ISRs[]);
void zyncoder_rbpi_ISR(uint8_t i);
void (*zyncoder_rbpi_ISRs[]);

void zynswitch_mcp23017_update(uint8_t i);
void zyncoder_mcp23017_update(uint8_t i);

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

#ifdef DEBUG
unsigned int int_to_int(unsigned int k) {
	return (k == 0 || k == 1 ? k : ((k % 2) + 10 * int_to_int(k / 2)));
}
#endif

//-----------------------------------------------------------------------------
// Zynswitch functions
//-----------------------------------------------------------------------------

void reset_zynswitches() {
	int i;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		zynswitches[i].enabled = 0;
		zynswitches[i].midi_event.type = NONE_EVENT;
	}
}

int get_num_zynswitches() {
	int i;
	int n = 0;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		if (zynswitches[i].enabled!=0) n++;
	}
	return n;
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

	//printf("SWITCH ISR %d => STATUS=%d\n",i,status);

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long int tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	//printf("SWITCH ISR %d => STATUS=%d (%lu)\n",i,zsw->status,tsus);
	if (zsw->status==1) {
		if (zsw->tsus>0) {
			unsigned int dtus=tsus-zsw->tsus;
			zsw->tsus=0;
			//Ignore spurious clicks (SW debouncing)
			if (dtus<1000) return;
			//printf("Debounced Switch %d\n",i);
			zsw->dtus=dtus;
		}
	} else {
		// Save push timestamp
		zsw->push=1;
		zsw->tsus=tsus;
		// Send MIDI when pushed => no SW debouncing!!
		send_zynswitch_midi(zsw, status);
	}
}

int setup_zynswitch(uint8_t i, uint8_t pin) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		printf("ZynCore->setup_zynswitch(%d, ...): Invalid index!\n", i);
		return 0;
	}
	
	zynswitch_t *zsw = zynswitches + i;
	zsw->enabled = 1;
	zsw->pin = pin;
	zsw->push=0;
	zsw->tsus = 0;
	zsw->dtus = 0;
	zsw->status = 0;

	if (pin>0) {
		pinMode(pin, INPUT);
		pullUpDnControl(pin, PUD_UP);

		// RBPi GPIO pin
		if (pin<100) {
			wiringPiISR(pin,INT_EDGE_BOTH, zynswitch_rbpi_ISRs[i]);
			zynswitch_rbpi_ISR(i);
		} 
		// MCP23017 pin
		else if (pin>=100) {
			zynswitch_mcp23017_update(i);
		}
	}

	return 1;
}

int setup_zynswitch_midi(uint8_t i, enum midi_event_type_enum midi_evt, uint8_t midi_chan, uint8_t midi_num, uint8_t midi_val) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		printf("ZynCore->setup_zynswitch_midi(%d, ...): Invalid index!\n", i);
		return 0;
	}

	zynswitch_t *zsw = zynswitches + i;
	zsw->midi_event.type = midi_evt;
	zsw->midi_event.chan = midi_chan;
	zsw->midi_event.num = midi_num;
	zsw->midi_event.val = midi_val;
	//printf("Zyncoder: Set Zynswitch %u MIDI %d: %u, %u, %u\n", i, midi_evt, midi_chan, midi_num, midi_val);

	zsw->last_cvgate_note = -1;

	#ifdef ZYNAPTIK_CONFIG
	if (midi_evt==CVGATE_OUT_EVENT) {
		pinMode(zsw->pin, OUTPUT);
		setup_zynaptik_cvout(midi_num, midi_evt, midi_chan, i);
	}
	else if (midi_evt==GATE_OUT_EVENT) {
		pinMode(zsw->pin, OUTPUT);
		setup_zynaptik_gateout(i, midi_evt, midi_chan, midi_num);
	}
	#endif

	return 1;
}

unsigned int get_zynswitch_dtus(uint8_t i, unsigned int long_dtus) {
	unsigned int dtus=zynswitches[i].dtus;
	if (dtus>0) {
		zynswitches[i].dtus=0;
		return dtus;
	}
	else if (zynswitches[i].tsus>0) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		dtus=ts.tv_sec*1000000 + ts.tv_nsec/1000 - zynswitches[i].tsus;
		if (dtus>long_dtus) {
			zynswitches[i].tsus=0;
			return dtus;
		}
	}
	return -1;
}

unsigned int get_zynswitch(uint8_t i, unsigned int long_dtus) {
	if (i >= MAX_NUM_ZYNSWITCHES) {
		printf("ZynCore->get_zynswitch(%d, ...): Invalid index!\n", i);
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

void send_zynswitch_midi(zynswitch_t *zsw, uint8_t status) {

	if (zsw->midi_event.type==CTRL_CHANGE) {
		uint8_t val;
		if (status==0) val=zsw->midi_event.val;
		else val=0;
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
		//Update zyncoders
		midi_event_zynpot(zsw->midi_event.chan, zsw->midi_event.num, val);
		//Send MIDI event to UI
		write_zynmidi_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
		//printf("ZynCore: Zynswitch MIDI CC event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, val);
	}
	else if (zsw->midi_event.type==CTRL_SWITCH_EVENT) {
		uint8_t val;
		uint8_t last_val = midi_filter.last_ctrl_val[zsw->midi_event.chan][zsw->midi_event.num];
		if (last_val>=64) val = 0;
		else val = 127;
		//Send MIDI event to engines and ouput (ZMOPS)
		internal_send_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
		//Update zyncoders
		midi_event_zynpot(zsw->midi_event.chan, zsw->midi_event.num, val);
		//Send MIDI event to UI
		write_zynmidi_ccontrol_change(zsw->midi_event.chan, zsw->midi_event.num, val);
		//printf("ZynCore: Zynswitch MIDI CC-Switch event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, val);
	}
	else if (zsw->midi_event.type==NOTE_ON) {
		if (status==0) {
			//Send MIDI event to engines and ouput (ZMOPS)
			internal_send_note_on(zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
			//Send MIDI event to UI
			write_zynmidi_note_on(zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
			//printf("ZynCore: Zynswitch MIDI Note-On event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, zsw->midi_event.val);
		}
		else {
			//Send MIDI event to engines and ouput (ZMOPS)
			internal_send_note_off(zsw->midi_event.chan, zsw->midi_event.num, 0);
			//Send MIDI event to UI
			write_zynmidi_note_off(zsw->midi_event.chan, zsw->midi_event.num, 0);
			//printf("ZynCore: Zynswitch MIDI Note-Off event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->midi_event.num, 0);
		}
	}
	#ifdef ZYNAPTIK_CONFIG
	else if (zsw->midi_event.type==CVGATE_IN_EVENT && zsw->midi_event.num<4) {
		if (status==0) {
			pthread_mutex_lock(&zynaptik_cvin_lock);
			int val=analogRead(ZYNAPTIK_ADS1115_BASE_PIN + zsw->midi_event.num);
			pthread_mutex_unlock(&zynaptik_cvin_lock);
			zsw->last_cvgate_note=(int)((k_cvin*6.144/(5.0*256.0))*val);
			if (zsw->last_cvgate_note>127) zsw->last_cvgate_note=127;
			else if (zsw->last_cvgate_note<0) zsw->last_cvgate_note=0;
			//Send MIDI event to engines and ouput (ZMOPS)
			internal_send_note_on(zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, zsw->midi_event.val);
			//Send MIDI event to UI
			write_zynmidi_note_on(zsw->midi_event.chan, (uint8_t)zsw->last_cvgate_note, zsw->midi_event.val);
			//printf("ZynCore: Zynswitch CV/Gate-IN event (chan=%d, raw=%d, num=%d) => %d\n",zsw->midi_event.chan, val, zsw->last_cvgate_note, zsw->midi_event.val);
		}
		else {
			//Send MIDI event to engines and ouput (ZMOPS)
			internal_send_note_off(zsw->midi_event.chan, zsw->last_cvgate_note, 0);
			//Send MIDI event to UI
			write_zynmidi_note_off(zsw->midi_event.chan, zsw->last_cvgate_note, 0);
			//printf("ZynCore: Zynswitch CV/Gate event (chan=%d, num=%d) => %d\n",zsw->midi_event.chan, zsw->last_cvgate_note, 0);
		}
	}
	#endif
	else if (zsw->midi_event.type==PROG_CHANGE) {
		if (status==0) {
			//Send MIDI event to engines and ouput (ZMOPS)
			internal_send_program_change(zsw->midi_event.chan, zsw->midi_event.num);
			//Send MIDI event to UI
			write_zynmidi_program_change(zsw->midi_event.chan, zsw->midi_event.num);
			//printf("ZynCore: Zynswitch MIDI Program Change event (chan=%d, num=%d)\n",zsw->midi_event.chan, zsw->midi_event.num);
		}
	}
}

//-----------------------------------------------------------------------------
// Zyncoder's zynpot API
//-----------------------------------------------------------------------------

void reset_zyncoders() {
	int i,j;
	for (i=0;i<MAX_NUM_ZYNCODERS;i++) {
		zyncoders[i].enabled = 0;
		zyncoders[i].inv = 0;
		zyncoders[i].value = 0;
		zyncoders[i].value_flag = 0;
 		zyncoders[i].zpot_i = -1;
		for (j=0;j<ZYNCODER_TICKS_PER_RETENT;j++)
			zyncoders[i].dtus[j] = 0;
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

	uint8_t encoded = (msb << 1) | lsb;
	uint8_t sum = (zcdr->last_encoded << 2) | encoded;
	int spin;
	if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) spin = 1;
	else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) spin = -1;
	else spin = 0;
	if (zcdr->inv) spin = -spin;
	#ifdef DEBUG
	//printf("zyncoder %2d - %08d\t%08d\t%d\n", i, int_to_int(encoded), int_to_int(sum), spin);
	#endif
	zcdr->last_encoded=encoded;

	int32_t value;
	if (zcdr->step==0) {
		//Get time interval from last tick
		struct timespec ts;
		unsigned long int tsus;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;
		unsigned int dtus=tsus-zcdr->tsus;
		//printf("ZYNCODER ISR %d => SUBVALUE=%d (%u)\n",i,zcdr->subvalue,dtus);
		//Ignore spurious ticks
		if (dtus<1000) return;
		//printf("ZYNCODER DEBOUNCED ISR %d => SUBVALUE=%d (%u)\n",i,zcdr->subvalue,dtus);
		//Calculate average dtus for the last ZYNCODER_TICKS_PER_RETENT ticks
		int j;
		unsigned int dtus_avg=dtus;
		for (j=0;j<ZYNCODER_TICKS_PER_RETENT;j++) dtus_avg+=zcdr->dtus[j];
		dtus_avg/=(ZYNCODER_TICKS_PER_RETENT+1);
		//Add last dtus to fifo array
		for (j=0;j<ZYNCODER_TICKS_PER_RETENT-1;j++)
			zcdr->dtus[j]=zcdr->dtus[j+1];
		zcdr->dtus[j]=dtus;
		//Calculate step value
		int32_t dsval=10000*ZYNCODER_TICKS_PER_RETENT/dtus_avg;
		if (dsval<1) dsval=1;
		else if (dsval>2*ZYNCODER_TICKS_PER_RETENT) dsval=2*ZYNCODER_TICKS_PER_RETENT;

		int32_t sv;
		if (spin>0) {
			sv = zcdr->subvalue + dsval;
			if (sv > zcdr->max_value) sv = zcdr->max_value;
		}
		else if (spin<0) {
			sv = zcdr->subvalue - dsval;
			if (sv < zcdr->min_value) sv = zcdr->min_value;
		}
		zcdr->subvalue = sv;
		value = sv / ZYNCODER_TICKS_PER_RETENT;
		zcdr->tsus=tsus;
		//printf("DTUS=%d, %d (%d)\n",dtus_avg,value,dsval);
	} 
	else {
		if (spin>0) {
			value = zcdr->value + zcdr->step;
			if (value>zcdr->max_value) value=zcdr->max_value;
		}
		else if (spin<0) {
			value = zcdr->value - zcdr->step;
			if (value<zcdr->min_value) value=zcdr->min_value;
		}
	}

	if (zcdr->value!=value) {
		zcdr->value=value;
		zcdr->value_flag = 1;
		if (zcdr->zpot_i>=0) {
			send_zynpot(zcdr->zpot_i);
		}
	}
}

int setup_zyncoder(uint8_t i, uint8_t pin_a, uint8_t pin_b) {
	if (i>=MAX_NUM_ZYNCODERS) {
		printf("ZynCore->setup_zyncoder(%d, ...): Invalid index!\n", i);
		return 0;
	}
	zyncoder_t *zcdr = zyncoders + i;

	//setup_rangescale_zyncoder(i,0,127,64,0);
	zcdr->inv = 0;
	zcdr->step = 1;
	zcdr->value = 0;
	zcdr->subvalue = 0;
	zcdr->min_value = 0;
	zcdr->max_value = 127;

	if (zcdr->enabled==0 || zcdr->pin_a!=pin_a || zcdr->pin_b!=pin_b) {
		zcdr->enabled = 1;
		zcdr->pin_a = pin_a;
		zcdr->pin_b = pin_b;
		zcdr->last_encoded = 0;
		zcdr->tsus = 0;

		if (zcdr->pin_a!=zcdr->pin_b) {
			pinMode(pin_a, INPUT);
			pinMode(pin_b, INPUT);
			pullUpDnControl(pin_a, PUD_UP);
			pullUpDnControl(pin_b, PUD_UP);

			// RBPi GPIO pins
			if (zcdr->pin_a<100 && zcdr->pin_b<100) {
				wiringPiISR(pin_a,INT_EDGE_BOTH, zyncoder_rbpi_ISRs[i]);
				wiringPiISR(pin_b,INT_EDGE_BOTH, zyncoder_rbpi_ISRs[i]);
				zyncoder_rbpi_ISR(i);
			} 
			// MCP23017 pins
			else if (zcdr->pin_a>=100 && zcdr->pin_b>=100) {
				zyncoder_mcp23017_update(i);
			}
			// Can't configure mixed pins!
			else {
				zcdr->enabled = 0;
				printf("ZynCore: Can't configure zyncoder with mixed pins!\n");
				return 0;
			}
		}
	}
	return 1;
}

int setup_rangescale_zyncoder(uint8_t i, int32_t min_value, int32_t max_value, int32_t value, int32_t step) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		printf("ZynCore->setup_rangescale_zyncoder(%d, ...): Invalid index!\n", i);
		return 0;
	}
	if (min_value==max_value) {
		//printf("ZynCore->setup_rangescale_zyncoder(%d, %d, %d, ...): Invalid range!\n", i, min_value, max_value);
		//return 0;
	}

	zyncoder_t *zcdr = zyncoders + i;

	if (min_value>max_value) {
		int32_t swapv = min_value;
		min_value = max_value;
		max_value = swapv;
		zcdr->inv = 1;
	}
	else {
		zcdr->inv = 0;
	}

	if (value>max_value) value = max_value;
	else if (value<min_value) value = min_value;

	zcdr->step = step;
	if (step==0) {
		zcdr->value = value;
		zcdr->subvalue = ZYNCODER_TICKS_PER_RETENT * value;
		zcdr->min_value = ZYNCODER_TICKS_PER_RETENT * min_value;
		zcdr->max_value = ZYNCODER_TICKS_PER_RETENT * (max_value + 1) - 1;
	} else {
		zcdr->value = value;
		zcdr->subvalue = 0;
		zcdr->min_value = min_value;
		zcdr->max_value = max_value;
	}
	zcdr->value_flag = 0;
}

int32_t get_value_zyncoder(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		printf("ZynCore->get_value_zyncoder(%d): Invalid index!\n", i);
		return 0;
	}
	zyncoders[i].value_flag = 0;
	return zyncoders[i].value;
}

uint8_t get_value_flag_zyncoder(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		printf("ZynCore->get_value_flag_zyncoder(%d): Invalid index!\n", i);
		return 0;
	}
	return zyncoders[i].value_flag;
}

int set_value_zyncoder(uint8_t i, int32_t v) {
	if (i>=MAX_NUM_ZYNCODERS || zyncoders[i].enabled==0) {
		printf("ZynCore->set_value_zyncoder(%d, ...): Invalid index!\n", i);
		return 0;
	}
	zyncoder_t *zcdr = zyncoders + i;

	if (zcdr->step==0) {
		v*=ZYNCODER_TICKS_PER_RETENT;
		if (v>zcdr->max_value) zcdr->subvalue=zcdr->max_value;
		else if (v<zcdr->min_value) zcdr->subvalue=zcdr->min_value;
		else zcdr->subvalue=v;
		zcdr->value=zcdr->subvalue/ZYNCODER_TICKS_PER_RETENT;
	} else {
		if (v>zcdr->max_value) zcdr->value=zcdr->max_value;
		else if (v<zcdr->min_value) zcdr->value=zcdr->max_value;
		else zcdr->value=v;
	}
	//zcdr->value_flag = 1;
	return 1;
}


//-----------------------------------------------------------------------------
// RBPi GPIO ISR
//-----------------------------------------------------------------------------

void zynswitch_rbpi_ISR(uint8_t i) {
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zynswitch_t *zsw = zynswitches + i;
	if (zsw->enabled==0) return;
	update_zynswitch(i, (uint8_t)digitalRead(zsw->pin));
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
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zyncoder_t *zcdr = zyncoders + i;
	if (zcdr->enabled==0) return;
	update_zyncoder(i, (uint8_t)digitalRead(zcdr->pin_a), (uint8_t)digitalRead(zcdr->pin_b));
}

void zyncoder_rbpi_ISR_0() { zyncoder_rbpi_ISR(0); }
void zyncoder_rbpi_ISR_1() { zyncoder_rbpi_ISR(1); }
void zyncoder_rbpi_ISR_2() { zyncoder_rbpi_ISR(2); }
void zyncoder_rbpi_ISR_3() { zyncoder_rbpi_ISR(3); }
void (*zyncoder_rbpi_ISRs[8])={
	zyncoder_rbpi_ISR_0,
	zyncoder_rbpi_ISR_1,
	zyncoder_rbpi_ISR_2,
	zyncoder_rbpi_ISR_3,
};

//-----------------------------------------------------------------------------
// MCP23008 Polling (only switches)
//-----------------------------------------------------------------------------

#ifdef MCP23008_ENCODERS

//Switch Polling interval
#define POLL_ZYNSWITCHES_US 10000

//Update NON-ISR switches (expanded GPIO with MCP23008 without INT => legacy V1's 2in1 module only!)
void update_expanded_zynswitches() {
	struct timespec ts;
	unsigned long int tsus;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsus=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	int i;
	uint8_t status;
	for (i=0;i<MAX_NUM_ZYNSWITCHES;i++) {
		zynswitch_t *zsw = zynswitches + i;
		if (!zsw->enabled || zsw->pin<100) continue;
		status=digitalRead(zsw->pin);
		//printf("POLLING SWITCH %d (%d) => %d\n",i,zsw->pin,status);
		if (status==zsw->status) continue;
		zsw->status=status;
		send_zynswitch_midi(zsw, status);
		//printf("POLLING SWITCH %d => STATUS=%d (%lu)\n",i,zsw->status,tsus);
		if (zsw->status==1) {
			if (zsw->tsus>0) {
				unsigned int dtus=tsus-zsw->tsus;
				zsw->tsus=0;
				//Ignore spurious ticks
				if (dtus<1000) return;
				//printf("Debounced Switch %d\n",i);
				zsw->dtus=dtus;
			}
		} else zsw->tsus=tsus;
	}
}

void * poll_zynswitches(void *arg) {
	while (1) {
		update_expanded_zynswitches();
		usleep(POLL_ZYNSWITCHES_US);
	}
	return NULL;
}

pthread_t init_poll_zynswitches() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_zynswitches, NULL);
	if (err != 0) {
		printf("ZynCore: Can't create zynswitches poll thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("ZynCore: Zynswitches poll thread created successfully\n");
		return tid;
	}
}
#endif

//-----------------------------------------------------------------------------
// MCP23017 initialization & ISR
//-----------------------------------------------------------------------------

#ifndef MCP23008_ENCODERS
struct wiringPiNodeStruct * init_mcp23017(int base_pin, uint8_t i2c_address, uint8_t inta_pin, uint8_t intb_pin, void (*isrs[2])) {
	uint8_t reg;

	mcp23017Setup(base_pin, i2c_address);

	// get the node corresponding to our mcp23017 so we can do direct writes
	struct wiringPiNodeStruct * mcp23017_node = wiringPiFindNode(base_pin);

	// setup all the pins on the banks as inputs and disable pullups on
	// the zyncoder input
	reg = 0xff;
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IODIRA, reg);
	wiringPiI2CWriteReg8(mcp23017_node->fd, MCP23x17_IODIRB, reg);

	// enable pullups on the unused pins (high two bits on each bank)
	reg = 0xff;
	//reg = 0xc0;
	//reg = 0x60;
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
	wiringPiISR(inta_pin, INT_EDGE_RISING, isrs[0]);
	// bank B
	wiringPiISR(intb_pin, INT_EDGE_RISING, isrs[1]);

	//Read data for first time ...
	wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_GPIOA);
	wiringPiI2CReadReg8(mcp23017_node->fd, MCP23x17_GPIOB);

	#ifdef DEBUG
	printf("ZynCore: MCP23017 at I2C %x initialized in base-pin %d: INTA %d, INTB %d\n", i2c_address, base_pin, inta_pin, intb_pin);
	#endif

	return mcp23017_node;
}

void zynswitch_mcp23017_update(uint8_t i) {
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zynswitch_t *zsw = zynswitches + i;
	if (zsw->enabled==0) return;

	uint8_t base_pin = (zsw->pin / 100) * 100;
	struct wiringPiNodeStruct * wpns = wiringPiFindNode(base_pin);

	uint8_t bit = zsw->pin % 100;
	uint8_t reg;
	// Bank A
	if (bit<8) {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOA);
	// Bank B
	} else if (bit<16) {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOB);
		bit-=8;
	// Pin out of range!!
	} else {
		printf("ZynCore: zynswitch_mcp23017_update(%d) => pin %d out of range!\n", i, bit);
	}
	update_zynswitch(i, (uint8_t)bitRead(reg, bit));
}

void zyncoder_mcp23017_update(uint8_t i) {
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zyncoder_t *zcdr = zyncoders + i;
	if (zcdr->enabled==0) return;
	
	uint8_t base_pin = (zcdr->pin_a / 100) * 100;
	struct wiringPiNodeStruct * wpns = wiringPiFindNode(base_pin);

	uint8_t bit_a = zcdr->pin_a % 100;
	uint8_t bit_b = zcdr->pin_b % 100;
	uint8_t reg;
	// Bank A
	if (bit_a<8 && bit_b<8) {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOA);
	// Bank B
	} else if (bit_a<16 && bit_b<16) {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOB);
		bit_a-=8;
		bit_b-=8;
	// Pin out of range!!
	} else {
		printf("ZynCore: zyncoder_mcp23017_update(%d) => pins (%d, %d) out of range or in different bank!\n", i, bit_a, bit_b);
	}
	uint8_t state_a = bitRead(reg, bit_a);
	uint8_t state_b = bitRead(reg, bit_b);
	update_zyncoder(i, state_a, state_b);
	zcdr->pin_a_last_state = state_a;
	zcdr->pin_b_last_state = state_b;
}

// ISR for handling the mcp23017 interrupts
void zyncoder_mcp23017_ISR(struct wiringPiNodeStruct *wpns, uint16_t base_pin, uint8_t bank) {
	// the interrupt has gone off for a pin change on the mcp23017
	// read the appropriate bank and compare pin states to last
	// on a change, call the update function as appropriate
	int i;
	uint8_t reg;
	uint8_t pin_min, pin_max;

	#ifdef DEBUG
	printf("zyncoder_mcp23017_ISR() => %d, %d\n", base_pin, bank);
	#endif

	if (bank == 0) {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOA);
		//reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_INTCAPA);
		pin_min = base_pin;
	} else {
		reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_GPIOB);
		//reg = wiringPiI2CReadReg8(wpns->fd, MCP23x17_INTCAPB);
		pin_min = base_pin + 8;
	}
	pin_max = pin_min + 7;

	// search all encoders and switches for a pin in the bank's range
	// if the last state != current state then this pin has changed
	// call the update function
	for (i=0; i<MAX_NUM_ZYNCODERS; i++) {
		zyncoder_t *zcdr = zyncoders + i;
		if (zcdr->enabled==0) continue;

		// if either pin is in the range
		if ((zcdr->pin_a >= pin_min && zcdr->pin_a <= pin_max) ||
		    (zcdr->pin_b >= pin_min && zcdr->pin_b <= pin_max)) {
			uint8_t bit_a = zcdr->pin_a - pin_min;
			uint8_t bit_b = zcdr->pin_b - pin_min;
			uint8_t state_a = bitRead(reg, bit_a);
			uint8_t state_b = bitRead(reg, bit_b);
			// if either bit is different
			if ((state_a != zcdr->pin_a_last_state) ||
			    (state_b != zcdr->pin_b_last_state)) {
				update_zyncoder(i, state_a, state_b);
				zcdr->pin_a_last_state = state_a;
				zcdr->pin_b_last_state = state_b;
			}
		}
	}
	for (i=0; i<MAX_NUM_ZYNSWITCHES; i++) {
		zynswitch_t *zsw = zynswitches + i;
		if (zsw->enabled == 0) continue;

		// check the pin range
		if (zsw->pin >= pin_min && zsw->pin <= pin_max) {
			uint8_t bit = zsw->pin - pin_min;
			uint8_t state = bitRead(reg, bit);
			#ifdef DEBUG
			printf("MCP23017 Zynswitch %d => %d\n",i,state);
			#endif
			if (state != zsw->status) {
				update_zynswitch(i, state);
				// note that the update function updates status with state
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------

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
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <lo/lo.h>

#ifdef HAVE_WIRINGPI_LIB
	#include <wiringPi.h>
	#include <mcp23008.h>
#else
	#include "wiringPiEmu.h"
#endif

#include "zyncoder.h"

// ALSA MIDI vars
snd_seq_t *rencoder_seq_handle=NULL;
int rencoder_seq_portid;

// Liblo OSC vars
lo_address rencoder_lo_addr;
char rencoder_osc_port_str[8];

//Switch Polling interval
int poll_switches_us=10000;

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_rencoder(int osc_port) {
	int i;
	for (i=0;i<max_gpio_switches;i++) gpio_switches[i].enabled=0;
	for (i=0;i<max_midi_rencoders;i++) midi_rencoders[i].enabled=0;
	wiringPiSetup();
	mcp23008Setup (100, 0x20);
	init_poll_switches();
	return init_seq_midi_rencoder(osc_port);
}

int init_seq_midi_rencoder(int osc_port) {
	if (snd_seq_open(&rencoder_seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "Error creating ALSA client.\n");
		return -1;
	}
	snd_seq_set_client_name(rencoder_seq_handle, "Zynthian_rencoder" );

	if (( rencoder_seq_portid = snd_seq_create_simple_port(rencoder_seq_handle, "Output port",
		SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
		//SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr, "Error creating output port.\n" );
		return -2;
	}

	if (osc_port) {
		sprintf(rencoder_osc_port_str,"%d",osc_port);
		//printf("OSC PORT: %s\n",rencoder_osc_port_str);
		rencoder_lo_addr=lo_address_new(NULL,rencoder_osc_port_str);
	}

	return rencoder_seq_portid;
}

//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

//Update ISR switches (native GPIO)
void update_gpio_switch(unsigned int i) {
	struct timespec ts;
	double tsns;

	if (i>=max_gpio_switches) return;
	struct gpio_switch *gpio_switch = gpio_switches + i;
	if (gpio_switch->enabled==0) return;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsns=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	gpio_switch->status=digitalRead(gpio_switch->pin);
	if (gpio_switch->status==1) {
		if (gpio_switch->tsus>0) gpio_switch->dtus=tsns-gpio_switch->tsus;
	} else gpio_switch->tsus=tsns;
}

void update_gpio_switch_0() { update_gpio_switch(0); }
void update_gpio_switch_1() { update_gpio_switch(1); }
void update_gpio_switch_2() { update_gpio_switch(2); }
void update_gpio_switch_3() { update_gpio_switch(3); }
void update_gpio_switch_4() { update_gpio_switch(4); }
void update_gpio_switch_5() { update_gpio_switch(5); }
void update_gpio_switch_6() { update_gpio_switch(6); }
void update_gpio_switch_7() { update_gpio_switch(7); }
void (*update_gpio_switch_funcs[8])={
	update_gpio_switch_0,
	update_gpio_switch_1,
	update_gpio_switch_2,
	update_gpio_switch_3,
	update_gpio_switch_4,
	update_gpio_switch_5,
	update_gpio_switch_6,
	update_gpio_switch_7
};

//Update NON-ISR switches (expanded GPIO)
void update_expanded_gpio_switches() {
	struct timespec ts;
	double tsns;
	unsigned int status;
	int i;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	tsns=ts.tv_sec*1000000 + ts.tv_nsec/1000;

	for (i=0;i<max_gpio_switches;i++) {
		struct gpio_switch *gpio_switch = gpio_switches + i;
		if (!gpio_switch->enabled || gpio_switch->pin<100) continue;
		status=digitalRead(gpio_switch->pin);
		//printf("POLLING SWITCH %d (%d) => %d\n",i,gpio_switch->pin,status);
		if (status==gpio_switch->status) continue;
		gpio_switch->status=status;
		if (gpio_switch->status==1) {
			if (gpio_switch->tsus>0) gpio_switch->dtus=tsns-gpio_switch->tsus;
		} else gpio_switch->tsus=tsns;
	}
}

void * poll_switches(void *arg) {
	while (1) {
		update_expanded_gpio_switches();
		usleep(poll_switches_us);
	}
	return NULL;
}

pthread_t init_poll_switches() {
	pthread_t tid;
	int err=pthread_create(&tid, NULL, &poll_switches, NULL);
	if (err != 0) {
		printf("Can't create poll thread :[%s]", strerror(err));
		return 0;
	} else {
		printf("Poll thread created successfully\n");
		return tid;
	}
}

//-----------------------------------------------------------------------------

struct gpio_switch *setup_gpio_switch(unsigned int i, unsigned int pin) {
	if (i >= max_gpio_switches) {
		printf("Maximum number of gpio switches exceded: %i\n", max_gpio_switches);
		return NULL;
	}
	
	struct gpio_switch *gpio_switch = gpio_switches + i;
	gpio_switch->enabled = 1;
	gpio_switch->pin = pin;
	gpio_switch->tsus = 0;
	gpio_switch->dtus = 0;
	gpio_switch->status = 0;

	pinMode(pin, INPUT);
	pullUpDnControl(pin, PUD_UP);
	if (pin<100) wiringPiISR(pin,INT_EDGE_BOTH, update_gpio_switch_funcs[i]);

	return gpio_switch;
}

unsigned int get_gpio_switch_dtus(unsigned int i) {
	if (i >= max_gpio_switches) return 0;
	unsigned int dtus=gpio_switches[i].dtus;
	gpio_switches[i].dtus=0;
	return dtus;
}

unsigned int get_gpio_switch(unsigned int i) {
	return get_gpio_switch_dtus(i);
}

//-----------------------------------------------------------------------------
// Generic Rotary Encoders
//-----------------------------------------------------------------------------

void send_midi_rencoder(unsigned int i) {
	if (i>=max_midi_rencoders) return;
	struct midi_rencoder *midi_rencoder = midi_rencoders + i;
	if (midi_rencoder->enabled==0) return;

	if (rencoder_seq_handle!=NULL && midi_rencoder->midi_ctrl>0) {
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_direct(&ev);
		//snd_seq_ev_set_dest(&ev, 64, 0); /* send to 64:0 */
		snd_seq_ev_set_subs(&ev);        /* send to subscribers of source port */
		snd_seq_ev_set_controller(&ev, midi_rencoder->midi_chan, midi_rencoder->midi_ctrl, midi_rencoder->value);
		snd_seq_event_output_direct(rencoder_seq_handle, &ev);
		//snd_seq_drain_output(rencoder_seq_handle);
		//printf("SEND MIDI CHAN %d, CTRL %d = %d\n",midi_rencoder->midi_chan,midi_rencoder->midi_ctrl,midi_rencoder->value);
	}
	else if (rencoder_lo_addr!=NULL && midi_rencoder->osc_path[0]) {
		lo_send(rencoder_lo_addr,midi_rencoder->osc_path, "i", midi_rencoder->value);
		//printf("SEND OSC %s => %d\n",midi_rencoder->osc_path,midi_rencoder->value);
	}
}

void update_midi_rencoder(unsigned int i) {
	if (i>=max_midi_rencoders) return;
	struct midi_rencoder *midi_rencoder = midi_rencoders + i;
	if (midi_rencoder->enabled==0) return;

	unsigned int MSB = digitalRead(midi_rencoder->pin_a);
	unsigned int LSB = digitalRead(midi_rencoder->pin_b);
	unsigned int encoded = (MSB << 1) | LSB;
	unsigned int sum = (midi_rencoder->last_encoded << 2) | encoded;
	unsigned int last_value=midi_rencoder->value;

	if (midi_rencoder->value>midi_rencoder->max_value) midi_rencoder->value=midi_rencoder->max_value;

	if (midi_rencoder->max_value-midi_rencoder->value>=midi_rencoder->step && (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)) {
		midi_rencoder->value+=midi_rencoder->step;
	}
	else if (midi_rencoder->value>=midi_rencoder->step && (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)) {
		midi_rencoder->value-=midi_rencoder->step;
	}
	midi_rencoder->last_encoded = encoded;

	if (last_value!=midi_rencoder->value) send_midi_rencoder(i);
}

void update_midi_rencoder_0() { update_midi_rencoder(0); }
void update_midi_rencoder_1() { update_midi_rencoder(1); }
void update_midi_rencoder_2() { update_midi_rencoder(2); }
void update_midi_rencoder_3() { update_midi_rencoder(3); }
void update_midi_rencoder_4() { update_midi_rencoder(4); }
void update_midi_rencoder_5() { update_midi_rencoder(5); }
void update_midi_rencoder_6() { update_midi_rencoder(6); }
void update_midi_rencoder_7() { update_midi_rencoder(7); }
void (*update_midi_rencoder_funcs[8])={
	update_midi_rencoder_0,
	update_midi_rencoder_1,
	update_midi_rencoder_2,
	update_midi_rencoder_3,
	update_midi_rencoder_4,
	update_midi_rencoder_5,
	update_midi_rencoder_6,
	update_midi_rencoder_7
};

//-----------------------------------------------------------------------------

struct midi_rencoder *setup_midi_rencoder(unsigned int i, unsigned int pin_a, unsigned int pin_b, unsigned int midi_chan, unsigned int midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step) {
	if (i > max_midi_rencoders) {
		printf("Maximum number of midi rotary encoders exceded: %i\n", max_midi_rencoders);
		return NULL;
	}

	struct midi_rencoder *rencoder = midi_rencoders + i;
	if (midi_chan>15) midi_chan=0;
	if (midi_ctrl>127) midi_ctrl=1;
	if (value>max_value) value=max_value;
	rencoder->midi_chan = midi_chan;
	rencoder->midi_ctrl = midi_ctrl;
	//printf("OSC PATH: %s\n",osc_path);
	if (osc_path) strcpy(rencoder->osc_path,osc_path);
	else rencoder->osc_path[0]=0;
	rencoder->max_value = max_value;
	rencoder->step = step;
	rencoder->value = value;

	if (rencoder->enabled==0 || rencoder->pin_a!=pin_a || rencoder->pin_b!=pin_b) {
		rencoder->enabled = 1;
		rencoder->pin_a = pin_a;
		rencoder->pin_b = pin_b;
		rencoder->last_encoded = 0;

		pinMode(pin_a, INPUT);
		pinMode(pin_b, INPUT);
		pullUpDnControl(pin_a, PUD_UP);
		pullUpDnControl(pin_b, PUD_UP);
		wiringPiISR(pin_a,INT_EDGE_BOTH, update_midi_rencoder_funcs[i]);
		wiringPiISR(pin_b,INT_EDGE_BOTH, update_midi_rencoder_funcs[i]);
	}

	return rencoder;
}

unsigned int get_value_midi_rencoder(unsigned int i) {
	if (i >= max_midi_rencoders) return 0;
	return midi_rencoders[i].value;
}

void set_value_midi_rencoder(unsigned int i, unsigned int v) {
	if (i >= max_midi_rencoders) return;
	if (v>midi_rencoders[i].max_value) v=midi_rencoders[i].max_value;
	midi_rencoders[i].value=v;
	send_midi_rencoder(i);
}

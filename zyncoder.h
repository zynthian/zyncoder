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
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 * 
 * ******************************************************************
 */

//-----------------------------------------------------------------------------
// Library Initialization
//-----------------------------------------------------------------------------

int init_rencoder(int osc_port);
int init_seq_midi_rencoder(int osc_port);
pthread_t init_poll_switches();

//-----------------------------------------------------------------------------
// GPIO Switches
//-----------------------------------------------------------------------------

// The real limit in RPi2 is 17
#define max_gpio_switches 8

struct gpio_switch
{
	unsigned int enabled;
	unsigned int pin;
	volatile double tsus;
	volatile unsigned int dtus;
	volatile unsigned int status;
};
struct gpio_switch gpio_switches[max_gpio_switches];

void update_gpio_switch(unsigned int i);
struct gpio_switch *setup_gpio_switch(unsigned int i, unsigned int pin); 
unsigned int get_gpio_switch(unsigned int i);
unsigned int get_gpio_switch_dtus(unsigned int i);

//-----------------------------------------------------------------------------
// MIDI Rotary Encoders
//-----------------------------------------------------------------------------

// 17 pins / 2 pins per encoder = 8 maximum encoders
#define max_midi_rencoders 8

struct midi_rencoder
{
	unsigned int enabled;
	unsigned int pin_a;
	unsigned int pin_b;
	unsigned int midi_chan;
	unsigned int midi_ctrl;
	char osc_path[512];
	unsigned int max_value;
	unsigned int step;
	volatile unsigned int value;
	volatile unsigned int last_encoded;
};
struct midi_rencoder midi_rencoders[max_midi_rencoders];

void send_seq_midi_reconder(unsigned int i);
void update_midi_rencoder(unsigned int i);
struct midi_rencoder *setup_midi_rencoder(unsigned int i, unsigned int pin_a, unsigned int pin_b, unsigned int midi_chan, unsigned int midi_ctrl, char *osc_path, unsigned int value, unsigned int max_value, unsigned int step); 
unsigned int get_value_midi_rencoder(unsigned int i);
void set_value_midi_rencoder(unsigned int i, unsigned int v);

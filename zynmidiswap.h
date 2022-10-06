/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ZynMidiSwap Library
 * 
 * MIDI CC Swap library: Implements MIDI CC swap
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

typedef struct mf_arrow_st {
	uint8_t chan_from;
	uint8_t num_from;
	uint8_t chan_to;
	uint8_t num_to;
	midi_event_type type;
} mf_arrow_t;

//MIDI CC Filter Swap Mapping
int get_mf_arrow_from(uint8_t chan, uint8_t num, mf_arrow_t *arrow);
int get_mf_arrow_to(uint8_t chan, uint8_t num, mf_arrow_t *arrow);
int set_midi_filter_cc_swap(uint8_t chan_from, uint8_t num_from, uint8_t chan_to, uint8_t num_to);
int del_midi_filter_cc_swap(uint8_t chan, uint8_t num);
uint16_t get_midi_filter_cc_swap(uint8_t chan, uint8_t num);
void reset_midi_filter_cc_swap();

//-----------------------------------------------------------------------------

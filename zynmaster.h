/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Master Output Library
 *
 * Jack client for managing Master Audio & MIDI output
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

#include <jack/jack.h>

//-----------------------------------------------------------------------------
// Jack MIDI Process
//-----------------------------------------------------------------------------

int init_zynmaster_jack();
int end_zynmaster_jack();
int zynmaster_jack_process(jack_nframes_t nframes, void *arg);

//-----------------------------------------------------------------------------

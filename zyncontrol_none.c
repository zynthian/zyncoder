/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Null zyncontrol Library (touch only)
 * 
 * Do nothing!
 * 
 * Copyright (C) 2015-2025 Fernando Moyano <jofemodo@zynthian.org>
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


//-----------------------------------------------------------------------------
// Wrap functions to inform the GUI about not having zynpots nor zynswitches
//-----------------------------------------------------------------------------


int get_num_zynswitches() {
	return 0;
}

int get_last_zynswitch_index() {
	return -1;
}

int get_num_zynpots() {
	return 0;
}

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

int init_zyncontrol() {
	return 1;
}

int end_zyncontrol() {
	return 1;
}

//-----------------------------------------------------------------------------

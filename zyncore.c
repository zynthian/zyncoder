/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zynthian Core Library Initialization
 *
 * Initialize core modules
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

#include "gpiod_callback.h"
#include "zyncontrol.h"
#include "zynmidirouter.h"
#include "zynmaster.h"

//-----------------------------------------------------------------------------

int init_zyncore()
{
	if (!init_zyncontrol())
		return 1;
	if (!init_zynmidirouter())
		return 2;
	if (!init_zynmaster_jack())
		return 3;
	return 0;
}

int init_zyncore_minimal()
{
	// gpiod_init_callbacks();
	return 0;
}

int end_zyncore()
{
	if (!end_zynmaster_jack())
		return 0;
	if (!end_zynmidirouter())
		return 0;
	if (!end_zyncontrol())
		return 0;
	return 1;
}

//-----------------------------------------------------------------------------

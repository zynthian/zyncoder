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

#include "zyncontrol.h"
#include "zynmidirouter.h"
#include "zynmaster.h"

#ifdef ZYNAPTIK_CONFIG
#include "zynaptik.h"
#endif

#ifdef ZYNTOF_CONFIG
#include "zyntof.h"
#endif

//-----------------------------------------------------------------------------

int init_zyncore() {
	if (!init_zyncontrol()) return 0;
	if (!init_zynmidirouter()) return 0;
	#ifdef ZYNAPTIK_CONFIG
	if (!init_zynaptik()) return 0;
	#endif
	#ifdef ZYNTOF_CONFIG
	if (!init_zyntof()) return 0;
	#endif
	if (!init_zynmaster_jack()) return 0;
	return 1;
}

int end_zyncore() {
	if (!end_zynmaster_jack()) return 0;
	#ifdef ZYNTOF_CONFIG
	if (!end_zyntof()) return 0;
	#endif
	#ifdef ZYNAPTIK_CONFIG
	if (!end_zynaptik()) return 0;
	#endif
	if (!end_zynmidirouter()) return 0;
	if (!end_zyncontrol()) return 0;
	return 1;
}

//-----------------------------------------------------------------------------

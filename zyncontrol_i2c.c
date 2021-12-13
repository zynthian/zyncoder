/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncontrol Library for Zynthian Kits V1-V4
 * 
 * Initialize & configure control hardware for Zynthian Kits V1-V4
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

#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "zynpot.h"
#include "zyncoder_i2c.h"

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

#define HWC_ADDR 0x08 // I2C address of riban hardware controller

#if !defined(MCP23017_INTA_PIN)
	#define INTERRUPT_PIN 7
#else
    #define INTERRUPT_PIN MCP23017_INTA_PIN
#endif

int init_zyncontrol() {
	reset_zyncoders();
	wiringPiSetup();
	hwci2c_fd = wiringPiI2CSetup(HWC_ADDR);
	wiringPiI2CWriteReg8(hwci2c_fd, 0, 0); // Reset HWC
	wiringPiISR(INTERRUPT_PIN, INT_EDGE_FALLING, handleRibanHwc);
	return 1;
}

int end_zyncontrol() {
	reset_zyncoders();
	return 1;
}

//-----------------------------------------------------------------------------

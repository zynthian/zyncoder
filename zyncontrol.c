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

#include "zynpot.h"
#include "zyncoder.h"

//-----------------------------------------------------------------------------
// GPIO Expander
//-----------------------------------------------------------------------------

#if defined(HAVE_WIRINGPI_LIB)
	#if defined(MCP23017_ENCODERS)
		// pins 100-115 are located on the MCP23017
		#define MCP23017_BASE_PIN 100
		// define default I2C Address for MCP23017
		#if !defined(MCP23017_I2C_ADDRESS)
			#define MCP23017_I2C_ADDRESS 0x20
		#endif
		// define default interrupt pins for the MCP23017
		#if !defined(MCP23017_INTA_PIN)
			#define MCP23017_INTA_PIN 27
		#endif
		#if !defined(MCP23017_INTB_PIN)
			#define MCP23017_INTB_PIN 25
		#endif

	#elif defined(MCP23008_ENCODERS)
		// pins 100-107 are located on the MCP23008
		#define MCP23008_BASE_PIN 100
		#define MCP23008_I2C_ADDRESS 0x20
	#endif

#else
	#define MCP23008_BASE_PIN 100
	#define MCP23008_I2C_ADDRESS 0x20
#endif


#if defined(MCP23017_ENCODERS)

// WiringPi node struct for direct access to the mcp23017
struct wiringPiNodeStruct *zyncoder_mcp23017_node;

// two ISR routines for the two banks
void zyncoder_mcp23017_bankA_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node, MCP23017_BASE_PIN, 0);
}
void zyncoder_mcp23017_bankB_ISR() {
	zyncoder_mcp23017_ISR(zyncoder_mcp23017_node, MCP23017_BASE_PIN, 1);
}
void (*zyncoder_mcp23017_bank_ISRs[2])={
	zyncoder_mcp23017_bankA_ISR,
	zyncoder_mcp23017_bankB_ISR
};

#endif

//-----------------------------------------------------------------------------
// Zyncontrol Initialization
//-----------------------------------------------------------------------------

int init_zyncontrol() {
	reset_zyncoders();
	wiringPiSetup();
	#if defined(MCP23017_ENCODERS)
	zyncoder_mcp23017_node = init_mcp23017(MCP23017_BASE_PIN, MCP23017_I2C_ADDRESS, MCP23017_INTA_PIN, MCP23017_INTB_PIN, zyncoder_mcp23017_bank_ISRs);
#elif defined(MCP23008_ENCODERS)   
	mcp23008Setup(MCP23008_BASE_PIN, MCP23008_I2C_ADDRESS);
	init_poll_zynswitches();
#endif
	return 1;
}

int end_zyncontrol() {
	reset_zyncoders();
	return 1;
}

//-----------------------------------------------------------------------------

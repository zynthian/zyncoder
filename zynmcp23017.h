/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 * 
 * Library for interfacing MCP23017 using IRQs.
 * 
 * Copyright (C) 2015-2022 Fernando Moyano <jofemodo@zynthian.org>
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

//-----------------------------------------------------------------------------
// MCP23017 stuff
//-----------------------------------------------------------------------------

#define MAX_NUM_MCP23017 4

typedef enum zynmcp23017_pin_action_enum {
	NONE_PIN_ACTION = 0,
	ZYNSWITCH_PIN_ACTION = 1,
	ZYNCODER_PIN_ACTION = 2
} zynmcp23017_pin_action_t;


typedef struct zynmcp23017_st {
	uint8_t enabled;

	uint16_t base_pin;
	uint8_t i2c_address;
	uint8_t intA_pin;
	uint8_t intB_pin;
	struct wiringPiNodeStruct * wpi_node;

	uint16_t last_state;

	zynmcp23017_pin_action_t pin_action[16];
	uint16_t pin_action_num[16];

} zynmcp23017_t;


void reset_zynmcp23017s();
int setup_zynmcp23017(uint8_t i, uint16_t base_pin, uint8_t i2c_address, uint8_t intA_pin, uint8_t intB_pin, void (*isrs[2]));
int get_last_zynmcp23017_index();

int pin2index_zynmcp23017(uint16_t pin);
int setup_pin_action_zynmcp23017(uint16_t pin, zynmcp23017_pin_action_t action, uint16_t num);
int reset_pin_action_zynmcp23017(uint16_t pin);

void zynswitch_update_zynmcp23017(uint8_t i);
void zyncoder_update_zynmcp23017(uint8_t i);

// ISR callback function
void zynmcp23017_ISR(uint8_t i, uint8_t bank);

//-----------------------------------------------------------------------------

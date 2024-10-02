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

#ifndef ZYNMCP23017_H
#define ZYNMCP23017_H

#include <stdint.h>

//-----------------------------------------------------------------------------
// IC registers
//-----------------------------------------------------------------------------

// MCP23x17 Registers
#define	MCP23x17_IODIRA		0x00
#define	MCP23x17_IPOLA		0x02
#define	MCP23x17_GPINTENA	0x04
#define	MCP23x17_DEFVALA	0x06
#define	MCP23x17_INTCONA	0x08
#define	MCP23x17_IOCON		0x0A
#define	MCP23x17_GPPUA		0x0C
#define	MCP23x17_INTFA		0x0E
#define	MCP23x17_INTCAPA	0x10
#define	MCP23x17_GPIOA		0x12
#define	MCP23x17_OLATA		0x14

#define	MCP23x17_IODIRB		0x01
#define	MCP23x17_IPOLB		0x03
#define	MCP23x17_GPINTENB	0x05
#define	MCP23x17_DEFVALB	0x07
#define	MCP23x17_INTCONB	0x09
#define	MCP23x17_IOCONB		0x0B
#define	MCP23x17_GPPUB		0x0D
#define	MCP23x17_INTFB		0x0F
#define	MCP23x17_INTCAPB	0x11
#define	MCP23x17_GPIOB		0x13
#define	MCP23x17_OLATB		0x15

// Bits in the IOCON register
#define	IOCON_UNUSED	0x01
#define	IOCON_INTPOL	0x02
#define	IOCON_ODR		0x04
#define	IOCON_HAEN		0x08
#define	IOCON_DISSLW	0x10
#define	IOCON_SEQOP		0x20
#define	IOCON_MIRROR	0x40
#define	IOCON_BANK_MODE	0x80

// Default initialisation mode
#define	IOCON_INIT	(IOCON_SEQOP)

// SPI Command codes
#define	CMD_WRITE	0x40
#define CMD_READ	0x41

// Pin modes
#define PIN_MODE_OUTPUT	0x0
#define PIN_MODE_INPUT	0x1
#define PIN_PUD_DOWN	0x0
#define PIN_PUD_UP		0x1

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

	int fd;
	uint16_t base_pin;
	uint8_t i2c_address;
	uint8_t intA_pin;
	uint8_t intB_pin;

	uint8_t last_state_A;
	uint8_t last_state_B;
	uint8_t output_state_A;
	uint8_t output_state_B;

	zynmcp23017_pin_action_t pin_action[16];
	uint16_t pin_action_num[16];

} zynmcp23017_t;


void reset_zynmcp23017s();
int setup_zynmcp23017(uint8_t i, uint16_t base_pin, uint8_t i2c_address, uint8_t intA_pin, uint8_t intB_pin, void (*isrs[2]));

int get_last_zynmcp23017_index();
int pin2index_zynmcp23017(uint16_t pin);

int setup_pin_action_zynmcp23017(uint16_t pin, zynmcp23017_pin_action_t action, uint16_t num);
int reset_pin_action_zynmcp23017(uint16_t pin);

int set_pin_mode_zynmcp23017(uint16_t pin, uint8_t mode);
int set_pull_up_down_zynmcp23017(uint16_t pin, uint8_t mode);
int write_pin_zynmcp23017(uint16_t pin, uint8_t val);
int read_pin_zynmcp23017(uint16_t pin);

void zynswitch_update_zynmcp23017(uint8_t i);
void zyncoder_update_zynmcp23017(uint8_t i);

// ISR callback function
void zynmcp23017_ISR(uint8_t i, uint8_t bank);

//-----------------------------------------------------------------------------

#endif
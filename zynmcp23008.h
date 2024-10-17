/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library
 *
 * Library for interfacing MCP23008 using polling (only zynswitches!).
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

// WARNING! It's not possible to use this with zynmcp23017!!

#ifndef ZYNMCP23008_H
#define ZYNMCP23008_H

#include <pthread.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// IC registers
//-----------------------------------------------------------------------------

// MCP23x08 Registers
#define MCP23x08_IODIR 0x00
#define MCP23x08_IPOL 0x01
#define MCP23x08_GPINTEN 0x02
#define MCP23x08_DEFVAL 0x03
#define MCP23x08_INTCON 0x04
#define MCP23x08_IOCON 0x05
#define MCP23x08_GPPU 0x06
#define MCP23x08_INTF 0x07
#define MCP23x08_INTCAP 0x08
#define MCP23x08_GPIO 0x09
#define MCP23x08_OLAT 0x0A

// Bits in the IOCON register
#define IOCON_UNUSED 0x01
#define IOCON_INTPOL 0x02
#define IOCON_ODR 0x04
#define IOCON_HAEN 0x08
#define IOCON_DISSLW 0x10
#define IOCON_SEQOP 0x20
#define IOCON_MIRROR 0x40
#define IOCON_BANK_MODE 0x80

// Default initialisation mode
#define IOCON_INIT (IOCON_SEQOP)

// SPI Command codes
#define CMD_WRITE 0x40
#define CMD_READ 0x41

// Pin modes
#define PIN_MODE_OUTPUT 0x0
#define PIN_MODE_INPUT 0x1
#define PIN_PUD_DOWN 0x0
#define PIN_PUD_UP 0x1

//-----------------------------------------------------------------------------
// MCP23008 stuff
//-----------------------------------------------------------------------------

#define MAX_NUM_MCP23008 4
// Switch Polling interval
#define POLL_ZYNSWITCHES_US 10000

typedef struct zynmcp23008_st {
    uint8_t enabled;
    int fd;
    uint16_t base_pin;
    uint8_t i2c_address;
    uint8_t output_state;
} zynmcp23008_t;

void reset_zynmcp23008s();
int setup_zynmcp23008(uint8_t i, uint16_t base_pin, uint8_t i2c_address);

int8_t zynmcp23008_get_last_index();
int8_t zynmcp23008_pin2index(uint16_t pin);

void zynmcp23008_set_pin_mode(uint8_t i, uint16_t pin, uint8_t mode);
void zynmcp23008_set_pull_up_down(uint8_t i, uint16_t pin, uint8_t mode);
void zynmcp23008_write_pin(uint8_t i, uint16_t pin, uint8_t val);
uint8_t zynmcp23008_read_pin(uint8_t i, uint16_t pin);
uint8_t zynmcp23008_read_pins_(uint8_t i);

// Switches Polling Thread (should be avoided!)
pthread_t init_poll_zynswitches();
void end_poll_zynswitches();

//-----------------------------------------------------------------------------

#endif
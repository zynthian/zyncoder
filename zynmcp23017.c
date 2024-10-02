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

//#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>

#include "gpiod_callback.h"
#include "wiringPiI2C.h"
#include "zynmcp23017.h"
#include "zyncoder.h"

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern zynswitch_t zynswitches[MAX_NUM_ZYNSWITCHES];
extern zyncoder_t zyncoders[MAX_NUM_ZYNCODERS];

zynmcp23017_t zynmcp23017s[MAX_NUM_MCP23017];

//-----------------------------------------------------------------------------
// MCP23017 functions
//-----------------------------------------------------------------------------

void reset_zynmcp23017s() {
	int i;
	for (i=0;i<MAX_NUM_MCP23017;i++) {
		zynmcp23017s[i].fd = 0;
		zynmcp23017s[i].enabled = 0;
	}
}

int setup_zynmcp23017(uint8_t i, uint16_t base_pin, uint8_t i2c_address, uint8_t intA_pin, uint8_t intB_pin, void (*isrs[2])) {
	if (i >= MAX_NUM_MCP23017) {
		fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Invalid index!\n", i);
		return 0;
	}

	// Setup IC using I2C bus
	int fd = wiringPiI2CSetup(i2c_address);
	if (fd < 0) {
		fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't open I2C device at %d!\n", i, i2c_address);
		return 0;
	}
	// Initialize IC
	wiringPiI2CWriteReg8(fd, MCP23x17_IOCON, IOCON_INIT);
	uint8_t olata = wiringPiI2CReadReg8 (fd, MCP23x17_OLATA);
	uint8_t olatb = wiringPiI2CReadReg8 (fd, MCP23x17_OLATB);

	// setup all the pins on the banks as inputs and disable pullups on
	// the zyncoder input
	uint8_t reg = 0xff;
	wiringPiI2CWriteReg8(fd, MCP23x17_IODIRA, reg);
	wiringPiI2CWriteReg8(fd, MCP23x17_IODIRB, reg);

	// enable pullups on the unused pins (high two bits on each bank)
	reg = 0xff;
	//reg = 0xc0;
	//reg = 0x60;
	wiringPiI2CWriteReg8(fd, MCP23x17_GPPUA, reg);
	wiringPiI2CWriteReg8(fd, MCP23x17_GPPUB, reg);

	// disable polarity inversion
	reg = 0;
	wiringPiI2CWriteReg8(fd, MCP23x17_IPOLA, reg);
	wiringPiI2CWriteReg8(fd, MCP23x17_IPOLB, reg);

	// disable the comparison to DEFVAL register
	reg = 0;
	wiringPiI2CWriteReg8(fd, MCP23x17_INTCONA, reg);
	wiringPiI2CWriteReg8(fd, MCP23x17_INTCONB, reg);

	// configure the interrupt behavior for bank A
	uint8_t ioconf_value = wiringPiI2CReadReg8(fd, MCP23x17_IOCON);
	bitWrite(ioconf_value, 6, 0);	// banks are not mirrored
	bitWrite(ioconf_value, 2, 0);	// interrupt pin is not floating
	bitWrite(ioconf_value, 1, 1);	// interrupt is signaled by high
	wiringPiI2CWriteReg8(fd, MCP23x17_IOCON, ioconf_value);

	// configure the interrupt behavior for bank B
	ioconf_value = wiringPiI2CReadReg8(fd, MCP23x17_IOCONB);
	bitWrite(ioconf_value, 6, 0);	// banks are not mirrored
	bitWrite(ioconf_value, 2, 0);	// interrupt pin is not floating
	bitWrite(ioconf_value, 1, 1);	// interrupt is signaled by high
	wiringPiI2CWriteReg8(fd, MCP23x17_IOCONB, ioconf_value);

	// finally, enable the interrupt pins for banks a and b
	// enable interrupts on all pins
	reg = 0xff;
	wiringPiI2CWriteReg8(fd, MCP23x17_GPINTENA, reg);
	wiringPiI2CWriteReg8(fd, MCP23x17_GPINTENB, reg);

	// Setup data struct
	zynmcp23017s[i].fd = fd;
	zynmcp23017s[i].base_pin = base_pin;
	zynmcp23017s[i].i2c_address = i2c_address;
	zynmcp23017s[i].output_state_A = olata;
	zynmcp23017s[i].output_state_B = olatb;
	zynmcp23017s[i].intA_pin = intA_pin;
	zynmcp23017s[i].intB_pin = intB_pin;
	zynmcp23017s[i].last_state_A = wiringPiI2CReadReg8(fd, MCP23x17_GPIOA);
	zynmcp23017s[i].last_state_B = wiringPiI2CReadReg8(fd, MCP23x17_GPIOB);
	int j;
	for (j=0; j<16; j++) {
		zynmcp23017s[i].pin_action[j] = NONE_PIN_ACTION;
		zynmcp23017s[i].pin_action_num[j] = 0;
	}
	zynmcp23017s[i].enabled = 1;

	// Setup callbacks for the interrupt pins
	struct gpiod_line *line_a = gpiod_chip_get_line(gpio_chip, intA_pin);
	if (line_a) {
		if (gpiod_line_request_rising_edge_events_flags(line_a, ZYNCORE_CONSUMER, 0) < 0) {
			fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't request line for INTA pin %d\n", i, intA_pin);
			return 0;
		}
		if (gpiod_line_register_callback(line_a, isrs[0]) < 0) {
			fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't register callback for INTA pin %d\n", i, intA_pin);
			return 0;
		}
	} else {
		fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't get line for INTA pin %d\n", i, intA_pin);
		return 0;
	}
	struct gpiod_line *line_b = gpiod_chip_get_line(gpio_chip, intB_pin);
	if (line_b) {
		if (gpiod_line_request_rising_edge_events_flags(line_b, ZYNCORE_CONSUMER, 0) < 0) {
			fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't request line for INTB pin %d\n", i, intB_pin);
			return 0;
		}
		if (gpiod_line_register_callback(line_b, isrs[1]) < 0) {
			fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't register callback for INTB pin %d\n", i, intB_pin);
			return 0;
		}
	} else {
		fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): Can't get line for INTB pin %d\n", i, intB_pin);
		return 0;
	}
	//fprintf(stderr, "ZynCore->setup_zynmcp23017(%d, ...): I2C %x, base-pin %d, INTA %d, INTB %d\n", i, i2c_address, base_pin, intA_pin, intB_pin);

	return 1;
}

int get_last_zynmcp23017_index() {
	int i;
	int li = 0;
	for (i=0;i<MAX_NUM_MCP23017;i++) {
		if (zynmcp23017s[i].enabled!=0) li = i;
	}
	return li;
}

int pin2index_zynmcp23017(uint16_t pin) {
	int i;
	for (i=0;i<MAX_NUM_MCP23017;i++) {
		if (zynmcp23017s[i].enabled) {
			if (pin >= zynmcp23017s[i].base_pin && pin < (zynmcp23017s[i].base_pin+16)) return i;
		}
	}
	return -1;
}

int setup_pin_action_zynmcp23017(uint16_t pin, zynmcp23017_pin_action_t action, uint16_t num) {
	int i = pin2index_zynmcp23017(pin);
	if (i < 0) {
		fprintf(stderr, "ZynCore->setup_pin_action_zynmcp23017(%d, ...): Not a MCP23017 pin!\n", pin);
		return 0;
	}
	int j = pin - zynmcp23017s[i].base_pin;
	if (j>=0 && j<16) {
		zynmcp23017s[i].pin_action[j] = action;
		zynmcp23017s[i].pin_action_num[j] = num;
		//fprintf(stderr, "ZynCore->setup_pin_action_zynmcp23017(%d, %d, %d)\n", pin, action, num);
	}
	else {
		fprintf(stderr, "ZynCore->setup_pin_action_zynmcp23017(%d, ...): Pin out of range!\n", pin);
		return 0;
	}
	return 1;
}

int reset_pin_action_zynmcp23017(uint16_t pin) {
	int i = pin2index_zynmcp23017(pin);
	if (i < 0) {
		fprintf(stderr, "ZynCore->reset_pin_action_zynmcp23017(%d): Not a MCP23017 pin!\n", pin);
		return 0;
	}
	int j = pin - zynmcp23017s[i].base_pin;
	if (j>=0 && j<16) {
		zynmcp23017s[i].pin_action[j] = NONE_PIN_ACTION;
		zynmcp23017s[i].pin_action_num[j] = 0;
	}
	else {
		fprintf(stderr, "ZynCore->reset_pin_action_zynmcp23017(%d, ...): Pin out of range!\n", pin);
		return 0;
	}
	return 1;
}

/*
 * set_pin_mode_zynmcp23017:
 *  pin : pin number
 *  mode : PIN_MODE_INPUT=1, PIN_MODE_OUTPUT=0
 */
int set_pin_mode_zynmcp23017(uint16_t pin, uint8_t mode) {
	int i = pin2index_zynmcp23017(pin);
	if (i >= 0) {
		int mask, old, reg;
		pin -= zynmcp23017s[i].base_pin;	// Pin now 0-15
		// Bank A
		if (pin < 8) reg = MCP23x17_IODIRA;
		// Bank B
		else {
			reg = MCP23x17_IODIRB;
			pin &= 0x07;
		}
		mask = 1 << pin ;
		old = wiringPiI2CReadReg8(zynmcp23017s[i].fd, reg);
		if (mode == PIN_MODE_OUTPUT) old &= (~mask);
		else old |= mask;
		wiringPiI2CWriteReg8 (zynmcp23017s[i].fd, reg, old) ;
		return 0;
	}
	fprintf(stderr, "ZynCore: set_pin_mode_zynmcp23017(%d) => invalid pin!\n", pin);
	return -1;
}

/*
 * set_pull_up_down_zynmcp23017:
 *  pin : pin number
 *  mode : PIN_PUD_UP=1, PIN_PUD_DOWN=0
 */
int set_pull_up_down_zynmcp23017(uint16_t pin, uint8_t mode) {
	int i = pin2index_zynmcp23017(pin);
	if (i >= 0) {
		int mask, old, reg ;
		pin -= zynmcp23017s[i].base_pin;	// Pin now 0-15
		// Bank A
		if (pin < 8) reg = MCP23x17_GPPUA;
		// Bank B
		else {
			reg = MCP23x17_GPPUA;
			pin &= 0x07;
		}
		mask = 1 << pin;
		old = wiringPiI2CReadReg8(zynmcp23017s[i].fd, reg);
		if (mode == PIN_PUD_DOWN) old &= (~mask);
		else old |= mask;
		wiringPiI2CWriteReg8 (zynmcp23017s[i].fd, reg, old);
		return 0;
	}
	fprintf(stderr, "ZynCore: set_pull_up_down_zynmcp23017(%d) => invalid pin!\n", pin);
	return -1;
}

/*
 * write_pin_zynmcp23017:
 *  pin : pin number
 *  val : value to write (1/0)
 */
int write_pin_zynmcp23017(uint16_t pin, uint8_t val) {
	int i = pin2index_zynmcp23017(pin);
	if (i>=0) {
		int bit, old ;
  		pin -= zynmcp23017s[i].base_pin ;	// Pin now 0-15
		bit = 1 << (pin & 7) ;
		// Bank A
		if (pin < 8) {
			old = zynmcp23017s[i].output_state_A;
			if (val == 0) old &= (~bit);
			else old |= bit;
			wiringPiI2CWriteReg8(zynmcp23017s[i].fd, MCP23x17_GPIOA, old);
			zynmcp23017s[i].output_state_A = old;
		}
		// Bank B
		else {
			old = zynmcp23017s[i].output_state_B;
			if (val == 0) old &= (~bit);
			else old |= bit;
			wiringPiI2CWriteReg8(zynmcp23017s[i].fd, MCP23x17_GPIOB, old);
			zynmcp23017s[i].output_state_B = old;
		}
		return 0;
	}
	fprintf(stderr, "ZynCore: write_pin_zynmcp23017(%d) => invalid pin!\n", pin);
	return -1;
}

int read_pin_zynmcp23017(uint16_t pin) {
	int i = pin2index_zynmcp23017(pin);
	if (i>=0) {
		uint8_t bit = pin - zynmcp23017s[i].base_pin;
		uint16_t reg;
		// Bank A
		if (bit<8) {
			reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_GPIOA);
			zynmcp23017s[i].last_state_A = reg;
			return bitRead(reg, bit);
		// Bank B
		} else if (bit<16) {
			reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_GPIOB);
			zynmcp23017s[i].last_state_B = reg;
			return bitRead(reg, (bit - 8));
		} else {
			fprintf(stderr, "ZynCore: read_pin_zynmcp23017(%d) => pin %d out of range!\n", pin, pin);
			return -1;
		}
	}
	fprintf(stderr, "ZynCore: read_pin_zynmcp23017(%d) => invalid pin!\n", pin);
	return -1;
}

void zynswitch_update_zynmcp23017(uint8_t i) {
	if (i>=MAX_NUM_ZYNSWITCHES) return;
	zynswitch_t *zsw = zynswitches + i;
	if (zsw->enabled==0) return;

	int res = read_pin_zynmcp23017(zsw->pin);
	if (res>=0) update_zynswitch(i, (uint8_t)res);
}

void zyncoder_update_zynmcp23017(uint8_t i) {
	if (i>=MAX_NUM_ZYNCODERS) return;
	zyncoder_t *zcdr = zyncoders + i;
	if (zcdr->enabled==0) return;

	int res_a = read_pin_zynmcp23017(zcdr->pin_a);
	int res_b = read_pin_zynmcp23017(zcdr->pin_b);
	if (res_a >=0 && res_b >=0) update_zyncoder(i, (uint8_t)res_a, (uint8_t)res_b);
}


// ISR for handling the mcp23017 interrupts
void zynmcp23017_ISR(uint8_t i, uint8_t bank) {
	if (i >= MAX_NUM_MCP23017) {
		fprintf(stderr, "ZynCore->zynmcp23017_ISR(%d, %d): Invalid index!\n", i, bank);
		return;
	}
	if (!zynmcp23017s[i].enabled) return;

	#ifdef DEBUG
	fprintf(stderr, "zyncoder_mcp23017_ISR(%d, %d)\n", i, bank);
	#endif

	uint16_t pin_offset;
	uint16_t reg;
	uint8_t rdiff;

	if (bank == 0) {
		pin_offset = 0;
		reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_GPIOA);
		//reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_INTCAPA);
		rdiff = reg ^ zynmcp23017s[i].last_state_A;
		zynmcp23017s[i].last_state_A = reg;
	} else if (bank == 1) {
		pin_offset = 8;
		reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_GPIOB);
		//reg = wiringPiI2CReadReg8(zynmcp23017s[i].fd, MCP23x17_INTCAPB);
		rdiff = reg ^ zynmcp23017s[i].last_state_B;
		zynmcp23017s[i].last_state_B = reg;
	} else {
		fprintf(stderr, "ZynCore->zynmcp23017_ISR(%d, %d): Invalid bank!\n", i, bank);
		return;
	}
	
	uint8_t j = 0;
	uint8_t k, bit_a, bit_b;
	while (rdiff != 0) {
		if (rdiff & 0x01) {
			//fprintf(stderr, "zyncoder_mcp23017_ISR(%d, %d) => pin %d changed, action %d\n", i, bank, j, zynmcp23017s[i].pin_action[j]);
			switch(zynmcp23017s[i].pin_action[j + pin_offset]) {
				case ZYNSWITCH_PIN_ACTION:
					k = zynmcp23017s[i].pin_action_num[j + pin_offset];
					zynswitch_t *zsw = zynswitches + k;
					bit_a = zsw->pin - (zynmcp23017s[i].base_pin + pin_offset);
					update_zynswitch(k, bitRead(reg, bit_a));
					break;
				case ZYNCODER_PIN_ACTION:
					k = zynmcp23017s[i].pin_action_num[j + pin_offset];
					zyncoder_t *zcdr = zyncoders + k;
					bit_a = zcdr->pin_a - (zynmcp23017s[i].base_pin + pin_offset);
					bit_b = zcdr->pin_b - (zynmcp23017s[i].base_pin + pin_offset);
					update_zyncoder(k, bitRead(reg, bit_a), bitRead(reg, bit_b));
					// Avoid processing same zyncoder twice
					rdiff &= ~ ((0x1 << bit_a) | (0x1 << bit_b)) >> j;
					break;
			}
		}
		rdiff >>= 1;
		j++;
	}
}

//-----------------------------------------------------------------------------

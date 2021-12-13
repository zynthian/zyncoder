
/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ADS1115 library
 * 
 * Library for managing ADS1115 ADCs.
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

#include <stdio.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <ads1115.h>

#include "zynads1115.h"

//-----------------------------------------------------------------------------

struct wiringPiNodeStruct * init_ads1115(uint16_t base_pin, uint16_t i2c_address, uint8_t gain, uint8_t rate) {
	ads1115Setup(base_pin, i2c_address);
	digitalWrite(base_pin, gain);
	digitalWrite(base_pin + 1, rate);
	return wiringPiFindNode(base_pin);
}

void set_ads1115_gain(uint16_t base_pin, uint8_t gain) {
	digitalWrite(base_pin, gain);
}

void set_ads1115_rate(uint16_t base_pin, uint8_t rate) {
	digitalWrite(base_pin + 1, rate);
}

//-----------------------------------------------------------------------------

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

#include <stdint.h>
#include <wiringPi.h>

//-----------------------------------------------------------------------------

#define ADS1115_GAIN_VREF_6_144 0
#define ADS1115_GAIN_VREF_4_096 1
#define ADS1115_GAIN_VREF_2_048 2
#define ADS1115_GAIN_VREF_1_024 3
#define ADS1115_GAIN_VREF_0_512 4
#define ADS1115_GAIN_VREF_0_256 5

#define ADS1115_RATE_8SPS 0
#define ADS1115_RATE_16SPS 1
#define ADS1115_RATE_32SPS 2
#define ADS1115_RATE_64SPS 3
#define ADS1115_RATE_128SPS 4
#define ADS1115_RATE_475SPS 5
#define ADS1115_RATE_860SPS 6

//-----------------------------------------------------------------------------

struct wiringPiNodeStruct * init_ads1115(uint16_t base_pin, uint16_t i2c_address, uint8_t gain, uint8_t rate);
void set_ads1115_gain(uint16_t base_pin, uint8_t gain);
void set_ads1115_rate(uint16_t base_pin, uint8_t rate);

//-----------------------------------------------------------------------------


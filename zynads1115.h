/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ads1115 low level access
 *
 * Implements the low level code to interface ADS1115 I2C 16-bit ADC
 *
 * Copyright (C) 2021-2024 Fernando Moyano <jofemodo@zynthian.org>
 * Copyright (C) 2016 Gordon Henderson
 * This code inherits from the venerable but currently
 * unmaintained wiringPi library, by Gordon Henderson
 * Thanks for your great work, Gordon!
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

#ifndef ZYNADS1115_H
#define ZYNADS1115_H

#include <stdint.h>

//-----------------------------------------------------------------------------

//	Gain
#define ADS1115_GAIN_6 0
#define ADS1115_GAIN_4 1
#define ADS1115_GAIN_2 2
#define ADS1115_GAIN_1 3
#define ADS1115_GAIN_HALF 4
#define ADS1115_GAIN_QUARTER 5

#define ADS1115_GAIN_VREF_6_144 0
#define ADS1115_GAIN_VREF_4_096 1
#define ADS1115_GAIN_VREF_2_048 2
#define ADS1115_GAIN_VREF_1_024 3
#define ADS1115_GAIN_VREF_0_512 4
#define ADS1115_GAIN_VREF_0_256 5

//	Data rate
#define ADS1115_DR_8 0
#define ADS1115_DR_16 1
#define ADS1115_DR_32 2
#define ADS1115_DR_64 3
#define ADS1115_DR_128 4
#define ADS1115_DR_250 5
#define ADS1115_DR_475 6
#define ADS1115_DR_860 7

#define ADS1115_RATE_8SPS 0
#define ADS1115_RATE_16SPS 1
#define ADS1115_RATE_32SPS 2
#define ADS1115_RATE_64SPS 3
#define ADS1115_RATE_128SPS 4
#define ADS1115_RATE_250SPS 5
#define ADS1115_RATE_475SPS 6
#define ADS1115_RATE_860SPS 7

#ifdef __cplusplus
extern "C"
{
#endif

	//-----------------------------------------------------------------------------

	typedef struct ads1115_st
	{
		uint16_t i2c_address;
		int fd;
		uint16_t gain;
		uint16_t rate;
		uint16_t base_config;
		uint32_t read_wait_us;
	} ads1115_t;

	//-----------------------------------------------------------------------------

	int init_ads1115(ads1115_t *ads1115, uint16_t i2c_address, uint8_t gain, uint8_t rate);
	void ads1115_set_gain(ads1115_t *ads1115, uint8_t gain);
	void ads1115_set_rate(ads1115_t *ads1115, uint8_t rate);
	void ads1115_set_comparator_threshold(ads1115_t *ads1115, uint8_t chan, int16_t data);
	int16_t ads1115_analog_read(ads1115_t *ads1115, uint8_t chan);
	void delay_microseconds(unsigned int howLong);

#ifdef __cplusplus
}
#endif

//-----------------------------------------------------------------------------

#endif
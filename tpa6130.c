/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Headphones Volume Control for TPA6130 amplifier
 *
 * Library for interfacing the TPA6130 headphones amplifier.
 * It implements the volume control using I2C
 *
 * Copyright (C) 2015-2023 Fernando Moyano <jofemodo@zynthian.org>
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

#include "wiringPiI2C.h"
#include "tpa6130.h"

//-------------------------------------------------------------------

// #define DEBUG

#define TPA6130_I2C_ADDRESS 0x60
#define AMP_MAX_VOL 0x3F // 0-63

int tpa6130_fd = 0x0;

//-------------------------------------------------------------------

uint8_t tpa6130_set_volume(uint8_t vol)
{
	wiringPiI2CWriteReg8(tpa6130_fd, 0x2, AMP_MAX_VOL & vol);
	return vol;
}

uint8_t tpa6130_get_volume()
{
	return wiringPiI2CReadReg8(tpa6130_fd, 0x2) & 0x3C;
}

uint8_t tpa6130_get_volume_max()
{
	return AMP_MAX_VOL;
}

void tpa6130_init()
{
	tpa6130_fd = wiringPiI2CSetup(TPA6130_I2C_ADDRESS);
	wiringPiI2CWriteReg8(tpa6130_fd, 0x1, 0xC0);
	tpa6130_set_volume(20);
}

void tpa6130_end()
{
	wiringPiI2CWriteReg8(tpa6130_fd, 0x1, 0x00);
}

//-------------------------------------------------------------------

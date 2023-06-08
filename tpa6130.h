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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

//-------------------------------------------------------------------

uint8_t tpa6130_set_volume(uint8_t vol);
uint8_t tpa6130_get_volume();
uint8_t tpa6130_get_volume_max();
void tpa6130_init();
void tpa6130_end();

//-------------------------------------------------------------------

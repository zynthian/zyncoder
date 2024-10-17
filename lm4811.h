/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Headphones Volume Control for LM4811 amplifier
 *
 * Library for interfacing the LM4811 headphones amplifier.
 * It implements the volume control using 2 GPIO pins:
 *   - AMP_VOL => 1=Up / 0=Down
 *   - AMP_CLK => Volume step control
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//-------------------------------------------------------------------

void lm4811_volume_steps(int n);
void lm4811_reset_volume();
uint8_t lm4811_set_volume(uint8_t vol);
uint8_t lm4811_get_volume();
uint8_t lm4811_get_volume_max();
int lm4811_init();
int lm4811_end();

//-------------------------------------------------------------------

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Headphones Volume Control for LM4811 amplifier
 *
 * Basic CLI for setting volume
 * Usage:
 *   lm4811_set_volume [0-15]
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

#include "lm4811.h"

//-------------------------------------------------------------------

int main(int argc, char* argv[]) {
    uint8_t vol = 10;
    if (argc >= 2) {
        vol = atoi(argv[1]);
    }
    lm4811_init();
    printf("Setting LM4811 volume to %d\n", vol);
    lm4811_set_volume(vol);
    // lm4811_end();
}

//-------------------------------------------------------------------

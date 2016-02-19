/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library Tests
 * 
 * Library for interfacing Rotary Encoders & Switches connected 
 * to RBPi native GPIOs or expanded with MCP23008. Includes an 
 * emulator mode to ease developping.
 * 
 * Copyright (C) 2015-2016 Fernando Moyano <jofemodo@zynthian.org>
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
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 * 
 * ******************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "zyncoder.h"

int main() {
	printf("INITIALIZING ZYNCODER!\n");

	init_rencoder(6699);

	printf("SETTING UP SWITCHES!\n");

	setup_gpio_switch(0,3);
	setup_gpio_switch(0,4);

	printf("SETTING UP RENCODERS!\n");

	setup_midi_rencoder(0,25,27,0,1,NULL,90,127,1);
	setup_midi_rencoder(1,26,21,0,10,NULL,90,127,1);
	setup_midi_rencoder(2,4,3,0,71,NULL,90,127,1);
	setup_midi_rencoder(3,0,7,0,74,NULL,90,127,1);

	printf("TESTING ...\n");

	while(1) {
		/*
		int i;
		for (i=0;i<100;i++) {
		update_midi_rencoder(0);
		update_midi_rencoder(1);
		update_midi_rencoder(2);
		update_midi_rencoder(3);
		usleep(5000);
		}
		*/
		printf("SW0 = %d\n", get_gpio_switch(0));
		printf("SW1 = %d\n", get_gpio_switch(1));
		printf("ZC0 = %d\n", get_value_midi_rencoder(0));
		printf("ZC1 = %d\n", get_value_midi_rencoder(1));
		printf("ZC2 = %d\n", get_value_midi_rencoder(2));
		printf("ZC3 = %d\n", get_value_midi_rencoder(3));
		usleep(500000);
	}

	return 0;
}


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
 * For a full copy of the GNU General Public License see the LICENSE.txt file.
 * 
 * ******************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "zyncoder.h"

#ifndef HAVE_WIRINGPI_LIB
unsigned int zyncoder_pin_a[4]={4,5,6,7};
unsigned int zyncoder_pin_b[4]={8,9,10,11};
unsigned int zynswitch_pin[4]={0,1,2,3};
#else

#ifdef MCP23017_ENCODERS
unsigned int zyncoder_pin_a[4] = { 102, 105, 110, 113 };
unsigned int zyncoder_pin_b[4] = { 101, 104, 109, 112 };
unsigned int zynswitch_pin[4]  = { 100, 103, 108, 111 };
#else
//PROTOTYPE-3
//unsigned int zyncoder_pin_a[4]={27,21,3,7};
//unsigned int zyncoder_pin_b[4]={25,26,4,0};
//PROTOTYPE-4
unsigned int zyncoder_pin_a[4]={26,25,0,4};
unsigned int zyncoder_pin_b[4]={21,27,7,3};
unsigned int zynswitch_pin[4]={107,23,106,2};
#endif

#endif

int main() {
	int i;

	printf("INITIALIZING ZYNCODER LIBRARY!\n");
	init_zynlib();

	printf("SETTING UP ZYNSWITCHES!\n");
	for (i=0;i<4;i++) {
		setup_zynswitch(i,zynswitch_pin[i]);
	}

	printf("SETTING UP ZYNCODERS!\n");
	for (i=0;i<4;i++) {
		setup_zyncoder(i,zyncoder_pin_a[i],zyncoder_pin_b[i],0,70+i,NULL,64,127,1);
	}

	printf("TESTING ...\n");
	while(1) {
		for (i=0;i<4;i++) {
			printf("SW%d = %d\n", i, get_zynswitch(i,2000000));
			printf("ZC%d = %d\n", i, get_value_zyncoder(i));
		}
		printf("-----------------------\n");
		usleep(500000);
	}

	return 0;
}

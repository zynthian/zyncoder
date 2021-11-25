/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zyncoder Library Test
 * 
 * Test switches & encoders for zynthian kit configurations
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
#include <stdint.h>
#include <unistd.h>

#include "zynpot.h"
#include "zyncoder.h"
#include "zyncontrol.h"

//-----------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------

int main() {
	int i;

	printf("Starting ZynCore...\n");
	init_zyncontrol();
	init_zynmidirouter();

	#ifdef DEBUG
	if (zynpots[0].type==ZYNPOT_RV112) {
		fprintf(stdout, "Range 25 = %d\n", RV112_ADS1115_RANGE_25);
		fprintf(stdout, "Range 50 = %d\n", RV112_ADS1115_RANGE_50);
		fprintf(stdout, "Range 75 = %d\n", RV112_ADS1115_RANGE_75);
		fprintf(stdout, "Range 100 = %d\n", RV112_ADS1115_RANGE_100);
	}
	#endif	

	int num_zynswitches = get_num_zynswitches();
	int num_zynpots = get_num_zynpots();
	
	printf("Testing switches & rotaries...\n");
	while(1) {
		for (i=0;i<num_zynswitches;i++) {
			int ts = get_zynswitch(i,2000000);
			if (ts>0) fprintf(stdout, "SW-%d = %d\n", i, ts);
		}
		for (i=0;i<num_zynpots;i++) {
			if (get_value_flag_zynpot(i)) {
				printf("PT-%d = %d\n", i, get_value_zynpot(i));
			}
		}
		usleep(5000);
	}

	return 0;
}

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: WiringPi Emulation Library
 * 
 * Emulates WiringPi library using POSIX RT signals
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
#include <signal.h>
#include <unistd.h>

#include "wiringPiEmu.h"

//-------------------------------------------------------------------
// GPIO Emulation using RT POSIX signals
//-------------------------------------------------------------------

#define GPIO_MAX 15

//GPIO Emulation Data Structure
struct gpio_pin {
	int pin;
	int pinmode;
	int pullUpDnCtr;
	int isrmode;
	void (*isrfunc)(void);
	volatile unsigned int status;
};
struct gpio_pin gpio[GPIO_MAX];

//POSIX RT Signal Handling
void signal_handler(int signo) {
	if (signo>=SIGRTMIN && signo<=SIGRTMAX) {
		int pin=(signo-SIGRTMIN);
		int val=pin%2;
		pin=pin>>1;
		gpio[pin].status=val;
		if (gpio[pin].isrfunc) gpio[pin].isrfunc();
		//printf("INFO WiringPiEmu: Received GPIO %d => %d\n",pin,val);
	}
}

//-------------------------------------------------------------------
// WiringPi Library Emulation
//-------------------------------------------------------------------

int wiringPiSetup(void) {
	int i,signo;
	for (i=0;i<2*GPIO_MAX;i++) {
		signo=SIGRTMIN+i;
		//Reset GPIO Data Structures
		gpio[i].pin=i;
		gpio[i].pinmode=INPUT;
		gpio[i].pullUpDnCtr=PUD_OFF;
		gpio[i].isrmode=INT_EDGE_SETUP;
		gpio[i].isrfunc=NULL;
		gpio[i].status=0;
		//Setup Signal Catching for GPIO Emulation
		if (signal(signo,signal_handler)==SIG_ERR) {
			printf("ERROR WiringPiEmu: Can't catch signal %d\n",signo);
		}
	}
	return 1;
}

int mcp23008Setup(int pin_offset, int addr_base) {
	return 1;
}

void pinMode(int pin, int mode) {
	if (pin>=GPIO_MAX) {
		printf("ERROR WiringPiEmu: pin number (%d) is out of range\n",pin);
		return;
	}
	gpio[pin].pinmode=mode;
}

void pullUpDnControl(int pin, int pud) {
	if (pin>=GPIO_MAX) {
		printf("ERROR WiringPiEmu: pin number (%d) is out of range\n",pin);
		return;
	}
	gpio[pin].pullUpDnCtr=pud;
	if (pud==PUD_UP) gpio[pin].status=1;
	else gpio[pin].status=0;
}

void digitalWrite(int pin, int value) {
	if (pin>=GPIO_MAX) {
		printf("ERROR WiringPiEmu: pin number (%d) is out of range\n",pin);
		return;
	}
	//if (gpio_status[pin].pinmode==OUTPUT)
	gpio[pin].status=value;
}

int digitalRead(int pin) {
	if (pin>=GPIO_MAX) {
		printf("ERROR WiringPiEmu: pin number (%d) is out of range\n",pin);
		return 0;
	}
	return gpio[pin].status;
}

int wiringPiISR(int pin, int mode, void (*function)(void)) {
	if (pin>=GPIO_MAX) {
		printf("ERROR WiringPiEmu: pin number (%d) is out of range\n",pin);
		return 0;
	}
	gpio[pin].isrmode=mode;
	gpio[pin].isrfunc=function;
	return 1;
}

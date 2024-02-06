/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: libgpiod callbacks
 *
 * Implements a callback mechanism for libgpiod
 *
 * Copyright (C) 2015-2024 Fernando Moyano <jofemodo@zynthian.org>
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

#ifndef GPIOD_CALLBACK_H
#define GPIOD_CALLBACK_H

#include <gpiod.h>

#define	NUM_RPI_PINS		28
#define RPI_CHIP_NAME "gpiochip0"
#define ZYNCORE_CONSUMER "zyncore"

// Callback data structure
struct gpiod_callback {
	int pin;
	struct gpiod_line *line;
	void (*callback)(void);
};

extern struct gpiod_chip *rpi_chip;

// -------------------------------------------------------------------

int gpiod_init_callbacks();
int gpiod_line_register_callback(struct gpiod_line *line, void (*callback)(void));
int gpiod_line_unregister_callback(struct gpiod_line *line);
int gpiod_start_callbacks();
int gpiod_stop_callbacks();
int gpiod_restart_callbacks();

// -------------------------------------------------------------------

#endif

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: RV112 + ADS1115
 *
 * Library for reading RV112 infinite potentiometer using ADS1115.
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

#include <boost/circular_buffer.hpp>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "zynads1115.h"
#include "zynpot.h"
#include "zynrv112.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

rv112_t rv112s[MAX_NUM_RV112];

//-----------------------------------------------------------------------------

extern void (*zynpot_cb)(int8_t, int32_t);

//-----------------------------------------------------------------------------
// RV112's zynpot API
//-----------------------------------------------------------------------------

void init_rv112s() {
    int i, j;
    boost::circular_buffer<int32_t>* dvbuf;
    for (i = 0; i < MAX_NUM_RV112; i++) {
        rv112s[i].enabled = 0;
        rv112s[i].value   = 0;
        rv112s[i].zpot_i  = -1;
        rv112s[i].lastdv  = 0;
        rv112s[i].valraw  = 0;
        rv112s[i].dvavg   = 0;
        rv112s[i].dvbuf = dvbuf = new boost::circular_buffer<int32_t>(DVBUF_SIZE);
        for (j = 0; j < DVBUF_SIZE; j++)
            dvbuf->push_back(0);
    }
}

void end_rv112s() {
    int i;
    for (i = 0; i < MAX_NUM_RV112; i++) {
        rv112s[i].enabled = 0;
        rv112s[i].value   = 0;
        rv112s[i].zpot_i  = -1;
        rv112s[i].lastdv  = 0;
        rv112s[i].valraw  = 0;
        rv112s[i].dvavg   = 0;
        delete (boost::circular_buffer<int32_t>*)rv112s[i].dvbuf;
        rv112s[i].dvbuf = NULL;
    }
}

int get_num_rv112s() {
    int i;
    int n = 0;
    for (i = 0; i < MAX_NUM_RV112; i++) {
        if (rv112s[i].enabled != 0)
            n++;
    }
    return n;
}

int setup_rv112(uint8_t i, ads1115_t* ads1115, uint8_t reversed_chans) {
    if (i > MAX_NUM_RV112) {
        fprintf(stderr, "ZynCore->setup_rv112(%d): Invalid index!\n", i);
        return 0;
    }
    uint8_t pos            = (i % 2) * 2;
    rv112s[i].ads1115_node = ads1115;
    if (reversed_chans == 0) {
        rv112s[i].chA = pos + 1;
        rv112s[i].chB = pos;
    } else {
        rv112s[i].chA = pos;
        rv112s[i].chB = pos + 1;
    }
    rv112s[i].valA    = ads1115_analog_read(ads1115, rv112s[i].chA);
    rv112s[i].valB    = ads1115_analog_read(ads1115, rv112s[i].chB);
    rv112s[i].curseg  = 0;
    rv112s[i].lastdv  = 0;
    rv112s[i].value   = 0;
    rv112s[i].step    = 1;
    rv112s[i].valraw  = 0;
    rv112s[i].enabled = 1;
    return 1;
}

int setup_behaviour_rv112(uint8_t i, int32_t step) {
    if (i >= MAX_NUM_RV112 || rv112s[i].enabled == 0) {
        fprintf(stderr, "ZynCore->setup_step_rv112(%d, ...): Invalid index!\n", i);
        return 0;
    }

    rv112s[i].step   = step;
    rv112s[i].valraw = 0;
    rv112s[i].value  = 0;

    return 1;
}

int32_t get_value_rv112(uint8_t i) {
    if (i >= MAX_NUM_RV112 || rv112s[i].enabled == 0) {
        fprintf(stderr, "ZynCore->get_value_rv112(%d): Invalid index!\n", i);
        return 0;
    }
    int32_t res = rv112s[i].value;
    if (res != 0) {
        rv112s[i].valraw = 0;
        rv112s[i].value  = 0;
    }
    return res;
}

//-----------------------------------------------------------------------------
// RV112 specific functions
//-----------------------------------------------------------------------------

int16_t read_rv112(uint8_t i) {
    int32_t vA = ads1115_analog_read(rv112s[i].ads1115_node, rv112s[i].chA);
    int32_t vB = ads1115_analog_read(rv112s[i].ads1115_node, rv112s[i].chB);
    int16_t d  = 0;

    switch (rv112s[i].curseg) {
    case 0:
        if (vB < RV112_ADS1115_RANGE_25) {
            d = rv112s[i].valA - vA;
            break;
        } else if (vA > RV112_ADS1115_RANGE_75) {
            d                = rv112s[i].valB - vB;
            rv112s[i].curseg = 1;
            break;
        } else if (vA < RV112_ADS1115_RANGE_25) {
            d                = vB - rv112s[i].valB;
            rv112s[i].curseg = 3;
            break;
        } else if (vB > RV112_ADS1115_RANGE_75) {
            d                = vA - rv112s[i].valA;
            rv112s[i].curseg = 2;
            break;
        }
        break;

    case 1:
        if (vA > RV112_ADS1115_RANGE_75) {
            d = rv112s[i].valB - vB;
            break;
        } else if (vB > RV112_ADS1115_RANGE_75) {
            d                = vA - rv112s[i].valA;
            rv112s[i].curseg = 2;
            break;
        } else if (vB < RV112_ADS1115_RANGE_25) {
            d                = rv112s[i].valA - vA;
            rv112s[i].curseg = 0;
            break;
        } else if (vA < RV112_ADS1115_RANGE_25) {
            d                = vB - rv112s[i].valB;
            rv112s[i].curseg = 3;
            break;
        }
        break;

    case 2:
        if (vB > RV112_ADS1115_RANGE_75) {
            d = vA - rv112s[i].valA;
            break;
        } else if (vA < RV112_ADS1115_RANGE_25) {
            d                = vB - rv112s[i].valB;
            rv112s[i].curseg = 3;
            break;
        } else if (vA > RV112_ADS1115_RANGE_75) {
            d                = rv112s[i].valB - vB;
            rv112s[i].curseg = 1;
            break;
        } else if (vB < RV112_ADS1115_RANGE_25) {
            d                = rv112s[i].valA - vA;
            rv112s[i].curseg = 0;
            break;
        }
        break;

    case 3:
        if (vA < RV112_ADS1115_RANGE_25) {
            d = vB - rv112s[i].valB;
            break;
        } else if (vB < RV112_ADS1115_RANGE_25) {
            d                = rv112s[i].valA - vA;
            rv112s[i].curseg = 0;
            break;
        } else if (vB > RV112_ADS1115_RANGE_75) {
            d                = vA - rv112s[i].valA;
            rv112s[i].curseg = 2;
            break;
        } else if (vA > RV112_ADS1115_RANGE_75) {
            d                = rv112s[i].valB - vB;
            rv112s[i].curseg = 1;
            break;
        }
        break;
    }

    rv112s[i].valA = vA;
    rv112s[i].valB = vB;
    // fprintf(stderr, "RV112[%d]: curseg = %d, vA = %d, vB = %d, d = %d\n", i, rv112s[i].curseg, vA, vB, d);
    return d / RV112_ADS1115_NOISE_DIV;
}

void* poll_rv112(void* arg) {
    int i = 0;
    int j = 0;
    int32_t vr, v;
    boost::circular_buffer<int32_t>* dvbuf;
    while (1) {
        if (rv112s[i].enabled) {
            rv112s[i].lastdv = read_rv112(i);
            // Calculate moving average for adaptative speed variation
            if (rv112s[i].step == 0) {
                dvbuf = (boost::circular_buffer<int32_t>*)rv112s[i].dvbuf;
                dvbuf->push_back(abs(rv112s[i].lastdv));
                rv112s[i].dvavg += (*dvbuf)[DVBUF_SIZE - 1] - (*dvbuf)[0];
                // fprintf(stderr, "DVAVG %d = %d\n", i, rv112s[i].dvavg);
            }
            if (rv112s[i].lastdv != 0) {
                // Adaptative speed variation using a moving average
                if (rv112s[i].step == 0) {
                    if (rv112s[i].dvavg < 1000)
                        rv112s[i].lastdv /= 8;
                    else if (rv112s[i].dvavg < 2000)
                        rv112s[i].lastdv /= 4;
                    else if (rv112s[i].dvavg < 4000)
                        rv112s[i].lastdv /= 2;
                } else {
                    rv112s[i].lastdv /= (8 * rv112s[i].step);
                }
                vr = rv112s[i].valraw + rv112s[i].lastdv;

                // calculate value & call CB function
                if (vr != rv112s[i].valraw) {
                    rv112s[i].valraw = vr;
                    rv112s[i].value  = vr / RV112_ADS1115_RAW_DIV;
                    if (rv112s[i].value != 0 && zynpot_cb) {
                        // fprintf(stderr, "RV112(%d): Vraw=%d, Value=%d\n", i, vr, rv112s[i].value);
                        zynpot_cb(rv112s[i].zpot_i, rv112s[i].value);
                        rv112s[i].valraw = 0;
                        rv112s[i].value  = 0;
                    }
                }
            }
        }
        // Calc next rotary ...
        i = (i + 1) % MAX_NUM_RV112;
        // prioritize active rotaries, cycling among them, but force a full cyle from time to time ... ...
        if (j < MAX_NUM_RV112) {
            j++;
        } else if (j < 3 * MAX_NUM_RV112) {
            for (int k = 0; k < MAX_NUM_RV112; k++) {
                if (rv112s[i].lastdv != 0)
                    break;
                i = (i + 1) % MAX_NUM_RV112;
            }
            j++;
        }
        //
        else {
            j = 0;
        }
    }
    return NULL;
}

pthread_t init_poll_rv112() {
    pthread_t tid;
    int err = pthread_create(&tid, NULL, &poll_rv112, NULL);
    if (err != 0) {
        fprintf(stderr, "ZynCore: Can't create RV112 poll thread :[%s]", strerror(err));
        return 0;
    } else {
        fprintf(stderr, "ZynCore: RV112 poll thread created successfully\n");
        return tid;
    }
}

//-----------------------------------------------------------------------------

/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: ads1115 low level access
 *
 * Implements the low level code to interface ADS1115 I2C 16-bit ADC
 *
 * Copyright (C) 2021-2024 Fernando Moyano <jofemodo@zynthian.org>
 * Copyright (C) 2016 Gordon Henderson
 * This code inherits from the venerable but currently
 * unmaintained wiringPi library, by Gordon Henderson
 * Thanks for your great work, Gordon!
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

#include <byteswap.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "wiringPiI2C.h"
#include "zynads1115.h"


// Bits in the config register (it's a 16-bit register)
#define	CONFIG_OS_MASK		(0x8000)	// Operational Status Register
#define	CONFIG_OS_SINGLE	(0x8000)	// Write - Starts a single-conversion
										// Read    1 = Conversion complete

// The multiplexor
#define	CONFIG_MUX_MASK		(0x7000)
// Differential modes
#define	CONFIG_MUX_DIFF_0_1	(0x0000)	// Pos = AIN0, Neg = AIN1 (default)
#define	CONFIG_MUX_DIFF_0_3	(0x1000)	// Pos = AIN0, Neg = AIN3
#define	CONFIG_MUX_DIFF_1_3	(0x2000)	// Pos = AIN1, Neg = AIN3
#define	CONFIG_MUX_DIFF_2_3	(0x3000)	// Pos = AIN2, Neg = AIN3 (2nd differential channel)
// Single-ended modes
#define	CONFIG_MUX_SINGLE_0	(0x4000)	// AIN0
#define	CONFIG_MUX_SINGLE_1	(0x5000)	// AIN1
#define	CONFIG_MUX_SINGLE_2	(0x6000)	// AIN2
#define	CONFIG_MUX_SINGLE_3	(0x7000)	// AIN3

// Programmable Gain Amplifier
#define	CONFIG_PGA_MASK		(0x0E00)
#define	CONFIG_PGA_6_144V	(0x0000)	// +/-6.144V range = Gain 2/3
#define	CONFIG_PGA_4_096V	(0x0200)	// +/-4.096V range = Gain 1
#define	CONFIG_PGA_2_048V	(0x0400)	// +/-2.048V range = Gain 2 (default)
#define	CONFIG_PGA_1_024V	(0x0600)	// +/-1.024V range = Gain 4
#define	CONFIG_PGA_0_512V	(0x0800)	// +/-0.512V range = Gain 8
#define	CONFIG_PGA_0_256V	(0x0A00)	// +/-0.256V range = Gain 16

#define	CONFIG_MODE		(0x0100)	// 0 is continuous, 1 is single-shot (default)

// Data Rate
#define	CONFIG_DR_MASK		(0x00E0)
#define	CONFIG_DR_8SPS		(0x0000)	//   8 samples per second
#define	CONFIG_DR_16SPS		(0x0020)	//  16 samples per second
#define	CONFIG_DR_32SPS		(0x0040)	//  32 samples per second
#define	CONFIG_DR_64SPS		(0x0060)	//  64 samples per second
#define	CONFIG_DR_128SPS	(0x0080)	// 128 samples per second (default)
#define	CONFIG_DR_250SPS	(0x00A0)	// 250 samples per second
#define	CONFIG_DR_475SPS	(0x00C0)	// 475 samples per second
#define	CONFIG_DR_860SPS	(0x00E0)	// 860 samples per second

// Comparator mode
#define	CONFIG_CMODE_MASK	(0x0010)
#define	CONFIG_CMODE_TRAD	(0x0000)	// Traditional comparator with hysteresis (default)
#define	CONFIG_CMODE_WINDOW	(0x0010)	// Window comparator

// Comparator polarity - the polarity of the output alert/rdy pin
#define	CONFIG_CPOL_MASK	(0x0008)
#define	CONFIG_CPOL_ACTVLOW	(0x0000)	// Active low (default)
#define	CONFIG_CPOL_ACTVHI	(0x0008)	// Active high

// Latching comparator - does the alert/rdy pin latch
#define	CONFIG_CLAT_MASK	(0x0004)
#define	CONFIG_CLAT_NONLAT	(0x0000)	// Non-latching comparator (default)
#define	CONFIG_CLAT_LATCH	(0x0004)	// Latching comparator

// Comparator queue
#define	CONFIG_CQUE_MASK	(0x0003)
#define	CONFIG_CQUE_1CONV	(0x0000)	// Assert after one conversions
#define	CONFIG_CQUE_2CONV	(0x0001)	// Assert after two conversions
#define	CONFIG_CQUE_4CONV	(0x0002)	// Assert after four conversions
#define	CONFIG_CQUE_NONE	(0x0003)	// Disable the comparator (default)

#define	CONFIG_DEFAULT		(0x8583)	// From the datasheet


static const uint16_t dataRates [8] =
{
  CONFIG_DR_8SPS, CONFIG_DR_16SPS, CONFIG_DR_32SPS, CONFIG_DR_64SPS, CONFIG_DR_128SPS, CONFIG_DR_250SPS, CONFIG_DR_475SPS, CONFIG_DR_860SPS
} ;

static const uint16_t dataGains [6] =
{
  CONFIG_PGA_6_144V, CONFIG_PGA_4_096V, CONFIG_PGA_2_048V, CONFIG_PGA_1_024V, CONFIG_PGA_0_512V, CONFIG_PGA_0_256V
} ;


//-----------------------------------------------------------------------------

/*
 * ads115_config:
 *	Calculate the base content of the config register
 *	Calculate the time (us) to wait for the conversion to complete
 *********************************************************************************
 */

int ads1115_config (ads1115_t *ads1115)
{
  ads1115->base_config = CONFIG_DEFAULT ;

  // Set PGA/voltage range
  ads1115->base_config &= ~CONFIG_PGA_MASK ;
  ads1115->base_config |= ads1115->gain ;

  // Set sample speed
  ads1115->base_config &= ~CONFIG_DR_MASK ;
  ads1115->base_config |= ads1115->rate ;

  // Calculate the time (us) to wait for the conversion to complete
  uint32_t dus ;
  switch (ads1115->rate >> 5)
  {
    case 0: dus = 1000000 / 8; break ;
    case 1: dus = 1000000 / 16; break ;
    case 2: dus = 1000000 / 32; break ;
    case 3: dus = 1000000 / 64; break ;
    case 4: dus = 1000000 / 128 ;break ;
    case 5: dus = 1000000 / 250 ;break ;
    case 6: dus = 1000000 / 475 ;break ;
    case 7: dus = 1000000 / 860 ;break ;
    default: dus = 10000; break ;
  }
  dus = 20 + 11 * dus / 10 ;
  ads1115->read_wait_us = dus ;

  //fprintf(stderr, "ADS1115 on address 0x%x => config = 0x%x, wait = %d\n", ads1115->i2c_address, ads1115->base_config, ads1115->read_wait_us);

  return 1 ;
}

/*
 * init_ads1115:
 *	Initialize an ads1115 IC using the I2C interface.
 *
 *	i2c_address : I2C address
 *	uint8_t gain : Initial gain
 *	uint8_t rate : Initial sample rate
 *	Returns 1 on success, 0 on fail
 *********************************************************************************
 */
int init_ads1115(ads1115_t *ads1115, uint16_t i2c_address, uint8_t gain, uint8_t rate) {
  int fd = wiringPiI2CSetup(i2c_address);
  if (fd < 0) return 0 ;

  // Use default if out of range
  if (gain > 5) gain = ADS1115_GAIN_VREF_2_048 ;
  if (rate > 7) rate = ADS1115_RATE_128SPS ;

  ads1115->i2c_address = i2c_address;
  ads1115->fd = fd ;
  ads1115->gain = dataGains[gain] ; // Gain
  ads1115->rate = dataRates[rate] ;	// Samples/sec
  return ads1115_config (ads1115) ;
}

/*
 * set_ads1115_gain:
 *  gain : gain index
  *********************************************************************************
 */
void ads1115_set_gain(ads1115_t *ads1115, uint8_t gain) {
  if (gain > 5) gain = ADS1115_GAIN_VREF_2_048 ;	// Use default if out of range
  ads1115->gain = dataGains [gain] ;
  ads1115_config (ads1115) ;
}

/*
 * set_ads1115_rate:
 *  rate : rate index
  *********************************************************************************
 */
void ads1115_set_rate(ads1115_t *ads1115, uint8_t rate) {
	if (rate > 7) rate = ADS1115_RATE_128SPS ;	// Use default if out of range
    ads1115->rate = dataRates [rate] ;
    ads1115_config (ads1115) ;
}

/*
 * ads1115_set_comparator_threshold:
 *  chan : channel to configure
 *	data : data to write to the 2 comparator threshold registers.
 *
 *********************************************************************************
 */
void ads1115_set_comparator_threshold (ads1115_t *ads1115, uint8_t chan, int16_t data) {
  chan &= 3 ;
  int reg = chan + 2 ;
  data = __bswap_16 (data) ;
  wiringPiI2CWriteReg16 (ads1115->fd, reg, data) ;
}

/*
 * ads1115_analog_read
 *	channels 0-3 are single ended inputs (pin voltage)
 *	channels 4-7 are the various differential combinations.
 *********************************************************************************
 */
int16_t ads1115_analog_read(ads1115_t *ads1115, uint8_t chan) {
  int16_t result;

  chan &= 3 ;

  // Setup the configuration register
  uint16_t config = ads1115->base_config ;

  // Set single-ended channel or differential mode
  config &= ~CONFIG_MUX_MASK ;
  switch (chan) {
    case 0: config |= CONFIG_MUX_SINGLE_0 ; break ;
    case 1: config |= CONFIG_MUX_SINGLE_1 ; break ;
    case 2: config |= CONFIG_MUX_SINGLE_2 ; break ;
    case 3: config |= CONFIG_MUX_SINGLE_3 ; break ;

    case 4: config |= CONFIG_MUX_DIFF_0_1 ; break ;
    case 5: config |= CONFIG_MUX_DIFF_2_3 ; break ;
    case 6: config |= CONFIG_MUX_DIFF_0_3 ; break ;
    case 7: config |= CONFIG_MUX_DIFF_1_3 ; break ;
  }

  // Start a single conversion
  config |= CONFIG_OS_SINGLE ;

  config = __bswap_16 (config) ;
  wiringPiI2CWriteReg16 (ads1115->fd, 1, config) ;

  // Wait for the conversion to complete
  int i = 0;
  for (;;) {
    delay_microseconds (ads1115->read_wait_us) ;
    result = wiringPiI2CReadReg16 (ads1115->fd, 1) ;
    result = __bswap_16 (result) ;
    if ((result & CONFIG_OS_MASK) != 0) break ;
    else if (i++ > 10) {
    	fprintf(stderr, "ZynCore->ads1115_analog_read(0x%x, %d): TimeOut with status 0x%x!!\n", ads1115->i2c_address, chan, result);
    	return 0;
    }
  }

  // Read the result
  result = wiringPiI2CReadReg16 (ads1115->fd, 0) ;
  result = __bswap_16 (result) ;

  // Sometimes with a 0v input on a single-ended channel the internal 0v reference
  // can be higher than the input, so you get a negative result...
  if ( (chan < 4) && (result < 0) ) return 0 ;
  else return result ;
}

//-----------------------------------------------------------------------------

/*
 * delayMicroseconds:
 *	This is somewhat interesting. It seems that on the Pi, a single call
 *	to nanosleep takes some 80 to 130 microseconds anyway, so while
 *	obeying the standards (may take longer), it's not always what we
 *	want!
 *
 *	So what I'll do now is if the delay is less than 100uS we'll do it
 *	in a hard loop, watching a built-in counter on the ARM chip. This is
 *	somewhat sub-optimal in that it uses 100% CPU, something not an issue
 *	in a microcontroller, but under a multi-tasking, multi-user OS, it's
 *	wastefull, however we've no real choice )-:
 *
 *  Plan B: It seems all might not be well with that plan, so changing it
 *  to use gettimeofday () and poll on that instead...
 *********************************************************************************
 */

void delay_microseconds_hard (unsigned int howLong)
{
  struct timeval tNow, tLong, tEnd ;
  gettimeofday (&tNow, NULL) ;
  tLong.tv_sec = howLong / 1000000 ;
  tLong.tv_usec = howLong % 1000000 ;
  timeradd (&tNow, &tLong, &tEnd) ;

  while (timercmp (&tNow, &tEnd, <))
    gettimeofday (&tNow, NULL) ;
}

void delay_microseconds (unsigned int howLong)
{
  if (howLong == 0)
    return ;
  else if (howLong < 100)
    delay_microseconds_hard (howLong) ;
  else
  {
    struct timespec sleeper ;
    unsigned int uSecs = howLong % 1000000 ;
    unsigned int wSecs = howLong / 1000000 ;
    sleeper.tv_sec = wSecs ;
    sleeper.tv_nsec = (long)(uSecs * 1000L) ;
    nanosleep (&sleeper, NULL) ;
  }
}

//-----------------------------------------------------------------------------

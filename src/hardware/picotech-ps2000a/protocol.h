/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Martin Ling <martin-git@earth.li>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_PICOTECH_PS2000A_PROTOCOL_H
#define LIBSIGROK_HARDWARE_PICOTECH_PS2000A_PROTOCOL_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

#define LOG_PREFIX "picotech-ps2000a"

#include <libps2000a-1.1/ps2000aApi.h>
#ifndef PICO_STATUS
#include <libps2000a-1.1/PicoStatus.h>
#endif

#define BUFFER_SIZE 	1024

#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

typedef enum
{
	ANALOGUE, DIGITAL, AGGREGATED, MIXED
} MODE;

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
} CHANNEL_SETTINGS;

typedef enum
{
	MODEL_NONE   = 0,
	MODEL_PS2205 = 0x2205,
	MODEL_PS2205MSO = 0xE205,
	MODEL_PS2206 = 0x2206,
	MODEL_PS2207 = 0x2207,
	MODEL_PS2208 = 0x2208,
	MODEL_PS2206A = 0xA206,
	MODEL_PS2207A = 0xA207,
	MODEL_PS2208A = 0xA208,

} MODEL_TYPE;

typedef struct tTriggerDirections
{
	PS2000A_THRESHOLD_DIRECTION channelA;
	PS2000A_THRESHOLD_DIRECTION channelB;
	PS2000A_THRESHOLD_DIRECTION channelC;
	PS2000A_THRESHOLD_DIRECTION channelD;
	PS2000A_THRESHOLD_DIRECTION ext;
	PS2000A_THRESHOLD_DIRECTION aux;
} TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	PS2000A_PWQ_CONDITIONS * conditions;
	int16_t nConditions;
	PS2000A_THRESHOLD_DIRECTION direction;
	uint32_t lower;
	uint32_t upper;
	PS2000A_PULSE_WIDTH_TYPE type;
} PWQ;

typedef struct
{
	int16_t handle;
	MODEL_TYPE			model;
	char				modelString[8];
	char				serial[10];
	int16_t				complete;
	int16_t				openStatus;
	int16_t				openProgress;
	PS2000A_RANGE			firstRange;
	PS2000A_RANGE			lastRange;
	int16_t				channelCount;
	int16_t				maxValue;
	int16_t				signalGenerator;
	int16_t				ETS;
	int16_t				AWGFileSize;
	CHANNEL_SETTINGS		channelSettings [PS2000A_MAX_CHANNELS];
	int16_t				digitalPorts;
        struct sr_channel_group         **analog_groups;
        struct sr_channel_group         **digital_groups;
} UNIT;

/*
uint32_t timebase = 8;
int16_t oversample = 1;
BOOL scaleVoltages = TRUE;

uint16_t inputRanges[PS2000A_MAX_RANGES] =
{ 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };

BOOL g_ready = FALSE;
int64_t g_times[PS2000A_MAX_CHANNELS];
int16_t g_timeUnit;
int32_t      		g_sampleCount;
uint32_t g_startIndex;
int16_t g_autoStopped;
int16_t g_trig = 0;
uint32_t g_trigAt = 0;
int16_t g_overflow = 0;
*/

void set_info(UNIT *unit);

#endif

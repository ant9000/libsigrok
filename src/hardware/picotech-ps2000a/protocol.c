/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Martin Ling <martin-git@earth.li>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2013 Mathias Grimmberger <mgri@zaphod.sax.de>
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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "protocol.h"


void set_info(UNIT *unit)
{
	int16_t r = 0;
	char line[8] = {0};
	int32_t variant = 0;

	if (unit->handle) 
	{
		// info = 3 - PICO_VARIANT_INFO
		ps2000aGetUnitInfo(unit->handle, (int8_t *)line, sizeof(line), &r, 3);
		variant = atoi((char *)line);
		memcpy(&(unit->modelString),line,sizeof(unit->modelString)==7?7:sizeof(unit->modelString));
		//To identify variants.....

		if (strlen(line) == 5)						// A variant unit
		{
			line[4] = toupper(line[4]);

			if (line[4] == 'A')		// i.e 2207A -> 0xA207
				variant += 0x9968;
		} else
			variant += 0x1968;   // 2206 -> 0x2206

		if (strlen(line) == 7)
		{
			line[4] = toupper(line[4]);
			line[5] = toupper(line[5]);
			line[6] = toupper(line[6]);

			if(strcmp(line+4, "MSO") == 0)
				variant += 0xC000;						// 2205MSO -> 0xE205
		}

		// info = 4 - PICO_BATCH_AND_SERIAL
		ps2000aGetUnitInfo(unit->handle, (int8_t *)unit->serial, sizeof (unit->serial), &r, 4);
	}

	switch (variant)
	{

		case MODEL_PS2205MSO:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = FALSE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 2;
			break;

		case MODEL_PS2206:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = FALSE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		case MODEL_PS2207:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = FALSE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		case MODEL_PS2208:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = TRUE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		case MODEL_PS2206A:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = FALSE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		case MODEL_PS2207A:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = FALSE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		case MODEL_PS2208A:
			unit->model = variant;
			unit->signalGenerator = TRUE;
			unit->ETS = TRUE;
			unit->firstRange = PS2000A_50MV;
			unit->lastRange = PS2000A_20V;
			unit->channelCount = DUAL_SCOPE;
			unit->digitalPorts = 0;
			break;

		default:
			break;
	}
}

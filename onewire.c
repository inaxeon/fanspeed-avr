/*
 *   File:   ds18x20.c
 *   Author: Matthew Millman
 *
 *   Created on 26 July 2016, 11:13
 *
 *   This is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   You should have received a copy of the GNU General Public License
 *   along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "project.h"

#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "onewire.h"
#include "ow_bitbang.h"
#include "ds2482.h"

#define OW_SEARCH_FIRST           0xFF
#define OW_PRESENCE_ERR           0xFF
#define OW_DATA_ERR               0xFE
#define OW_LAST_DEVICE            0x00

static int8_t match_family_code(uint8_t family_code, uint8_t *family_codes, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        if (family_codes[i] == family_code)
            return (int8_t)i;
    }

    return -1;
}

static bool onewire_find_device(uint8_t *diff, uint8_t *id, uint8_t *family_codes, uint8_t family_codes_len)
{
    uint8_t go = 1;

    do
    {
        *diff = ow_rom_search(*diff, id);
        if (*diff == OW_PRESENCE_ERR || *diff == OW_DATA_ERR || *diff == OW_LAST_DEVICE)
        {
            go = 0;
        }
        else if (*diff == OW_COMMS_ERR)
        {
            return false;
        }
        else
        {
            if (match_family_code(id[0], family_codes, family_codes_len) >= 0)
                go = 0;
        }
    } while (go);

    return true;
}

bool onewire_search_devices(uint8_t(*sensor_ids)[OW_ROMCODE_SIZE], uint8_t *family_codes, uint8_t *counts, uint8_t family_codes_len)
{
    bool presense;
    uint8_t i;
    uint8_t total = 0;
    uint8_t id[OW_ROMCODE_SIZE];
    uint8_t diff;
    
    for (i = 0; i < family_codes_len; i++)
        counts[i] = 0;
    
    if (!ow_bus_reset(&presense))
        return false;
    if (!presense)
        return true;
    
    diff = OW_SEARCH_FIRST;

    while (diff != OW_LAST_DEVICE && total < MAX_SENSORS)
    {
        int8_t family_matched;

        if (!onewire_find_device(&diff, id, family_codes, family_codes_len))
            return false;
        
        if (diff == OW_PRESENCE_ERR)
            break;
        if (diff == OW_DATA_ERR)
            break;

        family_matched = match_family_code(id[0], family_codes, family_codes_len);

        if (family_matched < 0)
            continue;

        counts[family_matched]++;
        
        for (i = 0; i < OW_ROMCODE_SIZE; i++)
            sensor_ids[total][i] = id[i];

        total++;
    }

    return true;
}

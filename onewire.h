/*
 *   File:   onewire.h
 *   Author: Matt
 *
 *   Created on 22 July 2016, 16:50
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

#ifndef __ONEWIRE_H__
#define __ONEWIRE_H__

#define OW_READ_ROM     0x33
#define OW_MATCH_ROM    0x55
#define OW_SKIP_ROM     0xCC
#define OW_SEARCH_ROM   0xF0

#define OW_SEARCH_FIRST 0xFF        /* Start new search */
#define OW_PRESENCE_ERR 0xFF
#define OW_DATA_ERR     0xFE
#define OW_COMMS_ERR    0xFD
#define OW_LAST_DEVICE  0x00        /* Last device found */

#define OW_ROMCODE_SIZE 8

bool onewire_search_devices(uint8_t(*sensor_ids)[OW_ROMCODE_SIZE], uint8_t *family_codes, uint8_t *counts, uint8_t family_codes_len);

#ifdef _OW_BITBANG_

#define ow_init()
#define ow_bus_reset(presense) owbitbang_bus_reset(presense)
#define ow_select(id) owbitbang_select(id)
#define ow_write(data, len) owbitbang_write(data, len)
#define ow_read(data, len) owbitbang_read(data, len)
#define ow_bit_io(bit) owbitbang_bit_io(bit)
#define ow_rom_search(diff, id) owbitbang_rom_search(diff, id)

#endif /* _OW_BITBANG_ */

#ifdef _OW_DS2482_

#define ow_init() ds2482_init()
#define ow_bus_reset(presense) ds2482_bus_reset(presense)
#define ow_select(id) ds2482_select(id)
#define ow_write(data, len) ds2482_write(data, len)
#define ow_read(data, len) ds2482_read(data, len)
#define ow_bit_io(bit) ds2482_bit_io(bit)
#define ow_rom_search(diff, id) ds2482_rom_search(diff, id)

#endif /* _OW_DS2482_ */

#endif /* __ONEWIRE_H__ */

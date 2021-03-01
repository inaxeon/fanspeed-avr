/*
 *   File:   ow_bitbang.h
 *   Author: Matthew Millman
 *
 *   Fan speed controller. OSS AVR Version.
 *
 *   Created on 26 August 2014, 20:27
 *   Ported from MPLAB XC8 project 17 December 2020, 07:54
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

#ifndef __OW_BITBANG_H__
#define __OW_BITBANG_H__

#include <stdint.h>
#include <stdbool.h>

bool owbitbang_init(void);
bool owbitbang_bus_reset(bool *presense_detect);
bool owbitbang_bit_io(bool *bit);
bool owbitbang_read(uint8_t *buf, uint8_t len);
uint8_t owbitbang_rom_search(uint8_t diff, uint8_t *id);
bool owbitbang_select(const uint8_t *id);
bool owbitbang_write(const uint8_t *data, uint8_t len);

#endif /* __OW_BITBANG_H__ */


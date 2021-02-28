/*
 *   File:   ds2482.h
 *   Author: Matt
 *
 *   Created on 22 July 2016, 11:13
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

#ifndef __DS2482_H__
#define	__DS2482_H__

bool ds2482_init(void);
bool ds2482_bus_reset(bool *presense_detect);
bool ds2482_select(const uint8_t *id);
bool ds2482_read(uint8_t *buf, uint8_t len);
bool ds2482_write(const uint8_t *data, uint8_t len);
bool ds2482_bit_io(bool *bit);
uint8_t ds2482_rom_search(uint8_t diff, uint8_t *id);

#endif /* __DS2482_H__ */
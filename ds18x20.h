/*
 *   File:   ds18x20.h
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

#ifndef __ds18b20_H__
#define __ds18b20_H__

#define DS18B20_FAMILY_CODE         0x28

#define DS18B20_TCONV_12BIT         750

bool ds18b20_find_sensor(uint8_t *diff, uint8_t *id);
bool ds18b20_start_meas(uint8_t *id);
bool ds18b20_read_decicelsius(uint8_t *id, int16_t *decicelsius);
bool ds18b20_search_sensors(uint8_t *count, uint8_t(*sensor_ids)[OW_ROMCODE_SIZE]);
void ds18b20_authenticity_check(uint8_t *addr);
void ds18b20_classify_sensor(uint8_t *addr);

#endif /* __ds18b20_H__ */
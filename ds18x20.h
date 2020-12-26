/*
* File:   ds18x20.h
* Author: Matt
*
* Created on 26 July 2016, 11:13
*/

#ifndef __DS18X20_H__
#define __DS18X20_H__

#include <stdint.h>
#include <stdbool.h>

#include "onewire.h"
#include "config.h"

#define DS18B20_TCONV_12BIT       750

bool ds18x20_find_sensor(uint8_t *diff, uint8_t *id);
bool ds18x20_start_meas(uint8_t *id);
bool ds18x20_read_decicelsius(uint8_t *id, int16_t *decicelsius);
bool ds18x20_search_sensors(uint8_t *count, uint8_t(*sensor_ids)[DS18X20_ROMCODE_SIZE]);

#endif /* __DS18X20_H__ */
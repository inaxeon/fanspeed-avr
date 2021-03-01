/*
 *   File:   config.c
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

#define OW_ROMCODE_SIZE 8

typedef struct {
    uint16_t magic;
#ifdef _SINGLEZONE_
    uint8_t num_fans;
    uint8_t fans_max;
    uint8_t fans_min;
    uint8_t fans_start;
    uint16_t fans_minrpm;
    bool fans_minoff;
    uint8_t min_temps;
    int16_t temp_min;
    int16_t temp_max;
    uint16_t temp_hyst;
    char temp1_desc[MAX_DESC];
    char temp2_desc[MAX_DESC];
    char temp3_desc[MAX_DESC];
    char temp4_desc[MAX_DESC];
#else
    uint8_t fan1_max;
    uint8_t fan1_min;
    uint8_t fan1_start;
    uint16_t fan1_minrpm;
    bool fan1_minoff;
    uint8_t fan2_max;
    uint8_t fan2_min;
    uint8_t fan2_start;
    uint16_t fan2_minrpm;
    bool fan2_minoff;
    int16_t temp1_min;
    int16_t temp1_max;
    uint16_t temp1_hyst;
    int16_t temp2_min;
    int16_t temp2_max;
    uint16_t temp2_hyst;
    bool fan2_enabled;
    char temp1_desc[MAX_DESC];
    char temp2_desc[MAX_DESC];
#endif /* _SINGLEZONE_ */
    bool manual_assignment;
    uint8_t sensor1_addr[OW_ROMCODE_SIZE];
    uint8_t sensor2_addr[OW_ROMCODE_SIZE];
#ifdef _SINGLEZONE_
    uint8_t sensor3_addr[OW_ROMCODE_SIZE];
    uint8_t sensor4_addr[OW_ROMCODE_SIZE];
#endif /* _SINGLEZONE_ */
} sys_config_t;

void configuration_bootprompt(sys_config_t *config);
void load_configuration(sys_config_t *config);
void set_start_duty(sys_config_t *config);

#endif /* __CONFIG_H__ */
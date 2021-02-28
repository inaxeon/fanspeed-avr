/*
 *   File:   util.c
 *   Author: Matthew Millman
 *
 *   Fan speed controller. Single thermal path, OSS AVR Version.
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

#include "project.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/wdt.h>
#include <avr/eeprom.h> 

#include "util.h"
#include "usart.h"
#include "config.h"

void reset(void)
{
    /* Uses the watch dog timer to reset */
    wdt_enable(WDTO_15MS);
    while (1);
}

int print_char(char byte, FILE *stream)
{
    while (console_busy());
    console_put(byte);
    return 0;
}

void putch(char byte)
{
    while (console_busy());
    console_put(byte);
}

char wdt_getch(void)
{
    while (!console_data_ready())
        wdt_reset();
    console_clear_oerr();
    return console_get();
}

void eeprom_write_data(uint8_t addr, uint8_t *bytes, uint8_t len)
{
    uint16_t dest = addr;
    eeprom_update_block(bytes, (void *)dest, len);
}

void eeprom_read_data(uint8_t addr, uint8_t *bytes, uint8_t len)
{
    uint16_t dest = addr;
    eeprom_read_block(bytes, (void *)dest, len);
}

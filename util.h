/*
 *   File:   util.h
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

#ifndef __UTIL_H__
#define	__UTIL_H__

void delay_10ms(uint8_t delay);
void reset(void);
void format_fixedpoint(char *buf, int16_t value, uint8_t type);
void eeprom_read_data(uint8_t addr, uint8_t *bytes, uint8_t len);
void eeprom_write_data(uint8_t addr, uint8_t *bytes, uint8_t len);
char wdt_getch(void);
void putch(char byte);
int print_char(char byte, FILE *stream);

#undef printf
#define printf(fmt, ...) printf_P(PSTR(fmt) __VA_OPT__(,) __VA_ARGS__)
#define sprintf(buf, fmt, ...) sprintf_P(buf, PSTR(fmt) __VA_OPT__(,) __VA_ARGS__)
#define strcmp_p(str, to) strcmp_P(str, PSTR(to))
#define strncmp_p(str, to, n) strncmp_P(str, PSTR(to), n)
#define stricmp(str, to) strcasecmp_P(str, PSTR(to))
#define delay_10ms(x) _delay_ms((x) * 10)

#define I_1DP               0
#define U_1DP               1

#define fixedpoint_sign(value, tag) \
    char tag##_sign[2]; \
    tag##_sign[1] = 0; \
    if (value < 0 ) \
        tag##_sign[0] = '-'; \
    else \
        tag##_sign[0] = 0; \

#define fixedpoint_arg(value, tag) tag##_sign, (abs(value) / _1DP_BASE), (abs(value) % _1DP_BASE)
#define fixedpoint_arg_u(value) (value / _1DP_BASE), (value % _1DP_BASE)
#define max_(x, y) (x > y ? x : y)
#define min_(x, y) (x < y ? x : y)

#define MAX_FDP 8 /* "-30.000" + NUL */

#endif	/* __UTIL_H__ */


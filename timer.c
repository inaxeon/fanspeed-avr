/*
 *   File:   timer.c
 *   Author: Matt
 *
 *   Created on 19 May 2018, 18:02
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
#include <avr/io.h>

#include "timer.h"

void timer0_init(void)
{
    TCCR0A = 0x00;

    // CLK(i/o) prescaler 1024
    TCCR0B |= _BV(CS02);
    TCCR0B &= ~_BV(CS01);
    TCCR0B |= _BV(CS00);
}

void timer0_start(void)
{
    TIMSK0 |= _BV(TOIE0);
}

void timer0_stop()
{
    TIMSK0 &= ~_BV(TOIE0);
}

void timer0_reload(uint8_t val)
{
    TCNT0 = val;
}
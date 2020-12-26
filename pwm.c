/*
 *   File:   pwm.c
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

#ifndef __PWM_H__
#define	__PWM_H__

#include "project.h"

#include <stdint.h>
#include <avr/io.h>

#include "iopins.h"
#include "pwm.h"

void pwm_init(void)
{
    // 9-bit, precaler = 1, non inverting.
    TCCR1A = _BV(WGM11) | _BV(COM1B1) | _BV(COM1A1);
    TCCR1B = _BV(WGM12) | _BV(CS10);

    IO_OUTPUT(F1PWM);
    IO_OUTPUT(F2PWM);
}

void pwm_setduty(uint8_t pwm, uint8_t pct)
{
    uint16_t duty;

    duty = ((uint32_t)pct * PWM_BASE) / 100;

    if (pwm)
        OCR1B = duty;
    else
        OCR1A = duty;
}

#endif	/* __PWM_H__ */


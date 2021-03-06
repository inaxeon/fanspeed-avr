/*
 *   File:   project.h
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

#ifndef __PROJECT_H__
#define __PROJECT_H__

// High level features

// Uncomment to enable single thermal zone build
#define _SINGLEZONE_

// Uncomment to use DS2482 onewire bus master instead of bitbang
//#define _OW_DS2482_

#define _DS18B20_AUTHCHECK_

// Common limits

#define MAX_DESC             16
#define MAX_FANS             2
#ifdef _SINGLEZONE_
#define MAX_SENSORS          4
#else
#define MAX_SENSORS          2
#endif

// Default config

#define DEF_PCT_MAX          100     /* 100% on */
#define DEF_PCT_MIN          20      /* Idle speed */
#define DEF_TEMP_MIN         180     /* Fan at minimum (18 degrees) */
#define DEF_TEMP_MAX         300     /* Fan 100% on (30 degrees) */
#define DEF_MIN_RPM          0       /* Minimum fan speed before restore kicks in */

#define UART_BAUD            9600   // 38400 is the maximum accurate baud for the 12.288MHz crystal installed

// Constants (which shouldn't be changed)

#ifdef _SINGLEZONE_
#define CONFIG_MAGIC         0x4644
#else
#define CONFIG_MAGIC         0x4643
#endif

#define PWM_BASE             512

#define FAN1                 0
#define FAN2                 1
#define FAN3                 2
#define FAN4                 3
#define FAN5                 4
#define FAN6                 5

#define _1DP_BASE            10
#define I2C_FREQ             400
// 12.288 MHz is used because it generates accurate baud up to 38400, and it provides the required 24 KHz PWM for the fans
#define F_CPU                12288000
#define TIMER0VAL            136

#define _USART1_
#ifdef _OW_DS2482_
#define _I2C_
#define _I2C_XFER_
#define _I2C_XFER_BYTE_
#define _I2C_DS2482_SPECIAL_
#else
#define _OW_BITBANG_
#endif /* _OW_DS2482_ */

// Function redefinitions

#define console_busy         usart1_busy
#define console_put          usart1_put
#define console_data_ready   usart1_data_ready
#define console_get          usart1_get

#define g_irq_disable cli
#define g_irq_enable sei

#endif /* __PROJECT_H__ */
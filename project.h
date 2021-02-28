/*
 *   File:   project.h
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

#ifndef __PROJECT_H__
#define __PROJECT_H__

// Limits

#define MAX_DESC             16
#define MAXFANS              2
#define MAX_SENSORS          4

// Constants

#define CONFIG_MAGIC         0x4644
#define PWM_BASE             512

#define DEF_PCT_MAX          100     /* 100% on */
#define DEF_PCT_MIN          20      /* Idle speed */
#define DEF_TEMP_MIN         180     /* Fan at minimum (18 degrees) */
#define DEF_TEMP_MAX         300     /* Fan 100% on (30 degrees) */
#define DEF_MIN_RPM          0       /* Minimum fan speed before restore kicks in */

#define FAN1                 0
#define FAN2                 1
#define FAN3                 2
#define FAN4                 3
#define FAN5                 4
#define FAN6                 5

#define _1DP_BASE            10
#define UART_BAUD            9600
#define I2C_FREQ             400
#define F_CPU                12288000
#define TIMER0VAL            136

// Features

#define _USART1_
#define _OW_BITBANG_
//#define _OW_DS2482_
#ifdef _OW_DS2482_
#define _I2C_
#define _I2C_XFER_
#define _I2C_XFER_BYTE_
#define _I2C_DS2482_SPECIAL_
#endif /* _OW_DS2482_ */

// Function redefinitions

#define console_busy         usart1_busy
#define console_put          usart1_put
#define console_data_ready   usart1_data_ready
#define console_get          usart1_get
#define console_clear_oerr   usart1_clear_oerr

#define g_irq_disable cli
#define g_irq_enable sei

#endif /* __PROJECT_H__ */
/*
 *   File:   timer.h
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

#ifndef __TIMER_H__
#define __TIMER_H__

void timer1_init(void);
void timer1_start(void);
void timer1_stop(void);
void timer1_reload(uint16_t val);

void timer0_init(void);
void timer0_start(void);
void timer0_stop(void);
void timer0_reload(uint8_t val);

#endif /* __TIMER_H__ */
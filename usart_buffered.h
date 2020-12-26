/*
 *   File:   usart_buffered.h
 *   Author: Matt
 *
 *   Created on 22 May 2018, 16:32
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

#ifndef __USART_BUFFERED_H__
#define __USART_BUFFERED_H__

#define USART_SYNC            0x01
#define USART_9BIT            0x02
#define USART_SYNC_MASTER     0x04
#define USART_CONT_RX         0x08
#define USART_BRGH            0x10
#define USART_IOR             0x20
#define USART_IOT             0x40

#define UART_BUFFER_OVERFLOW  0x02

#ifdef _USART1_

void usart1_open(uint8_t flags, uint16_t brg);
bool usart1_busy(void);
void usart1_put(char c);
bool usart1_data_ready(void);
char usart1_get(void);
void usart1_clear_oerr(void);
uint8_t usart1_get_last_rx_error(void);

#endif /* _USART1_ */

#endif /* __USART_BUFFERED_H__ */

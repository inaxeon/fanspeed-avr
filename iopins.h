/*
 *   File:   iopins.h
 *   Author: Matt
 *
 *   Created on 31st July 2020, 07:00
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


#ifndef __IOPINS_H__
#define __IOPINS_H__

#define IO_INPUT(pin) pin##_DDR &= ~_BV(pin)
#define IO_OUTPUT(pin) pin##_DDR |= _BV(pin)

#define IO_HIGH(pin) pin##_PORT |= _BV(pin)
#define IO_LOW(pin) pin##_PORT &= ~_BV(pin)

#define IO_TOGGLE(pin) (pin##_PORT) ^= _BV(pin)

#define IO_IN_HIGH(pin) ((pin##_PIN & _BV(pin)) == _BV(pin))
#define IO_IN_LOW(pin) ((pin##_PIN & _BV(pin)) == 0x00)

#define F1TACH             PC2
#define F2TACH             PC3
#define F1PWM              PB1
#define F2PWM              PB2
#define F1ON               PD4
#define F2ON               PD7
#define ONEWIRE            PC1
#define SP1                PB0
#define SP2                PD5
#define SP3                PD6
#define SP4                PC0
#define SP5                PD3
#define SP6                PD2

#define F1TACH_PIN         PINC
#define F2TACH_PIN         PINC
#define F1PWM_PIN          PINB
#define F2PWM_PIN          PINB
#define F1ON_PIN           PIND
#define F2ON_PIN           PIND
#define ONEWIRE_PIN        PINC
#define SP1_PIN            PINB
#define SP2_PIN            PIND
#define SP3_PIN            PIND
#define SP4_PIN            PINC
#define SP5_PIN            PIND
#define SP6_PIN            PIND

#define F1TACH_PORT        PORTC
#define F2TACH_PORT        PORTC
#define F1PWM_PORT         PORTB
#define F2PWM_PORT         PORTB
#define F1ON_PORT          PORTD
#define F2ON_PORT          PORTD
#define ONEWIRE_PORT       PORTC
#define SP1_PORT           PORTB
#define SP2_PORT           PORTD
#define SP3_PORT           PORTD
#define SP4_PORT           PORTC
#define SP5_PORT           PORTD
#define SP6_PORT           PORTD

#define F1TACH_DDR         DDRC
#define F2TACH_DDR         DDRC
#define F1PWM_DDR          DDRB
#define F2PWM_DDR          DDRB
#define F1ON_DDR           DDRD
#define F2ON_DDR           DDRD
#define ONEWIRE_DDR        DDRC
#define SP1_DDR            DDRB
#define SP2_DDR            DDRD
#define SP3_DDR            DDRD
#define SP4_DDR            DDRC
#define SP5_DDR            DDRD
#define SP6_DDR            DDRD

#define SDA_DDR            PORTC
#define SDA_PIN            PINC
#define SDA_PORT           PORTC
#define SDA                PC4

#define SCL_DDR            PORTC
#define SCL_PIN            PINC
#define SCL_PORT           PORTC
#define SCL                PC5

#define USART1_DDR         DDRD
#define USART1_TX          PD1
#define USART1_RX          PD0
#define USART1_XCK         PD4

#define SPI_DDR            DDRB
#define SPI_PORT           PORTB
#define SPI_MISO           PB4
#define SPI_MOSI           PB2
#define SPI_SCK            PB5

#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)

#define UCSRAA             UCSR0A
#define UCSRAB             UCSR0B
#define UCSRAC             UCSR0C
#define UBRRAL             UBRR0L
#define UBRRAH             UBRR0H
#define UDRIEA             UDRIE0
#define UCSZA0             UCSZ00
#define UCSZA1             UCSZ01
#define UCSZA2             UCSZ02
#define RXCIEA             RXCIE0
#define RXENA              RXEN0
#define TXENA              TXEN0
#define UMSELA0            UMSEL00
#define UDREA              UDRE0
#define UDRA               UDR0
#define DORA               DOR0
#define FEA                FE0

#define USARTA_RX_vect     USART_RX_vect
#define USARTA_UDRE_vect   USART_UDRE_vect

#endif /* __AVR_ATmega328P__ */

#endif /* __IOPINS_H__ */
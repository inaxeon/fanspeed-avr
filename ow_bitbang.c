/*
 *   File:   ow_bitbang.c
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

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "iopins.h"
#include "config.h"
#include "onewire.h"
#include "ow_bitbang.h"

#ifdef _OW_BITBANG_

#define OW_GET_IN()   IO_IN_HIGH(ONEWIRE)
#define OW_OUT_LOW()  IO_LOW(ONEWIRE)
#define OW_OUT_HIGH() IO_HIGH(ONEWIRE)
#define OW_DIR_IN()   IO_INPUT(ONEWIRE)
#define OW_DIR_OUT()  IO_OUTPUT(ONEWIRE)

#define OW_CONF_DELAYOFFSET 0

/*
 * Recovery time (T_Rec) minimum 1usec - increase for long lines
 * 5 usecs is a value give in some Maxim AppNotes
 * 30u secs seem to be reliable for longer lines
 */
#define OW_RECOVERY_TIME         20  /* usec */

bool owbitbang_bus_idle()
{
    return OW_GET_IN();
}

bool owbitbang_bus_reset(bool *presense_detect)
{
    bool ret;
    uint8_t intsave;

    OW_OUT_LOW();
    OW_DIR_OUT();             /* Pull OW-Pin low for 480us */
    _delay_us(240);
    _delay_us(240);

    intsave = (SREG & _BV(SREG_I)) == _BV(SREG_I);
    g_irq_disable();

    OW_DIR_IN();
    OW_OUT_HIGH();
    _delay_us(64);
    ret = !(OW_GET_IN());

    if (intsave)
        g_irq_enable();

    /*
     * After a delay the clients should release the line
     * and input-pin gets back to high by pull-up-resistor.
     */
    _delay_us(240);
    _delay_us(240 - 64);     /* Was 480-66 */
    if (OW_GET_IN() == 0)
        ret = false;          /* Short circuit, expected low but got high */

    *presense_detect = ret;
    return ret;
}

/*
 * HOW TO CALIBRATE:
 *
 * Place a GPIO pulse at the line mentioned below
 * Adjust OW_CONF_DELAYOFFSET until the delta between
 * the first pulse and the calibration pulse is 60uS.
 *
 * Nothing else is needed.
 */
static uint8_t owbitbang_bit_xch(uint8_t b)
{
    uint8_t intsave;

    intsave = (SREG & _BV(SREG_I)) == _BV(SREG_I);
    g_irq_disable();

    OW_OUT_LOW();
    OW_DIR_OUT();      /* Drive bus low */

    _delay_us(2);     /* T_INT > 1usec accoding to timing-diagramm */
    if (b)
    {
        OW_DIR_IN();   /* To write "1" release bus, resistor pulls high */
        OW_OUT_HIGH();
    }

    /* "Output data from the DS18B20 is valid for 15usec after the falling
     * edge that initiated the read time slot. Therefore, the master must
     * release the bus and then sample the bus state within 15ussec from
     * the start of the slot."
     */

    _delay_us(15-2+OW_CONF_DELAYOFFSET);

    if (OW_GET_IN() == 0)
    {
        b = 0;  /* Sample at end of read-timeslot */
    }

    _delay_us(60-15-2+OW_CONF_DELAYOFFSET);

    /* CALIBRATION PULSE GOES HERE */
    
    OW_OUT_HIGH();
    OW_DIR_IN();

    if (intsave)
        g_irq_enable();

    _delay_us(OW_RECOVERY_TIME); /* May be increased for longer wires */

    return b;
}

bool owbitbang_bit_io(bool *bit)
{
    *bit = owbitbang_bit_xch(*bit);
    return true;
}

uint8_t owbitbang_byte_xch(uint8_t b)
{
    uint8_t i = 8;
    uint8_t j;

    do
    {
        j = owbitbang_bit_xch(b & 1);
        b >>= 1;
        if (j)
            b |= 0x80;
    } while (--i);

    return b;
}

bool owbitbang_read(uint8_t *buf, uint8_t len)
{
    while (len--)
        *buf++ = owbitbang_byte_xch(0xFF);

    return true;
}

bool owbitbang_read_byte(uint8_t *ret)
{
    *ret = owbitbang_byte_xch(0xFF);
    return true;
}

uint8_t owbitbang_rom_search(uint8_t diff, uint8_t *id)
{
    bool presense;
    uint8_t i;
    uint8_t j;
    uint8_t next_diff;
    uint8_t b;

    if (!owbitbang_bus_reset(&presense) || !presense)
        return OW_PRESENCE_ERR;                /* Error: No device found. early exit. */

    owbitbang_byte_xch(OW_SEARCH_ROM);         /* ROM search command */
    next_diff = OW_LAST_DEVICE;                /* Unchanged on last device */

    i = DS18X20_ROMCODE_SIZE * 8;                   /* 8 bytes */

    do
    {
        j = 8;                                 /* 8 bits */
        do
        {
            b = owbitbang_bit_xch(1);           /* Read bit */
            if (owbitbang_bit_xch(1))
            {                                  /* Read complement bit */
                if (b)
                {                              /* 0b11 */
                    return OW_DATA_ERR;        /* Data error. Early exit. */
                }
            }
            else
            {
                if (!b)
                {                              /* 0b00 = 2 devices */
                    if (diff > i ||            /* true if last result wasn't a discrepancy */
                      ((*id & 1) && diff != i) /* true when the the search has ended */)
                    {
                        b = 1;                 /* Use '1' on this pass */
                        next_diff = i;         /* Setup next pass to use '0' */
                    }
                }
            }

            owbitbang_bit_xch(b);               /* Write bit */
            *id >>= 1;

            if (b)
                *id |= 0x80;                   /* Store bit */

            i--;

        } while (--j);

        id++;                                  /* Next byte */

    } while (i);

    return next_diff;                          /* To continue search */
}

bool owbitbang_select(const uint8_t *id)
{
    bool presense;
    uint8_t i;

    if (!owbitbang_bus_reset(&presense) || !presense)
        return false;

    if (id)
    {
        owbitbang_byte_xch(OW_MATCH_ROM); /* To a single device */
        i = DS18X20_ROMCODE_SIZE;
        do
        {
            owbitbang_byte_xch(*id);
            id++;
        } while (--i);
    }
    else
    {
        owbitbang_byte_xch(OW_SKIP_ROM);  /* To all devices */
    }

    return true;
}

bool owbitbang_write(const uint8_t *data, uint8_t len)
{
    while (len--)
        owbitbang_byte_xch(*data++);
    
    return true;
}

#endif /* _OW_BITBANG_ */

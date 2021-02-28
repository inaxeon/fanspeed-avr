/*
 *   File:   i2c.c
 *   Author: Matthew Millman
 *
 *   Multi-project AVR I2C driver
 *
 *   Created on 11 May 2018, 11:45
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
#include <stdio.h>

#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>

#include "i2c.h"

#define I2C_PRESCALER 1
#define I2C_READ    1
#define I2C_WRITE   0

#ifdef _I2C_

void i2c_init(uint16_t freq_khz)
{
    TWSR = 0;
	TWBR = (uint8_t)((((F_CPU / freq_khz * 1000) / I2C_PRESCALER) - 16) / 2);
}

bool i2c_sync(void)
{
    uint16_t timeout = 500;

    while (!(TWCR & _BV(TWINT)) && timeout)
    {
        _delay_us(1);
        timeout--;
    }
    return (timeout != 0);
}

uint8_t i2c_wait_stop(void)
{
    uint16_t timeout = 500;

    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);

    while ((TWCR & _BV(TWSTO)) && timeout)
    {
        _delay_us(1);
        timeout--;
    }

    return (timeout != 0);
}

uint8_t i2c_start_wait(uint8_t addr)
{
    uint8_t twst;
    uint16_t retry = 100;

    while (1)
    {
        // send START condition
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);

        // wait until transmission completed
        if (!i2c_sync())
            break;

        // check value of TWI Status Register. Mask prescaler bits.
        twst = TW_STATUS & 0xF8;
        if ((twst != TW_START) && (twst != TW_REP_START))
            continue;

        // send device address
        TWDR = addr;
        TWCR = _BV(TWINT) | _BV(TWEN);

        // wail until transmission completed
        if (!i2c_sync())
            break;

        // check value of TWI Status Register. Mask prescaler bits.
        twst = TW_STATUS & 0xF8;
        if ((twst == TW_MT_SLA_NACK) || (twst == TW_MR_DATA_NACK))
        {
            /* device busy, send stop condition to terminate write operation */
            if (!i2c_wait_stop())
                continue;

            if (!(retry--))
                break;

            continue;
        }

        return true;
        break;
    }

    return false;
}

static bool i2c_byte_out(uint8_t data)
{
    uint8_t twst;

    // send data to the previously addressed device
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);

    // wait until transmission completed
    i2c_sync();

    // check value of TWI Status Register. Mask prescaler bits
    twst = TW_STATUS & 0xF8;
    if (twst != TW_MT_DATA_ACK)
        return false;

    return true;
}

static bool i2c_read_ack(uint8_t *ret)
{
    bool result;
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
    result = i2c_sync();
    *ret = TWDR;
    return result;
}

static bool i2c_read_nack(uint8_t *ret)
{
    bool result;
    TWCR = _BV(TWINT) | _BV(TWEN);
    result = i2c_sync();
    *ret = TWDR;
    return result;
}

#ifdef _I2C_XFER_

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *ret)
{
    if (!i2c_start_wait(addr << 1 | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(reg))
        goto fail;

    if (!i2c_start_wait((addr << 1 | I2C_READ)))
        goto fail;

    if (!i2c_read_nack(ret))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

bool i2c_write(uint8_t addr, uint8_t reg, uint8_t data)
{
    if (!i2c_start_wait((addr << 1) | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(reg))
        goto fail;

    if (!i2c_byte_out(data))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

#endif /* _I2C_XFER_ */

#ifdef _I2C_XFER_BYTE_

bool i2c_read_byte(uint8_t addr, uint8_t *ret)
{
    if (!i2c_start_wait((addr << 1) | I2C_READ))
        goto fail;

    if (!i2c_read_nack(ret))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

bool i2c_write_byte(uint8_t addr, uint8_t data)
{
    if (!i2c_start_wait((addr << 1) | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(data))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

#endif /* _I2C_XFER_BYTE_ */

#ifdef _I2C_XFER_MANY_

bool i2c_write_buf(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (!i2c_start_wait((addr << 1) | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(reg))
        goto fail;

    for (i = 0; i < len; i++)
    {
        if (!i2c_byte_out(*(data + i)))
            goto fail;
    }

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

bool i2c_read_buf(uint8_t addr, uint8_t reg, uint8_t *ret, uint8_t len)
{
    if (!i2c_start_wait(addr << 1 | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(reg))
        goto fail;

    if (!i2c_start_wait((addr << 1 | I2C_READ)))
        goto fail;

    while (len > 1)
    {
        if (!i2c_read_ack(ret))
            goto fail;
                
        ret++;
        len--;
    }

    if (!i2c_read_nack(ret))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

#endif /* _I2C_XFER_MANY_ */

#ifdef _I2C_XFER_X16_

bool i2c_write16(uint8_t addr, uint8_t reg, uint16_t data)
{
    if (!i2c_start_wait((addr << 1) | I2C_WRITE))
        goto fail;
    
    if (!i2c_byte_out(reg))
        goto fail;

    if (!i2c_byte_out(*((uint8_t *)&data + 1)))
        goto fail;

    if (!i2c_byte_out(*((uint8_t *)&data)))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

bool i2c_read16(uint8_t addr, uint8_t reg, uint16_t *ret)
{
    if (!i2c_start_wait(addr << 1 | I2C_WRITE))
        goto fail;

    if (!i2c_byte_out(reg))
        goto fail;

    if (!i2c_start_wait((addr << 1 | I2C_READ)))
        goto fail;

    if (!i2c_read_ack((uint8_t *)ret + 1))
        goto fail;

    if (!i2c_read_nack((uint8_t *)ret))
        goto fail;

    return i2c_wait_stop();
fail:
    i2c_wait_stop();
    return false;
}

#endif /* _I2C_XFER_X16_ */

#ifdef _I2C_DS2482_SPECIAL_

bool i2c_await_flag(uint8_t addr, uint8_t mask, uint8_t *ret, uint8_t attempts)
{
    uint8_t status;

    if (!i2c_start_wait((addr << 1) | I2C_READ))
        goto fail;

    while (attempts--)
    {
        if (!i2c_read_ack(&status))
            goto fail;

        if (!(status & mask))
            break;
    }

    if (!i2c_read_nack(&status))
        goto fail;

    i2c_wait_stop();
    *ret = status;
    return !(status & mask);

fail:
    i2c_wait_stop();
    return false;
}

#endif /* _I2C_DS2482_SPECIAL_ */

#endif /* _I2C_ */
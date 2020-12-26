/*
 *   File:   i2c.h
 *   Author: Matthew Millman
 *
 *   FNP600/850/1000 Adapter Board
 *
 *   Created on 12 August 2015, 12:02
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

#ifndef __I2C_H__
#define __I2C_H__

void i2c_init(uint16_t freq_khz);

#ifdef _I2C_BRUTEFORCE_RESET_
void i2c_bruteforce_reset(void);
#endif /* _I2C_BRUTEFORCE_RESET_ */

#ifdef _I2C_XFER_
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *ret);
bool i2c_write(uint8_t addr, uint8_t reg, uint8_t data);
#endif /* _I2C_XFER_ */

#ifdef _I2C_XFER_BYTE_
bool i2c_write_byte(uint8_t addr, uint8_t data);
bool i2c_read_byte(uint8_t addr, uint8_t *ret);
#endif /* _I2C_XFER_BYTE_ */

#ifdef _I2C_XFER_MANY_
bool i2c_read_buf(uint8_t addr, uint8_t offset, uint8_t *ret, uint8_t len);
bool i2c_write_buf(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);
#endif /* I2C_XFER_MANY */

#ifdef _I2C_XFER_MANY_TO_UART_
bool i2c_read_to_uart(uint8_t addr, uint8_t offset, uint8_t len);
#endif /* _I2C_XFER_MANY_TO_UART_ */

#ifdef _I2C_XFER_X16_
bool i2c_read16(uint8_t addr, uint8_t reg, uint16_t *ret);
bool i2c_write16(uint8_t addr, uint8_t reg, uint16_t data);
#endif /* _I2C_XFER_X16_ */

#ifdef _I2C_DS2482_SPECIAL_
bool i2c_await_flag(uint8_t addr, uint8_t mask, uint8_t *ret, uint8_t attempts);
#endif /* _I2C_DS2482_SPECIAL_ */

#endif /* __I2C_H__ */
/*
 *   File:   ds18x20.c
 *   Author: Matthew Millman
 *
 *   Created on 26 July 2016, 11:13
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
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "config.h"
#include "onewire.h"
#include "ow_bitbang.h"
#include "ds18x20.h"
#include "ds2482.h"
#include "crc8.h"
#include "util.h"

#define OW_SEARCH_FIRST                 0xFF
#define OW_PRESENCE_ERR                 0xFF
#define OW_DATA_ERR                     0xFE
#define OW_LAST_DEVICE                  0x00

#define DS18B20_READ_ROM                0x33
#define DS18B20_SP_SIZE                 9
#define DS18B20_READ                    0xBE
#define DS18B20_CONVERT_T               0x44

#define DS18B20_CONF_REG                4
#define DS18B20_9_BIT                   0
#define DS18B20_10_BIT                  (1 << 5)
#define DS18B20_11_BIT                  (1 << 6)
#define DS18B20_12_BIT                  ((1 << 6) | (1 << 5))
#define DS18B20_RES_MASK                ((1 << 6) | (1 << 5))
#define DS18B20_9_BIT_UNDF              ((1 << 0) | (1 << 1) | (1 << 2))
#define DS18B20_10_BIT_UNDF             ((1 << 0) | (1 << 1))
#define DS18B20_11_BIT_UNDF             ((1 << 0))

#define DS18B20_INVALID_DECICELSIUS     0x7FFF

#ifdef _DS18B20_AUTHCHECK_
static void ds18b20_print_array(uint8_t *data, int n, char sep);
static void ds18b20_print_hex(uint8_t value);
static void ds18b20_send_reset(uint8_t *addr);
static uint8_t ds18b20_one_byte_return(uint8_t *addr, uint8_t cmd);
static uint8_t ds18b20_bit_invert(uint8_t a);
static void ds18b20_param2trim(uint16_t offset_param_11bit, uint8_t curve_param_5bit, uint8_t *param1, uint8_t *param2);
static void ds18b20_trim2param(uint8_t param1, uint8_t param2, uint16_t *offset_param_11bit, uint8_t *curve_param_5bit);
static void ds18b20_get_trim_A(uint8_t *addr, uint8_t *trim1, uint8_t *trim2);
static void ds18b20_set_trim_A(uint8_t *addr, uint8_t trim1, uint8_t trim2);
static void ds18b20_get_trim_params_A(uint8_t *addr, uint16_t *offset_param_11bit, uint8_t *curve_param_5bit);
static void ds18b20_set_trim_params_A(uint8_t *addr, uint16_t offset_param_11bit, uint8_t curve_param_5bit);
static bool ds18b20_is_valid_A_scratchpad(uint8_t *buff);
static bool ds18b20_is_all_00(uint8_t *buff, int N);
static void ds18b20_trigger_convert(uint8_t *addr, uint8_t conf, uint16_t wait);
static int ds18b20_curve_param_prop(uint8_t *addr);
#endif /* _DS18B20_AUTHCHECK_ */

static bool ds18b20_read_scratchpad(uint8_t *id, uint8_t *sp, uint8_t n)
{
    uint8_t data = DS18B20_READ;

    if (!ow_select(id))
        return false;

    if (!ow_write(&data, 1))
        return false;

    if (!ow_read(sp, n))
        return false;

    if (crc8(sp, DS18B20_SP_SIZE))
        return false;

    return true;
}

/* Convert scratchpad data to physical value in unit decicelsius. Default 12 bit conversion is assumed. */
static int16_t ds18b20_raw_to_decicelsius(uint8_t *sp)
{
    uint16_t measure;
    uint8_t negative;
    int16_t decicelsius;
    uint16_t fract;

    measure = sp[0] | (sp[1] << 8);

    /* Check for negative */
    if (measure & 0x8000)
    {
        negative = 1;      /* Mark negative */
        measure ^= 0xffff; /* convert to positive => (twos complement)++ */
        measure++;
    }
    else
    {
        negative = 0;
    }

        /* Clear undefined bits for DS18B20 != 12bit resolution */
    switch(sp[DS18B20_CONF_REG] & DS18B20_RES_MASK)
    {
    case DS18B20_9_BIT:
        measure &= ~(DS18B20_9_BIT_UNDF);
        break;
    case DS18B20_10_BIT:
        measure &= ~(DS18B20_10_BIT_UNDF);
        break;
    case DS18B20_11_BIT:
        measure &= ~(DS18B20_11_BIT_UNDF);
        break;
    default:
        /* 12 bit - all bits valid */
        break;
    }

    decicelsius = (measure >> 4);
    decicelsius *= 10;

    /* decicelsius += ((measure & 0x000F) * 640 + 512) / 1024;
     * 625/1000 = 640/1024
     */
    fract = (measure & 0x000F) * 640;
    if (!negative)
        fract += 512;
    fract /= 1024;
    decicelsius += fract;

    if (negative)
        decicelsius = -decicelsius;

    if (/* decicelsius == 850 || */ decicelsius < -550 || decicelsius > 1250)
        return DS18B20_INVALID_DECICELSIUS;

    return decicelsius;
}

bool ds18b20_read_decicelsius(uint8_t *id, int16_t *decicelsius)
{
    int16_t ret;
    uint8_t sp[DS18B20_SP_SIZE];

    if (!ds18b20_read_scratchpad(id, sp, DS18B20_SP_SIZE))
        return false;

    ret = ds18b20_raw_to_decicelsius(sp);

    if (ret == DS18B20_INVALID_DECICELSIUS)
        return false;

    *decicelsius = ret;
    return true;
}

bool ds18b20_start_meas(uint8_t *id)
{
    uint8_t data = DS18B20_CONVERT_T;

    if (!ow_select(id))
        return false;

    return ow_write(&data, 1);
}

#ifdef _DS18B20_AUTHCHECK_

// Taken from https://github.com/cpetrich/counterfeit_DS18B20
// Adapted to work outside of Arduino world

void ds18b20_authenticity_check(uint8_t *addr)
{
    bool presense_detect;

    // buffers for scratchpad register
    uint8_t buffer0[9];
    uint8_t buffer1[9];
    uint8_t buffer2[9];
    uint8_t buffer3[9];
    uint8_t buffer4[4];

    // flag to indicate if validation
    //  should be repeated at a different
    //  sensor temperature
    bool t_ok;

    int fake_flags = 0;

    ds18b20_print_array(addr, 8, ':');

    if (0 != crc8(addr, 8))
    {
        // some fake sensors can have their ROM overwritten with
        // arbitrary nonsense, so we don't expect anything good
        // if the ROM doesn't check out
        fake_flags += 1;
        printf(" (CRC Error -> Error.)");
    }

    if ((addr[6] != 0) || (addr[5] != 0) || (addr[0] != 0x28))
    {
        fake_flags += 1;
        printf(": ROM does not follow expected pattern 28:XX:XX:XX:XX:00:00:CRC. Error.");
    }
    else
    {
        printf(": ROM ok.");
    }

    printf("\r\n");

    if (!ds18b20_read_scratchpad(addr, buffer0, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buffer0, DS18B20_SP_SIZE);

    printf("  Scratchpad Register: ");
    ds18b20_print_array(buffer0, 9, '/');

    if (0 != crc8(buffer0, 9))
    {
        // Unlikely that a sensor will mess up the CRC of the scratchpad.
        // --> Assume we're dealing with a bad connection rather than a bad
        //     sensor, dump data, and move on to next sensor.
        printf("  CRC Error. Check connections or replace sensor.\r\n");
        return;
    }

    printf("\r\n");

    // Check content of user EEPROM. Since the EEPROM may have been programmed by the user earlier
    // we do not use this as a test. Rather, we dump this as info.
    printf("  Info only: Scratchpad bytes 2,3,4 (");
    ds18b20_print_array(buffer0 + 2, 3, '/');
    printf("): ");

    if ((buffer0[2] != 0x4b) || (buffer0[3] != 0x46) || (buffer0[4] != 0x7f))
        printf(" not Maxim default values 4B/46/7F.\r\n");
    else
        printf(" Maxim default values.\r\n");

    printf("  Scratchpad byte 5 (0x");
    ds18b20_print_hex(buffer0[5]);
    printf("): ");

    if (buffer0[5] != 0xff)
    {
        fake_flags += 1;
        printf(" should have been 0xFF according to datasheet. Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  Scratchpad byte 6 (0x");
    ds18b20_print_hex(buffer0[6]);
    printf("): ");

    if (((buffer0[6] == 0x00) || (buffer0[6] > 0x10)) ||                                                     // totall wrong value
        (((buffer0[0] != 0x50) || (buffer0[1] != 0x05)) && ((buffer0[0] != 0xff) || (buffer0[1] != 0x07)) && // check for valid conversion...
         (((buffer0[0] + buffer0[6]) & 0x0f) != 0x00)))
    { //...before assessing DS18S20 compatibility.
        fake_flags += 1;
        printf(" unexpected value. Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  Scratchpad byte 7 (0x");
    ds18b20_print_hex(buffer0[7]);
    printf("): ");

    if (buffer0[7] != 0x10)
    {
        fake_flags += 1;
        printf(" should have been 0x10 according to datasheet. Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    // set the resolution to 10 bit and modify alarm registers
    ow_bus_reset(&presense_detect);
    ow_select(addr);
    buffer4[0] = 0x4E;
    buffer4[1] = buffer0[2] ^ 0xff;
    buffer4[2] = buffer0[3] ^ 0xff;
    buffer4[3] = 0x3F;
    ow_write(buffer4, 4);

    if (!ds18b20_read_scratchpad(addr, buffer1, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buffer1, DS18B20_SP_SIZE);

    printf("  0x4E modifies alarm registers: ");

    if ((buffer1[2] != (buffer0[2] ^ 0xff)) || (buffer1[3] != (buffer0[3] ^ 0xff)))
    {
        fake_flags += 1;
        printf(" cannot modify content as expected (want: ");
        ds18b20_print_hex(buffer0[2] ^ 0xff);
        putch('/');
        ds18b20_print_hex(buffer0[3] ^ 0xff);
        printf(", got: ");
        ds18b20_print_array(buffer1 + 2, 2, '/');
        printf("). Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  0x4E accepts 10 bit resolution: ");

    if (buffer1[4] != 0x3f)
    {
        fake_flags += 1;
        printf(" rejected (want: 0x3F, got: ");
        ds18b20_print_hex(buffer1[4]);
        printf("). Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  0x4E preserves reserved bytes: ");

    if ((buffer1[5] != buffer0[5]) || (buffer1[6] != buffer0[6]) || (buffer1[7] != buffer0[7]))
    {
        fake_flags += 1;
        printf(" no, got: ");
        ds18b20_print_array(buffer1 + 5, 3, '/');
        printf(". Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    // set the resolution to 12 bit
    ow_bus_reset(&presense_detect);
    ow_select(addr);
    buffer4[0] = 0x4E;
    buffer4[1] = buffer0[2];
    buffer4[2] = buffer0[3];
    buffer4[3] = 0x7F;
    ow_write(buffer4, 4);

    if (!ds18b20_read_scratchpad(addr, buffer2, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buffer2, DS18B20_SP_SIZE);

    printf("  0x4E accepts 12 bit resolution: ");

    if (buffer2[4] != 0x7f)
    {
        fake_flags += 1;
        printf(" rejected (expected: 0x7F, got: ");
        ds18b20_print_hex(buffer2[4]);
        printf("). Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  0x4E preserves reserved bytes: ");

    if ((buffer2[5] != buffer1[5]) || (buffer2[6] != buffer1[6]) || (buffer2[7] != buffer1[7]))
    {
        fake_flags += 1;
        printf(" no, got: ");
        ds18b20_print_array(buffer2 + 5, 3, '/');
        printf(". Error.\r\n");
    }
    else
    {
        printf(" ok.\r\n");
    }

    printf("  Checking byte 6 upon temperature change: ");

    if ((((buffer2[0] == 0x50) && (buffer2[1] == 0x05)) || ((buffer2[0] == 0xff) && (buffer2[1] == 0x07)) ||
         ((buffer2[6] == 0x0c) && (((buffer2[0] + buffer2[6]) & 0x0f) == 0x00))) &&
        ((buffer2[6] >= 0x00) && (buffer2[6] <= 0x10)))
    {
        // byte 6 checked out as correct in the initial test but the test ambiguous.
        //   we need to check if byte 6 is consistent after temperature conversion

        // We'll do a few temperature conversions in a row.
        // Usually, the temperature rises slightly if we do back-to-back
        //   conversions.
        int count = 5;

        do
        {
            count--;

            if (count < 0)
                break;

            // perform temperature conversion
            ow_bus_reset(&presense_detect);
            ow_select(addr);
            buffer4[0] = 0x44;
            ow_write(buffer4, 1);
            _delay_ms(750);

            if (!ds18b20_read_scratchpad(addr, buffer3, DS18B20_SP_SIZE))
                ds18b20_read_scratchpad(addr, buffer3, DS18B20_SP_SIZE);

        } while (((buffer3[0] == 0x50) && (buffer3[1] == 0x05)) || ((buffer3[0] == 0xff) && (buffer3[1] == 0x07)) ||
                 ((buffer3[6] == 0x0c) && (((buffer3[0] + buffer3[6]) & 0x0f) == 0x00)));

        if (count < 0)
        {
            printf(" Inconclusive. Please change sensor temperature and repeat.\r\n");
            t_ok = false;
        }
        else
        {
            t_ok = true;
            if ((buffer3[6] != 0x0c) && (((buffer3[0] + buffer3[6]) & 0x0f) == 0x00))
            {
                printf(" ok.\r\n");
            }
            else
            {
                fake_flags += 1;
                printf(" Temperature LSB = 0x");
                ds18b20_print_hex(buffer3[0]);
                printf(" but byte 6 = 0x");
                ds18b20_print_hex(buffer3[6]);
                printf(". Error.\r\n");
            }
        }
    }
    else
    {
        printf("not necessary. Skipped.\r\n");
        t_ok = true;
    }

    printf("  --> ");

    if (!t_ok)
    {
        printf("DS18S20 counterfeit test not completed, otherwise sensor");
    }
    else
    {
        printf("Sensor");
    }

    if (fake_flags == 0)
    {
        printf(" responded like a genuie Maxim.\r\n");
    }
    else
    {
        printf(" appears to be counterfeit based on %u deviations.\r\n", fake_flags);
        if (fake_flags == 1)
        {
            printf("  The number of deviations is unexpectedly small.\r\n");
        }
    }
}

void ds18b20_classify_sensor(uint8_t *addr)
{
    bool presense_detect;
    uint8_t buffer4[4];

    // dump ROM
    ds18b20_print_array(addr, 8, ':');

    if (0 != crc8(addr, 8))
        printf(" (CRC Error)");

    printf(":");

    int identified = 0;

    { // test for family A
        uint8_t r68 = ds18b20_one_byte_return(addr, 0x68);
        ds18b20_one_byte_return(addr, 0x93);
        if (r68 != 0xff)
        {
            int cpp = ds18b20_curve_param_prop(addr);
            if (cpp == 1)
                printf(" Family A1 (Genuie Maxim)."); // unsigned and 3.8 oC range
            else if (cpp == 2)
                printf(" Family A2 (Clone)."); // signed and 32 oC range
            else if (cpp == -3)
                printf(" (Error reading scratchpad register [A].)");
            else if (cpp == -1)
            {
                printf(" Family A, unknown subtype (0x93=");
                ds18b20_print_hex(ds18b20_one_byte_return(addr, 0x93));
                printf(", 0x68=");
                ds18b20_print_hex(r68);
                printf(").");
            }
            // cpp==0 or cpp==-2: these aren't Family A as we know them.
            if ((cpp == 1) || (cpp == 2) || (cpp == -1))
                identified++;
        }
        ds18b20_send_reset(addr);
    }

    { // test for family B
        uint8_t r97 = ds18b20_one_byte_return(addr, 0x97);
        if (r97 == 0x22)
            printf(" Family B1 (Clone).");
        else if (r97 == 0x31)
            printf(" Family B2 (Clone).");
        else if (r97 != 0xFF)
        {
            printf(" Family B (Clone), unknown subtype (0x97=");
            ds18b20_print_hex(r97);
            printf(").");
        }
        if (r97 != 0xff)
            identified++;
    }

    { // test for family D
        uint8_t r8B = ds18b20_one_byte_return(addr, 0x8B);
        // mention if parasitic power is known not to work.
        if (r8B == 0x06)
            printf(" Family D1 (Clone w/o parasitic power).");
        else if (r8B == 0x02)
            printf(" Family D1 (Clone).");
        else if (r8B == 0x00)
            printf(" Family D2 (Clone w/o parasitic power).");
        else if (r8B != 0xFF)
        {
            printf(" Family D (Clone), unknown subtype (0x8B=");
            ds18b20_print_hex(r8B);
            printf(").");
        }
        if (r8B != 0xff)
            identified++;
    }

    {
        // this should be Family C by implication.
        // Don't have a test for response to undocumented codes, so check if
        // config register is constant, which is unique property among 2019 Families.
        uint8_t buff[9];
        uint8_t cfg1, cfg2;

        ow_bus_reset(&presense_detect);
        ow_select(addr);
        buffer4[0] = 0x4E;
        buffer4[1] = 0xaa;
        buffer4[2] = 0x55;
        buffer4[3] = 0x00;
        ow_write(buffer4, 4);

        if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
            ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

        if (ds18b20_is_all_00(buff, 9) || (0 != crc8(buff, 9)))
            goto err_C;

        cfg1 = buff[4];

        ow_bus_reset(&presense_detect);
        ow_select(addr);
        buffer4[0] = 0x4E;
        buffer4[1] = 0xaa;
        buffer4[2] = 0x55;
        buffer4[3] = 0xff;
        ow_write(buffer4, 4);

        if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
            ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

        if (ds18b20_is_all_00(buff, 9) || (0 != crc8(buff, 9)))
            goto err_C;

        cfg2 = buff[4];

        ds18b20_send_reset(addr);

        if (cfg1 == cfg2)
        {
            printf(" Family C (Clone).");
            identified++;
        }
        if (0)
        {
        err_C:
            printf(" (Error reading scratchpad register [C].)");
        }
    }

    if (identified == 0)
        printf(" (Could not identify Family.)");

    if (identified > 1)
        printf(" (Part may belong to a Family not seen in 2019.)");

    printf("\r\n\r\n");
}

static void ds18b20_print_array(uint8_t *data, int n, char sep)
{
    int idx;
    for (idx = 0; idx < n; idx++)
    {
        printf("%02X", data[idx]);
        if (idx != n - 1)
            putch(sep);
    }
}

static void ds18b20_print_hex(uint8_t value)
{
    printf("%02X", value);
}

static void ds18b20_send_reset(uint8_t *addr)
{
    bool presense_detect;
    uint8_t buffer;

    ow_bus_reset(&presense_detect);
    ow_select(addr);
    buffer = 0x64;
    ow_write(&buffer, 1);
}

static uint8_t ds18b20_one_byte_return(uint8_t *addr, uint8_t cmd)
{
    uint8_t buffer;
    bool presense_detect;

    ow_bus_reset(&presense_detect);
    ow_select(addr);
    ow_write(&cmd, 1);
    ow_read(&buffer, 1);

    return buffer;
}

static uint8_t ds18b20_bit_invert(uint8_t a)
{
    uint8_t b = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        b *= 2;
        b += (a & 0x01);
        a /= 2;
    }

    return b;
}

static void ds18b20_param2trim(uint16_t offset_param_11bit, uint8_t curve_param_5bit, uint8_t *param1, uint8_t *param2)
{
    *param1 = ds18b20_bit_invert(offset_param_11bit & 0x0ff);
    *param2 = curve_param_5bit * 8 + offset_param_11bit / 256;
}

static void ds18b20_trim2param(uint8_t param1, uint8_t param2, uint16_t *offset_param_11bit, uint8_t *curve_param_5bit)
{
    *offset_param_11bit = ds18b20_bit_invert(param1) + ((uint16_t)(param2 & 0x07)) * 256;
    *curve_param_5bit = param2 / 8;
}

static void ds18b20_get_trim_A(uint8_t *addr, uint8_t *trim1, uint8_t *trim2)
{
    bool presense_detect;
    uint8_t buffer;
    
    ow_bus_reset(&presense_detect);

    if (addr)
    {
        ow_select(addr);
    }
    else
    {
        buffer = 0xCC;
        ow_write(&buffer, 1);
    }

    buffer = 0x93;
    ow_write(&buffer, 1);
    ow_read(trim1, 1);
    ow_bus_reset(&presense_detect);

    if (addr)
    {
        ow_select(addr);
    }
    else
    {
        buffer = 0xCC;
        ow_write(&buffer, 1);
    }

    buffer = 0x68;
    ow_write(&buffer, 1);
    ow_read(trim2, 1);
}

static void ds18b20_set_trim_A(uint8_t *addr, uint8_t trim1, uint8_t trim2)
{
    bool presense_detect;
    uint8_t buffer[2];

    ow_bus_reset(&presense_detect);
    ow_select(addr);
    
    buffer[0] = 0x95;
    buffer[1] = trim1;

    ow_write(buffer, 2);
    ow_bus_reset(&presense_detect);
    ow_select(addr);

    buffer[0] = 0x63;
    buffer[1] = trim2;

    ow_write(buffer, 2);
    // don't store permanently, don't call reset
}

static void ds18b20_get_trim_params_A(uint8_t *addr, uint16_t *offset_param_11bit, uint8_t *curve_param_5bit)
{
    uint8_t trim1, trim2;

    ds18b20_get_trim_A(addr, &trim1, &trim2);
    ds18b20_trim2param(trim1, trim2, offset_param_11bit, curve_param_5bit);
}

static void ds18b20_set_trim_params_A(uint8_t *addr, uint16_t offset_param_11bit, uint8_t curve_param_5bit)
{
    uint8_t trim1, trim2;

    ds18b20_param2trim(offset_param_11bit, curve_param_5bit, &trim1, &trim2);
    ds18b20_set_trim_A(addr, trim1, trim2);
}

static bool ds18b20_is_valid_A_scratchpad(uint8_t *buff)
{
    if ((buff[4] != 0x7f) && (buff[4] != 0x5f) && (buff[4] != 0x3f) && (buff[4] != 0x1f))
        return false;

    if ((buff[0] == 0x50) && (buff[1] == 0x05) && (buff[6] == 0x0C))
        return true; // power-up

    if ((buff[0] == 0xff) && (buff[1] == 0x07) && (buff[6] == 0x0C))
        return true; // unsuccessful conversion

    return buff[6] == (0x10 - (buff[0] & 0x0f));
}

static bool ds18b20_is_all_00(uint8_t *buff, int N)
{
    int i;
    for (i = 0; i < N; i++)
    {
        if (buff[i] != 0x00)
            return false;
    }

    return true;
}

static void ds18b20_trigger_convert(uint8_t *addr, uint8_t conf, uint16_t wait)
{
    bool presense_detect;
    uint8_t buffer[4];

    ow_bus_reset(&presense_detect);
    ow_select(addr);
    
    buffer[0] = 0x4E;
    buffer[1] = 0xaf;
    buffer[2] = 0xfe;
    buffer[3] = conf;

    ow_write(buffer, 4);
    ow_bus_reset(&presense_detect);
    ow_select(addr);

    buffer[0] = 0x44;

    ow_write(buffer, 1); // start conversion and keep data line high in case we need parasitic power

    for (uint16_t i = 0; i < wait; i++)
        _delay_ms(1);
}

static int ds18b20_curve_param_prop(uint8_t *addr)
{
    // check if the curve parameter is unsigned (1) signed (2), temperature range inconclusive (-1), doesn't seem to be A (-2), CRC error (-3), or doesn't seem to do anything (0)
    uint8_t buff[9];
    uint16_t off = 0x3ff; // half way

    ds18b20_set_trim_params_A(addr, off, 0x0f);
    ds18b20_trigger_convert(addr, 0x7f, 800);

    if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

    if (0 != crc8(buff, 9))
        return -3;

    if (!ds18b20_is_valid_A_scratchpad(buff))
        return -2;

    int16_t r0f = buff[0] + 256 * buff[1];
    {
        // check if we can set and read trim parameters
        uint16_t o;
        uint8_t c;
        
        ds18b20_get_trim_params_A(addr, &o, &c);

        if ((o != off) || (c != 0x0f))
            return 0;
    }

    ds18b20_set_trim_params_A(addr, off, 0x00);
    ds18b20_trigger_convert(addr, 0x7f, 800);

    if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

    if (0 != crc8(buff, 9))
        return -3;

    if (!ds18b20_is_valid_A_scratchpad(buff))
        return -2;

    int16_t r00 = buff[0] + 256 * buff[1];

    ds18b20_set_trim_params_A(addr, off, 0x1f);
    ds18b20_trigger_convert(addr, 0x7f, 800);
    if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

    if (0 != crc8(buff, 9))
        return -3;

    if (!ds18b20_is_valid_A_scratchpad(buff))
        return -2;

    int16_t r1f = buff[0] + 256 * buff[1];

    ds18b20_set_trim_params_A(addr, off, 0x10);
    ds18b20_trigger_convert(addr, 0x7f, 800);
    if (!ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE))
        ds18b20_read_scratchpad(addr, buff, DS18B20_SP_SIZE);

    if (0 != crc8(buff, 9))
        return -3;

    if (!ds18b20_is_valid_A_scratchpad(buff))
        return -2;

    int16_t r10 = buff[0] + 256 * buff[1];
    int16_t mini = min_(r00, min_(r0f, min_(r10, r1f)));
    int16_t maxi = max_(r00, max_(r0f, max_(r10, r1f)));
    bool is_signed = (r0f - r10 > r1f - r00);
    bool is_unsigned = (r0f - r10 < r1f - r00);

    if (is_signed && (maxi - mini > 20 * 16))
        return 2; // A2

    if (is_unsigned && (maxi - mini > 1 * 16) && (maxi - mini < 6 * 16))
        return 1; // A1

    return -1;
}

#endif /* _DS18B20_AUTHCHECK_ */

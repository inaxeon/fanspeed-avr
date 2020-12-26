/*
 *   File:   config.c
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "util.h"
#include "usart.h"
#include "onewire.h"
#include "ds18x20.h"
#include "i2c.h"

#define CMD_NONE              0x00
#define CMD_READLINE          0x01
#define CMD_COMPLETE          0x02
#define CMD_ESCAPE            0x03
#define CMD_AWAIT_NAV         0x04
#define CMD_PREVCOMMAND       0x05
#define CMD_NEXTCOMMAND       0x06
#define CMD_DEL               0x07
#define CMD_DROP_NAV          0x08
#define CMD_CANCEL            0x10

#define CTL_CANCEL            0x03
#define CTL_XOFF              0x13
#define CTL_U                 0x15

#define SEQ_ESCAPE_CHAR       0x1B
#define SEQ_CTRL_CHAR1        0x5B
#define SEQ_ARROW_UP          0x41
#define SEQ_ARROW_DOWN        0x42
#define SEQ_HOME              0x31
#define SEQ_INS               0x32
#define SEQ_DEL               0x33
#define SEQ_END               0x34
#define SEQ_PGUP              0x35
#define SEQ_PGDN              0x36
#define SEQ_NAV_END           0x7E

#define CMD_MAX_LINE          64
#define CMD_MAX_HISTORY       4

#define PARAM_I16_1DP_TEMP    0
#define PARAM_U16             1
#define PARAM_U16_1DP_TEMPMAX 2
#define PARAM_U8_PCT          3
#define PARAM_U8_BIT          4
#define PARAM_U8_SIDX         5
#define PARAM_U8_TCNT         6
#define PARAM_U8_FCNT         7
#define PARAM_DESC            8

static inline int8_t configuration_prompt_handler(char *message, sys_config_t *config);
static int8_t get_line(char *str, int8_t max, uint8_t *ignore_lf);
static uint8_t parse_param(void *param, uint8_t type, char *arg);
static void save_configuration(sys_config_t *config);
static void default_configuration(sys_config_t *config);
static void do_readtemp(void);
static int8_t parse_owid(uint8_t *param, char *arg);

uint8_t _g_max_history;
uint8_t _g_show_history;
uint8_t _g_next_history;
char _g_cmd_history[CMD_MAX_HISTORY][CMD_MAX_LINE];

void configuration_bootprompt(sys_config_t *config)
{
    char cmdbuf[64];
    uint8_t i;
    int8_t enter_bootpromt = 0;
    uint8_t ignore_lf = 0;

    printf("<Press Ctrl+C to enter configuration prompt>\r\n");

    for (i = 0; i < 100; i++)
    {
        if (usart1_data_ready())
        {
            char c = usart1_get();
            if (c == 3) /* Ctrl + C */
            {
                enter_bootpromt = 1;
                break;
            }
        }
        delay_10ms(1);
    }

    if (!enter_bootpromt)
        return;

    printf("\r\n");
    
    for (;;)
    {
        int8_t ret;

        ret = get_line(cmdbuf, sizeof(cmdbuf), &ignore_lf);

        if (ret == 0 || ret == -1) {
            printf("\r\n");
            continue;
        }

        ret = configuration_prompt_handler(cmdbuf, config);

        if (ret > 0)
            printf("Error: command failed\r\n");

        if (ret == -1) {
            return;
        }

        /* Hack to update PWMs while in the menu.
         * Assists with configuration
         */
        set_start_duty(config);
    }
}

static void do_show(sys_config_t *config)
{
    fixedpoint_sign(config->temp_max, temp_max);
    fixedpoint_sign(config->temp_min, temp_min);

    printf(
            "\r\nCurrent configuration:\r\n\r\n"
            "\tnumfans ...........: %u\r\n"
            "\tfansmax ...........: %u\r\n"
            "\tfansmin ...........: %u\r\n"
            "\tfansstart .........: %u\r\n"
            "\tfansminrpm ........: %u\r\n"
            "\tfansminoff ........: %u\r\n"
            "\r\n"
            "\tmintemps ..........: %u\r\n"
            "\ttempmax ...........: %s%u.%u\r\n"
            "\ttempmin ...........: %s%u.%u\r\n"
            "\ttemphyst ..........: %u.%u\r\n"
            "\ttemp1desc .........: %s\r\n"
            "\ttemp2desc .........: %s\r\n"
            "\ttemp3desc .........: %s\r\n"
            "\ttemp4desc .........: %s\r\n"
            "\r\n"
            "\tmanualassignment ..: %u\r\n"
            "\tsensor1addr .......: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n"
            "\tsensor2addr .......: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n"
            "\tsensor3addr .......: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n"
            "\tsensor4addr .......: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n"
#ifdef _I2C_SLAVE_
            "\r\n"
            "\tslaveindex ........: %u\r\n"
            "\tallowoverride .....: %u\r\n"
#endif /* _I2C_SLAVE_ */
          , config->num_fans,
            config->fans_max,
            config->fans_min,
            config->fans_start,
            config->fans_minrpm,
            config->fans_minoff,
            config->min_temps,
            fixedpoint_arg(config->temp_max, temp_max),
            fixedpoint_arg(config->temp_min, temp_min),
            fixedpoint_arg_u(config->temp_hyst),
            config->temp1_desc,
            config->temp2_desc,
            config->temp3_desc,
            config->temp4_desc,
            config->manual_assignment,
            config->sensor1_addr[0]
#if DS18X20_ROMCODE_SIZE == 8
          , config->sensor1_addr[1],
            config->sensor1_addr[2],
            config->sensor1_addr[3],
            config->sensor1_addr[4],
            config->sensor1_addr[5],
            config->sensor1_addr[6],
            config->sensor1_addr[7]
#else
          , 0, 0, 0, 0, 0, 0, 0
#endif /* DS18X20_ROMCODE_SIZE == 8 */
          , config->sensor2_addr[0]
#if DS18X20_ROMCODE_SIZE == 8
          , config->sensor2_addr[1],
            config->sensor2_addr[2],
            config->sensor2_addr[3],
            config->sensor2_addr[4],
            config->sensor2_addr[5],
            config->sensor2_addr[6],
            config->sensor2_addr[7]
#else
          , 0, 0, 0, 0, 0, 0, 0
#endif /* DS18X20_ROMCODE_SIZE == 8 */
          , config->sensor3_addr[0]
#if DS18X20_ROMCODE_SIZE == 8
          , config->sensor3_addr[1],
            config->sensor3_addr[2],
            config->sensor3_addr[3],
            config->sensor3_addr[4],
            config->sensor3_addr[5],
            config->sensor3_addr[6],
            config->sensor3_addr[7]
#else
          , 0, 0, 0, 0, 0, 0, 0
#endif /* DS18X20_ROMCODE_SIZE == 8 */
          , config->sensor4_addr[0]
#if DS18X20_ROMCODE_SIZE == 8
          , config->sensor4_addr[1],
            config->sensor4_addr[2],
            config->sensor4_addr[3],
            config->sensor4_addr[4],
            config->sensor4_addr[5],
            config->sensor4_addr[6],
            config->sensor4_addr[7]
#else
          , 0, 0, 0, 0, 0, 0, 0
#endif /* DS18X20_ROMCODE_SIZE == 8 */
#ifdef _I2C_SLAVE_
          , config->i2c_index
          , config->allow_override
#endif /* _I2C_SLAVE_ */
        );

    printf("\r\n");
}

static void do_help(void)
{
    printf(
        "\r\nCommands:\r\n\r\n"
        "\tshow\r\n"
        "\t\tShow current configuration\r\n\r\n"
        "\tdefault\r\n"
        "\t\tLoad the default configuration\r\n\r\n"
        "\tsave\r\n"
        "\t\tSave current configuration\r\n\r\n"
        "\texit\r\n"
        "\t\tExit this menu and start\r\n\r\n"
        "\tnumfans [0 to %u]\r\n"
        "\t\tSets the number of fans connected\r\n\r\n"
        "\tfansmax [0 to 100]\r\n"
        "\t\tSets the maximum duty cycle for fans\r\n\r\n"
        "\tfansmin [0 to 100]\r\n"
        "\t\tSets the minimum duty cycle for fans\r\n\r\n"
        "\tfansstart [0 to 100]\r\n"
        "\t\tSets the duty cycle to use between reset and first calculation\r\n"
        "\t\tand when in the configuration prompt\r\n\r\n"
        "\tfansminrpm [0 to 65535]\r\n"
        "\t\tSets the stall-restart threshold RPM for all fans\r\n\r\n"
        "\tfansminoff [0 or 1]\r\n"
        "\t\tSet to '1' to power off fan below minimum temp\r\n\r\n"
        "\t\tStall checking is not performed when set to '1'\r\n\r\n"
#ifdef _6SENSOR_
        "\tmintemps [0 to 2]\r\n"
#else
        "\tmintemps [0 to 6]\r\n"
#endif
        "\t\tSets the number of expected temperature sensors\r\n\r\n"
        "\ttempmax [-55.0 to 125.0]\r\n"
        "\t\tSets the temperature at which fan is set to the maximum\r\n"
        "\t\tconfigured duty cycle\r\n\r\n"
        "\ttempmin [-55.0 to 125.0]\r\n"
        "\t\tSets the temperature threshold at which fan starts to\r\n"
        "\t\tincrease from the minimum configured duty cycle\r\n\r\n"
        "\ttemphyst [0 to 180.0]\r\n"
        "\t\tSets hysteresis when using 'minoff'. The fan will not switch off\r\n"
        "\t\tuntil current temp is less than temp1min, minus temp1hyst\r\n\r\n"
        "\ttemp1desc [desc]\r\n"
        "\ttemp2desc [desc]\r\n"
#ifdef _6SENSOR_
        "\ttemp3desc [desc]\r\n"
        "\ttemp4desc [desc]\r\n"
        "\ttemp5desc [desc]\r\n"
        "\ttemp6desc [desc]\r\n"
#endif
        "\t\tSets descriptions (15 chars max)\r\n\r\n"
        "\treadtemp\r\n"
#if defined(_I2C_MSSP_) || defined(_I2C_)
        "\t\tReset internal I2C Bus\r\n\r\n"
        "\ti2creset\r\n"
#endif
        "\t\tProbe and read out all attached sensors\r\n\r\n"
        "\tmanualassignment [0 or 1]\r\n"
        "\t\tSet to '1' to enable manual assignment of sensor address-to-index\r\n\r\n"
        "\tsensor1addr [addr or 'none']\r\n"
        "\tsensor2addr [addr or 'none']\r\n"
#ifdef _6SENSOR_
        "\tsensor3addr [addr or 'none']\r\n"
        "\tsensor4addr [addr or 'none']\r\n"
        "\tsensor5addr [addr or 'none']\r\n"
        "\tsensor6addr [addr or 'none']\r\n"
#endif /* _6SENSOR_ */
        "\t\tSets addresses of sensors\r\n\r\n"
#ifdef _I2C_SLAVE_
        "\tslaveindex [0-15]\r\n"
        "\t\tSets I2C slave address. Results as addresses 0x30-0x3F.\r\n\r\n"
        "\tallowoverride [0 or 1]\r\n"
        "\t\tAllows fan state to be set via I2C interface\r\n\r\n"
#endif /* _I2C_SLAVE_ */
    , MAXFANS);
}

static void do_readtemp(void)
{
    uint8_t i;
    uint8_t num_sensors;
    uint8_t sensor_ids[MAXSENSORS][DS18X20_ROMCODE_SIZE];
    int16_t reading;
    
    if (ds18x20_search_sensors(&num_sensors, sensor_ids))
        printf("\r\nFound %u of %u maximum sensors\r\n", num_sensors, MAXSENSORS);
    else
        printf("\r\nHardware error searching for sensors\r\n");

    if (!num_sensors)
        goto done;
    
    for (i = 0; i < num_sensors; i++)
        ds18x20_start_meas(sensor_ids[i]);

    delay_10ms(75);

    for (i = 0; i < num_sensors; i++)
    {
        if (ds18x20_read_decicelsius(sensor_ids[i], &reading))
        {
            fixedpoint_sign(reading, reading);

            printf(
                "\r\nSensor %u:\r\n"
                "\tTemp (C) .............: %s%u.%u\r\n"
                "\tBurned in ID .........: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                i + 1,
                fixedpoint_arg(reading, reading),
                sensor_ids[i][0]
#if DS18X20_ROMCODE_SIZE == 8
              , sensor_ids[i][1],
                sensor_ids[i][2],
                sensor_ids[i][3],
                sensor_ids[i][4],
                sensor_ids[i][5],
                sensor_ids[i][6],
                sensor_ids[i][7]
#else
              , 0, 0, 0, 0, 0, 0, 0
#endif /* DS18X20_ROMCODE_SIZE == 8 */
            );
        }
        else
        {
            printf("Failed to read sensor %u\r\n", i + 1);
        }
    }
    
done:
    printf("\r\n");
}

static inline int8_t configuration_prompt_handler(char *text, sys_config_t *config)
{
    char *command;
    char *arg;

    command = strtok(text, " ");
    arg = strtok(NULL, "");
        
    if (!stricmp(command, "numfans")) {
        return parse_param(&config->num_fans, PARAM_U8_FCNT, arg);
    }
    else if (!stricmp(command, "fansmax")) {
        return parse_param(&config->fans_max, PARAM_U8_PCT, arg);
    }
    else if (!stricmp(command, "fansmin")) {
        return parse_param(&config->fans_min, PARAM_U8_PCT, arg);
    }
    else if (!stricmp(command, "fansstart")) {
        return parse_param(&config->fans_start, PARAM_U8_PCT, arg);
    }
    else if (!stricmp(command, "fansminrpm")) {
        return parse_param(&config->fans_minrpm, PARAM_U16, arg);
    }
    else if (!stricmp(command, "mintemps")) {
        return parse_param(&config->min_temps, PARAM_U8_TCNT, arg);
    }
    else if (!stricmp(command, "tempmax")) {
        return parse_param(&config->temp_max, PARAM_I16_1DP_TEMP, arg);
    }
    else if (!stricmp(command, "tempmin")) {
        return parse_param(&config->temp_min, PARAM_I16_1DP_TEMP, arg);
    }
    else if (!stricmp(command, "temphyst")) {
        return parse_param(&config->temp_hyst, PARAM_U16_1DP_TEMPMAX, arg);
    }
    else if (!stricmp(command, "fansminoff")) {
        return parse_param(&config->fans_minoff, PARAM_U8_BIT, arg);
    }
    else if (!stricmp(command, "temp1desc")) {
        return parse_param(config->temp1_desc, PARAM_DESC, arg);
    }
    else if (!stricmp(command, "temp2desc")) {
        return parse_param(config->temp2_desc, PARAM_DESC, arg);
    }
#ifdef _6SENSOR_
    else if (!stricmp(command, "temp3desc")) {
        return parse_param(config->temp3_desc, PARAM_DESC, arg);
    }
    else if (!stricmp(command, "temp4desc")) {
        return parse_param(config->temp4_desc, PARAM_DESC, arg);
    }
    else if (!stricmp(command, "temp5desc")) {
        return parse_param(config->temp5_desc, PARAM_DESC, arg);
    }
    else if (!stricmp(command, "temp6desc")) {
        return parse_param(config->temp6_desc, PARAM_DESC, arg);
    }
#endif /* _6SENSOR_ */
    else if (!stricmp(command, "manualassignment")) {
        return parse_param(&config->manual_assignment, PARAM_U8_BIT, arg);
    }
    else if (!stricmp(command, "sensor1addr")) {
        return parse_owid(config->sensor1_addr, arg);
    }
    else if (!stricmp(command, "sensor2addr")) {
        return parse_owid(config->sensor2_addr, arg);
    }
#ifdef _6SENSOR_
    else if (!stricmp(command, "sensor3addr")) {
        return parse_owid(config->sensor3_addr, arg);
    }
    else if (!stricmp(command, "sensor4addr")) {
        return parse_owid(config->sensor4_addr, arg);
    }
    else if (!stricmp(command, "sensor5addr")) {
        return parse_owid(config->sensor5_addr, arg);
    }
    else if (!stricmp(command, "sensor6addr")) {
        return parse_owid(config->sensor6_addr, arg);
    }
#endif /* _6SENSOR_ */
#ifdef _I2C_SLAVE_
    else if (!stricmp(command, "slaveindex")) {
        return parse_param(&config->i2c_index, PARAM_U8_SIDX, arg);
    }
    else if (!stricmp(command, "allowoverride")) {
        return parse_param(&config->allow_override, PARAM_U8_BIT, arg);
    }
#endif /* _I2C_SLAVE_ */
    else if (!stricmp(command, "save")) {
        save_configuration(config);
        printf("\r\nConfiguration saved.\r\n\r\n");
        return 0;
    }
    else if (!stricmp(command, "default")) {
        default_configuration(config);
        printf("\r\nDefault configuration loaded.\r\n\r\n");
        return 0;
    }
    else if (!stricmp(command, "run")) {
        printf("\r\nStarting...\r\n");
        return -1;
    }
    else if (!stricmp(command, "readtemp")) {
        do_readtemp();
    }
#if defined(_I2C_MSSP_) || defined(_I2C_)
else if (!stricmp(command, "i2creset")) {
        i2c_bruteforce_reset();
        i2c_init(I2C_FREQ);
    }
#endif
    else if (!stricmp(command, "show")) {
        do_show(config);
    }
    else if ((!stricmp(command, "help") || !stricmp(command, "?"))) {
        do_help();
        return 0;
    }
    else
    {
        printf("Error: no such command (%s)\r\n", command);
        return 1;
    }

    return 0;
}

static uint8_t parse_param(void *param, uint8_t type, char *arg)
{
    int16_t i16param;
    uint8_t u8param;
    uint8_t dp = 0;
    uint8_t un = 0;
    char *s;
    char *sparam;

    if (!arg || !*arg)
    {
        /* Avoid stack overflow */
        printf("Error: Missing parameter\r\n");
        return 1;
    }

    switch (type)
    {
        case PARAM_U8_PCT:
        case PARAM_U8_BIT:
        case PARAM_U8_SIDX:
        case PARAM_U8_FCNT:
        case PARAM_U8_TCNT:
            if (*arg == '-')
                return 1;
            u8param = (uint8_t)atoi(arg);
            if (type == PARAM_U8_BIT && u8param > 1)
                return 1;
            if (type == PARAM_U8_PCT && u8param > 100)
                return 1;
            if (type == PARAM_U8_SIDX && u8param > 15)
                return 1;
            if (type == PARAM_U8_TCNT && u8param > 6)
                return 1;
            if (type == PARAM_U8_FCNT && u8param > MAXFANS)
                return 1;
            *(uint8_t *)param = u8param;
            break;
        case PARAM_I16_1DP_TEMP:
        case PARAM_U16:
        case PARAM_U16_1DP_TEMPMAX:
            s = strtok(arg, ".");
            i16param = atoi(s);
            switch (type)
            {
                case PARAM_U16:
                    un = 1;
                    break;
                case PARAM_I16_1DP_TEMP:
                    i16param *= _1DP_BASE;
                    dp = 1;
                    break;
                case PARAM_U16_1DP_TEMPMAX:
                    i16param *= _1DP_BASE;
                    dp = 1;
                    un = 1;
                    break;
            }

            if (un && *arg == '-')
                return 1;

            s = strtok(NULL, "");
            if (s && *s != 0)
            {
                if (dp == 0)
                    return 1;
                if (dp == 1 && strlen(s) > 1)
                    return 1;
                
                if (*arg == '-')
                    i16param -= atoi(s);
                else
                    i16param += atoi(s);
            }
            if (type == PARAM_I16_1DP_TEMP)
            {
                if (i16param < -550)
                    i16param = -550;
                if (i16param > 1250)
                    i16param = 1250;
            }
            if (type == PARAM_U16_1DP_TEMPMAX)
            {
                if (i16param > 1800)
                    i16param = 1800;
            }
            *(int16_t *)param = i16param;
            break;
        case PARAM_DESC:
            sparam = (char *)param;
            strncpy(sparam, arg, MAX_DESC);
            sparam[MAX_DESC - 1] = 0;
            break;
    }
    return 0;
}

static int8_t parse_owid(uint8_t *param, char *arg)
{
    uint8_t i = 0;
    if (!stricmp(arg, "none"))
    {
        memset(param, 0x00, DS18X20_ROMCODE_SIZE);
        return 0;
    }
    char *s = strtok(arg, ":");
    do
    {
        if (!s || !*s)
            return 1;
        param[i] = (uint8_t)strtoul(s, NULL, 16);
        s = strtok(NULL, ":");
    } while (++i < DS18X20_ROMCODE_SIZE);
    return 0;
}

static void cmd_erase_line(uint8_t count)
{
    printf("%c[%dD%c[K", SEQ_ESCAPE_CHAR, count, SEQ_ESCAPE_CHAR);
}

static void config_next_command(char *cmdbuf, int8_t *count)
{
    uint8_t previdx;

    if (!_g_max_history)
        return;

    if (*count)
        cmd_erase_line(*count);

    previdx = ++_g_show_history;

    if (_g_show_history == CMD_MAX_HISTORY)
    {
        _g_show_history = 0;
        previdx = 0;
    }

    strcpy(cmdbuf, _g_cmd_history[previdx]);
    *count = strlen(cmdbuf);
    printf("%s", cmdbuf);
}

static void config_prev_command(char *cmdbuf, int8_t *count)
{
    uint8_t previdx;

    if (!_g_max_history)
        return;

    if (*count)
        cmd_erase_line(*count);

    if (_g_show_history == 0)
        _g_show_history = CMD_MAX_HISTORY;

    previdx = --_g_show_history;

    strcpy(cmdbuf, _g_cmd_history[previdx]);
    *count = strlen(cmdbuf);
    printf("%s", cmdbuf);
}

static int get_string(char *str, int8_t max, uint8_t *ignore_lf)
{
    unsigned char c;
    uint8_t state = CMD_READLINE;
    int8_t count;

    count = 0;
    do {
        c = wdt_getch();

        if (state == CMD_ESCAPE) {
            if (c == SEQ_CTRL_CHAR1) {
                state = CMD_AWAIT_NAV;
                continue;
            }
            else {
                state = CMD_READLINE;
                continue;
            }
        }
        else if (state == CMD_AWAIT_NAV)
        {
            if (c == SEQ_ARROW_UP) {
                config_prev_command(str, &count);
                state = CMD_READLINE;
                continue;
            }
            else if (c == SEQ_ARROW_DOWN) {
                config_next_command(str, &count);
                state = CMD_READLINE;
                continue;
            }
            else if (c == SEQ_DEL) {
                state = CMD_DEL;
                continue;
            }
            else if (c == SEQ_HOME || c == SEQ_END || c == SEQ_INS || c == SEQ_PGUP || c == SEQ_PGDN) {
                state = CMD_DROP_NAV;
                continue;
            }
            else {
                state = CMD_READLINE;
                continue;
            }
        }
        else if (state == CMD_DEL) {
            if (c == SEQ_NAV_END && count) {
                putch('\b');
                putch(' ');
                putch('\b');
                count--;
            }

            state = CMD_READLINE;
            continue;
        }
        else if (state == CMD_DROP_NAV) {
            state = CMD_READLINE;
            continue;
        }
        else
        {
            if (count >= max) {
                count--;
                break;
            }

            if (c == 19) /* Swallow XOFF */
                continue;

            if (c == CTL_U) {
                if (count) {
                    cmd_erase_line(count);
                    *(str) = 0;
                    count = 0;
                }
                continue;
            }

            if (c == SEQ_ESCAPE_CHAR) {
                state = CMD_ESCAPE;
                continue;
            }

            /* Unix telnet sends:    <CR> <NUL>
            * Windows telnet sends: <CR> <LF>
            */
            if (*ignore_lf && (c == '\n' || c == 0x00)) {
                *ignore_lf = 0;
                continue;
            }

            if (c == 3) { /* Ctrl+C */
                return -1;
            }

            if (c == '\b' || c == 0x7F) {
                if (!count)
                    continue;

                putch('\b');
                putch(' ');
                putch('\b');
                count--;
                continue;
            }
            if (c != '\n' && c != '\r') {
                putch(c);
            }
            else {
                if (c == '\r') {
                    *ignore_lf = 1;
                    break;
                }

                if (c == '\n')
                    break;
            }
            str[count] = c;
            count++;
        }
    } while (1);

    str[count] = 0;
    return count;
}

static int8_t get_line(char *str, int8_t max, uint8_t *ignore_lf)
{
    uint8_t i;
    int8_t ret;
    int8_t tostore = -1;

    ret = get_string(str, max, ignore_lf);

    if (ret <= 0) {
        return ret;
    }
    
    if (_g_next_history >= CMD_MAX_HISTORY)
        _g_next_history = 0;
    else
        _g_max_history++;

    for (i = 0; i < CMD_MAX_HISTORY; i++)
    {
        if (!strcasecmp(_g_cmd_history[i], str))
        {
            tostore = i;
            break;
        }
    }

    if (tostore < 0)
    {
        // Don't have this command in history. Store it
        strcpy(_g_cmd_history[_g_next_history], str);
        _g_next_history++;
        _g_show_history = _g_next_history;
    }
    else
    {
        // Already have this command in history, set the 'up' arrow to retrieve it.
        tostore++;

        if (tostore == CMD_MAX_HISTORY)
            tostore = 0;

        _g_show_history = tostore;
    }

    printf("\r\n");

    return ret;
}


void load_configuration(sys_config_t *config)
{
    uint16_t config_size = sizeof(sys_config_t);
    if (config_size > 0x100)
    {
        printf("\r\nConfiguration size is too large. Currently %u bytes.", config_size);
        reset();
    }
    
    eeprom_read_data(0, (uint8_t *)config, sizeof(sys_config_t));

    if (config->magic != CONFIG_MAGIC)
    {
        printf("\r\nNo configuration found. Setting defaults\r\n");
        default_configuration(config);
        save_configuration(config);
    }
}

static void default_configuration(sys_config_t *config)
{
    config->magic = CONFIG_MAGIC;
    config->num_fans = 1;
    config->fans_max = DEF_PCT_MAX;
    config->fans_min = DEF_PCT_MIN;
    config->fans_start = DEF_PCT_MIN;
    config->fans_minrpm = DEF_MIN_RPM;
    config->temp_min = DEF_TEMP_MIN;
    config->temp_max = DEF_TEMP_MAX;
    config->temp_hyst = 0;
    config->fans_minoff = false;
    config->min_temps = 0;
    config->temp1_desc[0] = 0;
    config->temp2_desc[0] = 0;
    config->temp3_desc[0] = 0;
    config->temp4_desc[0] = 0;
    config->manual_assignment = false;
    memset(config->sensor1_addr, 0x00, DS18X20_ROMCODE_SIZE);
    memset(config->sensor2_addr, 0x00, DS18X20_ROMCODE_SIZE);
    memset(config->sensor3_addr, 0x00, DS18X20_ROMCODE_SIZE);
    memset(config->sensor4_addr, 0x00, DS18X20_ROMCODE_SIZE);
#ifdef _I2C_SLAVE_
    config->i2c_index = 0;
    config->allow_override = 0;
#endif /* _I2C_SLAVE_ */
}

static void save_configuration(sys_config_t *config)
{
    eeprom_write_data(0, (uint8_t *)config, sizeof(sys_config_t));
}

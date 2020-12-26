/*
 *   File:   main.c
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
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "iopins.h"
#include "timer.h"
#include "ds18x20.h"
#include "onewire.h"
#include "config.h"
#include "util.h"
#include "pwm.h"
#include "usart.h"
#ifdef _OW_DS2482_
#include "i2c.h"
#endif /* _OW_DS2482_ */

char _g_dotBuf[MAX_DESC];

typedef struct {
    uint8_t sensor_ids[MAXSENSORS][DS18X20_ROMCODE_SIZE];
    uint8_t num_sensors;
    uint8_t tach_timeout;
    uint8_t last_portb;
    uint16_t tach_count[MAXFANS];
    uint16_t tach_rpm[MAXFANS];
    uint8_t hyst_lockout;
    uint8_t sensor_state;
    int16_t temp_result[MAXSENSORS];
} sys_runstate_t;

sys_config_t _g_cfg;
sys_runstate_t _g_rs;

static void io_init(void);
static char *dots_for(const char *str);
static void fan_set_duty(uint8_t pwm, uint8_t pct);
static void print_duty(uint8_t duty);
static void print_fan(uint8_t fan, uint16_t tach_rpm, uint8_t nl);
static void print_temp(uint8_t temp, int16_t result, const char *desc, uint8_t nl);
static uint8_t calc_pwm_duty(int16_t measured, uint8_t pct_max, uint8_t pct_min, int16_t temp_max, int16_t temp_min, uint16_t hyst, uint8_t min_off, uint8_t *hyst_lockout);
static void main_process(sys_runstate_t *rs, sys_config_t *config);
static void stall_check(sys_runstate_t *rs, sys_config_t *config);
uint8_t build_sensorlist_from_config(sys_runstate_t *rs, sys_config_t *config);

ISR(PCINT0_vect)
{
    if (!(_g_rs.last_portb & _BV(F1TACH)) && IO_IN_HIGH(F1TACH))
        _g_rs.tach_count[0]++;

    if (!(_g_rs.last_portb & _BV(F2TACH)) && IO_IN_HIGH(F2TACH))
        _g_rs.tach_count[1]++;

    _g_rs.last_portb = F1TACH_PIN;
}

ISR(TIMER0_OVF_vect)
{
    _g_rs.tach_timeout++;

    if (_g_rs.tach_timeout == 200)
    {
        uint8_t i = 0;
        for (i = 0; i < MAXFANS; i++)
        {
            /* Sampled every 2 seconds. Multiply count by 30 to get RPM */
            _g_rs.tach_rpm[i] = _g_rs.tach_count[i] * 30;
            _g_rs.tach_count[i] = 0;
        }
        _g_rs.tach_timeout = 0;
    }

    timer0_reload(TIMER0VAL);
}

int main(void)
{
    uint8_t stall_check_cycleskip = 0;
    uint8_t i;
    sys_runstate_t *rs = &_g_rs;
    sys_config_t *config = &_g_cfg;

    /////////// WDT INIT?
    io_init();
    g_irq_disable();
    timer0_init();
    timer1_init();

#if defined(__PIC18_K40__) || defined(__PIC18_K42__)
    /* Tacho inputs 3,4,5,6 */
    TRISAbits.TRISA4 = 1;
    TRISCbits.TRISC0 = 1;
    TRISCbits.TRISC5 = 1;
    TRISAbits.TRISA2 = 1;
#endif

    usart1_open(USART_CONT_RX | USART_BRGH, (((F_CPU / UART_BAUD) / 16) - 1));
    
#ifdef _OW_DS2482_
    i2c_init(I2C_FREQ);
#endif /* _OW_DS2482_ */
    ow_init();
    
    load_configuration(config);

    /* Enable PWM 1 and 2 */
    pwm_init();
    set_start_duty(config);

    configuration_bootprompt(config);

    /* Enable Interrupts */
    g_irq_enable();

    /* Clear tachos */
    rs->tach_timeout = 0;
    rs->last_portb = PORTB;
    rs->sensor_state = 0;

    for (i = 0; i < MAXSENSORS; i++)
        rs->temp_result[i] = 0;

    for (i = 0; i < MAXFANS; i++)
    {
        rs->tach_count[i] = 0;
        rs->tach_rpm[i] = 0;
    }

    /* Hysteresis lockout on so we don't start fans if temp is inside hysteresis window */
    rs->hyst_lockout = 1;
    
    if (config->manual_assignment)
    {
        rs->num_sensors = build_sensorlist_from_config(rs, config);
        printf("\r\nManually assigned %u of %u maximum sensors\r\n", rs->num_sensors, MAXSENSORS);
    }
    else
    {        
        if (ds18x20_search_sensors(&rs->num_sensors, rs->sensor_ids))
            printf("\r\nFound %u of %u maximum sensors\r\n", rs->num_sensors, MAXSENSORS);
        else
            printf("\r\nHardware error searching for sensors\r\n");
    }

    if (rs->num_sensors == 0)
        printf("No sensors found. Fans will be set to max\r\n");

    printf("Using %u of %u maximum fans\r\n", config->num_fans, MAXFANS);

	wdt_reset();

    printf("Press Ctrl+D at any time to reset\r\n");
    
    for (;;)
    {
       main_process(rs, config);

       /* Don't do the stall check straight away */
       if (stall_check_cycleskip < 5)
           stall_check_cycleskip++;
       else
           stall_check(rs, config);
    }
}

static void io_init(void)
{
    IO_INPUT(F1TACH);
    IO_INPUT(F2TACH);
    IO_OUTPUT(F1ON);
    IO_OUTPUT(F2ON);
    IO_INPUT(SP1);
    IO_INPUT(SP2);
    IO_INPUT(SP3);
    IO_INPUT(SP4);
    IO_INPUT(SP5);
    IO_INPUT(SP6);

    PCICR |= _BV(PCIE1);
    PCMSK1 |= _BV(PCINT10);
    PCMSK1 |= _BV(PCINT11);
}

static void main_process(sys_runstate_t *rs, sys_config_t *config)
{
    uint8_t i;
    int16_t result = 0;
    uint8_t state_temp = 0;

    for (i = 0; i < rs->num_sensors; i++)
        ds18x20_start_meas(rs->sensor_ids[i]);

    delay_10ms(76);

    usart1_clear_oerr();

    for (i = 0; i < rs->num_sensors; i++)
    {
        int16_t reading_temp;

        if (ds18x20_read_decicelsius(rs->sensor_ids[i], &reading_temp))
        {
            result = max_(result, reading_temp);
            rs->temp_result[i] = reading_temp;
            state_temp |= (1 << i);
        }
    }
    
    rs->sensor_state = state_temp;
        
    if (usart1_data_ready())
    {
        char c = usart1_get();
        if (c == 4)
        {
            printf("\r\nCtrl+D received. Resetting...\r\n");
            while (usart1_busy());
            reset();
        }
    }
    
    if (rs->num_sensors == 0)
    {
        if (config->num_fans > 0)
        {
            for (i = 0; i < config->num_fans; i++)
                print_fan(i, rs->tach_rpm[i], i == 0);

            print_duty(config->fans_max);
            fan_set_duty(FAN1, config->fans_max);
            fan_set_duty(FAN2, config->fans_max);
        }
        else
        {
            printf("Nothing to do\r\n");
        }
    }

    if (rs->num_sensors > 0)
    {
        // Bail out if any temperature sensors are offline
        if (((1 << rs->num_sensors) - 1) == rs->sensor_state && rs->num_sensors >= config->min_temps)
        {
            uint8_t duty = calc_pwm_duty(result, config->fans_max, config->fans_min, config->temp_max,
                    config->temp_min, config->temp_hyst, config->fans_minoff, &rs->hyst_lockout);

            for (i = 0; i < rs->num_sensors; i++)
            {
                const char *desc = NULL;
                
                switch (i)
                {
                case 0:
                    desc = config->temp1_desc;
                    break;
                case 1:
                    desc = config->temp2_desc;
                    break;
                case 2:
                    desc = config->temp3_desc;
                    break;
                case 3:
                    desc = config->temp4_desc;
                    break;
                }

                print_temp(i, rs->temp_result[i], desc, i == 0);
            }

            print_temp(rs->num_sensors, result, "max", 0);

            if (config->num_fans > 0)
            {
                for (i = 0; i < config->num_fans; i++)
                    print_fan(i, rs->tach_rpm[i], i == 0);

                print_duty(duty);
                
                fan_set_duty(FAN1, duty);
                fan_set_duty(FAN2, duty);
            }
        }
        else
        {
            if (config->num_fans > 0)
            {
                printf("Insufficient number of operational sensors. Setting to max\r\n");

                fan_set_duty(FAN1, config->fans_max);
                fan_set_duty(FAN2, config->fans_max);
            }
            else
            {
                printf("No fans or insufficient sensors present\r\n");
            }
        }
    }
}

static void stall_check(sys_runstate_t *rs, sys_config_t *config)
{
    uint16_t minrpm = 0xFFFF;
    uint8_t i;
    
    for (i = 0; i < config->num_fans; i++)
        minrpm = min_(minrpm, rs->tach_rpm[i]);
    
   /* Check for fan 1 stall */
    if (!config->fans_minoff && (minrpm < config->fans_minrpm))
    {
        printf("Fan stall. Restarting...\r\n");
        fan_set_duty(FAN1, config->fans_max);
        fan_set_duty(FAN2, config->fans_max);
        delay_10ms(150);
        delay_10ms(150);
    }
}

static void print_temp(uint8_t temp, int16_t dec, const char *desc, uint8_t nl)
{
    fixedpoint_sign(dec, dec);

    printf("%sTemp %c (C) [%s] %s..: %s%u.%u\r\n",
         nl ? "\r\n" : "",
        '1' + temp,
        desc, dots_for(desc), fixedpoint_arg(dec, dec));
}

static void print_fan(uint8_t fan, uint16_t tach_rpm, uint8_t nl)
{
    printf("%sFan %c RPM  ....................: ", nl ? "\r\n" : "", '1' + fan);
    printf("%u\r\n", tach_rpm);
}

static void print_duty(uint8_t duty)
{
    printf("PWM Duty ......................: %d%%\r\n", duty);
}

static void fan_set_duty(uint8_t pwm, uint8_t pct)
{
    if (pwm == FAN1)
        pwm_setduty(FAN1, pct);
    
    if (pwm == FAN2)
        pwm_setduty(FAN2, pct);
}

static char *dots_for(const char *str)
{
    uint8_t len = (MAX_DESC - 1) - strlen(str);
    uint8_t di = 0;
    for (di = 0; di < len; di++)
        _g_dotBuf[di] = '.';
    _g_dotBuf[di] = 0;
    return _g_dotBuf;
}

static uint8_t calc_pwm_duty(int16_t measured, uint8_t pct_max, uint8_t pct_min, int16_t temp_max,
        int16_t temp_min, uint16_t hyst, uint8_t min_off, uint8_t *hyst_lockout)
{
    int16_t temprange = temp_max - temp_min;
    int16_t pctrange = pct_max - pct_min;
    int16_t actual;
    int16_t percent;
    int16_t result;
    int16_t scaled;

    if (min_off)
    {
        if (measured < (temp_min - hyst))
        {
            *hyst_lockout = 1;
            return 0;
        }

        if (*hyst_lockout)
        {
            if (measured >= temp_min)
                *hyst_lockout = 0;
            else
                return 0;
        }
    }
    
    if (measured < temp_min)
        measured = temp_min;

    if (measured > temp_max)
        measured = temp_max;

    actual = measured - temp_min;
    percent = (actual * 100) / temprange;
    scaled = pctrange * percent;
    
    result = pct_min + (scaled / 100);
    if (result > pct_max)
        result = pct_max;

    return result;
}

void set_start_duty(sys_config_t *config)
{
    fan_set_duty(FAN1, config->fans_start);
    fan_set_duty(FAN2, config->fans_start);
}

uint8_t build_sensorlist_from_config(sys_runstate_t *rs, sys_config_t *config)
{
    uint8_t i;
    memcpy(rs->sensor_ids[0], config->sensor1_addr, DS18X20_ROMCODE_SIZE);
    memcpy(rs->sensor_ids[1], config->sensor2_addr, DS18X20_ROMCODE_SIZE);
    memcpy(rs->sensor_ids[2], config->sensor3_addr, DS18X20_ROMCODE_SIZE);
    memcpy(rs->sensor_ids[3], config->sensor4_addr, DS18X20_ROMCODE_SIZE);

    for (i = 0; i < MAXSENSORS; i++)
    {
        if (rs->sensor_ids[i][0] == 0x00)
            break;
    }

    return i;
}

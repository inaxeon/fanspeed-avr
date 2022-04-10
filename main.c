/*
 *   File:   main.c
 *   Author: Matthew Millman
 *
 *   Fan speed controller. OSS AVR Version.
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
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "iopins.h"
#include "timer.h"
#include "config.h"
#include "onewire.h"
#include "util.h"
#include "pwm.h"
#include "usart.h"
#include "i2c.h"
#include "ds2482.h"
#include "ds18x20.h"

char _g_dotBuf[MAX_DESC];

typedef struct {
    uint8_t sensor_ids[MAX_SENSORS][OW_ROMCODE_SIZE];
    uint8_t num_sensors;
    uint8_t tach_timeout;
    uint8_t last_portc;
    uint16_t tach_count[MAX_FANS];
    uint16_t tach_rpm[MAX_FANS];
#ifdef _SINGLEZONE_
    bool output_on;
    bool hyst_lockout;
    int32_t hyst_timer;
    int32_t tick_timer;
#else
    bool hyst_lockout[MAX_FANS];
#endif
    uint8_t sensor_state;
    int16_t temp_result[MAX_SENSORS];
} sys_runstate_t;

sys_config_t _g_cfg;
sys_runstate_t _g_rs;

static void io_init(void);
static void fan_set_duty(uint8_t pwm, uint8_t pct);
static void print_fan(uint8_t fan, uint16_t tach_rpm, uint8_t nl);
static void main_process(sys_runstate_t *rs, sys_config_t *config);

FILE uart_str = FDEV_SETUP_STREAM(print_char, NULL, _FDEV_SETUP_RW);

ISR(PCINT1_vect)
{
    if (!(_g_rs.last_portc & _BV(F1TACH)) && IO_IN_HIGH(F1TACH))
        _g_rs.tach_count[0]++;

    if (!(_g_rs.last_portc & _BV(F2TACH)) && IO_IN_HIGH(F2TACH))
        _g_rs.tach_count[1]++;

    _g_rs.last_portc = F1TACH_PIN;
}

ISR(TIMER0_OVF_vect)
{
    _g_rs.tach_timeout++;
    _g_rs.tick_timer++;

    if (_g_rs.tach_timeout == 50)
    {
        uint8_t i = 0;
        for (i = 0; i < MAX_FANS; i++)
        {
            /* Sampled every 500ms. Multiply count by 120 to get RPM */
            _g_rs.tach_rpm[i] = _g_rs.tach_count[i] * 120;
            _g_rs.tach_count[i] = 0;
        }
        _g_rs.tach_timeout = 0;
    }

    timer0_reload(TIMER0VAL);
}

int main(void)
{
    uint8_t i;
    sys_runstate_t *rs = &_g_rs;
    sys_config_t *config = &_g_cfg;

    wdt_enable(WDTO_2S);
    wdt_reset();
    g_irq_enable();
    io_init();
    timer0_init();

    usart1_open(USART_CONT_RX | USART_BRGH, (((F_CPU / UART_BAUD) / 16) - 1));
    stdout = &uart_str;

#ifdef _OW_DS2482_
    i2c_init(I2C_FREQ);
#endif /* _OW_DS2482_ */
    ow_init();
    
    load_configuration(config);

    /* Enable PWM 1 and 2 */
    pwm_init();
    set_start_duty(config);

    configuration_bootprompt(config);

    /* Clear tachos */
    rs->tach_timeout = 0;
    rs->last_portc = PORTB;
    rs->sensor_state = 0;
    rs->tick_timer = 0;

    for (i = 0; i < MAX_SENSORS; i++)
        rs->temp_result[i] = 0;

    for (i = 0; i < MAX_FANS; i++)
    {
        rs->tach_count[i] = 0;
        rs->tach_rpm[i] = 0;
    }

    /* Hysteresis lockout on so we don't start fans if temp is inside hysteresis window */
#ifdef _SINGLEZONE_
    rs->hyst_lockout = true;
    rs->hyst_timer = 1200;
    rs->output_on = false;
#else
    for (i = 0; i < MAX_FANS; i++)
        rs->hyst_lockout[i] = true;
#endif /* _SINGLEZONE_ */
    
    timer0_start();
	wdt_reset();

    printf("Press Ctrl+D at any time to reset\r\n");
    
    for (;;)
    {
        main_process(rs, config);
        wdt_reset();
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
    IO_OUTPUT(SP3);
    IO_LOW(SP3);
    IO_INPUT(SP4);
    IO_INPUT(SP5);
    IO_INPUT(SP6);

    PCICR |= _BV(PCIE1);
    PCMSK1 |= _BV(PCINT10);
    PCMSK1 |= _BV(PCINT11);

    _g_rs.last_portc = F1TACH_PIN;
}

static void main_process(sys_runstate_t *rs, sys_config_t *config)
{
    uint8_t i;
    int16_t result = 0;
    uint8_t state_temp = 0;

    delay_10ms(50);

    for (i = 0; i < rs->num_sensors; i++)
    {
        int16_t reading_temp;

        if (ds18b20_read_decicelsius(rs->sensor_ids[i], &reading_temp))
        {
            result = max_(result, reading_temp);
            rs->temp_result[i] = reading_temp;
            state_temp |= (1 << i);
        }
    }
    
    rs->sensor_state = state_temp;
        
    if (console_data_ready())
    {
        char c = console_get();
        if (c == 4)
        {
            printf("\r\nCtrl+D received. Resetting...\r\n");
            while (console_busy());
            reset();
        }
    }
    
    print_fan(0, rs->tach_rpm[0], true);

    if (!rs->hyst_lockout)
    {
        if (!rs->output_on && (rs->tach_rpm[0] > config->fans_minrpm))
        {
            IO_OUTPUT(SP2);
            IO_LOW(SP2);
            IO_INPUT(SP3);

            rs->hyst_lockout = true;
            rs->hyst_timer = (rs->tick_timer + config->temp_hyst);
            rs->output_on = true;

            printf("Output on! Entering on hyst lockout!\r\n");

        }
        else if (rs->output_on && (rs->tach_rpm[0] < config->fans_maxrpm))
        {
            IO_INPUT(SP2);
            IO_OUTPUT(SP3);
            IO_LOW(SP3);

            rs->hyst_lockout = true;
            rs->hyst_timer = (rs->tick_timer + config->temp_hyst);
            rs->output_on = false;

            printf("Output off! Entering off hyst lockout!\r\n");
        }
        else
        {
            printf("Nothing to do!\r\n");
        }
    }
    else
    {
        int32_t diff = rs->hyst_timer - rs->tick_timer;
        printf("Hyst lockout! %ld\r\n", diff);

        if (diff <= 0)
            rs->hyst_lockout = false;
    }
}

void set_start_duty(sys_config_t *config)
{
    fan_set_duty(FAN1, config->fans_start);
    fan_set_duty(FAN2, config->fans_start);
}

static void print_fan(uint8_t fan, uint16_t tach_rpm, uint8_t nl)
{
    printf("%sFan %c RPM  ....................: ", nl ? "\r\n" : "", '1' + fan);
    printf("%u\r\n", tach_rpm);
}

static void fan_set_duty(uint8_t pwm, uint8_t pct)
{
    if (pwm == FAN1)
        pwm_setduty(FAN1, pct);
    
    if (pwm == FAN2)
        pwm_setduty(FAN2, pct);
}

/* 
 * File:   config.h
 * Author: Matt
 *
 * Created on 25 November 2014, 15:54
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef _DS18X20_SINGLE_DEV_PER_CHANNEL_
#define DS18X20_ROMCODE_SIZE 1
#else
#define DS18X20_ROMCODE_SIZE 8
#endif

typedef struct {
    uint16_t magic;
    uint8_t num_fans;
    uint8_t fans_max;
    uint8_t fans_min;
    uint8_t fans_start;
    uint16_t fans_minrpm;
    bool fans_minoff;
    uint8_t min_temps;
    int16_t temp_min;
    int16_t temp_max;
    uint16_t temp_hyst;
    char temp1_desc[MAX_DESC];
    char temp2_desc[MAX_DESC];
    char temp3_desc[MAX_DESC];
    char temp4_desc[MAX_DESC];
    bool manual_assignment;
    uint8_t sensor1_addr[DS18X20_ROMCODE_SIZE];
    uint8_t sensor2_addr[DS18X20_ROMCODE_SIZE];
    uint8_t sensor3_addr[DS18X20_ROMCODE_SIZE];
    uint8_t sensor4_addr[DS18X20_ROMCODE_SIZE];
#ifdef _I2C_SLAVE_
    uint8_t i2c_index;
    uint8_t allow_override;
#endif /* _I2C_SLAVE_ */
} sys_config_t;

void configuration_bootprompt(sys_config_t *config);
void load_configuration(sys_config_t *config);
void set_start_duty(sys_config_t *config);

#endif /* __CONFIG_H__ */
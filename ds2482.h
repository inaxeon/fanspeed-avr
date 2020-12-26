/*
 * File:   ds2482.h
 * Author: Matt
 *
 * Created on 22 July 2016, 11:13
 */

#ifndef __DS2482_H__
#define	__DS2482_H__

#include <stdint.h>
#include <stdbool.h>

bool ds2482_init(void);
bool ds2482_bus_reset(bool *presense_detect);
bool ds2482_command(uint8_t command, uint8_t *id);
bool ds2482_read_byte(uint8_t *ret);
bool ds2482_write_byte(uint8_t data);
#ifdef _OW_DS2482_800_
bool ds2482_select_channel(uint8_t channel);
#endif /* _OW_DS2482_800_ */
uint8_t ds2482_rom_search(uint8_t diff, uint8_t *id);

#endif /* __DS2482_H__ */
/*
 * File:   ow_bitbang.h
 * Author: Matt
 *
 * Created on 22 July 2016, 16:50
 */

#ifndef __OW_BITBANG_H__
#define __OW_BITBANG_H__

#include <stdint.h>
#include <stdbool.h>

bool owbitbang_init(void);
bool owbitbang_bus_reset(bool *presense_detect);
bool owbitbang_bus_idle(void);
bool owbitbang_command(uint8_t command, uint8_t *id);
uint8_t owbitbang_byte_xch(uint8_t b);
uint8_t owbitbang_rom_search(uint8_t diff, uint8_t *id);

#endif /* __OW_BITBANG_H__ */


/*
 * File:   ow_bitbang.h
 *   Author: Matthew Millman
 *
 * Created on 22 July 2016, 16:50
 */

#ifndef __OW_BITBANG_H__
#define __OW_BITBANG_H__

#include <stdint.h>
#include <stdbool.h>

bool owbitbang_init(void);
bool owbitbang_bus_reset(bool *presense_detect);
bool owbitbang_bit_io(bool *bit);
bool owbitbang_read(uint8_t *buf, uint8_t len);
uint8_t owbitbang_rom_search(uint8_t diff, uint8_t *id);
bool owbitbang_select(const uint8_t *id);
bool owbitbang_write(const uint8_t *data, uint8_t len);

#endif /* __OW_BITBANG_H__ */


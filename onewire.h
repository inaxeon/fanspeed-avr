/*
* File:   onewire.h
* Author: Matt
*
* Created on 22 July 2016, 16:50
*/

#ifndef __ONEWIRE_H__
#define __ONEWIRE_H__

#include "project.h"

#include <stdint.h>
#include <stdbool.h>

#include "ow_bitbang.h"
#include "ds2482.h"

#define OW_READ_ROM     0x33
#define OW_MATCH_ROM    0x55
#define OW_SKIP_ROM     0xCC
#define OW_SEARCH_ROM   0xF0

#define OW_SEARCH_FIRST 0xFF        /* Start new search */
#define OW_PRESENCE_ERR 0xFF
#define OW_DATA_ERR     0xFE
#define OW_COMMS_ERR    0xFD
#define OW_LAST_DEVICE  0x00        /* Last device found */

#ifdef _OW_BITBANG_

#define ow_init()
#define ow_bus_reset(presense) owbitbang_bus_reset(presense)
#define ow_command(cmd, id) owbitbang_command(cmd, id)
#define ow_read_byte(ret) ((*ret = owbitbang_byte_xch(0xFF)) | 1)
#define ow_write_byte(data) (owbitbang_byte_xch(data) | 1)
#define ow_rom_search(diff, id) owbitbang_rom_search(diff, id)
#define ow_bus_idle() owbitbang_bus_idle()

#endif /* _OW_BITBANG_ */

#ifdef _OW_DS2482_

#define ow_init() ds2482_init()
#define ow_bus_reset(presense) ds2482_bus_reset(presense)
#define ow_command(cmd, id) ds2482_command(cmd, id)
#define ow_read_byte(ret) ds2482_read_byte(ret)
#define ow_write_byte(data) ds2482_write_byte(data)
#define ow_rom_search(diff, id) ds2482_rom_search(diff, id)
#define ow_bus_idle() true
#define ow_select_channel(channel) ds2482_select_channel(channel)

#endif /* _OW_DS2482_ */

#endif /* __ONEWIRE_H__ */

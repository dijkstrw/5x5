/*
 * Copyright (c) 2022 by Willem Dijkstra <wpd@xs4all.nl>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the auhor nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * flash
 *
 * User flash used for storing keymaps and macros.
 */

#include <stdint.h>
#include <string.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include "config.h"
#include "keymap.h"
#include "keyboard.h"
#include "macro.h"
#include "elog.h"

#if MACRO_MAXKEYS % 4
/* flash reads and writes are in 4 byte increments. While other values
 * will work, they can clobber whatever is allocated right next to
 * macro_len */
#error MACRO_MAXKEYS must be a multiple of 4
#endif

typedef struct {
    const uint8_t data[FLASH_PAGE_NUM][FLASH_PAGE_SIZE];
} __attribute__ ((packed)) flashpage_t;

typedef struct {
    event_t keymap[LAYERS_NUM][ROWS_NUM][COLS_NUM];
    event_t macro_buffer[MACRO_MAXKEYS][MACRO_MAXLEN];
    uint8_t macro_len[MACRO_MAXKEYS];
    uint32_t layer;
    uint32_t nkro_active;
} __attribute__ ((packed)) flashdata_t;

typedef struct {
    uint32_t data[sizeof(flashdata_t) >> 2];
    uint32_t zero[(sizeof(flashpage_t) - sizeof(flashdata_t) - sizeof(uint32_t)) >> 2];
    uint32_t crc;
} __attribute__ ((packed, aligned(4))) flashcrc_t;

typedef union {
    flashpage_t page;
    flashdata_t data;
    flashcrc_t crc;
} __attribute__ ((packed, aligned(4))) flash_t;

flash_t flash __attribute__ ((section(".userflash")));

static uint32_t
flash_crc()
{
    uint32_t result;

    crc_reset();
    result = crc_calculate_block(&flash.crc.data[0], sizeof(flash.crc.data) >> 2);
    elog("crc %08x", result);
    return result;
}

static uint8_t
flash_crc_check()
{
    uint32_t stored_crc = flash.crc.crc;
    uint32_t calced_crc = flash_crc();

    return (stored_crc == calced_crc);
}

void
crc_init()
{
    rcc_periph_clock_enable(RCC_CRC);
}

static uint32_t
flash_erase()
{
    uint32_t i;
    uint32_t status;

    elog("erasing flash");
    flash_clear_status_flags();
    flash_unlock();
    for (i = 0; i < FLASH_PAGE_NUM; i++) {
        flash_erase_page((uint32_t)&flash.page.data[i]);
        status = flash_get_status_flags();
        if (status != FLASH_SR_EOP) {
            elog("page erase %d: status error %02x", i, status);
            return 0;
        }
    }

    return 1;
}

uint32_t
flash_clear_config(void)
{
    flash_erase();
    flash_lock();
    return 1;
}

uint32_t
flash_read_config(void)
{
    elog("reading configuration");

    if (! flash_crc_check()) {
        elog("crc not correct");
        return 0;
    }

    cm_disable_interrupts();
    memcpy(keymap, flash.data.keymap, sizeof(flash.data.keymap));
    memcpy(macro_buffer, flash.data.macro_buffer, sizeof(flash.data.macro_buffer));
    memcpy(macro_len, flash.data.macro_len, sizeof(flash.data.macro_len));
    layer = flash.data.layer;
    nkro_active = flash.data.nkro_active;
    cm_enable_interrupts();

    return 1;
}

static uint32_t
flash_write_block(void *dest, const void *src, uint32_t len)
{
    uint32_t i;
    uint32_t status;
    uint32_t *d = dest;
    const uint32_t *s = src;

    if (((uint32_t)d < (uint32_t)&flash) ||
        (((uint32_t)d + len) > ((uint32_t)&flash + sizeof(flash)))) {
        elog("write address address outside of user flash range");
        return 0;
    }

    for (i = 0; i < len; i += sizeof(uint32_t)) {
        flash_program_word((uint32_t)d++, *s++);
        status = flash_get_status_flags();
        if (status != FLASH_SR_EOP) {
            elog("error after write %d:%02x", i, status);
            return 0;
        }
    }

    return 1;
}

uint32_t
flash_write_config(void)
{
    uint32_t data;
    uint32_t status;
    uint32_t crc;

    if (! flash_erase()) {
        return 0;
    }

    elog("writing configuration");

    if (! flash_write_block(&flash.data.keymap,
                            keymap,
                            sizeof(flash.data.keymap))) {
        return 0;
    }
    if (! flash_write_block(&flash.data.macro_buffer,
                            macro_buffer,
                            sizeof(flash.data.macro_buffer))) {
        return 0;
    }
    if (!flash_write_block(&flash.data.macro_len,
                           macro_len,
                           sizeof(flash.data.macro_len))) {
        return 0;
    }
    data = (uint32_t)layer;
    if (!flash_write_block(&flash.data.layer,
                           &data,
                           sizeof(data))) {
        return 0;
    }
    data = (uint32_t)nkro_active;
    if (!flash_write_block(&flash.data.nkro_active,
                           &data,
                           sizeof(data))) {
        return 0;
    }
    crc = flash_crc();
    if (!flash_write_block(&flash.crc.crc, &crc, sizeof(crc))) {
        return 0;
    }

    flash_lock();
    return 1;
}

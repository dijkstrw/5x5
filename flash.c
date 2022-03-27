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
 *
 * Hardware 32 bit word access is abstracted to a per-character interface.
 *
 * For writing to flash follow this procedure:
 * - flash_write_start  - unlock and erase flash, setup char interface
 * - flash_write_byte   - write one byte, do this upto 2k - 1 times
 * - flash_write_end    - write crc, lock flash, disable char interface
 *
 * For reading flash:
 * - flash_read_start   - setup char interface, check crc
 * - flash_read_byte    - read one byte, do this upto 2k - 1 times
 * - flash_read_end     - disable char interface
 */

#include <stdint.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include "elog.h"

/*
 * STM32 flash page size depends on the device:
 * stm32f103xx, with < 128k flash = 1k
 */
#define FLASH_PAGE_NUM          4
#define FLASH_PAGE_SIZE         0x400
#define FLASH_LEN               (FLASH_PAGE_NUM * FLASH_PAGE_SIZE)
#define FLASH_LEN_CRC           (FLASH_LEN - sizeof(uint32_t))

static const uint8_t flash[FLASH_PAGE_NUM][FLASH_PAGE_SIZE] __attribute__((section(".userflash")));

#define FLASH_START              ((uint32_t)&flash)
#define FLASH_END               (FLASH_START + FLASH_LEN)
#define FLASH_CRC               (FLASH_START + FLASH_LEN_CRC)

static volatile uint32_t flash_current = 0;
static volatile uint8_t flash_byte_len = 0;
static volatile uint32_t flash_data = 0;

static uint32_t
flash_crc()
{
    uint32_t result;

    crc_reset();
    result = crc_calculate_block((uint32_t *)FLASH_START, FLASH_LEN_CRC >> 2);
    elog("crc %08x", result);
    return result;
}

static uint8_t
flash_crc_check()
{
    uint32_t stored_crc = *(uint32_t *)FLASH_CRC;
    uint32_t calced_crc = flash_crc();

    return (stored_crc == calced_crc);
}

void
crc_init()
{
    rcc_periph_clock_enable(RCC_CRC);
}

static uint32_t
flash_read()
{
    uint32_t data;

    if ((flash_current < FLASH_START) ||
        (flash_current >= FLASH_END)) {
        elog("read address outside of user flash range");
        return 0;
    }

    data = *(uint32_t *)(flash_current);
    flash_current += sizeof(uint32_t);
    return data;
}

static void
flash_write(uint32_t data)
{
    uint32_t flash_status = 0;

    if ((flash_current < FLASH_START) ||
        (flash_current >= FLASH_END)) {
        elog("write address address outside of user flash range");
        return;
    }

    flash_program_word(flash_current, data);
    flash_status = flash_get_status_flags();
    if (flash_status != FLASH_SR_EOP) {
        elog("error after write %02x", flash_status);
        return;
    }

    flash_current += sizeof(uint32_t);
}

uint8_t
flash_read_start()
{
    if (! flash_crc_check()) {
        elog("flash crc not correct");
        return 0;
    }

    flash_current = FLASH_START;
    flash_byte_len = 0;
    flash_data = 0;

    return 1;
}

uint8_t
flash_read_byte()
{
    uint8_t data;

    if (flash_byte_len == 0) {
        flash_data = flash_read();
        flash_byte_len = sizeof(uint32_t);
    }

    data = (uint8_t)(flash_data >> 24) & 0xff;
    flash_data <<= 8;
    flash_byte_len--;

    return data;
}

void
flash_read_stop()
{
    flash_current = 0;
}

void
flash_write_byte(uint8_t data)
{
    flash_data <<= 8;
    flash_data |= data;

    flash_byte_len++;

    if (flash_byte_len == sizeof(uint32_t)) {
        flash_write(flash_data);
        flash_byte_len = 0;
        flash_data = 0;
    }
}

uint8_t
flash_write_start()
{
    uint8_t i;
    uint32_t flash_status = 0;

    flash_clear_status_flags();
    flash_unlock();
    for (i = 0; i < FLASH_PAGE_NUM; i++) {
        flash_erase_page((uint32_t)&flash[i]);
        flash_status = flash_get_status_flags();
        if (flash_status != FLASH_SR_EOP) {
            elog("page erase %d: status error %02x", i, flash_status);
            return 0;
        }
    }

    flash_current = FLASH_START;
    flash_byte_len = 0;
    flash_data = 0;

    return 1;
}

void
flash_write_stop()
{
    uint32_t crc;

    while (flash_current < FLASH_CRC) {
        flash_write_byte(0);
    }
    flash_write(flash_crc());
    flash_lock();
}

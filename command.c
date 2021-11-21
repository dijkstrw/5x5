/*
 * Copyright (c) 2021 by Willem Dijkstra <wpd@xs4all.nl>.
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

#include "command.h"
#include "config.h"
#include "elog.h"
#include "keyboard.h"
#include "keymap.h"
#include "macro.h"
#include "ring.h"
#include "serial.h"
#include "usb.h"

static uint8_t
hex_digit(uint8_t in)
{
    if (('0' <= in) && (in <= '9')) {
        return (in - '0');
    }
    if (('A' <= in) && (in <= 'F')) {
        return 10 + (in - 'A');
    }
    if (('a' <= in) && (in <= 'f')) {
        return 10 + (in - 'a');
    }
    return 0;
}

static uint8_t
read_hex_8(struct ring *input_ring)
{
    uint8_t c;
    uint16_t result = 0;

    if (ring_read_ch(input_ring, &c) != -1) {
        result = hex_digit(c) << 4;
        if (ring_read_ch(input_ring, &c) != -1) {
            result |= hex_digit(c);
        }
    }
    return result;
}

static void
command_identify(void)
{
    uint8_t i;

    for (i = 0; i < STRI_MAX; i++) {
        printfnl("%02x: %s", i, usb_strings[i]);
    }
}

static void
command_set_keymap(struct ring *input_ring)
{
    uint8_t alayer, arow, acolumn;
    event_t event;

    alayer = read_hex_8(input_ring);
    arow = read_hex_8(input_ring);
    acolumn = read_hex_8(input_ring);

    event.type = read_hex_8(input_ring);
    event.args.num1 = read_hex_8(input_ring);
    event.args.num2 = read_hex_8(input_ring);
    event.args.num3 = read_hex_8(input_ring);

    keymap_set(alayer, arow, acolumn, &event);
}

static void
command_set_macro(struct ring *input_ring)
{
    uint8_t number = read_hex_8(input_ring);
    uint8_t buffer[SERIAL_BUF_SIZEIN];
    uint8_t len = 0;
    uint8_t c;

    while (ring_read_ch(input_ring, &c) != -1) {
        if ((c == '\n') ||
            (c == '\r')) {
            if (len) {
                macro_set_phrase(number, (uint8_t *)&buffer, len);
                return;
            } else {
                elog("macro with empty phrase");
            }
        }

        buffer[len++] = c;

        if (len >= sizeof(buffer)) {
            elog("macro len exceeds buffer");
            return;
        }
    }

    elog("macro not closed of with eol");
}

void
command_process(struct ring *input_ring)
{
    uint8_t c;

    while (ring_read_ch(input_ring, &c) != -1) {
        switch (c) {
            case CMD_IDENTIFY:
                command_identify();
                break;

            case CMD_KEYMAP_DUMP:
                keymap_dump();
                break;

            case CMD_KEYMAP_SET:
                command_set_keymap(input_ring);
                break;

            case CMD_MACRO_CLEAR:
                macro_init();
                break;

            case CMD_MACRO_SET:
                command_set_macro(input_ring);
                break;

            case CMD_NKRO_CLEAR:
                nkro_active = 0;
                printfnl("nkro %d", nkro_active);
                break;

            case CMD_NKRO_SET:
                nkro_active = 1;
                printfnl("nkro %d", nkro_active);
                break;

            case '?':
                printfnl("commands:");
                printfnl("i                - identify");
                printfnl("k                - dump keymap");
                printfnl("Kllrrcctta1a2a3  - set keymap layer, row, column, type, arg1-3");
                printfnl("m                - clear all macro keys");
                printfnl("Mnnstring        - set macro nn with string");
                printfnl("n                - clear nkro");
                printfnl("N                - set nkro");
                break;

            default:
                /* lost sync; process until newline */
                ring_skip_line(input_ring);
                break;
        }
    }
}

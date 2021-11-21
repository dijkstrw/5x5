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

/*
 * macro
 *
 * Insert preset macro sequences
 */

#include <string.h>

#include "automouse.h"
#include "config.h"
#include "elog.h"
#include "extrakey.h"
#include "keyboard.h"
#include "keymap.h"
#include "led.h"
#include "macro.h"
#include "map_ascii.h"
#include "mouse.h"

event_t macro_buffer[MACRO_MAXKEYS][MACRO_MAXLEN];
uint8_t macro_len[MACRO_MAXKEYS];

enum {
    MACRO_INIT,
    MACRO_PRESSED,
    MACRO_UNPRESSED
};

volatile uint8_t macro_active = 0;
volatile uint8_t macro_key;
volatile uint8_t macro_position;
volatile uint8_t macro_operation;

void
macro_init()
{
    elog("macro: clearing all macros");

    memset(&macro_buffer, 0, sizeof(macro_buffer));
    memset(&macro_len, 0, sizeof(macro_len));
}

void
macro_set_phrase(uint8_t key, uint8_t *phrase, uint8_t size)
{
    uint8_t i;
    event_t *event;

    if (key > (MACRO_MAXKEYS - 1)) {
        elog("macro number beyond max");
        return;
    }

    if (size > (MACRO_MAXLEN - 1 - 1)) {
        elog("macro sequence too long");
        return;
    }

    for (i = 0; i < size; i++) {
        event = map_ascii_to_event(*phrase);
        if (event) {
            memcpy(&macro_buffer[key][i], event, sizeof(event_t));
        } else {
            elog("cannot translate %02x to event", *phrase);
            return;
        }
        phrase++;
    }
    macro_len[key] = size;
    elog("macro %d defined with len %d", key, size);
}

void
macro_event(event_t *event, bool pressed)
{
    elog("macro %02x %d", event->macro.number, pressed);

    if (pressed) {
        macro_key = event->macro.number;
        macro_position = 0;
        macro_operation = MACRO_INIT;
        macro_active = 1;
    }
}

static uint8_t
send_if_idle(event_t *event, uint8_t press)
{
    switch (event->type) {
        case KMT_KEY:
            if (usb_ep_keyboard_idle && usb_ep_nkro_idle) {
                keyboard_event(event, press);
            } else {
                return 0;
            }
            break;

        case KMT_MOUSE:
            if (usb_ep_mouse_idle) {
                mouse_event(event, press);
            } else {
                return 0;
            }
            break;

        case KMT_AUTOMOUSE:
            if (usb_ep_mouse_idle) {
                automouse_event(event, press);
            } else {
                return 0;
            }
            break;

        case KMT_WHEEL:
            if (usb_ep_mouse_idle) {
                wheel_event(event, press);
            } else {
                return 0;
            }
            break;

        case KMT_CONSUMER:
            if (usb_ep_extrakey_idle) {
                extrakey_consumer_event(event, press);
            } else {
                return 0;
            }
            break;

        case KMT_SYSTEM:
            if (usb_ep_extrakey_idle) {
                extrakey_system_event(event, 1);
            } else {
                return 0;
            }
            break;
    }
    return 1;
}

void
macro_run()
{
    event_t *event;

    if (macro_active) {
        led_set(MACRO_LED_ACTIVE);
        event = &macro_buffer[macro_key][macro_position];
        switch (macro_operation) {
            case MACRO_INIT:
                if (send_if_idle(event, 1)) {
                    macro_operation = MACRO_PRESSED;
                }
                break;

            case MACRO_PRESSED:
                if (send_if_idle(event, 0)) {
                    macro_operation = MACRO_UNPRESSED;
                }
                break;

            case MACRO_UNPRESSED:
                macro_operation = MACRO_INIT;
                macro_position++;
                if (macro_position >= macro_len[macro_key]) {
                    macro_active = 0;
                    led_set(0);
                }
        }
    }
}

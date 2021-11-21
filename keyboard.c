/*
 * Copyright (c) 2015-2016 by Willem Dijkstra <wpd@xs4all.nl>.
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
 * Keyboard
 *
 * Receive keyboard events and process them into usb communication with the
 * host.
 */

#include "usb.h"
#include "keyboard.h"
#include "usb_keycode.h"
#include "led.h"
#include "elog.h"

static report_keyboard_t keyboard_state;
static bool keyboard_dirty = false;
bool keyboard_active = false;
uint8_t keyboard_idle = 0;

static report_nkro_t nkro_state;
bool nkro_active = false;
static bool nkro_dirty = false;
uint8_t nkro_idle = 0;

void
keyboard_set_protocol(uint8_t protocol)
{
    nkro_active = (protocol == 1);
}

uint8_t *
keyboard_get_protocol()
{
    return (uint8_t *) &nkro_active;
}
report_keyboard_t *
keyboard_report()
{
    return &keyboard_state;
}

report_nkro_t *
nkro_report()
{
    return &nkro_state;
}

void
keyboard_set_leds(uint8_t leds)
{
    led_set(leds);
}

void
keyboard_event(event_t *event, bool pressed)
{
    uint8_t mod = event->key.mod;
    uint8_t key = event->key.code;

    elog("key %02x %02x %d", mod, key, pressed);

    if (mod) {
        if (pressed) {
            keyboard_add_modifier(mod);
        } else {
            keyboard_del_modifier(mod);
        }
    }

    if (key) {
        if (pressed) {
            keyboard_add_key(key);
        } else {
            keyboard_del_key(key);
        }
    }

    if (keyboard_dirty) {
        usb_update_keyboard(&keyboard_state);
        keyboard_dirty = false;
    }

    if (nkro_dirty) {
        usb_update_nkro(&nkro_state);
        nkro_dirty = false;
    }
}

void
keyboard_add_key(uint8_t key)
{
    uint8_t i;
    uint8_t k;

    if (nkro_active) {
        /*
         * NKRO is coded as n bits where each bit corresponds with an pressed
         * key. The first bit corresponds with the first real key that can be
         * pressed, i.e. KEY_A
         */
        k = key - KEY_A;
        if ((k >> 3) < sizeof(nkro_state.bits)) {
            nkro_state.bits[k >> 3] |= (1 << (k & 0x07));
            nkro_dirty = true;
            return;
        }
    }

    /* nkro inactive or keycode too large for nkro, on to boot keyboard */
    for (i = 0; i < sizeof(keyboard_state.keys); i++) {
        if ((keyboard_state.keys[i] == 0) ||
            (keyboard_state.keys[i] == key)) {
            keyboard_state.keys[i] = key;
            keyboard_dirty = true;
            return;
        }
    }
}

void
keyboard_del_key(uint8_t key)
{
    uint8_t i;
    uint8_t k;

    if (nkro_active) {
        k = key - KEY_A;
        if ((k >> 3) < sizeof(nkro_state.bits)) {
            nkro_state.bits[k >> 3] &= ~(1 << (k & 0x07));
            nkro_dirty = true;
            return;
        }
    }

    /* nkro inactive or keycode too large for nkro, on to boot keyboard */

    for (i = 0; i < sizeof(keyboard_state.keys); i++) {
        if (keyboard_state.keys[i] == key) {
            keyboard_state.keys[i] = 0;
            keyboard_dirty = true;
            return;
        }
    }
}

void
keyboard_add_modifier(uint8_t modifier)
{
    keyboard_state.mods |= MODIFIER_BIT(modifier);
    nkro_state.mods |= MODIFIER_BIT(modifier);

    if (nkro_active) {
        nkro_dirty = true;
    } else {
        keyboard_dirty = true;
    }
}

void
keyboard_del_modifier(uint8_t modifier)
{
    keyboard_state.mods &= ~MODIFIER_BIT(modifier);
    nkro_state.mods &= ~MODIFIER_BIT(modifier);

    if (nkro_active) {
        nkro_dirty = true;
    } else {
        keyboard_dirty = true;
    }
}

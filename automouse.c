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
 * automouse
 *
 * Mouse autoclicker -- something my son needed, click mouse button
 * many times, while wiggling the pointer somewhat.
 */

#include "automouse.h"
#include "clock.h"
#include "elog.h"
#include "led.h"
#include "mouse.h"

static report_mouse_t automouse_state;

volatile uint8_t automouse_active = 0;
static volatile uint8_t automouse_press = 0;
static volatile uint8_t automouse_times = 0;
static uint32_t automouse_timer = 0;

void
automouse_event(event_t *event, bool pressed)
{
    elog("automouse %02x %02x %d", event->automouse.button, event->automouse.wiggle, pressed);

    automouse_state.buttons = event->automouse.button;
    automouse_state.x = event->automouse.wiggle;
    automouse_state.y = automouse_state.h = automouse_state.v = 0;

    automouse_times = 1000 / event->automouse.times;
    automouse_timer = timer_set(automouse_times);

    if (pressed) {
        automouse_active = (automouse_active ^ 1);
    }

    if (!automouse_active) {
        led_clear(AUTOMOUSE_LED_ACTIVE | AUTOMOUSE_LED_PRESS);
    }
}

void
automouse_repeat()
{
    if (!usb_ep_mouse_idle) {
        return;
    }

    if (automouse_active &&
        timer_passed(automouse_timer)) {

        automouse_press = (automouse_press ^ 1);

        if (automouse_press) {
            led_state(AUTOMOUSE_LED_ACTIVE | AUTOMOUSE_LED_PRESS);
            mouse_state.buttons = automouse_state.buttons;
            mouse_state.x = automouse_state.x;
            mouse_state.y = automouse_state.y;
        } else {
            mouse_state.buttons = 0;
            led_state(AUTOMOUSE_LED_ACTIVE);
            mouse_state.x = -automouse_state.x;
            mouse_state.y = -automouse_state.y;
        }
        mouse_state.h = automouse_state.v = 0;
        usb_update_mouse(&mouse_state);

        automouse_timer = timer_set(automouse_times);
    }
}

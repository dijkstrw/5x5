/*
 * Copyright (c) 2015-2022 by Willem Dijkstra <wpd@xs4all.nl>.
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

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>

#include "automouse.h"
#include "clock.h"
#include "elog.h"
#include "keyboard.h"
#include "led.h"
#include "macro.h"
#include "matrix.h"
#include "mouse.h"
#include "serial.h"
#include "usb.h"

static bool enumeration_active;

static void
mcu_init(void)
{
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE16_72MHZ]);
}

/*
 * Usb Event handlers
 */
void
usb_reset(void)
{
    elog("usb reset");
    enumeration_active = true;
}

void
usb_resume(void)
{
    elog("usb resume");
}

void
usb_suspend(void)
{
    elog("usb suspend");
}

int
main(void)
{
    uint32_t enumeration_timer;

    mcu_init();

    usb_prevent_enumeration();

    clock_init();
    serial_init();
    led_init();
    matrix_init();
    macro_init();
    usb_init();

    elog("initialized");

    enumeration_active = true;

    while (1) {
        if (enumeration_active) {
            /*
             * Note that this is the start state, but renewing
             * enumeration can also be requested by the host at any
             * time using an usb reset.
             *
             * This phase can complete partially. Keyboard and mouse
             * are standard, but our serial comms could require a
             * driver and not be enumerated succesfully.
             */
            usb_ifs_enumerated = 0;
            enumeration_timer = timer_set(MS_ENUMERATE);
            while ((usb_ifs_enumerated != ((1 << IF_KEYBOARD) |
                                           (1 << IF_MOUSE)    |
                                           (1 << IF_EXTRAKEY) |
                                           (1 << IF_NKRO)     |
                                           (1 << IF_SERIALCOMM))) &&
                   (!timer_passed(enumeration_timer))) {
                __asm__("nop");
            }

            if (!usb_ifs_enumerated) {
                elog("enumeration failed");
                scb_reset_system();
            }

            enumeration_active = false;
            keyboard_active = serial_active = true;
        }

        if (serial_active) {
            serial_out();
        }

        if (keyboard_active) {
            matrix_process();
        }

        if (automouse_active) {
            automouse_repeat();
        }

        if (macro_active) {
            macro_run();
        }
    }
}
